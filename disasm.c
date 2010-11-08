/*  This file is part of retrobox
    Copyright (C) 2010  Jamse A. Shackleford

    retrobox is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// file created: Sept 23rd, 2010
#include <stdlib.h>
#include <stdio.h>
#include "disasm.h"
#include "6502.h"
#include "6502_types.h"
#include "memory.h"

/********************************************************************
 * M A C R O S                                                      *
 *                                                                  *
 ********************************************************************/

// Memory Access Macros
#define MEM_READ(addr)                \
    read_mem_generic (addr, dax->mmap) 

#define MEM_WRITE(data,addr)                  \
    write_mem_generic (data, addr, dax->mmap)  

// Flag Modifier Macros
#define SET_FLAG(flag)      \
    dax->S |= (flag)        

#define UNSET_FLAG(flag)    \
    dax->S &= ~(flag)       

#define HANDLE_SIGN_FLAG(reg)  \
    dax->S &= ~FLAG_SIGN;     \
    dax->S |= (reg & BIT7);    

#define HANDLE_ZERO_FLAG(reg)  \
    if (reg) {                 \
        dax->S &= ~FLAG_ZERO; \
    } else {                   \
        dax->S |= FLAG_ZERO;  \
    }                           

#define HANDLE_LSB_CARRY_FLAG(reg) \
    dax->S &= ~FLAG_CARRY;        \
    dax->S |= (reg & BIT0);        

#define HANDLE_MSB_CARRY_FLAG(reg) \
    dax->S &= ~FLAG_CARRY;        \
    dax->S |= (reg & BIT7);        

// Stack Access Macros
#define STACK_PUSH(data)                \
    MEM_WRITE (data, 0x100+dax->SP--);  

#define STACK_POP(data)                 \
    dax->SP++;                         \
    data = MEM_READ (0x100+dax->SP);    


/********************************************************************
 * A D D R E S S I N G      M O D E S                               *
 *                                                                  *
 *   For all modes, P0 holds calculated data addresses              *
 *                                                                  *
 ********************************************************************/

// ABSOLUTE (4 cycles):
//   PC+0 Opcode
//   PC+1 Address of Data ( low byte)
//   PC+2 Address of Data (high byte)
static void
absolute (disasm_inst* dax)
{
    /* Grab high and low address byes and form data address */
    dax->P0 = MEM_READ(dax->PC) + (MEM_READ(dax->PC+1) << 8);

    printf ("$%.4X\n", dax->P0);

    /* Get ready for next opcode */
    dax->PC += 2;
}


// ABSOLUTE, X (4 or 5 cycles)
//   PC+0 Opcode
//   PC+1 Address of Data ( low byte)
//   PC+2 Address of Data (high byte)
static void
absolute_x (disasm_inst* dax)
{
    /* Grab high and low address byes and form data address */
    dax->P0 = MEM_READ(dax->PC) + (MEM_READ(dax->PC+1) << 8);

    printf ("$%.4X, X ", dax->P0);

    /* Finish up with the program counter */
    dax->PC += 2;

    /* Add one cycle if page boundry is crossed */
    if (dax->cycles[dax->OP]) {
        if ((dax->P0 >> 8) != ((dax->P0 + dax->X) >> 8)) {
            dax->xtra_cycles++;
        }
    }

    /* Construct final data address */
    dax->P0 += dax->X;   

    printf ("\t[$%.4X]\n", dax->P0);
}


// ABSOLUTE, Y (4 or 5 cycles)
//   PC+0 Opcode
//   PC+1 Address of Data ( low byte)
//   PC+2 Address of Data (high byte)
static void
absolute_y (disasm_inst* dax)
{
    /* Grab high and low address byes and form data address */
    dax->P0 = MEM_READ(dax->PC) + (MEM_READ(dax->PC+1) << 8);

    printf ("$%.4X, Y ", dax->P0);

    /* Finish up with the program counter */
    dax->PC += 2;

    /* Add one cycle if page boundry is crossed */
    if (dax->cycles[dax->OP]) {
        if ((dax->P0 >> 8) != ((dax->P0 + dax->Y) >> 8)) {
            dax->xtra_cycles++;
        }
    }

    /* Construct final data address */
    dax->P0 += dax->Y;   

    printf ("\t[$%.4X]\n", dax->P0);
}


// IMMEDIATE (2 cycles):
//   PC+0 Opcode
//   PC+1 Parameter (Data)
static void
immediate (disasm_inst* dax)
{
    // Store address of immediate parameter to P0 and inc PC
    printf ("#$%.2X\n", MEM_READ(dax->PC++));
}


// IMPLIED (0 cycles):
//   PC+0 Opcode
static void
implied (disasm_inst* dax)
{
    printf ("\n");
}


// INDIRECT (JMPs only):
//   PC+0 Opcode
//   PC+1 Address of Data ( low byte)
//   PC+2 Address of Data (high byte)
static void
indirect (disasm_inst* dax)
{
    // Build memory address *containing* jump address
    dax->P0 = MEM_READ(dax->PC) + (MEM_READ(dax->PC+1) << 8);

    printf ("($%.4X)", dax->P0);

    // Read jump address @ built memory address
    dax->P0 = MEM_READ(dax->P0) + (MEM_READ((dax->P0 & 0xFF00)+((dax->P0+1) & 0x00FF)) << 8);

    printf ("\t[$%.4X]\n", dax->P0);

    // Get ready for next opcode
    dax->PC += 2;

}


// INDIRECT, ABSOLUTE X (JMP only):
//   PC+0: Opcode
//   PC+1: Base address containing jump address base
static void
indirect_ax (disasm_inst* dax)
{
    /* Compute the base address */
    dax->P0 = MEM_READ(dax->PC)
             + (MEM_READ(dax->PC + 1) << 8);

    printf ("($%.4X, X)", dax->P0);

    dax->P0 += dax->X;

    /* Grab new PC value @ new address */
    dax->P0 = MEM_READ(dax->P0)
             + (MEM_READ(dax->P0 + 1) << 8);

    printf ("\t[$%.4X]\n", dax->P0);

    dax->PC += 2;
}


// INDIRECT, X (6 cycles):
//   PC+0 Opcode
//   PC+1 Page Zero Base Address (BAL)
//  BAL+0 Address of Data ( low byte)
//  BAL+1 Address of Data (high byte)
static void
indirect_x (disasm_inst* dax)
{
    // Fetch base address and get ready for next opcode.
    dax->D0 = MEM_READ(dax->PC++);

    printf ("($%.2X, X) @ %.2X", dax->D0, (byte)(dax->D0 + dax->X));

    // Fetch address of data
    dax->P0 = MEM_READ((byte)(dax->D0 + dax->X))
             + (MEM_READ((byte)(dax->D0 + dax->X + 1)) << 8);

    printf (" = $%.4X\n", dax->P0);
}


