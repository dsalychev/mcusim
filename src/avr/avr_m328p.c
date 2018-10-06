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

#include "mcusim/avr/sim/m328p.h"
#include "mcusim/avr/sim/mcu_init.h"

#define FUSE_LOW		0
#define FUSE_HIGH		1
#define FUSE_EXT		2
#define IS_SET(byte, bit)	(((byte)&(1UL<<(bit)))>>(bit))
#define IS_RISE(init, val, bit)	((!((init>>bit)&1)) & ((val>>bit)&1))
#define IS_FALL(init, val, bit)	(((init>>bit)&1) & (!((val>>bit)&1)))
#define CLEAR(byte, bit)	((byte)&=(~(1<<(bit))))
#define SET(byte, bit)		((byte)|=(1<<(bit)))

int MSIM_M328PInit(struct MSIM_AVR *mcu, struct MSIM_InitArgs *args)
{
	return mcu_init(mcu, args);
}

int MSIM_M328PSetFuse(struct MSIM_AVR *mcu, uint32_t fuse_n, uint8_t fuse_v)
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
				mcu->bls->start = 0x7E00; /* first byte */
				mcu->bls->end = 0x7FFF; /* last byte */
				mcu->bls->size = 512; /* bytes! */
				break;
			case 2:
				mcu->bls->start = 0x7C00;
				mcu->bls->end = 0x7FFF;
				mcu->bls->size = 1024; /* bytes */
				break;
			case 1:
				mcu->bls->start = 0x7800;
				mcu->bls->end = 0x7FFF;
				mcu->bls->size = 2048; /* bytes */
				break;
			case 0:
				mcu->bls->start = 0x7000;
				mcu->bls->end = 0x7FFF;
				mcu->bls->size = 4096; /* bytes */
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
				mcu->intr->reset_pc = 0x0000;
				mcu->pc = 0x0000;
			} else {
				mcu->intr->reset_pc = mcu->bls->start;
				mcu->pc = mcu->bls->start;
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

