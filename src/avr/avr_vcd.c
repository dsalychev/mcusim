/*
 * This file is part of MCUSim, an XSPICE library with microcontrollers.
 *
 * Copyright (C) 2017-2019 MCUSim Developers, see AUTHORS.txt for contributors.
 *
 * MCUSim is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * MCUSim is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/* Save samples of the AVR I/O registers to the VCD file. */
#include <stdint.h>
#include <time.h>
#include <inttypes.h>

#include "mcusim/mcusim.h"
#include "mcusim/bit/private/macro.h"
#include "mcusim/avr/sim/private/macro.h"

#define TERA			1000000000000.0
#define REG_NAMESZ		16

static void	print_reg16(char *buf, uint32_t len, uint8_t hr, uint8_t lr);
static void	print_reg(char *buf, uint32_t len, uint8_t r);
static void	print_regbit(char *buf, uint32_t len, uint8_t r, int8_t bit);

int
MSIM_AVR_VCDOpen(struct MSIM_AVR *mcu)
{
	time_t timer;
	struct tm *tm_info;
	uint32_t regs = MSIM_AVR_VCD_REGS;
	uint8_t rh, rl, rv;
	char buf[32];

	struct MSIM_AVR_VCD *vcd = &mcu->vcd;
	struct MSIM_AVR_VCDReg *reg;
	FILE *f = NULL;

	vcd->dump = fopen(vcd->dump_file, "w");
	if (vcd->dump == NULL) {
		return 75;
	} else {
		f = vcd->dump;
	}

	time(&timer);
	tm_info = localtime(&timer);
	strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%S", tm_info);

	/* Printing VCD header */
	fprintf(f, "$date\n\t%s\n$end\n", buf);
	fprintf(f, "$version\n\tGenerated by MCUSim %s\n$end\n", MSIM_VERSION);
	fprintf(f, "$comment\n\tDump of a simulated %s\n$end\n", mcu->name);
	fprintf(f, "$timescale\n\t%" PRIu64 " ps\n$end\n",
	        (uint64_t)((1.0/(double)mcu->freq)*TERA));
	fprintf(f, "$scope\n\tmodule %s\n$end\n", mcu->name);

	/* Declare VCD variables to dump */
	for (uint32_t i = 0; i < regs; i++) {
		if (vcd->regs[i].i < 0) {
			break;
		}
		reg = &vcd->regs[i];

		/* Are we going to dump a register bit only? */
		if (vcd->regs[i].reg_lowi >= 0) {
			fprintf(f, "$var reg 16 %s %s $end\n",
			        vcd->regs[i].name,
			        vcd->regs[i].name);
		} else if (vcd->regs[i].n < 0) {
			fprintf(f, "$var reg 8 %s %s $end\n",
			        reg->name, reg->name);
		} else {
			fprintf(f, "$var reg 1 %s%d %s%d $end\n",
			        reg->name, vcd->regs[i].n,
			        reg->name, vcd->regs[i].n);
		}
	}
	fprintf(f, "$upscope $end\n");
	fprintf(f, "$enddefinitions $end\n");

	/* Dumping initial register values to VCD file */
	fprintf(f, "$dumpvars\n");
	for (uint32_t i = 0; i < regs; i++) {
		if (vcd->regs[i].i < 0) {
			break;
		}

		reg = &vcd->regs[i];
		if (vcd->regs[i].reg_lowi >= 0) {
			rh = *mcu->ioregs[reg->i].addr;
			rl = *mcu->ioregs[reg->reg_lowi].addr;
		} else {
			rv = *mcu->ioregs[reg->i].addr;
		}

		if (vcd->regs[i].reg_lowi >= 0) {
			print_reg16(buf, sizeof buf, rh, rl);
			fprintf(f, "b%s %s\n", buf, vcd->regs[i].name);
		} else if (vcd->regs[i].n < 0) {
			print_reg(buf, sizeof buf, rv);
			fprintf(f, "b%s %s\n", buf, reg->name);
		} else {
			print_regbit(buf, sizeof buf, rv, vcd->regs[i].n);
			fprintf(f, "b%s %s%d\n", buf, reg->name,
			        vcd->regs[i].n);
		}
	}
	fprintf(f, "$end\n");

	return 0;
}

int
MSIM_AVR_VCDClose(struct MSIM_AVR *mcu)
{
	int rc = 0;

	/* Close dump file. */
	if (mcu->vcd.dump != NULL) {
		rc = fclose(mcu->vcd.dump);
	}
	return rc;
}

