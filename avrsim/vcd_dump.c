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
#include <stdio.h>
#include <stdint.h>
#include <time.h>

#include "mcusim/avr/sim/sim.h"
#include "mcusim/avr/sim/vcd_dump.h"

#define TERA			1000000000000.0
#define MAX_CLK_PRINTS		50

static void print_reg(char *buf, unsigned int len, unsigned char r);

FILE *MSIM_VCDOpenDump(void *vmcu, const char *dumpname)
{
	FILE *f;
	time_t timer;
	char buf[32];
	struct tm *tm_info;
	unsigned int i, regs;
	struct MSIM_AVR *mcu;
	struct MSIM_VCDRegister *reg;

	mcu = (struct MSIM_AVR *)vmcu;
	regs = sizeof mcu->vcd_regsn/sizeof mcu->vcd_regsn[0];

	f = fopen(dumpname, "w");
	if (!f)
		return NULL;

	time(&timer);
	tm_info = localtime(&timer);
	strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%S%z", tm_info);

	/* Printing VCD header */
	fprintf(f, "$date %s $end\n", buf);
	fprintf(f, "$version MCUSim %s $end\n", MSIM_VERSION);
	fprintf(f, "$comment It is a dump of simulated %s $end\n", mcu->name);
	fprintf(f, "$timescale %lu ps $end\n",
		(unsigned long)(((1.0/(double)mcu->freq)*TERA)/2.0));
	fprintf(f, "$scope module %s $end\n", mcu->name);

	/* Declare variables to dump */
	fprintf(f, "$var reg 1 CLK_IO CLK_IO $end\n");
	for (i = 0; i < regs; i++) {
		if (mcu->vcd_regsn[i] < 0)
			break;
		reg = &mcu->vcd_regs[mcu->vcd_regsn[i]];
		fprintf(f, "$var reg 8 %s %s $end\n", reg->name, reg->name);
	}
	fprintf(f, "$upscope $end\n");
	fprintf(f, "$enddefinitions $end\n");

	/* Dumping initial register values */
	fprintf(f, "$dumpvars\n");
	fprintf(f, "b0 CLK_IO\n");
	for (i = 0; i < regs; i++) {
		if (mcu->vcd_regsn[i] < 0)
			break;

		reg = &mcu->vcd_regs[mcu->vcd_regsn[i]];
		print_reg(buf, sizeof buf, *reg->addr);
		fprintf(f, "b%s %s\n", buf, reg->name);
	}
	fprintf(f, "$end\n");

	return f;
}

void MSIM_VCDDumpFrame(FILE *f, void *vmcu, unsigned long tick,
		       unsigned char fall)
{
	static unsigned int clk_prints_left;
	unsigned int i, regs;
	char buf[32], print_tick, new_value;
	struct MSIM_AVR *mcu;
	struct MSIM_VCDRegister *reg;

	mcu = (struct MSIM_AVR *)vmcu;
	regs = sizeof mcu->vcd_regsn/sizeof mcu->vcd_regsn[0];
	print_tick = 1;
	new_value = 0;

	/* Do we have at least one register which value has changed? */
	for (i = 0; i < regs; i++) {
		/* No register changes on fall should be */
		if (fall)
			break;
		/* First N registers to be dumped only */
		if (mcu->vcd_regsn[i] < 0)
			break;
		reg = &mcu->vcd_regs[mcu->vcd_regsn[i]];
		/* Has register been changed? */
		if (*reg->addr != reg->oldv) {
			new_value = 1;
			break;
		}
	}

	/*
	 * There is no register which value changed. Should we print a
	 * clock pulse in this case?
	 */
	if (!new_value) {
		if (!clk_prints_left)
			return;
		fprintf(f, "#%lu\n", tick);
		fprintf(f, "b%d CLK_IO\n", !fall ? 1 : 0);
		if (fall)
			clk_prints_left--;
		return;
	}

	/*
	 * We've at least one register which value changed.
	 * Let's print it.
	 */
	for (i = 0; i < regs; i++) {
		/* First N registers to be dumped only */
		if (mcu->vcd_regsn[i] < 0)
			break;

		reg = &mcu->vcd_regs[mcu->vcd_regsn[i]];
		/* Hasn't it been changed? */
		if (*reg->addr == reg->oldv)
			continue;

		/* Print current tick and main clock only once */
		if (print_tick) {
			print_tick = 0;
			fprintf(f, "#%lu\n", tick);
			fprintf(f, "b1 CLK_IO\n");
			clk_prints_left = MAX_CLK_PRINTS;
		}

		/* Print selected register */
		reg->oldv = *reg->addr;
		print_reg(buf, sizeof buf, *reg->addr);
		fprintf(f, "b%s %s\n", buf, reg->name);
	}
}

static void print_reg(char *buf, unsigned int len, unsigned char r)
{
	unsigned int i, j = 0;
	size_t rbits = (sizeof r)*8;

	if (len < rbits) {
		buf[0] = 0;
		return;
	}
	for (i = 0; i < rbits; i++) {
		if ((r >> (rbits-1-i)) & 1)
			buf[j++] = '1';
		else
			buf[j++] = '0';
	}
	buf[j] = 0;
}
