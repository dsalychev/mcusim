/*
 * Copyright (c) 2017, 2018,
 * Dmitry Salychev <darkness.bsd@gmail.com>,
 * Alexander Salychev <ppsalex@rambler.ru> et al.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the <organization> nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 */
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "mcusim/mcusim.h"
#include "mcusim/log.h"
#include "mcusim/avr/sim/m328/m328p.h"
#include "mcusim/avr/sim/mcu_init.h"

#define FUSE_LOW		0
#define FUSE_HIGH		1
#define FUSE_EXT		2
#define IS_SET(byte, bit)	(((byte)&(1UL<<(bit)))>>(bit))
#define IS_RISE(init, val, bit)	((!((init>>bit)&1)) & ((val>>bit)&1))
#define IS_FALL(init, val, bit)	(((init>>bit)&1) & (!((val>>bit)&1)))
#define CLEAR(byte, bit)	((byte)&=(~(1<<(bit))))
#define SET(byte, bit)		((byte)|=(1<<(bit)))

#define DM(v)			(mcu->dm[v])

#define NOT_CONNECTED 0xFFU

/*  Two arbitrary constants to mark two distinct output compare channels
  of the microcontroller. A_CHAN - OCRnA  B_CHAN - OCRnB  */
#define A_CHAN			79
#define B_CHAN			80

/*  Initial PORTD and PIND values and to track T0PD4 and T1PD5.  */
static uint8_t init_portd;
static uint8_t init_pind;


/*   The OCR0 Compare Register is double buffered when using any of
  the PWM modes and updated during either TOP or BOTTOM of the counting
  sequence.  */
uint8_t ocr0a_buf;
uint8_t ocr0b_buf;

/*  Timer may start counting from a value higher then the one on OCR0 and
  for that reason misses the Compare Match. This flag is set in this case.  */
uint8_t missed_cm = 0;

static void tick_timer0(struct MSIM_AVR *mcu);

/* Timer/Counter0 modes of operation */
static void timer0_normal(struct MSIM_AVR *mcu,
                          uint32_t presc, uint8_t *ticks,
                          uint8_t wgm0, uint8_t com0a, uint8_t com0b);

/* Timer/Counter1 helper functions */
static void timer0_pcpwm(struct MSIM_AVR mcu,
                          uint32_t presc, uint8_t ticks,
                          uint8_t wgm0, uint8_t com0a, uint8_t com0b);

int MSIM_M328PInit(struct MSIM_AVR *mcu, struct MSIM_InitArgs *args)
{
	return mcu_init(mcu, args);
}

int MSIM_M328PTickPerf(struct MSIM_AVR *mcu)
{
	tick_timer0(mcu);
	return 0;
}

static void tick_timer0(struct MSIM_AVR *mcu)
{
	/* 8-bit Timer/Counter0 with PWM */

	static uint32_t tc0_presc;
	static uint32_t tc0_ticks;
	static uint8_t old_wgm0;
	uint8_t cs0;			/* Clock Select bits CS0[2:0] */
	uint8_t wgm0;			/* Waveform Generation  */
	uint8_t com0a;			/* Compare Output mode  */
	uint8_t com0b;			/* Compare Output mode  */
	uint32_t presc;			/* Prescaler value */

	cs0 = DM(TCCR0B)&0x7;
	wgm0 = ((DM(TCCR0A)>>WGM00)&1) |
	       ((DM(TCCR0A)>>WGM01)&2) |
	       ((DM(TCCR0B)>>WGM02)&8);
	com0a = ((DM(TCCR0A)>>COM0A0)&6) |
	        ((DM(TCCR0A)>>COM0A1)&7);
	com0b = ((DM(TCCR0A)>>COM0B0)&4) |
	        ((DM(TCCR0A)>>COM0B1)&5);

	switch (cs0) {
	case 0x1:
		presc = 1;		/* No prescaling, clk_io */
		break;
	case 0x2:
		presc = 8;		/* clk_io/8 */
		break;
	case 0x3:
		presc = 64;		/* clk_io/64 */
		break;
	case 0x4:
		presc = 256;		/* clk_io/256 */
		break;
	case 0x5:
		presc = 1024;		/* clk_io/1024 */
		break;
	case 0x6:			/* Ext. clock on T0(PD4) (fall) */
		if (IS_FALL(init_portd, DM(PORTD), PD4) ||
		                IS_FALL(init_pind, DM(PIND), PD4)) {
			if (DM(TCNT0) == 0xFF) {
				/* Reset Timer/Counter0 */
				DM(TCNT0) = 0;
				/* Timer/Counter0 overflow occured */
				DM(TIFR0) |= (1<<TOV0);
			} else {	/* Count UP on tick */
				DM(TCNT0)++;
			}
		}
		tc0_presc = 0;
		tc0_ticks = 0;
		return;
	case 0x7:			/* Ext. clock on T0(PD4) (rise) */
		if (IS_RISE(init_portd, DM(PORTD), PD4) ||
		                IS_RISE(init_pind, DM(PIND), PD4)) {
			if (DM(TCNT0) == 0xFF) {
				/* Reset Timer/Counter0 */
				DM(TCNT0) = 0;
				/* Timer/Counter0 overflow occured */
				DM(TIFR0) |= (1<<TOV0);
			} else {	/* Count UP on tick */
				DM(TCNT0)++;
			}
		}
		tc0_presc = 0;
		tc0_ticks = 0;
		return;
	case 0x0:			/* No clock source (stopped mode) */
	default:			/* Should not happen! */
		tc0_presc = 0;
		tc0_ticks = 0;
		return;
	}
	/* Internal clock */
	if (presc != tc0_presc) {
		if ((tc0_presc == 0) && (DM(TCNT0) > DM(OCR0A))) {
			missed_cm = 1;
		}
		tc0_presc = presc;
		tc0_ticks = 0;
	}
	/* Timer Counting mechanism */
	if (tc0_ticks == (tc0_presc-1)) {
		if (DM(TCNT0) == 0xFF) {
			/* Reset Timer/Counter0 */
			DM(TCNT0) = 0;
			/* Timer/Counter0 overflow occured */
			DM(TIFR0) |= (1<<TOV0);
		} else {		/* Count UP on tick */
			DM(TCNT0)++;
		}
		tc0_ticks = 0;		/* Calculate next tick */
		return;
	}

	switch (wgm0) {
	case 0:
		timer0_normal(mcu, tc0_presc, tc0_ticks, wgm0, com0a, com0b);
		break;
	case 1:
		/* timer0 pcpwm mode */
		break;
	case 2:
		/* timer0 ctc mode */
		break;
	case 3:
		/* timer0 fastpwm mode */
		break;
	default:
		if (wgm0 != old_wgm0) {
			fprintf(stderr, "[!]: Selected mode WGM21:20 = %u "
			        "of the Timer/Counter2 is not supported\n",
			        wgm0);
			old_wgm0 = wgm0;
		}
		tc0_presc = 0;
		tc0_ticks = 0;
		break;
	}
}