// INDIRECT, Y (5 or 6 cycles):
//   PC+0 Opcode
//   PC+1 Base Address
static void
indirect_y (disasm_inst* dax)
{
    /* Fetch contents of base address */
    dax->D0 = MEM_READ(dax->PC++);

    printf ("($%.2X), Y", dax->D0);

    /* Construct base of jump address */
    dax->P0 = MEM_READ((byte)(dax->D0))
             + (MEM_READ((byte)(dax->D0 + 1)) << 8);

    /* Add Y to jump address base */
    dax->P0 += dax->Y;

    printf ("\t[$%.4X]\n", dax->P0);

    /* Add one cycle if page boundry is crossed */
    if (dax->cycles[dax->OP] == 5) {
        if ((dax->P0 >> 8) != ((dax->P0 + dax->Y) >> 8)) {
            dax->xtra_cycles++;
        }
    }
}


// RELATIVE:
//   PC+0 Opcode
//   PC+1 Signed value to add to PC
static void
relative (disasm_inst* dax)
{
    // Grab the signed PC modifier value
    dax->D0 = MEM_READ(dax->PC++);

    // Deal with negative PC modifiers
    if (dax->D0 & BIT7) {
        dax->D0 -= 0x100;
    }

    // Compute new PC value, store in P0
    dax->P0 = dax->PC + (signed char)dax->D0;

    printf ("$%.4X\n", dax->P0);

    // Adjust timing for page change
    if ((dax->P0 >> 8) != (dax->PC >> 8)) {
        dax->xtra_cycles++;
    }
}


// ZERO PAGE (3 cycles):
//   PC+0 Opcode
//   PC+1 Address of Data
static void
zeropage (disasm_inst* dax)
{
    // Fetch the specified address from MEM
    // and increment the PC to get ready for next
    // instruction.
    dax->P0 = MEM_READ(dax->PC++);
    printf ("$%.4X\n", dax->P0);
}


// INDIRECT ZERO PAGE
//   PC+0 Opcode
//   PC+1 Base of address containing jump address
static void
zeropage_id (disasm_inst* dax)
{
    /* Grab the base address & increment PC */
    dax->D0 = MEM_READ(dax->PC++);

    printf ("($%.2X)", dax->D0);

    /* Build new address */
    dax->P0 = MEM_READ(dax->D0) + (MEM_READ(dax->D0 + 1) << 8);

    printf ("\t[$%.4X]\n", dax->P0);
}


// ZERO PAGE, X
//   PC+0 Opcode
//   PC+1 Base Address
static void
zeropage_x (disasm_inst* dax)
{
    dax->P0 = MEM_READ(dax->PC++);

    printf ("$%.4X, X", dax->P0);

    dax->P0 += dax->X;

    dax->P0 &= 0xFF;

    printf ("\t[$%.4X]\n", dax->P0);
}


// ZERO PAGE, Y
//   PC+0 Opcode
//   PC+1 Base Address
static void
zeropage_y (disasm_inst* dax)
{
    dax->P0 = MEM_READ(dax->PC++);

    printf ("$%.4X, Y", dax->P0);

    dax->P0 += dax->Y;

    dax->P0 &= 0xFF;

    printf ("\t[$%.4X]\n", dax->P0);
}




/********************************************************************
 * O P C O D E S                                                    *
 *                                                                  *
 ********************************************************************/
static void
aac (disasm_inst* dax)
{
    printf ("%X\tAAC ", dax->PC-1);
    dax->amode[dax->OP](dax);
}

static void
adc (disasm_inst* dax)
{
    printf ("%X\tADC ", dax->PC-1);
    dax->amode[dax->OP](dax);
}

static void
and (disasm_inst* dax)
{
    printf ("%X\tAND A, ", dax->PC-1);
    dax->amode[dax->OP](dax);
}

static void
arr (disasm_inst* dax)
{
    printf ("%X\tARR ", dax->PC-1);
    dax->amode[dax->OP](dax);

}

static void
asl (disasm_inst* dax)
{
    printf ("%X\tASL ", dax->PC-1);
    dax->amode[dax->OP](dax);

}

static void
asla (disasm_inst* dax)
{
    printf ("%X\tASL A ", dax->PC-1);
    dax->amode[dax->OP](dax);
}

static void
asr (disasm_inst* dax)
{
    printf ("%X\tASR ", dax->PC-1);
    dax->amode[dax->OP](dax);
}

static void
atx (disasm_inst* dax)
{
    printf ("%X\tASR ", dax->PC-1);
    dax->amode[dax->OP](dax);
}


static void
bcc (disasm_inst* dax)
{
    printf ("%X\tBCC ", dax->PC-1);
    dax->amode[dax->OP](dax);
}

static void
bcs (disasm_inst* dax)
{
    printf ("%X\tBCS ", dax->PC-1);
    dax->amode[dax->OP](dax);
}

static void
beq (disasm_inst* dax)
{
    printf ("%X\tBEQ ", dax->PC-1);
    dax->amode[dax->OP](dax);
}

static void
bit (disasm_inst* dax)
{
    printf ("%X\tBIT ", dax->PC-1);
    dax->amode[dax->OP](dax);
}

static void
bmi (disasm_inst* dax)
{
    printf ("%X\tBMI ", dax->PC-1);
    dax->amode[dax->OP](dax);
}

static void
bne (disasm_inst* dax)
{
    printf ("%X\tBNE ", dax->PC-1);
    dax->amode[dax->OP](dax);
}

static void
bpl (disasm_inst* dax)
{
    printf ("%X\tBPL ", dax->PC-1);
    dax->amode[dax->OP](dax);
}

static void
bra (disasm_inst* dax)
{
    printf ("%X\tBRA ", dax->PC-1);
    dax->amode[dax->OP](dax);
}

static void
brk (disasm_inst* dax)
{
    printf ("%X\tBRK ", dax->PC-1);
    dax->amode[dax->OP](dax);
}

static void
bvc (disasm_inst* dax)
{
    printf ("%X\tBVC ", dax->PC-1);
    dax->amode[dax->OP](dax);
}

static void
bvs (disasm_inst* dax)
{
    printf ("%X\tBVS ", dax->PC-1);
    dax->amode[dax->OP](dax);
}

static void
clc (disasm_inst* dax)
{
    printf ("%X\tCLC ", dax->PC-1);
    dax->amode[dax->OP](dax);
}