void
MSIM_AVR_VCDDumpFrame(struct MSIM_AVR *mcu, uint64_t tick)
{
	uint32_t regs = MSIM_AVR_VCD_REGS;
	uint32_t reg_val;
	uint8_t new_value = 0;
	uint8_t rh, rl;
	char buf[32];

	struct MSIM_AVR_VCD *vcd = &mcu->vcd;
	struct MSIM_AVR_VCDReg *reg;
	FILE *f = vcd->dump;

	/* Do we have at least one register which value has changed? */
	for (uint32_t i = 0; i < regs; i++) {
		int8_t n; /* Bit index of a register */

		/* Only requested registers to be dumped only */
		if (vcd->regs[i].i < 0) {
			break;
		}
		reg = &vcd->regs[i];
		reg_val = *mcu->ioregs[reg->i].addr;
		if (vcd->regs[i].reg_lowi >= 0) {
			rh = *mcu->ioregs[reg->i].addr;
			rl = *mcu->ioregs[reg->reg_lowi].addr;
			reg_val = ((uint16_t)(rh<<8)&0xFF00U)|
			          (uint16_t)(rl&0x00FFU);
		}

		/* Has register value been changed? */
		n = vcd->regs[i].n;
		if ((n < 0) && (reg_val != reg->old_val)) {
			new_value = 1;
			break;
		}
		if (n >= 0 && (((reg_val >> n)&1)!=((reg->old_val >> n)&1))) {
			new_value = 1;
			break;
		}
	}

	/* There is no register which value changed. */
	if (new_value == 0) {
		return;
	} else {
		fprintf(f, "#%" PRIu64 "\n", tick);
	}

	/* We've at least one register which value changed. Let's print it. */
	for (uint32_t i = 0; i < regs; i++) {
		/* Only requested registers to be dumped only */
		if (vcd->regs[i].i < 0) {
			break;
		}

		reg = &vcd->regs[i];
		reg_val = *mcu->ioregs[reg->i].addr;
		if (vcd->regs[i].reg_lowi >= 0) {
			rh = *mcu->ioregs[reg->i].addr;
			rl = *mcu->ioregs[reg->reg_lowi].addr;
			reg_val = ((uint16_t)(rh<<8)&0xFF00U)|
			          (uint16_t)(rl&0x00FFU);
		}
		/* Has it been changed? */
		if (reg_val == reg->old_val) {
			continue;
		}

		/* Print selected register */
		if (reg->reg_lowi >= 0) {
			print_reg16(buf, sizeof buf, rh, rl);
			fprintf(f, "b%s %s\n", buf, &reg->name[0]);
		} else if (reg->n < 0) {
			print_reg(buf, sizeof buf, (uint8_t)reg_val);
			fprintf(f, "b%s %s\n", buf, &reg->name[0]);
		} else {
			print_regbit(buf, sizeof buf,
			             (uint8_t)reg_val, reg->n);
			fprintf(f, "b%s %s%d\n", buf, &reg->name[0], reg->n);
		}
	}

	/* Update previous values of the registers */
	for (uint32_t i = 0; i < regs; i++) {
		/* Only requested registers to be dumped only */
		if (vcd->regs[i].i < 0) {
			break;
		}

		reg = &vcd->regs[i];
		reg_val = *mcu->ioregs[reg->i].addr;
		if (vcd->regs[i].reg_lowi >= 0) {
			rh = *mcu->ioregs[reg->i].addr;
			rl = *mcu->ioregs[reg->reg_lowi].addr;
			reg_val = ((uint16_t)(rh<<8)&0xFF00U)|
			          (uint16_t)(rl&0x00FFU);
		}

		/* Has it been changed? */
		if (reg_val == reg->old_val) {
			continue;
		} else {
			reg->old_val = reg_val;
		}
	}
}

static void
print_reg16(char *buf, uint32_t len, uint8_t hr, uint8_t lr)
{
	uint32_t i;
	uint32_t j = 0;
	uint32_t reg;
	size_t rbits;

	j = 0;
	rbits = 16;
	reg = ((uint16_t)(hr<<8)&0xFF00U)|(uint16_t)(lr&0x00FFU);

	for (i = 0; i < rbits; i++) {
		if ((reg >> (rbits-1-i)) & 1) {
			buf[j++] = '1';
		} else {
			buf[j++] = '0';
		}
	}
	buf[j] = 0;
}

static void
print_reg(char *buf, uint32_t len, uint8_t r)
{
	uint32_t i;
	uint32_t j;
	size_t rbits;

	j = 0;
	rbits = 8;

	for (i = 0; i < rbits; i++) {
		if ((r >> (rbits-1-i)) & 1) {
			buf[j++] = '1';
		} else {
			buf[j++] = '0';
		}
	}
	buf[j] = 0;
}

static void
print_regbit(char *buf, uint32_t len, uint8_t r, int8_t bit)
{
	buf[0] = ((r >> bit) & 1) ? '1' : '0';
	buf[1] = 0;
}
