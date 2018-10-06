/*
 * Copyright (c) 2017, 2018, The MCUSim Contributors
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the MCUSim or its parts nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
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
 * This is a main header file which contains declarations to describe the
 * whole simulated microcontroller, and it's supposed to be AVR-agnostic.
 * It means that each declaration should be suitable for every available AVR
 * model.
 */
#ifndef MSIM_AVR_SIM_H_
#define MSIM_AVR_SIM_H_ 1

#ifndef MSIM_MAIN_HEADER_H_
#error "Please, include mcusim/mcusim.h instead of this header."
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* Forward declaration of the structure to describe AVR microcontroller
 * instance. */
struct MSIM_AVR;

/* MCU-specific functions.
 *
 * Simulated microcontroller may provide its own implementations of the
 * functions in order to support these features (fuses, locks, timers,
 * IRQs, etc.). ATmega8A is a good example of MCU to understand how
 * these functions can be declared (mcusim/avr/sim/simm8a.h) and
 * implemented (simm8a.c).
 */
typedef int (*MSIM_AVR_SetFuse_f)(struct MSIM_AVR *mcu, uint32_t fuse_n,
                                  uint8_t fuse_v);
typedef int (*MSIM_AVR_SetLock_f)(struct MSIM_AVR *mcu, uint8_t lock_v);
typedef int (*MSIM_AVR_TickTimers_f)(struct MSIM_AVR *mcu);
typedef int (*MSIM_AVR_ProvideIRQs_f)(struct MSIM_AVR *mcu);

/* State of a simulated AVR microcontroller. Some of these states are
 * AVR-native, others - added by the simulator to manipulate a simulation
 * process. */
enum MSIM_AVR_State {
	AVR_RUNNING,
	AVR_STOPPED,
	AVR_SLEEPING,

	/* Execute next instruction. */
	AVR_MSIM_STEP,
	/* Terminate simulation and exit. */
	AVR_MSIM_STOP,
	/* Terminate simulation because of test failure. */
	AVR_MSIM_TESTFAIL
};

enum MSIM_AVR_ClkSource {
	AVR_INT_CLK,
	AVR_EXT_CLK,
	AVR_LOWP_CRYSTAL_CLK,		/* Low power crystal */
	AVR_FULLSWING_CRYSTAL_CLK,	/* Full swing crystal */
	AVR_LOWFREQ_CRYSTAL_CLK,	/* Low frequency crystal */
	AVR_EXT_LOWF_CRYSTAL_CLK,	/* External low-freq crystal */
	AVR_INT_CAL_RC_CLK,		/* Internal calibrated RC */
	AVR_INT_128K_RC_CLK,	/* Internal 128kHz RC Oscillator*/
	AVR_EXT_RC_CLK,			/* External RC */
	AVR_EXT_CRYSTAL,		/* External crystal/ceramic resonator*/
	AVR_INT_128K_RC_CLK		/* Internal 128kHz RC Oscillator*/
};

enum MSIM_AVR_SREGFlag {
	AVR_SREG_CARRY,
	AVR_SREG_ZERO,
	AVR_SREG_NEGATIVE,
	AVR_SREG_TWOSCOM_OF,
	AVR_SREG_SIGN,
	AVR_SREG_HALF_CARRY,
	AVR_SREG_T_BIT,
	AVR_SREG_GLOB_INT
};
/* Instance of the AVR microcontroller. */
struct MSIM_AVR {
	char name[20];			/* Name of the MCU */
	unsigned char signature[3];	/* Signature of the MCU */
	unsigned char xmega;		/* XMEGA flag */
	unsigned char reduced_core;	/* Reduced core flag */

	unsigned long flashstart;	/* First program memory byte */
	unsigned long flashend;		/* Last program memory byte */
	unsigned long ramstart;		/* First on-chip SRAM byte */
	unsigned long ramend;		/* Last on-chip SRAM byte */
	unsigned long ramsize;		/* On-chip SRAM size, in bytes */
	unsigned int e2start;		/* First EEPROM byte */
	unsigned int e2end;		/* Last EEPROM byte */
	unsigned int e2size;		/* EEPROM size, in bytes */
	unsigned int e2pagesize;	/* EEPROM page size, in bytes */
	unsigned char lockbits;		/* Lock bits of the MCU */
	unsigned char fuse[6];		/* Fuse bytes of the MCU */

	unsigned int spm_pagesize;	/* For devices with bootloader support,
					   the flash pagesize (in bytes) to be
					   used for Self Programming Mode (SPM)
					   instruction. */
	unsigned char *spmcsr;		/* Store Program Memory Control
					   and Status Register (SPMCSR). */

	struct MSIM_AVR_Bootloader *bls; /* Bootloader Section */
	enum MSIM_AVR_State state;	 /* State of the MCU */
	enum MSIM_AVR_ClkSource clk_source; /* Clock source */

	unsigned long freq;		/* Current MCU frequency, Hz */
	unsigned char pc_bits;		/* 16-bit PC, 22-bit PC, etc. */
	unsigned long pc;		/* Current program counter */
	struct MSIM_AVR_Int *intr;	/* Interrupts and IRQs */
	unsigned char ic_left;		/* Cycles left to finish current
					   instruction */
	unsigned char in_mcinst;	/* Multi-cycle instruction flag */

	unsigned char *sreg;		/* SREG in the data memory */
	unsigned char *sph;		/* SP(high) in the data memory */
	unsigned char *spl;		/* SP(low) in the data memory */

	unsigned char *eind;		/* Extended indirect register.*/
	unsigned char *rampz;		/* Extended Z-pointer register */
	unsigned char *rampy;		/* Extended Y-pointer register */
	unsigned char *rampx;		/* Extended X-pointer register */
	unsigned char *rampd;		/* Extended direct register
					   NOTE: Email me at darkness.bsd at
					   gmail.com if you're aware of
					   AVR MCUs which use this register */

	unsigned char *pm;		/* Program memory (PM) */
	unsigned char *pmp;		/* Page buffer of PM */
	unsigned char *dm;		/* GP, I/O registers and SRAM */ 
	// What is size of dm? 
	unsigned char *mpm;		/* Memory to store instructions at
					   breakpoints */
	unsigned long pm_size;		/* Allocated size of PM */
	unsigned long dm_size;		/* Allocated size of the data memory */
	unsigned char read_from_mpm;	/* Flag to read from breakpoints
					   memory, it is 0 usually */

	unsigned int sfr_off;		/* Offset to the AVR special function
					   registers */
	unsigned int regs;		/* Number of GP registers */
	unsigned int io_regs;		/* Number of all I/O registers */

	MSIM_AVR_SetFuse_f set_fusef;	/* Function to set AVR fuse byte */
	MSIM_AVR_SetLock_f set_lockf;	/* Function to set AVR lock byte */
	MSIM_AVR_TickTimers_f tick_timers; /* Function to tick 8-bit timers */
	MSIM_AVR_ProvideIRQs_f provide_irqs; /* Function to check MCU flags and
					        set IRQs accordingly */

	struct MSIM_AVR_VCDDetails *vcdd; /* Details about registers to
					     be dumped into VCD file */
};

/* Structure to describe a memory operation requested by user. */
struct MSIM_AVR_MemOp {
	char memtype[16];		/* Type of MCU memory */
	char operation;			/* Memory operation */
	char operand[4096];		/* Path to file, value, etc. */
	char format;			/* Optional, value format */
};

#ifdef __cplusplus
}
#endif

#endif /* MSIM_AVR_SIM_H_ */