static void
cld (disasm_inst* dax)
{
    printf ("%X\tCLD ", dax->PC-1);
    dax->amode[dax->OP](dax);
}

static void
cli (disasm_inst* dax)
{
    printf ("%X\tCLI ", dax->PC-1);
    dax->amode[dax->OP](dax);
}

static void
clv (disasm_inst* dax)
{
    printf ("%X\tCLV ", dax->PC-1);
    dax->amode[dax->OP](dax);
}

static void
cmp (disasm_inst* dax)
{
    printf ("%X\tCMP ", dax->PC-1);
    dax->amode[dax->OP](dax);
}

static void
cpx (disasm_inst* dax)
{
    printf ("%X\tCPX ", dax->PC-1);
    dax->amode[dax->OP](dax);
}

static void
cpy (disasm_inst* dax)
{
    printf ("%X\tCPY ", dax->PC-1);
    dax->amode[dax->OP](dax);
}

static void
dea (disasm_inst* dax)
{
    printf ("%X\tDEA ", dax->PC-1);
    dax->amode[dax->OP](dax);
}

static void
dec (disasm_inst* dax)
{
    printf ("%X\tDEC ", dax->PC-1);
    dax->amode[dax->OP](dax);
}

static void
dex (disasm_inst* dax)
{
    printf ("%X\tDEX ", dax->PC-1);
    dax->amode[dax->OP](dax);
}

static void
dey (disasm_inst* dax)
{
    printf ("%X\tDEY ", dax->PC-1);
    dax->amode[dax->OP](dax);
}

static void
dcp (disasm_inst* dax)
{
    printf ("%X\tDCP ", dax->PC-1);
    dax->amode[dax->OP](dax);
}

static void
eor (disasm_inst* dax)
{
    printf ("%X\tEOR ", dax->PC-1);
    dax->amode[dax->OP](dax);
}

static void
ina (disasm_inst* dax)
{
    printf ("%X\tINA ", dax->PC-1);
    dax->amode[dax->OP](dax);
}

static void
inc (disasm_inst* dax)
{
    printf ("%X\tINC ", dax->PC-1);
    dax->amode[dax->OP](dax);
}

static void
inx (disasm_inst* dax)
{
    printf ("%X\tINX ", dax->PC-1);
    dax->amode[dax->OP](dax);
}

static void
isb (disasm_inst* dax)
{
    printf ("%X\tISB ", dax->PC-1);
    dax->amode[dax->OP](dax);
}

static void
iny (disasm_inst* dax)
{
    printf ("%X\tINY ", dax->PC-1);
    dax->amode[dax->OP](dax);
}

static void
jmp (disasm_inst* dax)
{
    printf ("%X\tJMP ", dax->PC-1);
    dax->amode[dax->OP](dax);
}

static void
jsr (disasm_inst* dax)
{
    printf ("%X\tJSR ", dax->PC-1);
    dax->amode[dax->OP](dax);
}

static void
lax (disasm_inst* dax)
{
    printf ("%X\tLAX ", dax->PC-1);
    dax->amode[dax->OP](dax);
}

static void
lda (disasm_inst* dax)
{
    printf ("%X\tLDA ", dax->PC-1);
    dax->amode[dax->OP](dax);
}

static void
ldx (disasm_inst* dax)
{
    printf ("%X\tLDX ", dax->PC-1);
    dax->amode[dax->OP](dax);
}

static void
ldy (disasm_inst* dax)
{
    printf ("%X\tLDY ", dax->PC-1);
    dax->amode[dax->OP](dax);
}

static void
lsr (disasm_inst* dax)
{
    printf ("%X\tLSR ", dax->PC-1);
    dax->amode[dax->OP](dax);
}

static void
lsra (disasm_inst* dax)
{
    printf ("%X\tLSR A ", dax->PC-1);
    dax->amode[dax->OP](dax);
}

static void
nop (disasm_inst* dax)
{
    printf ("%X\tNOP ", dax->PC-1);
    printf ("(%.2X) ", dax->OP);
    dax->amode[dax->OP](dax);
}

static void
ora (disasm_inst* dax)
{
    printf ("%X\tORA ", dax->PC-1);
    dax->amode[dax->OP](dax);
}

static void
pha (disasm_inst* dax)
{
    printf ("%X\tPHA ", dax->PC-1);
    dax->amode[dax->OP](dax);
}

static void
php (disasm_inst* dax)
{
    printf ("%X\tPHP ", dax->PC-1);
    dax->amode[dax->OP](dax);
}

static void
pla (disasm_inst* dax)
{
    printf ("%X\tPLA ", dax->PC-1);
    dax->amode[dax->OP](dax);
}

static void
plp (disasm_inst* dax)
{
    printf ("%X\tPLP ", dax->PC-1);
    dax->amode[dax->OP](dax);
}

static void
rla (disasm_inst* dax)
{
    printf ("%X\tRLA ", dax->PC-1);
    dax->amode[dax->OP](dax);
}

static void
rol (disasm_inst* dax)
{
    printf ("%X\tROL ", dax->PC-1);
    dax->amode[dax->OP](dax);
}

static void
rola (disasm_inst* dax)
{
    printf ("%X\tROL A", dax->PC-1);
    dax->amode[dax->OP](dax);
}

static void
ror (disasm_inst* dax)
{
    printf ("%X\tROR ", dax->PC-1);
    dax->amode[dax->OP](dax);
}

static void
rora (disasm_inst* dax)
{
    printf ("%X\tROR A ", dax->PC-1);
    dax->amode[dax->OP](dax);
}

static void
rra (disasm_inst* dax)
{
    printf ("%X\tRRA ", dax->PC-1);
    dax->amode[dax->OP](dax);
}

static void
rti (disasm_inst* dax)
{
    printf ("%X\tRTI ", dax->PC-1);
    dax->amode[dax->OP](dax);
}

static void
rts (disasm_inst* dax)
{
    printf ("%X\tRTS ", dax->PC-1);
    dax->amode[dax->OP](dax);
}

static void
sax (disasm_inst* dax)
{
    printf ("%X\tSAX ", dax->PC-1);
    dax->amode[dax->OP](dax);
}

static void
sbc (disasm_inst* dax)
{
    printf ("%X\tSBC ", dax->PC-1);
    dax->amode[dax->OP](dax);
}

static void
sec (disasm_inst* dax)
{
    printf ("%X\tSEC ", dax->PC-1);
    dax->amode[dax->OP](dax);
}

static void
sed (disasm_inst* dax)
{
    printf ("%X\tSED ", dax->PC-1);
    dax->amode[dax->OP](dax);
}