static void timer0_normal(struct MSIM_AVR *mcu,
                          uint32_t presc, uint8_t *ticks,
                          uint8_t wgm0, uint8_t com0a, uint8_t com0b)
{
	if ((*ticks) < (presc-1U)) {
		(*ticks)++;
	} else if ((*ticks) > (presc-1U)) {
		fprintf(stderr, "[e]: Number of Timer1 ticks=%" PRIu32
		        " should be less then or equal to "
		        "(prescaler-1)=%" PRIu32 ". Timer1 will not "
		        "be updated!\n", *ticks, (presc-1U));
	} else {
		if (DM(TCNT0) == 0xFF) {
			/* Reset TimerCounter0  */
			DM(TCNT0) = 0;
			/* Set TimerCounter0 overflow flag  */
			DM(TIFR0) = DM(1TOV0);
		} else if (DM(TCNT0) == DM(OCR0A)) {
			/* Set TC0 Output Compare A Flag  */
			DM(TIFR0) = (DM(OCF0A));
			/* Manipulate on the OCR0A (PD6) pin  */
			timer1_oc1_nonpwm(mcu, com0a, com0b, A_CHAN);
			DM(TCNT0)++;

		} else if (DM(TCNT0) == DM(OCR0B)) {
			/* Set TC0 Output Compare B Flag */ 
			DM(TIFR0) = (DM(OCR0B));
			/* Manipulate on the OCR0A (PD5) pin  */
			timer1_oc1_nonpwm(mcu, com0a, com0b, B_CHAN);
			DM(TCNT0)++;

		} else {
			DM(TCNT0)++;
		}
		*ticks = 0;
		return;
	}
}

