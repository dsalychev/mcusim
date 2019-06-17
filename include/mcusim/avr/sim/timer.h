/*
 * Copyright 2017-2019 The MCUSim Project.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the MCUSim or its parts nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * General-purpose AVR timer. It's supposed to be suitable for any AVR MCU.
 */
#ifndef MSIM_AVR_TIMER_H_
#define MSIM_AVR_TIMER_H_ 1

#define MSIM_AVR_TMR_STOPMODE		(-75)
#define MSIM_AVR_TMR_EXTCLK_RISE	(-76)
#define MSIM_AVR_TMR_EXTCLK_FALL	(-77)

/* Return codes of the timer functions. */
#define MSIM_AVR_TMR_OK			0
#define MSIM_AVR_TMR_NULL		75

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>

#include "mcusim/mcusim.h"
#include "mcusim/avr/sim/io.h"
#include "mcusim/avr/sim/interrupt.h"

/* Waveform generation modes */
enum {
	MSIM_AVR_TMR_WGM_None = 0,
	MSIM_AVR_TMR_WGM_Normal,	/* Normal mode */
	MSIM_AVR_TMR_WGM_CTC,		/* Clear timer on Compare Match */
	MSIM_AVR_TMR_WGM_PWM,		/* PWM */
	MSIM_AVR_TMR_WGM_FastPWM,	/* Fast PWM */
	MSIM_AVR_TMR_WGM_PCPWM,		/* Phase Correct PWM */
	MSIM_AVR_TMR_WGM_PFCPWM,	/* Phase and frequency correct PWM */
};

/* When to update buffered values */
enum {
	MSIM_AVR_TMR_UPD_ATNONE = 0,		/* Unknown value */
	MSIM_AVR_TMR_UPD_ATMAX,			/* At MAX value */
	MSIM_AVR_TMR_UPD_ATTOP,			/* At TOP value */
	MSIM_AVR_TMR_UPD_ATBOTTOM,		/* At BOTTOM value */
	MSIM_AVR_TMR_UPD_ATIMMEDIATE,		/* Immediately */
	MSIM_AVR_TMR_UPD_ATCM,			/* At Compare Match */
};

/* Output compare pin action. */
enum {
	MSIM_AVR_TMR_COM_DISC = 0,		/* Disconnected */
	MSIM_AVR_TMR_COM_TGONCM,		/* Toggle @ Compare Match */
	MSIM_AVR_TMR_COM_CLONCM,		/* Clear @ CM */
	MSIM_AVR_TMR_COM_STONCM,		/* Set @ CM */
	MSIM_AVR_TMR_COM_CLONCM_STATBOT,	/* Clear@CM, Set@BOTTOM */
	MSIM_AVR_TMR_COM_STONCM_CLATBOT,	/* Set@CM, Clear@BOTTOM */
	MSIM_AVR_TMR_COM_CLONUP_STONDOWN,	/* Clear@CM(UP), Set@CM(DOWN) */
	MSIM_AVR_TMR_COM_STONUP_CLONDOWN,	/* Set@CM(UP), Clear@CM(DOWN) */
};

/* Timer count direction */
enum {
	MSIM_AVR_TMR_CNTUP = 0,
	MSIM_AVR_TMR_CNTDOWN,
};

/* Waveform generator module */
typedef struct MSIM_AVR_TMR_WGM {
	uint8_t kind;				/* WGM type */
	uint8_t size;				/* Size, in bits */
	uint32_t top;				/* Fixed TOP value */
	uint32_t bottom;			/* Fixed BOTTOM value */
	uint8_t updocr_at;			/* Update OCR at */
	uint8_t settov_at;			/* Set TOV at */

	struct MSIM_AVR_IOBit rtop[4];		/* Register as TOP value */
	uint32_t rtop_buf;
} MSIM_AVR_TMR_WGM;

/* Comparator module */
typedef struct MSIM_AVR_TMR_COMP {
	struct MSIM_AVR_IOBit ocr[4];		/* Comparator register */
	struct MSIM_AVR_IOBit pin;		/* Pin to output waveform */
	struct MSIM_AVR_IOBit ddp;		/* Data direction for pin */
	uint32_t ocr_buf;			/* Buffered value of OCR */

	struct MSIM_AVR_IOBit com;		/* Comparator output mode */
	uint8_t com_op[16][16];			/* mode: [WGM][COM] */

	struct MSIM_AVR_INTVec iv;		/* Interrupt vector */
} MSIM_AVR_TMR_COMP;

/* AVR timer/counter */
typedef struct MSIM_AVR_TMR {
	struct MSIM_AVR_IOBit tcnt[4];		/* Timer counter */
	struct MSIM_AVR_IOBit disabled;		/* "disabled" bit */
	uint32_t scnt;				/* System clock counter */
	uint8_t cnt_dir;			/* Count direction */
	uint8_t size;				/* Resolution, in bits */

	struct MSIM_AVR_IOBit cs[4];		/* Clock source */
	uint8_t cs_div[16];			/* CS bits to prescaler */
	uint32_t presc;				/* Current prescaler */

	struct MSIM_AVR_IOBit ec_pin;		/* External clock pin */
	uint8_t ec_vold;			/* Old value of the ec pin */
	uint32_t ec_flags;			/* External clock flags */

	struct MSIM_AVR_IOBit wgm[4];		/* Waveform generation mode */
	struct MSIM_AVR_TMR_WGM wgm_op[16];	/* WGM types */
	struct MSIM_AVR_TMR_WGM *wgmval;	/* Current WGM type */
	int32_t wgmi;				/* Current WGM type (index) */

	struct MSIM_AVR_IOBit icr[4];		/* Input capture register */
	struct MSIM_AVR_IOBit icp;		/* Input capture pin */
	struct MSIM_AVR_IOBit ices[4];		/* Input capture edge select */
	uint8_t icpval;

	struct MSIM_AVR_INTVec iv_ovf;		/* Overflow */
	struct MSIM_AVR_INTVec iv_ic;		/* Input capture */

	struct MSIM_AVR_TMR_COMP comp[16];	/* Output compare channels */
} MSIM_AVR_TMR;

int MSIM_AVR_TMRUpdate(struct MSIM_AVR *mcu);

#ifdef __cplusplus
}
#endif

#endif /* MSIM_AVR_TIMER_H_ */