static void
sei (disasm_inst* dax)
{
    printf ("%X\tSEI ", dax->PC-1);
    dax->amode[dax->OP](dax);
}

static void
slo (disasm_inst* dax)
{
    printf ("%X\tSLO ", dax->PC-1);
    dax->amode[dax->OP](dax);
}

static void
sre (disasm_inst* dax)
{
    printf ("%X\tSRE ", dax->PC-1);
    dax->amode[dax->OP](dax);
}

static void
sta (disasm_inst* dax)
{
    printf ("%X\tSTA ", dax->PC-1);
    dax->amode[dax->OP](dax);
}

static void
stx (disasm_inst* dax)
{
    printf ("%X\tSTX ", dax->PC-1);
    dax->amode[dax->OP](dax);
}

static void
sty (disasm_inst* dax)
{
    printf ("%X\tSTY ", dax->PC-1);
    dax->amode[dax->OP](dax);
}

static void
stz (disasm_inst* dax)
{
    printf ("%X\tSTZ ", dax->PC-1);
    dax->amode[dax->OP](dax);
}

static void
tax (disasm_inst* dax)
{
    printf ("%X\tTAX ", dax->PC-1);
    dax->amode[dax->OP](dax);
}

static void
tay (disasm_inst* dax)
{
    printf ("%X\tTAY ", dax->PC-1);
    dax->amode[dax->OP](dax);
}

static void
trb (disasm_inst* dax)
{
    printf ("%X\tTRB ", dax->PC-1);
    dax->amode[dax->OP](dax);
}

static void
tsb (disasm_inst* dax)
{
    printf ("%X\tTSB ", dax->PC-1);
    dax->amode[dax->OP](dax);
}

static void
tsx (disasm_inst* dax)
{
    printf ("%X\tTSX ", dax->PC-1);
    dax->amode[dax->OP](dax);
}

static void
txa (disasm_inst* dax)
{
    printf ("%X\tTXA ", dax->PC-1);
    dax->amode[dax->OP](dax);
}

static void
txs (disasm_inst* dax)
{
    printf ("%X\tTXS ", dax->PC-1);
    dax->amode[dax->OP](dax);
}

static void
tya (disasm_inst* dax)
{
    printf ("%X\tTYA ", dax->PC-1);
    dax->amode[dax->OP](dax);
}

// Undocumented
static void
udoc (disasm_inst* dax)
{
    printf ("%X\t*NOP ", dax->PC-1);
    dax->amode[dax->OP](dax);
}

/********************************************************************
 * E N G I N E     I N T E R F A C E S                              *
 ********************************************************************/