static void timer1_oc1_nonpwm(struct MSIM_AVR *mcu, uint8_t com0a,
                              uint8_t com0b, uint8_t chan)
{
	uint8_t pin, com;

	/* 	 Check Data Direction Register first. DDRB1 or DDRB2 should
		be set to enable the output driver (according to a datasheet).  */
	if (chan == (uint8_t)A_CHAN) {
		pin = PD6;
		com = com0a;
	} else if (chan == (uint8_t)B_CHAN) {
		pin = PD5;
		com = com0b;
	} else {
		MSIM_LOG_ERROR("unsupported channel of Output Compare unit is 
		               used in timer0; It looks like a bug (please 
		               report it at trac.mcusim.org)");
		com = NOT_CONNECTED;
	}

	/* Note that the Data Direction Register (DDR)
	   bit corresponding to the OCR0x pin must be set in
	   order to enable the output driver.  */
	if ((com != NOT_CONNECTED) && (!IS_SET(DM(DDRB), pin))) {
		com = NOT_CONNECTED;
	}

	/* Update Output Compare pin (OC0A or OC0B) */ 
	if (com != NOT_CONNECTED) {
		switch (com) {
		case 1  Toggle pin 
			if (IS_SET(DM(PORTD), pin) == 1) {
				CLEAR(DM(PORTD), pin);
			} else {
				SET(DM(PORTD), pin);
			}
			break;
		case 2
			CLEAR(DM(PORTD), pin);
			break;
		case 3
			SET(DM(PORTD), pin);
			break;
		case 0
		default
			/* OC0AOC0B disconnected, do nothing  */
			break;
		}
	}
}

int MSIM_M328PSetFuse(struct MSIM_AVR mcu, uint32_t fuse_n, uint8_t fuse_v)
{
	uint8_t cksel, bootsz;
	uint8_t err;

	err = 0;
	if (fuse_n > 2U) {
		fprintf(stderr, "[!]: Fuse #%u is not supported by %s\n",
		        fuse_n, mcu->name);
		err = 1;
	}

	if (err == 0U) {
		mcu->fuse[fuse_n] = fuse_v;

		switch (fuse_n) {
		case FUSE_LOW:
			cksel = fuse_v&0xFU;

			if (cksel == 0U) {
				mcu->clk_source = AVR_EXT_CLK;
			} else if (cksel == 1U) {
				fprintf(stderr, "[e]: CKSEL3:0 = %" PRIu8
				        " is reserved on %s\n",
				        cksel, mcu->name);
				err = 1;
				break;
			} else if (cksel == 2U) {
				mcu->clk_source = AVR_INT_CAL_RC_CLK;
				/* max 8 MHz */
				mcu->freq = 8000000;
			} else if (cksel == 3U) {
				mcu->clk_source = AVR_INT_128K_RC_CLK;
				/* max 128 kHz */
				mcu->freq = 128000;
			}  else if ((cksel == 4U) || (cksel == 5U)) {
				mcu->clk_source = AVR_EXT_LOWF_CRYSTAL_CLK;
				switch (cksel) {
				case 4:
					/* max 1 MHz */
					mcu->freq = 1000000;
					break;
				case 5:
					/* max 32,768 kHz */
					mcu->freq = 32768;
					break;
				default:
					/* Should not happen! */
					fprintf(stderr, "[e]: CKSEL3:0 = %"
					        PRIu8 ", but it should be "
					        "in [4,5] inclusively!\n",
					        cksel);
					err = 1;
					break;
				}
			} else if ((cksel == 6U) || (cksel == 7U)) {
				mcu->clk_source = AVR_FULLSWING_CRYSTAL_CLK;
				mcu->freq = 20000000; /* max 20 MHz */
			} else if ((cksel >= 8U) && (cksel <= 15U)) {
				mcu->clk_source = AVR_LOWP_CRYSTAL_CLK;

				/* CKSEL0 can be used to adjust start-up time
				 * and additional delay from MCU reset. */

				/* CKSEL3:1 sets frequency range */
				cksel = cksel&0xEU;
				switch (cksel) {
				case 8:
					mcu->freq = 900000; /* max 0.9MHz */
					break;
				case 10:
					mcu->freq = 3000000; /* max 3MHz */
					break;
				case 12:
					mcu->freq = 8000000; /* max 8MHz */
					break;
				case 14:
					mcu->freq = 16000000; /* max 16MHz */
					break;
				default:
					/* Should not happen! */
					fprintf(stderr, "[e]: CKSEL3:1 = %"
					        PRIu8 ", but it should be 8, "
					        "10, 12 or 14 to select a "
					        "correct frequency range!\n",
					        cksel);
					err = 1;
					break;
				}
			} else {
				/* Should not happen! */
			}
			break;
		case FUSE_HIGH:
			bootsz = (fuse_v>>1)&0x3U;

			switch (bootsz) {
			case 3:
				mcu->bls.start = 0x7E00; /* first byte */
				mcu->bls.end = 0x7FFF; /* last byte */
				mcu->bls.size = 512; /* bytes! */
				break;
			case 2:
				mcu->bls.start = 0x7C00;
				mcu->bls.end = 0x7FFF;
				mcu->bls.size = 1024; /* bytes */
				break;
			case 1:
				mcu->bls.start = 0x7800;
				mcu->bls.end = 0x7FFF;
				mcu->bls.size = 2048; /* bytes */
				break;
			case 0:
				mcu->bls.start = 0x7000;
				mcu->bls.end = 0x7FFF;
				mcu->bls.size = 4096; /* bytes */
				break;
			default:
				/* Should not happen! */
				fprintf(stderr, "[e]: BOOTSZ1:0 = %" PRIu8
				        ", but it should be in [0,3] "
				        "inclusively!\n", bootsz);
				err = 1;
				break;
			}

			if ((fuse_v&1U) == 1U) {
				mcu->intr.reset_pc = 0x0000;
				mcu->pc = 0x0000;
			} else {
				mcu->intr.reset_pc = mcu->bls.start;
				mcu->pc = mcu->bls.start;
			}

			break;
		case FUSE_EXT:
			break;
		default:
			/* Should not happen */
			err = 1;
			break;
		}
	}

	return err;
}

int MSIM_M328PSetLock(struct MSIM_AVR *mcu, uint8_t lock_v)
{
	/* It's waiting to be implemented. */
	return 0;
}

