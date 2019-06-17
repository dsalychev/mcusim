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
#ifndef MSIM_AVR_INTERRUPT_H_
#define MSIM_AVR_INTERRUPT_H_ 1

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "mcusim/avr/sim/io.h"

/* AVR IRQ limit, i.e. maximum number of interrupt vectors. */
#define MSIM_AVR_IRQNUM			64

/* Forward declaration of the structure to describe AVR microcontroller
 * instance. */
struct MSIM_AVR;

/* Main structure to describe AVR interrupts within the simulated AVR
 * instance (reset address, IRQs, etc.) */
typedef struct MSIM_AVR_INT {
	uint32_t reset_pc;		/* Reset address */
	uint32_t ivt;			/* Interrupt vectors table address */
	uint8_t irq[MSIM_AVR_IRQNUM];	/* Flags for interrupt requests */
	uint8_t exec_main;		/* Exe instruction from the main
					   program after an exit from ISR */
	uint8_t trap_at_isr;		/* Flag to enter stopped mode when
					   interrupt occured */
} MSIM_AVR_INT;

/* Interrupt vector */
typedef struct MSIM_AVR_INTVec {
	MSIM_AVR_IOBit enable;		/* Interrupt "enabled" flag */
	MSIM_AVR_IOBit raised;		/* Interrupt flag */
	uint8_t vector;			/* Interrupt address (in IVT) */
	uint8_t pending;		/* Pending interrupt flag */
} MSIM_AVR_INTVec;

#ifdef __cplusplus
}
#endif

#endif /* MSIM_AVR_INTERRUPT_H_ */