// Initializes the 6502 CPU engine look up tables
disasm_luts*
init_disasm_engine ()
{
    /* Just used for shorthand */
    void (**opcode)(disasm_inst* dax);
    void (**amode)(disasm_inst* dax);
    int *cycles;

    /* This will serve an opcode look up table */
    disasm_luts* luts = (disasm_luts*) malloc (sizeof(disasm_luts));

    /* Allocate look up tables */
    luts->opcode = malloc (sizeof(void(*)(disasm_inst* dax)) * 256);
    luts->amode  = malloc (sizeof(void(*)(disasm_inst* dax)) * 256);
    luts->cycles = (int*) malloc (sizeof(int) * 256);

    opcode = luts->opcode;
    amode  = luts->amode;
    cycles = luts->cycles;

    /* Initialize look up tables */
    opcode[0x00] = brk;  amode[0x00] = implied;     cycles[0x00] = 7;
    opcode[0x01] = ora;  amode[0x01] = indirect_x;  cycles[0x01] = 6;
    opcode[0x02] = nop;  amode[0x02] = implied;     cycles[0x02] = 2;
    opcode[0x03] = slo;  amode[0x03] = indirect_x;  cycles[0x03] = 2;
    opcode[0x04] = udoc; amode[0x04] = zeropage;    cycles[0x04] = 3;
    opcode[0x05] = ora;  amode[0x05] = zeropage;    cycles[0x05] = 3;
    opcode[0x06] = asl;  amode[0x06] = zeropage;    cycles[0x06] = 5;
    opcode[0x07] = slo;  amode[0x07] = zeropage;    cycles[0x07] = 2;
    opcode[0x08] = php;  amode[0x08] = implied;     cycles[0x08] = 3;
    opcode[0x09] = ora;  amode[0x09] = immediate;   cycles[0x09] = 3;
    opcode[0x0A] = asla; amode[0x0A] = implied;     cycles[0x0A] = 2;
    opcode[0x0B] = aac;  amode[0x0B] = immediate;   cycles[0x0B] = 2;
    opcode[0x0C] = udoc; amode[0x0C] = absolute;    cycles[0x0C] = 4;
    opcode[0x0D] = ora;  amode[0x0D] = absolute;    cycles[0x0D] = 4;
    opcode[0x0E] = asl;  amode[0x0E] = absolute;    cycles[0x0E] = 6;
    opcode[0x0F] = slo;  amode[0x0F] = absolute;    cycles[0x0F] = 2;

    opcode[0x10] = bpl;  amode[0x10] = relative;    cycles[0x10] = 2;
    opcode[0x11] = ora;  amode[0x11] = indirect_y;  cycles[0x11] = 5;
    opcode[0x12] = ora;  amode[0x12] = zeropage_id; cycles[0x12] = 3;
    opcode[0x13] = slo;  amode[0x13] = indirect_y;  cycles[0x13] = 2;
    opcode[0x14] = udoc; amode[0x14] = zeropage_x;  cycles[0x14] = 3;
    opcode[0x15] = ora;  amode[0x15] = zeropage_x;  cycles[0x15] = 4;
    opcode[0x16] = asl;  amode[0x16] = zeropage_x;  cycles[0x16] = 6;
    opcode[0x17] = slo;  amode[0x17] = zeropage_x;  cycles[0x17] = 2;
    opcode[0x18] = clc;  amode[0x18] = implied;     cycles[0x18] = 2;
    opcode[0x19] = ora;  amode[0x19] = absolute_y;  cycles[0x19] = 4;
    opcode[0x1A] = nop;  amode[0x1A] = implied;     cycles[0x1A] = 2;
    opcode[0x1B] = slo;  amode[0x1B] = absolute_y;  cycles[0x1B] = 2;
    opcode[0x1C] = udoc; amode[0x1C] = absolute_x;  cycles[0x1C] = 4;
    opcode[0x1D] = ora;  amode[0x1D] = absolute_x;  cycles[0x1D] = 4;
    opcode[0x1E] = asl;  amode[0x1E] = absolute_x;  cycles[0x1E] = 7;
    opcode[0x1F] = slo;  amode[0x1F] = absolute_x;  cycles[0x1F] = 2;

    opcode[0x20] = jsr;  amode[0x20] = absolute;    cycles[0x20] = 6;
    opcode[0x21] = and;  amode[0x21] = indirect_x;  cycles[0x21] = 6;
    opcode[0x22] = nop;  amode[0x22] = implied;     cycles[0x22] = 2;
    opcode[0x23] = rla;  amode[0x23] = indirect_x;  cycles[0x23] = 2;
    opcode[0x24] = bit;  amode[0x24] = zeropage;    cycles[0x24] = 3;
    opcode[0x25] = and;  amode[0x25] = zeropage;    cycles[0x25] = 3;
    opcode[0x26] = rol;  amode[0x26] = zeropage;    cycles[0x26] = 5;
    opcode[0x27] = rla;  amode[0x27] = zeropage;    cycles[0x27] = 2;
    opcode[0x28] = plp;  amode[0x28] = implied;     cycles[0x28] = 4;
    opcode[0x29] = and;  amode[0x29] = immediate;   cycles[0x29] = 3;
    opcode[0x2A] = rola; amode[0x2A] = implied;     cycles[0x2A] = 2;
    opcode[0x2B] = aac;  amode[0x2B] = immediate;   cycles[0x2B] = 2;
    opcode[0x2C] = bit;  amode[0x2C] = absolute;    cycles[0x2C] = 4;
    opcode[0x2D] = and;  amode[0x2D] = absolute;    cycles[0x2D] = 4;
    opcode[0x2E] = rol;  amode[0x2E] = absolute;    cycles[0x2E] = 6;
    opcode[0x2F] = rla;  amode[0x2F] = absolute;    cycles[0x2F] = 2;

    opcode[0x30] = bmi;  amode[0x30] = relative;    cycles[0x30] = 2;
    opcode[0x31] = and;  amode[0x31] = indirect_y;  cycles[0x31] = 5;
    opcode[0x32] = and;  amode[0x32] = zeropage_id; cycles[0x32] = 3;
    opcode[0x33] = rla;  amode[0x33] = indirect_y;  cycles[0x33] = 2;
    opcode[0x34] = udoc; amode[0x34] = zeropage_x;  cycles[0x34] = 4;
    opcode[0x35] = and;  amode[0x35] = zeropage_x;  cycles[0x35] = 4;
    opcode[0x36] = rol;  amode[0x36] = zeropage_x;  cycles[0x36] = 6;
    opcode[0x37] = rla;  amode[0x37] = zeropage_x;  cycles[0x37] = 2;
    opcode[0x38] = sec;  amode[0x38] = implied;     cycles[0x38] = 2;
    opcode[0x39] = and;  amode[0x39] = absolute_y;  cycles[0x39] = 4;
    opcode[0x3A] = nop;  amode[0x3A] = implied;     cycles[0x3A] = 2;
    opcode[0x3B] = rla;  amode[0x3B] = absolute_y;  cycles[0x3B] = 2;
    opcode[0x3C] = udoc; amode[0x3C] = absolute_x;  cycles[0x3C] = 4;
    opcode[0x3D] = and;  amode[0x3D] = absolute_x;  cycles[0x3D] = 4;
    opcode[0x3E] = rol;  amode[0x3E] = absolute_x;  cycles[0x3E] = 7;
    opcode[0x3F] = rla;  amode[0x3F] = absolute_x;  cycles[0x3F] = 2;

    opcode[0x40] = rti;  amode[0x40] = implied;     cycles[0x40] = 6;
    opcode[0x41] = eor;  amode[0x41] = indirect_x;  cycles[0x41] = 6;
    opcode[0x42] = nop;  amode[0x42] = implied;     cycles[0x42] = 2;
    opcode[0x43] = sre;  amode[0x43] = indirect_x;  cycles[0x43] = 2;
    opcode[0x44] = udoc; amode[0x44] = zeropage;    cycles[0x44] = 2;
    opcode[0x45] = eor;  amode[0x45] = zeropage;    cycles[0x45] = 3;
    opcode[0x46] = lsr;  amode[0x46] = zeropage;    cycles[0x46] = 5;
    opcode[0x47] = sre;  amode[0x47] = zeropage;    cycles[0x47] = 2;
    opcode[0x48] = pha;  amode[0x48] = implied;     cycles[0x48] = 3;
    opcode[0x49] = eor;  amode[0x49] = immediate;   cycles[0x49] = 3;
    opcode[0x4A] = lsra; amode[0x4A] = implied;     cycles[0x4A] = 2;
    opcode[0x4B] = asr;  amode[0x4B] = immediate;   cycles[0x4B] = 2;
    opcode[0x4C] = jmp;  amode[0x4C] = absolute;    cycles[0x4C] = 3;
    opcode[0x4D] = eor;  amode[0x4D] = absolute;    cycles[0x4D] = 4;
    opcode[0x4E] = lsr;  amode[0x4E] = absolute;    cycles[0x4E] = 6;
    opcode[0x4F] = sre;  amode[0x4F] = absolute;    cycles[0x4F] = 2;

    opcode[0x50] = bvc;  amode[0x50] = relative;    cycles[0x50] = 2;
    opcode[0x51] = eor;  amode[0x51] = indirect_y;  cycles[0x51] = 5;
    opcode[0x52] = eor;  amode[0x52] = zeropage_id; cycles[0x52] = 3;
    opcode[0x53] = sre;  amode[0x53] = indirect_y;  cycles[0x53] = 2;
    opcode[0x54] = udoc; amode[0x54] = zeropage_x;  cycles[0x54] = 2;
    opcode[0x55] = eor;  amode[0x55] = zeropage_x;  cycles[0x55] = 4;
    opcode[0x56] = lsr;  amode[0x56] = zeropage_x;  cycles[0x56] = 6;
    opcode[0x57] = sre;  amode[0x57] = zeropage_x;  cycles[0x57] = 2;
    opcode[0x58] = cli;  amode[0x58] = implied;     cycles[0x58] = 2;
    opcode[0x59] = eor;  amode[0x59] = absolute_y;  cycles[0x59] = 4;
    opcode[0x5A] = nop;  amode[0x5A] = implied;     cycles[0x5A] = 3;
    opcode[0x5B] = sre;  amode[0x5B] = absolute_y;  cycles[0x5B] = 2;
    opcode[0x5C] = udoc; amode[0x5C] = absolute_x;  cycles[0x5C] = 2;
    opcode[0x5D] = eor;  amode[0x5D] = absolute_x;  cycles[0x5D] = 4;
    opcode[0x5E] = lsr;  amode[0x5E] = absolute_x;  cycles[0x5E] = 7;
    opcode[0x5F] = sre;  amode[0x5F] = absolute_x;  cycles[0x5F] = 2;

    opcode[0x60] = rts;  amode[0x60] = implied;     cycles[0x60] = 6;
    opcode[0x61] = adc;  amode[0x61] = indirect_x;  cycles[0x61] = 6;
    opcode[0x62] = nop;  amode[0x62] = implied;     cycles[0x62] = 2;
    opcode[0x63] = rra;  amode[0x63] = indirect_x;  cycles[0x63] = 2;
    opcode[0x64] = udoc; amode[0x64] = zeropage;    cycles[0x64] = 3;
    opcode[0x65] = adc;  amode[0x65] = zeropage;    cycles[0x65] = 3;
    opcode[0x66] = ror;  amode[0x66] = zeropage;    cycles[0x66] = 5;
    opcode[0x67] = rra;  amode[0x67] = zeropage;    cycles[0x67] = 2;
    opcode[0x68] = pla;  amode[0x68] = implied;     cycles[0x68] = 4;
    opcode[0x69] = adc;  amode[0x69] = immediate;   cycles[0x69] = 3;
    opcode[0x6A] = rora; amode[0x6A] = implied;     cycles[0x6A] = 2;
    opcode[0x6B] = arr;  amode[0x6B] = immediate;   cycles[0x6B] = 2;
    opcode[0x6C] = jmp;  amode[0x6C] = indirect;    cycles[0x6C] = 5;
    opcode[0x6D] = adc;  amode[0x6D] = absolute;    cycles[0x6D] = 4;
    opcode[0x6E] = ror;  amode[0x6E] = absolute;    cycles[0x6E] = 6;
    opcode[0x6F] = rra;  amode[0x6F] = absolute;    cycles[0x6F] = 2;

    opcode[0x70] = bvs;  amode[0x70] = relative;    cycles[0x70] = 2;
    opcode[0x71] = adc;  amode[0x71] = indirect_y;  cycles[0x71] = 5;
    opcode[0x72] = adc;  amode[0x72] = zeropage_id; cycles[0x72] = 3;
    opcode[0x73] = rra;  amode[0x73] = indirect_y;  cycles[0x73] = 2;
    opcode[0x74] = udoc; amode[0x74] = zeropage_x;  cycles[0x74] = 4;
    opcode[0x75] = adc;  amode[0x75] = zeropage_x;  cycles[0x75] = 4;
    opcode[0x76] = ror;  amode[0x76] = zeropage_x;  cycles[0x76] = 6;
    opcode[0x77] = rra;  amode[0x77] = zeropage_x;  cycles[0x77] = 2;
    opcode[0x78] = sei;  amode[0x78] = implied;     cycles[0x78] = 2;
    opcode[0x79] = adc;  amode[0x79] = absolute_y;  cycles[0x79] = 4;
    opcode[0x7A] = nop;  amode[0x7A] = implied;     cycles[0x7A] = 4;
    opcode[0x7B] = rra;  amode[0x7B] = absolute_y;  cycles[0x7B] = 2;
    opcode[0x7C] = udoc; amode[0x7C] = absolute_x;; cycles[0x7C] = 6;
    opcode[0x7D] = adc;  amode[0x7D] = absolute_x;  cycles[0x7D] = 4;
    opcode[0x7E] = ror;  amode[0x7E] = absolute_x;  cycles[0x7E] = 7;
    opcode[0x7F] = rra;  amode[0x7F] = absolute_x;  cycles[0x7F] = 2;

    opcode[0x80] = udoc; amode[0x80] = immediate;   cycles[0x80] = 2;
    opcode[0x81] = sta;  amode[0x81] = indirect_x;  cycles[0x81] = 6;
    opcode[0x82] = udoc; amode[0x82] = immediate;   cycles[0x82] = 2;
    opcode[0x83] = sax;  amode[0x83] = indirect_x;  cycles[0x83] = 2;
    opcode[0x84] = sty;  amode[0x84] = zeropage;    cycles[0x84] = 2;
    opcode[0x85] = sta;  amode[0x85] = zeropage;    cycles[0x85] = 2;
    opcode[0x86] = stx;  amode[0x86] = zeropage;    cycles[0x86] = 2;
    opcode[0x87] = sax;  amode[0x87] = zeropage;    cycles[0x87] = 2;
    opcode[0x88] = dey;  amode[0x88] = implied;     cycles[0x88] = 2;
    opcode[0x89] = udoc; amode[0x89] = immediate;   cycles[0x89] = 2;
    opcode[0x8A] = txa;  amode[0x8A] = implied;     cycles[0x8A] = 2;
    opcode[0x8B] = udoc; amode[0x8B] = immediate;   cycles[0x8B] = 2;
    opcode[0x8C] = sty;  amode[0x8C] = absolute;    cycles[0x8C] = 4;
    opcode[0x8D] = sta;  amode[0x8D] = absolute;    cycles[0x8D] = 4;
    opcode[0x8E] = stx;  amode[0x8E] = absolute;    cycles[0x8E] = 4;
    opcode[0x8F] = sax;  amode[0x8F] = absolute;    cycles[0x8F] = 2;

    opcode[0x90] = bcc;  amode[0x90] = relative;    cycles[0x90] = 2;
    opcode[0x91] = sta;  amode[0x91] = indirect_y;  cycles[0x91] = 6;
    opcode[0x92] = sta;  amode[0x92] = zeropage_id; cycles[0x92] = 3;
    opcode[0x93] = udoc; amode[0x93] = indirect_y;  cycles[0x93] = 2;
    opcode[0x94] = sty;  amode[0x94] = zeropage_x;  cycles[0x94] = 4;
    opcode[0x95] = sta;  amode[0x95] = zeropage_x;  cycles[0x95] = 4;
    opcode[0x96] = stx;  amode[0x96] = zeropage_y;  cycles[0x96] = 4;
    opcode[0x97] = sax;  amode[0x97] = zeropage_y;  cycles[0x97] = 2;
    opcode[0x98] = tya;  amode[0x98] = implied;     cycles[0x98] = 2;
    opcode[0x99] = sta;  amode[0x99] = absolute_y;  cycles[0x99] = 5;
    opcode[0x9A] = txs;  amode[0x9A] = implied;     cycles[0x9A] = 2;
    opcode[0x9B] = udoc; amode[0x9B] = absolute_y;  cycles[0x9B] = 2;
    opcode[0x9C] = stz;  amode[0x9C] = absolute;    cycles[0x9C] = 4;
    opcode[0x9D] = sta;  amode[0x9D] = absolute_x;  cycles[0x9D] = 5;
    opcode[0x9E] = stz;  amode[0x9E] = absolute_x;  cycles[0x9E] = 5;
    opcode[0x9F] = udoc; amode[0x9F] = absolute_y;  cycles[0x9F] = 2;

    opcode[0xA0] = ldy;  amode[0xA0] = immediate;   cycles[0xA0] = 3;
    opcode[0xA1] = lda;  amode[0xA1] = indirect_x;  cycles[0xA1] = 6;
    opcode[0xA2] = ldx;  amode[0xA2] = immediate;   cycles[0xA2] = 3;
    opcode[0xA3] = lax;  amode[0xA3] = indirect_x;  cycles[0xA3] = 2;
    opcode[0xA4] = ldy;  amode[0xA4] = zeropage;    cycles[0xA4] = 3;
    opcode[0xA5] = lda;  amode[0xA5] = zeropage;    cycles[0xA5] = 3;
    opcode[0xA6] = ldx;  amode[0xA6] = zeropage;    cycles[0xA6] = 3;
    opcode[0xA7] = lax;  amode[0xA7] = zeropage;    cycles[0xA7] = 2;
    opcode[0xA8] = tay;  amode[0xA8] = implied;     cycles[0xA8] = 2;
    opcode[0xA9] = lda;  amode[0xA9] = immediate;   cycles[0xA9] = 3;
    opcode[0xAA] = tax;  amode[0xAA] = implied;     cycles[0xAA] = 2;
    opcode[0xAB] = atx;  amode[0xAB] = immediate;   cycles[0xAB] = 2;
    opcode[0xAC] = ldy;  amode[0xAC] = absolute;    cycles[0xAC] = 4;
    opcode[0xAD] = lda;  amode[0xAD] = absolute;    cycles[0xAD] = 4;
    opcode[0xAE] = ldx;  amode[0xAE] = absolute;    cycles[0xAE] = 4;
    opcode[0xAF] = lax;  amode[0xAF] = absolute;    cycles[0xAF] = 2;

    opcode[0xB0] = bcs;  amode[0xB0] = relative;    cycles[0xB0] = 2;
    opcode[0xB1] = lda;  amode[0xB1] = indirect_y;  cycles[0xB1] = 5;
    opcode[0xB2] = lda;  amode[0xB2] = zeropage_id; cycles[0xB2] = 3;
    opcode[0xB3] = lax;  amode[0xB3] = indirect_y;  cycles[0xB3] = 2;
    opcode[0xB4] = ldy;  amode[0xB4] = zeropage_x;  cycles[0xB4] = 4;
    opcode[0xB5] = lda;  amode[0xB5] = zeropage_x;  cycles[0xB5] = 4;
    opcode[0xB6] = ldx;  amode[0xB6] = zeropage_y;  cycles[0xB6] = 4;
    opcode[0xB7] = lax;  amode[0xB7] = zeropage_y;  cycles[0xB7] = 2;
    opcode[0xB8] = clv;  amode[0xB8] = implied;     cycles[0xB8] = 2;
    opcode[0xB9] = lda;  amode[0xB9] = absolute_y;  cycles[0xB9] = 4;
    opcode[0xBA] = tsx;  amode[0xBA] = implied;     cycles[0xBA] = 2;
    opcode[0xBB] = udoc; amode[0xBB] = absolute_y;  cycles[0xBB] = 2;
    opcode[0xBC] = ldy;  amode[0xBC] = absolute_x;  cycles[0xBC] = 4;
    opcode[0xBD] = lda;  amode[0xBD] = absolute_x;  cycles[0xBD] = 4;
    opcode[0xBE] = ldx;  amode[0xBE] = absolute_y;  cycles[0xBE] = 4;
    opcode[0xBF] = lax;  amode[0xBF] = absolute_y;  cycles[0xBF] = 2;

    opcode[0xC0] = cpy;  amode[0xC0] = immediate;   cycles[0xC0] = 3;
    opcode[0xC1] = cmp;  amode[0xC1] = indirect_x;  cycles[0xC1] = 6;
    opcode[0xC2] = udoc; amode[0xC2] = immediate;   cycles[0xC2] = 2;
    opcode[0xC3] = dcp;  amode[0xC3] = indirect_x;  cycles[0xC3] = 2;
    opcode[0xC4] = cpy;  amode[0xC4] = zeropage;    cycles[0xC4] = 3;
    opcode[0xC5] = cmp;  amode[0xC5] = zeropage;    cycles[0xC5] = 3;
    opcode[0xC6] = dec;  amode[0xC6] = zeropage;    cycles[0xC6] = 5;
    opcode[0xC7] = dcp;  amode[0xC7] = zeropage;    cycles[0xC7] = 2;
    opcode[0xC8] = iny;  amode[0xC8] = implied;     cycles[0xC8] = 2;
    opcode[0xC9] = cmp;  amode[0xC9] = immediate;   cycles[0xC9] = 3;
    opcode[0xCA] = dex;  amode[0xCA] = implied;     cycles[0xCA] = 2;
    opcode[0xCB] = udoc; amode[0xCB] = immediate;   cycles[0xCB] = 2;
    opcode[0xCC] = cpy;  amode[0xCC] = absolute;    cycles[0xCC] = 4;
    opcode[0xCD] = cmp;  amode[0xCD] = absolute;    cycles[0xCD] = 4;
    opcode[0xCE] = dec;  amode[0xCE] = absolute;    cycles[0xCE] = 6;
    opcode[0xCF] = dcp;  amode[0xCF] = absolute;    cycles[0xCF] = 2;

    opcode[0xD0] = bne;  amode[0xD0] = relative;    cycles[0xD0] = 2;
    opcode[0xD1] = cmp;  amode[0xD1] = indirect_y;  cycles[0xD1] = 5;
    opcode[0xD2] = cmp;  amode[0xD2] = zeropage_id; cycles[0xD2] = 3;
    opcode[0xD3] = dcp;  amode[0xD3] = indirect_y;  cycles[0xD3] = 2;
    opcode[0xD4] = udoc; amode[0xD4] = zeropage_x;  cycles[0xD4] = 2;
    opcode[0xD5] = cmp;  amode[0xD5] = zeropage_x;  cycles[0xD5] = 4;
    opcode[0xD6] = dec;  amode[0xD6] = zeropage_x;  cycles[0xD6] = 6;
    opcode[0xD7] = dcp;  amode[0xD7] = zeropage_x;  cycles[0xD7] = 2;
    opcode[0xD8] = cld;  amode[0xD8] = implied;     cycles[0xD8] = 2;
    opcode[0xD9] = cmp;  amode[0xD9] = absolute_y;  cycles[0xD9] = 4;
    opcode[0xDA] = nop;  amode[0xDA] = implied;     cycles[0xDA] = 3;
    opcode[0xDB] = dcp;  amode[0xDB] = absolute_y;  cycles[0xDB] = 2;
    opcode[0xDC] = udoc; amode[0xDC] = absolute_x;  cycles[0xDC] = 2;
    opcode[0xDD] = cmp;  amode[0xDD] = absolute_x;  cycles[0xDD] = 4;
    opcode[0xDE] = dec;  amode[0xDE] = absolute_x;  cycles[0xDE] = 7;
    opcode[0xDF] = dcp;  amode[0xDF] = absolute_x;  cycles[0xDF] = 2;

    opcode[0xE0] = cpx;  amode[0xE0] = immediate;   cycles[0xE0] = 3;
    opcode[0xE1] = sbc;  amode[0xE1] = indirect_x;  cycles[0xE1] = 6;
    opcode[0xE2] = udoc; amode[0xE2] = immediate;   cycles[0xE2] = 2;
    opcode[0xE3] = isb;  amode[0xE3] = indirect_x;  cycles[0xE3] = 2;
    opcode[0xE4] = cpx;  amode[0xE4] = zeropage;    cycles[0xE4] = 3;
    opcode[0xE5] = sbc;  amode[0xE5] = zeropage;    cycles[0xE5] = 3;
    opcode[0xE6] = inc;  amode[0xE6] = zeropage;    cycles[0xE6] = 5;
    opcode[0xE7] = isb;  amode[0xE7] = zeropage;    cycles[0xE7] = 2;
    opcode[0xE8] = inx;  amode[0xE8] = implied;     cycles[0xE8] = 2;
    opcode[0xE9] = sbc;  amode[0xE9] = immediate;   cycles[0xE9] = 3;
    opcode[0xEA] = nop;  amode[0xEA] = implied;     cycles[0xEA] = 2;
    opcode[0xEB] = sbc;  amode[0xEB] = immediate;   cycles[0xEB] = 2;
    opcode[0xEC] = cpx;  amode[0xEC] = absolute;    cycles[0xEC] = 4;
    opcode[0xED] = sbc;  amode[0xED] = absolute;    cycles[0xED] = 4;
    opcode[0xEE] = inc;  amode[0xEE] = absolute;    cycles[0xEE] = 6;
    opcode[0xEF] = isb;  amode[0xEF] = absolute;    cycles[0xEF] = 2;

    opcode[0xF0] = beq;  amode[0xF0] = relative;    cycles[0xF0] = 2;
    opcode[0xF1] = sbc;  amode[0xF1] = indirect_y;  cycles[0xF1] = 5;
    opcode[0xF2] = sbc;  amode[0xF2] = zeropage_id; cycles[0xF2] = 3;
    opcode[0xF3] = isb;  amode[0xF3] = indirect_y;  cycles[0xF3] = 2;
    opcode[0xF4] = udoc; amode[0xF4] = zeropage_x;  cycles[0xF4] = 2;
    opcode[0xF5] = sbc;  amode[0xF5] = zeropage_x;  cycles[0xF5] = 4;
    opcode[0xF6] = inc;  amode[0xF6] = zeropage_x;  cycles[0xF6] = 6;
    opcode[0xF7] = isb;  amode[0xF7] = zeropage_x;  cycles[0xF7] = 2;
    opcode[0xF8] = sed;  amode[0xF8] = implied;     cycles[0xF8] = 2;
    opcode[0xF9] = sbc;  amode[0xF9] = absolute_y;  cycles[0xF9] = 4;
    opcode[0xFA] = nop;  amode[0xFA] = implied;     cycles[0xFA] = 4;
    opcode[0xFB] = isb;  amode[0xFB] = absolute_y;  cycles[0xFB] = 2;
    opcode[0xFC] = udoc; amode[0xFC] = absolute_x;  cycles[0xFC] = 2;
    opcode[0xFD] = sbc;  amode[0xFD] = absolute_x;  cycles[0xFD] = 4;
    opcode[0xFE] = inc;  amode[0xFE] = absolute_x;  cycles[0xFE] = 7;
    opcode[0xFF] = isb;  amode[0xFF] = absolute_x;  cycles[0xFF] = 2;

    return luts;
}

void
unload_disasm_engine (disasm_luts** luts)
{
    free ((*luts)->opcode);
    free ((*luts)->amode); free ((*luts)->cycles);
    free (*luts);
    *luts = 0;
}

disasm_inst*
make_disasm (disasm_luts* luts, cpu_inst* cpux)
{
    /* Allocate a CPU instance for a 6502 CPU */
    disasm_inst* dax = (disasm_inst*) malloc (sizeof(disasm_inst));

    /* Populate the registers with power-on values */
    dax->PC = cpux->PC;
    dax->SP = cpux->SP;
    dax->A  = cpux->A;
    dax->X  = cpux->X;
    dax->Y  = cpux->Y;
    dax->S  = cpux->S;

    dax->OP = cpux->OP;

    dax->P0 = cpux->P0;
    dax->D0 = cpux->D0;

    /* Reset extra cycles counter */
    dax->xtra_cycles = 0;

    /* Snag the Memory Map */
    dax->mmap = cpux->mmap;

    /* Attach look up tables to CPU instance */
    dax->opcode = luts->opcode;
    dax->amode  = luts->amode;
    dax->cycles = luts->cycles;

    /* Return the address of the instance */
    return dax;

}

void
destroy_disasm (disasm_inst** dax)
{
    free (*dax);
    *dax = 0;
}

// Disassemble instruction @ current PC
void
disasm (cpu_inst* cpux, disasm_inst* dax)
{
    // The PC is not taken from cpux because we may not
    // want to disassemble only the intended instruction
    // flow.  Setting the dax->PC prior to calling this
    // function gives us more control over how we debug.

    // sync disasm with cpu
    dax->A  = cpux->A;
    dax->X  = cpux->X;
    dax->Y  = cpux->Y;

#if defined (commentout)
    if (*cpux->NMI) {
        printf ("Non-Maskable Interrupt!\n");
    }
#endif

    // Fetch opcode from MEM & increment Program Counter
    dax->OP = MEM_READ(dax->PC++);

    // Execute opcode
    dax->opcode[dax->OP](dax);
}
