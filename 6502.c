/*  This file is part of retrobox
    Copyright (C) 2010  James A. Shackleford

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

// file created: Sept 17th, 2010

// TODO:
//  1) Get 06-abs_xy.nes to pass:
//     * implement SXA
//     * implement SYA
//
//  2) Get 02-immediate.nes to pass:
//     * implement ATX
//     * implement AXS
//
//  3) Check clock cycles on NOPs, DOPs, and TOPs

#include <stdlib.h>
#include <stdio.h>
#include "6502.h"
#include "6502_types.h"
#include "2C02.h"
#include "memory.h"
#include "timer.h"

/********************************************************************
 * M A C R O S                                                      *
 *                                                                  *
 ********************************************************************/

// Memory Access Macros
#define MEM_READ(addr)          \
    read_mem (addr, cpux)        

#define MEM_WRITE(data,addr)    \
    write_mem (data, addr, cpux) 

// Flag Modifier Macros
#define SET_FLAG(flag)      \
    cpux->S |= (flag)        

#define UNSET_FLAG(flag)    \
    cpux->S &= ~(flag)       

#define HANDLE_SIGN_FLAG(reg)  \
    cpux->S &= ~FLAG_SIGN;     \
    cpux->S |= (reg & BIT7);    

#define HANDLE_ZERO_FLAG(reg)  \
    if (reg) {                 \
        cpux->S &= ~FLAG_ZERO; \
    } else {                   \
        cpux->S |= FLAG_ZERO;  \
    }                           

#define HANDLE_LSB_CARRY_FLAG(reg) \
    cpux->S &= ~FLAG_CARRY;        \
    cpux->S |= (reg & BIT0);        

#define HANDLE_MSB_CARRY_FLAG(reg) \
    cpux->S &= ~FLAG_CARRY;        \
    cpux->S |= (reg & BIT7) >> 7;   

// Stack Access Macros
#define STACK_PUSH(data)                \
    MEM_WRITE (data, 0x100+cpux->SP--);  

#define STACK_POP(data)                 \
    cpux->SP++;                         \
    data = MEM_READ (0x100+cpux->SP);    


/********************************************************************
 * A D D R E S S I N G      M O D E S                               *
 *                                                                  *
 *   For all modes, P0 holds calculated data addresses              *
 *                                                                  *
 ********************************************************************/

// ABSOLUTE (1 cc opcode fetch + 2 cc addressing):
//   PC+0 Opcode
//   PC+1 Address of Data ( low byte)
//   PC+2 Address of Data (high byte)
static void
absolute (cpu_inst* cpux)
{
    /* Grab high and low address byes and form data address */
    cpux->P0 = MEM_READ(cpux->PC) + (MEM_READ(cpux->PC+1) << 8);

    /* Get ready for next opcode */
    cpux->PC += 2;
}


// ABSOLUTE, X (4 cycles)
//   PC+0 Opcode
//   PC+1 Address of Data ( low byte)
//   PC+2 Address of Data (high byte)
static void
absolute_x (cpu_inst* cpux)
{
    // Dummy read for cycle accuracy
    MEM_READ (cpux->PC);

    /* Grab high and low address byes and form data address */
    cpux->P0 = MEM_READ(cpux->PC) + (MEM_READ(cpux->PC+1) << 8);

    /* Finish up with the program counter */
    cpux->PC += 2;

    /* Construct final data address */
    cpux->P0 += cpux->X;   
}


// ABSOLUTE, Y (3 or 4 cycles)
//   PC+0 Opcode
//   PC+1 Address of Data ( low byte)
//   PC+2 Address of Data (high byte)
static void
absolute_y (cpu_inst* cpux)
{
    // Dummy read for cycle accuracy
    MEM_READ (cpux->PC);

    /* Grab high and low address byes and form data address */
    cpux->P0 = MEM_READ(cpux->PC) + (MEM_READ(cpux->PC+1) << 8);

    /* Finish up with the program counter */
    cpux->PC += 2;

    /* Construct final data address */
    cpux->P0 += cpux->Y;   
}



// ABSOLUTE, X (3 or 4 cycles)
// For Opcodes that only read from Memory
//   PC+0 Opcode
//   PC+1 Address of Data ( low byte)
//   PC+2 Address of Data (high byte)
static void
abs_x_read (cpu_inst* cpux)
{
    /* Grab high and low address byes and form data address */
    cpux->P0 = MEM_READ(cpux->PC) + (MEM_READ(cpux->PC+1) << 8);

    /* Finish up with the program counter */
    cpux->PC += 2;

    /* Add one cycle if page boundry is crossed */
    if (cpux->cycles[cpux->OP]) {
        if ((cpux->P0 >> 8) != ((cpux->P0 + cpux->X) >> 8)) {
            MEM_READ (cpux->PC);
            cpux->xtra_cycles++;
        }
    }

    /* Construct final data address */
    cpux->P0 += cpux->X;   
}


// ABSOLUTE, Y (3 or 4 cycles)
// For Opcodes that only read from Memory
//   PC+0 Opcode
//   PC+1 Address of Data ( low byte)
//   PC+2 Address of Data (high byte)
static void
abs_y_read (cpu_inst* cpux)
{
    /* Grab high and low address byes and form data address */
    cpux->P0 = MEM_READ(cpux->PC) + (MEM_READ(cpux->PC+1) << 8);

    /* Finish up with the program counter */
    cpux->PC += 2;

    /* Add one cycle if page boundry is crossed */
    if (cpux->cycles[cpux->OP]) {
        if ((cpux->P0 >> 8) != ((cpux->P0 + cpux->Y) >> 8)) {
            MEM_READ (cpux->PC);
            cpux->xtra_cycles++;
        }
    }

    /* Construct final data address */
    cpux->P0 += cpux->Y;   
}


// IMMEDIATE (1 cycles):
//   PC+0 Opcode
//   PC+1 Parameter (Data)
static void
immediate (cpu_inst* cpux)
{
    // Store address of immediate parameter to P0 and inc PC
    cpux->P0 = cpux->PC++;
}


// IMPLIED (2 cycles):
//   PC+0 Opcode
static void
implied (cpu_inst* cpux)
{
    // The PC has already been incremeted from
    // when the opcode was fetched.  The PC is now
    // set to read the next opcode, so we don't need to
    // do anything here execpt for a dummy memory read
    // for cycle accuracy:
    MEM_READ (cpux->PC);
}


// INDIRECT (JMPs only):
// NOTE: Bugged in 6502 hardware.  Bugged here to match.
//   PC+0 Opcode
//   PC+1 Address of Data ( low byte)
//   PC+2 Address of Data (high byte)
static void
indirect (cpu_inst* cpux)
{
    // Build memory address *containing* jump address
    cpux->P0 = MEM_READ(cpux->PC) + (MEM_READ(cpux->PC+1) << 8);

    // Read jump address @ built memory address
    // (Most significant byte is isolated as in bugged 6502)
    cpux->P0 = MEM_READ(cpux->P0) + (MEM_READ((cpux->P0 & 0xFF00)+((cpux->P0+1) & 0x00FF)) << 8);

    // Get ready for next opcode
    cpux->PC += 2;
}


// INDIRECT, ABSOLUTE X (JMP only):
//   PC+0: Opcode
//   PC+1: Base address containing jump address base
static void
indirect_ax (cpu_inst* cpux)
{
    /* Compute the base address */
    cpux->P0 = MEM_READ(cpux->PC)
             + (MEM_READ(cpux->PC + 1) << 8)
             + cpux->X;

    /* Grab new PC value @ new address */
    cpux->P0 = MEM_READ(cpux->P0)
             + (MEM_READ(cpux->P0 + 1) << 8);
}


// INDIRECT, X (6 cycles):
//   PC+0 Opcode
//   PC+1 Page Zero Base Address (BAL)
//  BAL+0 Address of Data ( low byte)
//  BAL+1 Address of Data (high byte)
static void
indirect_x (cpu_inst* cpux)
{
    // Fetch base address and get ready for next opcode.
    cpux->D0 = MEM_READ(cpux->PC++);

    // Dummy read goes here for cycle accuracy
    MEM_READ (cpux->PC);

    // Fetch address of data
    cpux->P0 = MEM_READ((byte)(cpux->D0 + cpux->X))
             + (MEM_READ((byte)(cpux->D0 + cpux->X + 1)) << 8);
}


// INDIRECT, Y (5 cycles):
//   PC+0 Opcode
//   PC+1 Base Address
static void
indirect_y (cpu_inst* cpux)
{
    /* Fetch contents of base address */
    cpux->D0 = MEM_READ(cpux->PC++);

    /* Dummy Read for cycle accuracy */
    MEM_READ (cpux->PC);

    /* Construct base of jump address */
    cpux->P0 = MEM_READ(cpux->D0)
             + (MEM_READ((byte)(cpux->D0 + 1)) << 8);

    /* Add Y to jump address base */
    cpux->P0 += cpux->Y;
}


// INDIRECT, Y (4 or 5 cycles):
// For Opcodes that only read from Memory
//   PC+0 Opcode
//   PC+1 Base Address
static void
ind_y_read (cpu_inst* cpux)
{
    /* Fetch contents of base address */
    cpux->D0 = MEM_READ(cpux->PC++);

    /* Construct base of jump address */
    cpux->P0 = MEM_READ(cpux->D0)
             + (MEM_READ((byte)(cpux->D0 + 1)) << 8);

    /* Add Y to jump address base */
    cpux->P0 += cpux->Y;

    /* Add one cycle if page boundry is crossed */
    if (cpux->cycles[cpux->OP] == 5) {
        if ((cpux->P0 >> 8) != ((cpux->P0 + cpux->Y) >> 8)) {
            MEM_READ (cpux->PC);    // Dummy read
            cpux->xtra_cycles++;
        }
    }
}


// RELATIVE:
//   PC+0 Opcode
//   PC+1 Signed value to add to PC
static void
relative (cpu_inst* cpux)
{
    // Grab the signed PC modifier value
    cpux->D0 = MEM_READ(cpux->PC++);

    // Deal with negative PC modifiers
    if (cpux->D0 & BIT7) {
        cpux->D0 -= 0x100;
    }

    // Compute new PC value, store in P0
    cpux->P0 = cpux->PC + (signed char)cpux->D0;

    // Adjust timing for page change
    if ((cpux->P0 >> 8) != (cpux->PC >> 8)) {
        MEM_READ (cpux->PC);
        cpux->xtra_cycles++;
    }
}


// ZERO PAGE (2 cycles):
//   PC+0 Opcode
//   PC+1 Address of Data
static void
zeropage (cpu_inst* cpux)
{
    // Fetch the specified address from MEM
    // and increment the PC to get ready for next
    // instruction.
    cpux->P0 = MEM_READ(cpux->PC++);
}


// ZERO PAGE, X (3 cycles)
//   PC+0 Opcode
//   PC+1 Base Address
static void
zeropage_x (cpu_inst* cpux)
{
    // Dummy read for cycle accuracy
    MEM_READ (cpux->PC);

    cpux->P0 = MEM_READ(cpux->PC++)
             + cpux->X;

    cpux->P0 &= 0xFF;
}


// ZERO PAGE, Y (3 cycles)
//   PC+0 Opcode
//   PC+1 Base Address
static void
zeropage_y (cpu_inst* cpux)
{
    // Dummy read for cycle accuracy
    MEM_READ (cpux->PC);

    cpux->P0 = MEM_READ(cpux->PC++)
             + cpux->Y;

    cpux->P0 &= 0xFF;
}




/********************************************************************
 * O P C O D E S                                                    *
 *                                                                  *
 *  [ S V B D I Z C ] Status Register Flags                         *
 *    | | | | | | |                                                 *
 *    | | | | | | +-- Carry                                         *
 *    | | | | | +---- Zero                                          *
 *    | | | | +------ IRQ Enable                                    *
 *    | | | +-------- Binary Coded Decimal Enable                   *
 *    | | +---------- Sofware Interrupt (i.e. BRK)                  *
 *    | +------------ Overflow                                      *
 *    +-------------- Signed                                        *
 *                                                                  *
 ********************************************************************/
// AAC - And Memory with Accumulator
//  [ S V B D I Z C ]
//  [ / . . . . / / ]
static void
aac (cpu_inst* cpux)
{
    // Compute operand base address
    cpux->amode[cpux->OP](cpux);

    // Grab operand
    cpux->D0 = MEM_READ (cpux->P0);
    
    cpux->A &= cpux->D0;

    HANDLE_ZERO_FLAG (cpux->A);
    HANDLE_SIGN_FLAG (cpux->A);

    // Carry flag is set if result is negative
    if (cpux->A & FLAG_SIGN) {
        SET_FLAG (FLAG_CARRY);
    } else {
        UNSET_FLAG (FLAG_CARRY);
    }
}


// ADC - Add Memory to Accumulator w/ Carry
//  NOTE: BCD support not yet implemented.
//  [ S V B D I Z C ]
//  [ / / . . . / / ]
static void
adc (cpu_inst* cpux)
{
    unsigned int sum;

    // Compute operand base address
    cpux->amode[cpux->OP](cpux);

    // Grab operand
    cpux->D0 = MEM_READ (cpux->P0);

    // Compute sum
    sum = cpux->A
        + cpux->D0
        + (cpux->S & FLAG_CARRY);

    // Did the Accumulator's MSB toggle?
    if ( !((cpux->A ^ cpux->D0) & BIT7) && ((cpux->A ^ sum) & BIT7)) {
        SET_FLAG (FLAG_OVR);
    } else {
        UNSET_FLAG (FLAG_OVR);
    }

    // Store sum to Accumulator
    cpux->A = (byte)sum;

    // Check sign bit
    HANDLE_SIGN_FLAG (cpux->A);

    // Check for carry out
    if (sum > 0xFF) {
        SET_FLAG (FLAG_CARRY);
    } else {
        UNSET_FLAG (FLAG_CARRY);
    }

    // Check for zero result
    HANDLE_ZERO_FLAG (cpux->A);
}


// AND - "AND" Memory with Accumulator
//  [ S V B D I Z C ]
//  [ / . . . . / . ]
static void
and (cpu_inst* cpux)
{
    // Compute operand base address
    cpux->amode[cpux->OP](cpux);

    // AND the accumulator w/ data from memory
    cpux->A &= MEM_READ(cpux->P0);

    HANDLE_ZERO_FLAG (cpux->A);
    HANDLE_SIGN_FLAG (cpux->A);
}


// ARR - AND Memory w/ Accumulator, then
//       Rotate Accumulator 1-bit right, then
//       Check bits 5 and 6
//  [ S V B D I Z C ]
//  [ / / . . . / / ]
static void
arr (cpu_inst* cpux)
{
    byte S;

    // Save the "old" Carry Bit
    S = cpux->S & FLAG_CARRY;

    // Compute operand base address
    cpux->amode[cpux->OP](cpux);

    // Grab data from memory
    cpux->D0 = MEM_READ (cpux->P0);

    // AND Memory w/ Accumulator
    cpux->A &= cpux->D0;

    // Rotate right w/ carry into MSB
    cpux->A = cpux->A >> 1;
    cpux->A |= (S << 7);

    HANDLE_ZERO_FLAG (cpux->A);
    HANDLE_SIGN_FLAG (cpux->A);

    // Deal with C and V flags depending
    // on Accumulator bits 5 and 6
    if ((cpux->A & BIT5) && (cpux->A & BIT6)) {
        SET_FLAG (FLAG_CARRY);
        UNSET_FLAG (FLAG_OVR);
    }
    else if (!(cpux->A & BIT5) && (cpux->A & BIT6)) {
        SET_FLAG (FLAG_CARRY);
        SET_FLAG (FLAG_OVR);
    }
    else if ((cpux->A & BIT5) && !(cpux->A & BIT6)) {
        UNSET_FLAG (FLAG_CARRY);
        SET_FLAG (FLAG_OVR);
    }
    else {
        UNSET_FLAG (FLAG_CARRY);
        UNSET_FLAG (FLAG_OVR);
    }
}


// ASL - Shift Left One Bit (Memory)
//  [ S V B D I Z C ]
//  [ / . . . . / / ]
static void
asl (cpu_inst* cpux)
{
    // Compute operand base address
    cpux->amode[cpux->OP](cpux);

    // Grab specified data
    cpux->D0 = MEM_READ(cpux->P0);

    // Dummy write back to keep cycle timing
    MEM_WRITE (cpux->D0, cpux->P0);

    // Set C Flag if LSB is 1
    HANDLE_MSB_CARRY_FLAG (cpux->D0);

    // Shift Value Right
    cpux->D0 = cpux->D0 << 1;

    HANDLE_ZERO_FLAG (cpux->D0);
    HANDLE_SIGN_FLAG (cpux->D0);

    // Save Back to Memory
    MEM_WRITE (cpux->D0, cpux->P0);

}


// ASLA - Shift Left One Bit (Accumulator)
//  [ S V B D I Z C ]
//  [ / . . . . / / ]
static void
asla (cpu_inst* cpux)
{
    cpux->amode[cpux->OP](cpux);

    // Set C Flag if MSB is 1
    HANDLE_MSB_CARRY_FLAG (cpux->A);

    // Shift Value Right
    cpux->A = cpux->A << 1;

    HANDLE_ZERO_FLAG (cpux->A);
    HANDLE_SIGN_FLAG (cpux->A);
}


// ASR - AND Memory w/ Accumulator, then
//       Shift Accumulator 1-bit right
//  [ S V B D I Z C ]
//  [ / . . . . / / ]
static void
asr (cpu_inst* cpux)
{
    cpux->amode[cpux->OP](cpux);

    cpux->D0 = MEM_READ (cpux->P0);

    cpux->A &= cpux->D0;

    // Set C Flag if LSB is 1
    HANDLE_LSB_CARRY_FLAG (cpux->A);

    // Shift Value Right
    cpux->A = cpux->A >> 1;

    HANDLE_ZERO_FLAG (cpux->A);
    HANDLE_SIGN_FLAG (cpux->A);

}


// ATX - AND Memory w/ Accumulator, then
//       Transfer Accumulator to X Index
//  [ S V B D I Z C ]
//  [ / . . . . / . ]
static void
atx (cpu_inst* cpux)
{
    cpux->amode[cpux->OP](cpux);
    cpux->D0 = MEM_READ (cpux->P0);

    cpux->A &= cpux->D0;
    cpux->X = cpux->A;

    HANDLE_ZERO_FLAG (cpux->X);
    HANDLE_SIGN_FLAG (cpux->X);
}


// BCC - Branch on Carry Clear
// (Only Relative)
//  [ S V B D I Z C ]
//  [ . . . . . . . ]
static void
bcc (cpu_inst* cpux)
{
    if ((cpux->S & FLAG_CARRY) == 0) {
        cpux->amode[cpux->OP](cpux);
        cpux->PC = cpux->P0;

        MEM_READ (cpux->PC);
        cpux->xtra_cycles++;
    } else {
        // Skip operand & don't branch
        cpux->PC++;
    }
}


// BCS - Branch on Carry Set
//  [ S V B D I Z C ]
//  [ . . . . . . . ]
static void
bcs (cpu_inst* cpux)
{
    if (cpux->S & FLAG_CARRY) {
        cpux->amode[cpux->OP](cpux);
        cpux->PC = cpux->P0;

        MEM_READ (cpux->PC);
        cpux->xtra_cycles++;
    } else {
        // Skip operand & don't branch
        cpux->PC++;
    }
}


// BEQ - Branch on Result Zero
//  [ S V B D I Z C ]
//  [ . . . . . . . ]
static void
beq (cpu_inst* cpux)
{
    if (cpux->S & FLAG_ZERO) {
        cpux->amode[cpux->OP](cpux);
        cpux->PC = cpux->P0;

        MEM_READ (cpux->PC);
        cpux->xtra_cycles++;
    } else {
        // Skip operand & don't branch
        cpux->PC++;
    }
}


// BIT - Test Bits in Mem w/ Accumulator
//  [ S V B D I Z C ]
//  [M7 M6. . . / . ]
static void
bit (cpu_inst* cpux)
{
    // Compute operand base address
    cpux->amode[cpux->OP](cpux);

    // Retrieve data from memory
    cpux->D0 = MEM_READ(cpux->P0);

    // AND Accumulator with Mem contents for Zero Flag
    if (cpux->D0 & cpux->A) {
        UNSET_FLAG (FLAG_ZERO);
    } else {
        SET_FLAG (FLAG_ZERO);
    }

    // Sign & Overflow bits come from memory data
    UNSET_FLAG (FLAG_SIGN | FLAG_OVR);
    SET_FLAG (cpux->D0 & (FLAG_SIGN | FLAG_OVR));
}


// BMI - Branch on Result Minus
//  [ S V B D I Z C ]
//  [ . . . . . . . ]
static void
bmi (cpu_inst* cpux)
{
    if (cpux->S & FLAG_SIGN) {
        cpux->amode[cpux->OP](cpux);
        cpux->PC = cpux->P0;

        MEM_READ (cpux->PC);
        cpux->xtra_cycles++;
    } else {
        // Skip operand & don't branch
        cpux->PC++;
    }
}


// BNE - Branch on Result NOT Zero
//  [ S V B D I Z C ]
//  [ . . . . . . . ]
static void
bne (cpu_inst* cpux)
{
    if ((cpux->S & FLAG_ZERO) == 0) {
        cpux->amode[cpux->OP](cpux);
        cpux->PC = cpux->P0;

        MEM_READ (cpux->PC);
        cpux->xtra_cycles++;
    } else {
        // Skip operand & don't branch
        cpux->PC++;
    }
}


// BPL - Branch on Result Plus
//  [ S V B D I Z C ]
//  [ . . . . . . . ]
static void
bpl (cpu_inst* cpux)
{
    if ((cpux->S & FLAG_SIGN) == 0) {
        cpux->amode[cpux->OP](cpux);
        cpux->PC = cpux->P0;

        MEM_READ (cpux->PC);
        cpux->xtra_cycles++;
    } else {
        // Skip operand & don't branch
        cpux->PC++;
    }
}


// BRA - UNDOCUMENTED
//  [ S V B D I Z C ]
//  [ ? ? ? ? ? ? ? ]
static void
bra (cpu_inst* cpux)
{
    // Compute operand base address
    cpux->amode[cpux->OP](cpux);
}


// BRK - Force Break
//  [ S V B D I Z C ]
//  [ . . 1 . 1 . . ]
static void
brk (cpu_inst* cpux)
{
    // Dummy Read for cycle timing accuracy
    MEM_READ (cpux->PC++);

    // Save the PC to the Stack MSB first
    STACK_PUSH ((byte)(cpux->PC >> 8));
    STACK_PUSH ((byte)(cpux->PC & 0x00FF));   //JAS: Fixed (0x0F to 0xFF)

    // Save Status Register to Stack
    STACK_PUSH (cpux->S | (BIT4 | BIT5));

    // Set SWI and IRQE Flags
    SET_FLAG (FLAG_SWI | FLAG_IRQE);

    // Grab jump address from IRQ/BRK vector @ 0xFFFE
    cpux->PC = MEM_READ (0xFFFE)
             + (MEM_READ (0xFFFF) << 8);
}


// BVC - Branch on Overflow Clear
//  [ S V B D I Z C ]
//  [ . . . . . . . ]
static void
bvc (cpu_inst* cpux)
{
    if ((cpux->S & FLAG_OVR) == 0) {
        cpux->amode[cpux->OP](cpux);
        cpux->PC = cpux->P0;

        MEM_READ (cpux->PC);
        cpux->xtra_cycles++;
    } else {
        // Skip operand & don't branch
        cpux->PC++;
    }
}


// BVS - Branch on Overflow Set
//  [ S V B D I Z C ]
//  [ . . . . . . . ]
static void
bvs (cpu_inst* cpux)
{
    if (cpux->S & FLAG_OVR) {
        cpux->amode[cpux->OP](cpux);
        cpux->PC = cpux->P0;

        MEM_READ (cpux->PC);
        cpux->xtra_cycles++;
    } else {
        // Skip operand & don't branch
        cpux->PC++;
    }
}


// CLC - Clear Carry Flag
//  [ S V B D I Z C ]
//  [ . . . . . . 0 ]
static void
clc (cpu_inst* cpux)
{
    cpux->amode[cpux->OP](cpux);
    UNSET_FLAG (FLAG_CARRY);
}


// CLD - Clear Binary Coded Decimal Flag
//  [ S V B D I Z C ]
//  [ . . . 0 . . . ]
static void
cld (cpu_inst* cpux)
{
    cpux->amode[cpux->OP](cpux);
    UNSET_FLAG (FLAG_BCD);
}


// CLI - Clear Interrupt Disable Flag
//  [ S V B D I Z C ]
//  [ . . . . 0 . . ]
static void
cli (cpu_inst* cpux)
{
    cpux->amode[cpux->OP](cpux);
    UNSET_FLAG (FLAG_IRQE);
}


// CLV - Clear Overflow Flag
//  [ S V B D I Z C ]
//  [ . 0 . . . . . ]
static void
clv (cpu_inst* cpux)
{
    cpux->amode[cpux->OP](cpux);
    UNSET_FLAG (FLAG_OVR);
}


// CMP - Compare Mem to Accumulator
//  [ S V B D I Z C ]
//  [ / . . . . / / ]
static void
cmp (cpu_inst* cpux)
{
    // Compute operand base address
    cpux->amode[cpux->OP](cpux);

    // Grab data from memory
    cpux->D0 = MEM_READ (cpux->P0);

    // Carry Flag if A > Mem
    if ((0x100 + cpux->A - cpux->D0) > 0xFF) {
        SET_FLAG (FLAG_CARRY);
    } else {
        UNSET_FLAG (FLAG_CARRY);
    }

    cpux->D0 = cpux->A - cpux->D0;

    if (cpux->D0) {
        UNSET_FLAG (FLAG_ZERO);
    } else {
        SET_FLAG (FLAG_ZERO);
    }

    HANDLE_SIGN_FLAG (cpux->D0 & FLAG_SIGN);
}


// CPX - Compare Mem to X Index
//  [ S V B D I Z C ]
//  [ / . . . . / / ]
static void
cpx (cpu_inst* cpux)
{
    // Compute operand base address
    cpux->amode[cpux->OP](cpux);

    // Grab data from memory
    cpux->D0 = MEM_READ (cpux->P0);

    // Carry Flag if X > Mem
    if ((0x100 + cpux->X - cpux->D0) > 0xFF) {
        SET_FLAG (FLAG_CARRY);
    } else {
        UNSET_FLAG (FLAG_CARRY);
    }

    cpux->D0 = cpux->X - cpux->D0;

    if (cpux->D0) {
        UNSET_FLAG (FLAG_ZERO);
    } else {
        SET_FLAG (FLAG_ZERO);
    }

    HANDLE_SIGN_FLAG (cpux->D0 & FLAG_SIGN);
}


// CPY - Compare Mem to Y Index
//  [ S V B D I Z C ]
//  [ / . . . . / / ]
static void
cpy (cpu_inst* cpux)
{
    // Compute operand base address
    cpux->amode[cpux->OP](cpux);

    // Grab data from memory
    cpux->D0 = MEM_READ (cpux->P0);

    // Carry Flag if Y > Mem
    if ((0x100 + cpux->Y - cpux->D0) > 0xFF) {
        SET_FLAG (FLAG_CARRY);
    } else {
        UNSET_FLAG (FLAG_CARRY);
    }

    cpux->D0 = cpux->Y - cpux->D0;

    if (cpux->D0) {
        UNSET_FLAG (FLAG_ZERO);
    } else {
        SET_FLAG (FLAG_ZERO);
    }

    HANDLE_SIGN_FLAG (cpux->D0 & FLAG_SIGN);
}


// DEA - UNDOCUMENTED
//  [ S V B D I Z C ]
//  [ ? ? ? ? ? ? ? ]
static void
dea (cpu_inst* cpux)
{
    // Compute operand base address
    cpux->amode[cpux->OP](cpux);
}


// DEC - Decrement Mem by 1
//  [ S V B D I Z C ]
//  [ / . . . . / . ]
static void
dec (cpu_inst* cpux)
{
    // Compute operand base address
    cpux->amode[cpux->OP](cpux);

    cpux->D0 = MEM_READ (cpux->P0);

    // Dummy write back to keep cycle timing
    MEM_WRITE (cpux->D0, cpux->P0);

    cpux->D0--;

    MEM_WRITE (cpux->D0, cpux->P0);

    HANDLE_SIGN_FLAG (cpux->D0);
    HANDLE_ZERO_FLAG (cpux->D0);
}


// DEX - Decrement X Index by 1
//  [ S V B D I Z C ]
//  [ / . . . . / . ]
static void
dex (cpu_inst* cpux)
{
    cpux->amode[cpux->OP](cpux);
    cpux->X--;

    HANDLE_SIGN_FLAG (cpux->X);
    HANDLE_ZERO_FLAG (cpux->X);
}


// DEY - Decrement Y Index by 1
//  [ S V B D I Z C ]
//  [ / . . . . / . ]
static void
dey (cpu_inst* cpux)
{
    cpux->amode[cpux->OP](cpux);
    cpux->Y--;

    HANDLE_SIGN_FLAG (cpux->Y);
    HANDLE_ZERO_FLAG (cpux->Y);
}

// DCP - Decrement Memory & Compare against A
//  [ S V B D I Z C ]
//  [ / . . . . / / ]
static void
dcp (cpu_inst* cpux)
{
    byte tmp;

    cpux->amode[cpux->OP](cpux);

    // Grab data from memory
    cpux->D0 = MEM_READ (cpux->P0);

    // Dummy write back to keep cycle timing
    MEM_WRITE (cpux->D0, cpux->P0);

    // Decrement
    cpux->D0--;
    MEM_WRITE (cpux->D0, cpux->P0);

    // Carry Flag if A > Mem
    if ((0x100 + cpux->A - cpux->D0) > 0xFF) {
        SET_FLAG (FLAG_CARRY);
    } else {
        UNSET_FLAG (FLAG_CARRY);
    }

    cpux->D0 = cpux->A - cpux->D0;

    if (cpux->D0) {
        UNSET_FLAG (FLAG_ZERO);
    } else {
        SET_FLAG (FLAG_ZERO);
    }

    HANDLE_SIGN_FLAG (cpux->D0 & FLAG_SIGN);
}

// EOR - XOR Accumulator with Memory
//  [ S V B D I Z C ]
//  [ / . . . . / . ]
static void
eor (cpu_inst* cpux)
{
    // Compute operand base address
    cpux->amode[cpux->OP](cpux);

    cpux->A ^= MEM_READ (cpux->P0);

    HANDLE_SIGN_FLAG (cpux->A);
    HANDLE_ZERO_FLAG (cpux->A);
}


// INA - UNDOCUMENTED
//  [ S V B D I Z C ]
//  [ ? ? ? ? ? ? ? ]
static void
ina (cpu_inst* cpux)
{
    // Compute operand base address
    cpux->amode[cpux->OP](cpux);
}


// INC - Increment Memory by 1
//  [ S V B D I Z C ]
//  [ / . . . . / . ]
static void
inc (cpu_inst* cpux)
{
    // Compute operand base address
    cpux->amode[cpux->OP](cpux);

    // Increment data @ memory address
    cpux->D0 = MEM_READ (cpux->P0);

    // Dummy write back to keep cycle timing
    MEM_WRITE (cpux->D0, cpux->P0);

    cpux->D0++;

    MEM_WRITE (cpux->D0, cpux->P0);

    HANDLE_ZERO_FLAG (cpux->D0);
    HANDLE_SIGN_FLAG (cpux->D0);
}


// INX - Increment X Index by 1
//  [ S V B D I Z C ]
//  [ / . . . . / . ]
static void
inx (cpu_inst* cpux)
{
    cpux->amode[cpux->OP](cpux);
    cpux->X++;

    HANDLE_SIGN_FLAG (cpux->X);
    HANDLE_ZERO_FLAG (cpux->X);
}


// INY - Increment Y Index by 1
//  [ S V B D I Z C ]
//  [ / . . . . / . ]
static void
iny (cpu_inst* cpux)
{
    cpux->amode[cpux->OP](cpux);
    cpux->Y++;

    HANDLE_SIGN_FLAG (cpux->Y);
    HANDLE_ZERO_FLAG (cpux->Y);
}


// ISB - Increment memory, then subtract Memory from Accumulator w/ Borrow
//  [ S V B D I Z C ]
//  [ / / . . . / / ]
//  (BCD Support not implemented)
static void
isb (cpu_inst* cpux)
{
    byte S;
    unsigned int sum;

    // Compute operand base address
    cpux->amode[cpux->OP](cpux);

    // Fetch memory data
    cpux->D0 = MEM_READ (cpux->P0);

    // Dummy write back to keep cycle timing
    MEM_WRITE (cpux->D0, cpux->P0);

    // Increment data from memory
    cpux->D0++;

    MEM_WRITE (cpux->D0, cpux->P0);

    // Save the Carry Flag
    S = cpux->S & FLAG_CARRY;

    // Perform Subtraction with Borrow
    sum = cpux->A - cpux->D0 - (1-S);

    HANDLE_SIGN_FLAG (sum);
    HANDLE_ZERO_FLAG (sum);

    // Did the Accumulator's MSB toggle?
    if ( ((cpux->A ^ cpux->D0) & BIT7) && ((cpux->A ^ sum) & BIT7)) {
        SET_FLAG (FLAG_OVR);
    } else {
        UNSET_FLAG (FLAG_OVR);
    }

    // Handle Carry Flag
    if (sum <= 0xFF) {
        SET_FLAG (FLAG_CARRY);
    } else {
        UNSET_FLAG (FLAG_CARRY);
    }

    cpux->A = (byte)(sum & 0xFF);
}


// JMP - Unconditional Jump
//  [ S V B D I Z C ]
//  [ . . . . . . . ]
static void
jmp (cpu_inst* cpux)
{
    // Compute operand base address
    cpux->amode[cpux->OP](cpux);

    cpux->PC = cpux->P0;
}


// JSR - Jump to new location and Push return address
//  [ S V B D I Z C ]
//  [ . . . . . . . ]
static void
jsr (cpu_inst* cpux)
{
    // Dummy read for cycle timing
    MEM_READ (cpux->PC);

    // Save Return Address MSB first
    cpux->PC++;
    STACK_PUSH ((byte)(cpux->PC >> 8));
    STACK_PUSH ((byte)(cpux->PC & 0x00FF));

    // Do the jump
    cpux->PC--;
    cpux->amode[cpux->OP](cpux);
    cpux->PC = cpux->P0;
}


// LAX - Load Accumulator & X Index with Memory
//  [ S V B D I Z C ]
//  [ / . . . . / . ]
static void
lax (cpu_inst* cpux)
{
    // Compute operand base address
    cpux->amode[cpux->OP](cpux);

    // Load value from memory and put in both A & X
    cpux->A = MEM_READ(cpux->P0);
    cpux->X = cpux->A;

    HANDLE_SIGN_FLAG (cpux->A);
    HANDLE_ZERO_FLAG (cpux->A);
}


// LDA - Load Accumulator with Memory
//  [ S V B D I Z C ]
//  [ / . . . . / . ]
static void
lda (cpu_inst* cpux)
{
    // Compute operand base address
    cpux->amode[cpux->OP](cpux);

    // Load value from memory
    cpux->A = MEM_READ(cpux->P0);

    HANDLE_SIGN_FLAG (cpux->A);
    HANDLE_ZERO_FLAG (cpux->A);
}


// LDX - Load X Index with Memory
//  [ S V B D I Z C ]
//  [ / . . . . / . ]
static void
ldx (cpu_inst* cpux)
{
    // Compute operand base address
    cpux->amode[cpux->OP](cpux);

    // Load value from memory
    cpux->X = MEM_READ(cpux->P0);

    HANDLE_SIGN_FLAG (cpux->X);
    HANDLE_ZERO_FLAG (cpux->X);
}


// LDY - Load Y Index with Memory
//  [ S V B D I Z C ]
//  [ / . . . . / . ]
static void
ldy (cpu_inst* cpux)
{
    // Compute operand base address
    cpux->amode[cpux->OP](cpux);

    // Load value from memory
    cpux->Y = MEM_READ(cpux->P0);

    HANDLE_SIGN_FLAG (cpux->Y);
    HANDLE_ZERO_FLAG (cpux->Y);
}


// LSR - LSB Shift Right 1-bit
//  [ S V B D I Z C ]
//  [ 0 . . . . / / ]
static void
lsr (cpu_inst* cpux)
{
    UNSET_FLAG (FLAG_SIGN);

    // Compute operand base address
    cpux->amode[cpux->OP](cpux);

    // Grab specified data
    cpux->D0 = MEM_READ(cpux->P0);

    // Dummy write to maintain cycle timing
    MEM_WRITE (cpux->D0, cpux->P0);

    // Set C Flag if LSB is 1
    HANDLE_LSB_CARRY_FLAG (cpux->D0);

    // Shift Value Right
    cpux->D0 = cpux->D0 >> 1;

    // Set Z Flag if Zero
    HANDLE_ZERO_FLAG (cpux->D0);

    // Save Back to Memory
    MEM_WRITE (cpux->D0, cpux->P0);

}


// LSR - LSB Shift Right 1-bit (for Implied Addressing)
//  [ S V B D I Z C ]
//  [ 0 . . . . / / ]
static void
lsra (cpu_inst* cpux)
{
    cpux->amode[cpux->OP](cpux);

    UNSET_FLAG (FLAG_SIGN);

    // Set C Flag if LSB is 1
    HANDLE_LSB_CARRY_FLAG (cpux->A);

    // Shift Accumulator Right
    cpux->A = cpux->A >> 1;

    // Set Z Flag if Zero
    HANDLE_ZERO_FLAG (cpux->A);
}


// NOP - Do Nothing
//  [ S V B D I Z C ]
//  [ . . . . . . . ]
static void
nop (cpu_inst* cpux)
{
    cpux->amode[cpux->OP](cpux);
    // ...
}


// ORA - OR Memory with Accumulator
//  [ S V B D I Z C ]
//  [ / . . . . / . ]
static void
ora (cpu_inst* cpux)
{
    // Compute operand base address
    cpux->amode[cpux->OP](cpux);

    cpux->A |= MEM_READ (cpux->P0);

    HANDLE_ZERO_FLAG (cpux->A);
    HANDLE_SIGN_FLAG (cpux->A);
}


// PHA - Push Accumulator onto Stack
//  [ S V B D I Z C ]
//  [ . . . . . . . ]
static void
pha (cpu_inst* cpux)
{
    cpux->amode[cpux->OP](cpux);
    STACK_PUSH (cpux->A);
}


// PHP - Push Processor Status onto Stack
//  [ S V B D I Z C ]
//  [ . . . . . . . ]
static void
php (cpu_inst* cpux)
{
    cpux->amode[cpux->OP](cpux);

    // Correct behavior seems to be to set
    // BIT4 (FLAG_SWI) on stack value?
    STACK_PUSH (cpux->S | FLAG_SWI);

}


// PLA - Pull Accumulator off of Stack
//  [ S V B D I Z C ]
//  [ / . . . . / . ]
static void
pla (cpu_inst* cpux)
{
    cpux->amode[cpux->OP](cpux);

    // Dummy read for cycle accuracy
    MEM_READ (cpux->PC);

    STACK_POP (cpux->A);
    HANDLE_ZERO_FLAG (cpux->A);
    HANDLE_SIGN_FLAG (cpux->A);
}


// PLP - Pull Processor Status off of Stack
//  [ S V B D I Z C ]
//  [ . . . . . . . ]
static void
plp (cpu_inst* cpux)
{
    cpux->amode[cpux->OP](cpux);

    // Dummy Read for cycle accuracy
    MEM_READ (cpux->PC);

    STACK_POP (cpux->S);

    // BRK (SWI) flag is never set *IN* status reg
    UNSET_FLAG (FLAG_SWI);
}


// RLA - Rotate one bit left (Memory)
//  [ S V B D I Z C ]
//  [ / . . . . / / ]
static void
rla (cpu_inst* cpux)
{
    byte S;

    // Save the "old" Carry Bit
    S = cpux->S & FLAG_CARRY;

    // Compute operand base address
    cpux->amode[cpux->OP](cpux);

    // Grab data from memory
    cpux->D0 = MEM_READ (cpux->P0);

    // Dummy write back to keep cycle timing
    MEM_WRITE (cpux->D0, cpux->P0);

    // Set the Carry bit to the MSB of Mem Data
    UNSET_FLAG (FLAG_CARRY);
    SET_FLAG ((cpux->D0 >> 7) & FLAG_CARRY);

    // Rotate left w/ carry into LSB
    cpux->D0 = cpux->D0 << 1;
    cpux->D0 |= S;

    // Put data back into memory
    MEM_WRITE (cpux->D0, cpux->P0);

    cpux->A &= cpux->D0;

    HANDLE_SIGN_FLAG (cpux->A);
    HANDLE_ZERO_FLAG (cpux->A);

}


// ROL - Rotate one bit left (Memory)
//  [ S V B D I Z C ]
//  [ / . . . . / / ]
static void
rol (cpu_inst* cpux)
{
    byte S;

    // Save the "old" Carry Bit
    S = cpux->S & FLAG_CARRY;

    // Compute operand base address
    cpux->amode[cpux->OP](cpux);

    // Grab data from memory
    cpux->D0 = MEM_READ (cpux->P0);

    // Dummy write back to keep cycle timing
    MEM_WRITE (cpux->D0, cpux->P0);

    // Set the Carry bit to the MSB of Mem Data
    UNSET_FLAG (FLAG_CARRY);
    SET_FLAG ((cpux->D0 >> 7) & FLAG_CARRY);

    // Rotate left w/ carry into LSB
    cpux->D0 = cpux->D0 << 1;
    cpux->D0 |= S;

    HANDLE_SIGN_FLAG (cpux->D0);
    HANDLE_ZERO_FLAG (cpux->D0);

    // Put data back into memory
    MEM_WRITE (cpux->D0, cpux->P0);
}


// ROL - Rotate one bit left (Accumulator)
//  [ S V B D I Z C ]
//  [ / . . . . / / ]
static void
rola (cpu_inst* cpux)
{
    cpux->amode[cpux->OP](cpux);

    // Save the "old" Carry Bit
    cpux->D0 = cpux->S & FLAG_CARRY;

    // Set the Carry bit to the MSB of Mem Data
    UNSET_FLAG (FLAG_CARRY);
    SET_FLAG ((cpux->A >> 7) & FLAG_CARRY);

    // Rotate left w/ carry into LSB
    cpux->A = cpux->A << 1;
    cpux->A |= cpux->D0;

    HANDLE_SIGN_FLAG (cpux->A);
    HANDLE_ZERO_FLAG (cpux->A);
}


// ROR - Rotate one bit right (Memory)
//  [ S V B D I Z C ]
//  [ / . . . . / / ]
static void
ror (cpu_inst* cpux)
{
    byte S;

    // Save the "old" Carry Bit
    S = cpux->S & FLAG_CARRY;

    // Compute operand base address
    cpux->amode[cpux->OP](cpux);

    // Grab data from memory
    cpux->D0 = MEM_READ (cpux->P0);

    // Dummy write back to keep cycle timing
    MEM_WRITE (cpux->D0, cpux->P0);

    // Set the Carry bit to the LSB of Mem Data
    UNSET_FLAG (FLAG_CARRY);
    SET_FLAG (cpux->D0 & FLAG_CARRY);

    // Rotate right w/ carry into MSB
    cpux->D0 = cpux->D0 >> 1;
    cpux->D0 |= (S << 7);

    HANDLE_SIGN_FLAG (cpux->D0);
    HANDLE_ZERO_FLAG (cpux->D0);

    // Put data back into memory
    MEM_WRITE (cpux->D0, cpux->P0);
}


// ROR - Rotate one bit right (Accumulator)
//  [ S V B D I Z C ]
//  [ / . . . . / / ]
static void
rora (cpu_inst* cpux)
{
    // Save the "old" Carry Bit
    cpux->D0 = cpux->S & FLAG_CARRY;

    // Compute operand base address
    cpux->amode[cpux->OP](cpux);

    // Set the Carry bit to the LSB of Mem Data
    UNSET_FLAG (FLAG_CARRY);
    SET_FLAG (cpux->A & FLAG_CARRY);

    // Rotate right w/ carry into MSB
    cpux->A = cpux->A >> 1;
    cpux->A |= (cpux->D0 << 7);

    HANDLE_SIGN_FLAG (cpux->A);
    HANDLE_ZERO_FLAG (cpux->A);
}


// RRA - Rotate Memory Right 1-bit, then add Memory to Accumulator w/ Carry
//  NOTE: BCD support not yet implemented.
//  [ S V B D I Z C ]
//  [ / / . . . / / ]
static void
rra (cpu_inst* cpux)
{
    byte S;
    unsigned int sum;

    // Save the "old" Carry Bit
    S = cpux->S & FLAG_CARRY;

    // Compute operand base address
    cpux->amode[cpux->OP](cpux);

    // Grab operand
    cpux->D0 = MEM_READ (cpux->P0);

    // Dummy write back to keep cycle timing
    MEM_WRITE (cpux->D0, cpux->P0);

    // Set the Carry bit to the LSB of Mem Data
    UNSET_FLAG (FLAG_CARRY);
    SET_FLAG (cpux->D0 & FLAG_CARRY);

    // Rotate right w/ carry into MSB
    cpux->D0 = cpux->D0 >> 1;
    cpux->D0 |= (S << 7);

    // Put data back into memory
    MEM_WRITE (cpux->D0, cpux->P0);

    // Compute sum
    sum = cpux->A
        + cpux->D0
        + (cpux->S & FLAG_CARRY);

    // Did the Accumulator's MSB toggle?
    if ( !((cpux->A ^ cpux->D0) & BIT7) && ((cpux->A ^ sum) & BIT7)) {
        SET_FLAG (FLAG_OVR);
    } else {
        UNSET_FLAG (FLAG_OVR);
    }

    // Store sum to Accumulator
    cpux->A = (byte)sum;

    // Check sign bit
    HANDLE_SIGN_FLAG (cpux->A);

    // Check for carry out
    if (sum > 0xFF) {
        SET_FLAG (FLAG_CARRY);
    } else {
        UNSET_FLAG (FLAG_CARRY);
    }

    // Check for zero result
    HANDLE_ZERO_FLAG (cpux->A);
}



// RTI - Return from Interrupt
//  [ S V B D I Z C ]
//  [   From Stack  ]
static void
rti (cpu_inst* cpux)
{
    cpux->amode[cpux->OP](cpux);

    // Dummy read for cycle accuracy
    MEM_READ (cpux->PC);

    // Restore Status Register
    STACK_POP (cpux->S);

    // Restore Program Counter
    STACK_POP (cpux->PC);
    STACK_POP (cpux->P0);       // JAS: Changed from D0 to P0
    cpux->PC |= cpux->P0 << 8;

}


// RTS - Return from Subroutine
//  [ S V B D I Z C ]
//  [ _ _ _ _ _ _ _ ]
static void
rts (cpu_inst* cpux)
{
    cpux->amode[cpux->OP](cpux);

    // Dummy read for cycle accuracy
    MEM_READ (cpux->PC);

    // Restore Program Counter
    STACK_POP (cpux->PC);
    STACK_POP (cpux->P0);       // JAS: Changed D0 to P0
    cpux->PC |= cpux->P0 << 8;

    // Dummy read and increment PC
    MEM_READ (cpux->PC++);
}

// SAX - Store (Accumulator) AND (X Index) to Memory
//  [ S V B D I Z C ]
//  [ . . . . . . . ]
static void
sax (cpu_inst* cpux)
{
    cpux->amode[cpux->OP](cpux);
    MEM_WRITE ((cpux->A & cpux->X), cpux->P0);
}


// SBC - Subtract Memory from Accumulator w/ Borrow
//  [ S V B D I Z C ]
//  [ / / . . . / / ]
//  (BCD Support not implemented)
static void
sbc (cpu_inst* cpux)
{
    byte S;
    unsigned int sum;

    // Compute operand base address
    cpux->amode[cpux->OP](cpux);

    // Fetch memory data
    cpux->D0 = MEM_READ (cpux->P0);

    // Save the Carry Flag
    S = cpux->S & FLAG_CARRY;

    // Perform Subtraction with Borrow
    sum = cpux->A - cpux->D0 - (1-S);

    HANDLE_SIGN_FLAG ((byte)(sum & 0xFF));
    HANDLE_ZERO_FLAG ((byte)(sum & 0xFF));

    // Did the Accumulator's MSB toggle?
    if ( ((cpux->A ^ cpux->D0) & BIT7) && ((cpux->A ^ sum) & BIT7)) {
        SET_FLAG (FLAG_OVR);
    } else {
        UNSET_FLAG (FLAG_OVR);
    }

    // Handle Carry Flag
    if (sum <= 0xFF) {
        SET_FLAG (FLAG_CARRY);
    } else {
        UNSET_FLAG (FLAG_CARRY);
    }

    cpux->A = (byte)(sum & 0xFF);
}


// SEC - Set Carry Flag
//  [ S V B D I Z C ]
//  [ . . . . . . 1 ]
static void
sec (cpu_inst* cpux)
{
    cpux->amode[cpux->OP](cpux);
    SET_FLAG (FLAG_CARRY);
}


// SED - Set BCD Mode
//  [ S V B D I Z C ]
//  [ . . . 1 . . . ]
static void
sed (cpu_inst* cpux)
{
    cpux->amode[cpux->OP](cpux);
    SET_FLAG (FLAG_BCD);
}


// SEI - Set Interrupt Disable Status
//  [ S V B D I Z C ]
//  [ . . . . 1 . . ]
static void
sei (cpu_inst* cpux)
{
    cpux->amode[cpux->OP](cpux);
    SET_FLAG (FLAG_IRQE);
}


// SLO - Shift Memory Left 1-bit, then OR with Accumulator
//  [ S V B D I Z C ]
//  [ / . . . . / / ]
static void
slo (cpu_inst* cpux)
{
    // Compute operand base address
    cpux->amode[cpux->OP](cpux);

    // Grab specified data
    cpux->D0 = MEM_READ(cpux->P0);

    // Dummy write back to keep cycle timing
    MEM_WRITE (cpux->D0, cpux->P0);

    // Set C Flag if LSB is 1
    HANDLE_MSB_CARRY_FLAG (cpux->D0);

    // Shift Value Right
    cpux->D0 = cpux->D0 << 1;

    // Save Back to Memory
    MEM_WRITE (cpux->D0, cpux->P0);

    cpux->A |= cpux->D0;

    HANDLE_ZERO_FLAG (cpux->A);
    HANDLE_SIGN_FLAG (cpux->A);

}


// SRE - Shift Memory Right 1-bit, then XOR Mem with Accumulator
//  [ S V B D I Z C ]
//  [ 0 . . . . / / ]
static void
sre (cpu_inst* cpux)
{
    // Compute operand base address
    cpux->amode[cpux->OP](cpux);

    // Grab specified data
    cpux->D0 = MEM_READ(cpux->P0);

    // Dummy write back to keep cycle timing
    MEM_WRITE (cpux->D0, cpux->P0);

    // Set C Flag if LSB is 1
    HANDLE_LSB_CARRY_FLAG (cpux->D0);

    // Shift Value Right
    cpux->D0 = cpux->D0 >> 1;

    // Save Back to Memory
    MEM_WRITE (cpux->D0, cpux->P0);

    cpux->A ^= cpux->D0;

    HANDLE_ZERO_FLAG (cpux->A);
    HANDLE_SIGN_FLAG (cpux->A)
}


// STA - Store Accumulator in Memory
//  [ S V B D I Z C ]
//  [ . . . . . . . ]
static void
sta (cpu_inst* cpux)
{
    cpux->amode[cpux->OP](cpux);
    MEM_WRITE (cpux->A, cpux->P0);
}


// STX - Store X Index in Memory
//  [ S V B D I Z C ]
//  [ . . . . . . . ]
static void
stx (cpu_inst* cpux)
{
    cpux->amode[cpux->OP](cpux);
    MEM_WRITE (cpux->X, cpux->P0);
}


// STY - Store Y Index in Memory
//  [ S V B D I Z C ]
//  [ . . . . . . . ]
static void
sty (cpu_inst* cpux)
{
    cpux->amode[cpux->OP](cpux);
    MEM_WRITE (cpux->Y, cpux->P0);
}


// STZ - UNDOCUMENTED
//  [ S V B D I Z C ]
//  [ ? ? ? ? ? ? ? ]
static void
stz (cpu_inst* cpux)
{
    // Compute operand base address
    cpux->amode[cpux->OP](cpux);
}


// TAX - Transfer Accumulator to X Index
//  [ S V B D I Z C ]
//  [ / . . . . / . ]
static void
tax (cpu_inst* cpux)
{
    cpux->amode[cpux->OP](cpux);
    cpux->X = cpux->A;

    HANDLE_SIGN_FLAG (cpux->X);
    HANDLE_ZERO_FLAG (cpux->X);
}


// TAY - Transfer Accumulator to Y Index
//  [ S V B D I Z C ]
//  [ / . . . . / . ]
static void
tay (cpu_inst* cpux)
{
    cpux->amode[cpux->OP](cpux);
    cpux->Y = cpux->A;

    HANDLE_SIGN_FLAG (cpux->Y);
    HANDLE_ZERO_FLAG (cpux->Y);
}


// TRB - UNDOCUMENTED
//  [ S V B D I Z C ]
//  [ ? ? ? ? ? ? ? ]
static void
trb (cpu_inst* cpux)
{
    printf ("trb\n");

    // Compute operand base address
    cpux->amode[cpux->OP](cpux);
}


// TSB - UNDOCUMENTED
//  [ S V B D I Z C ]
//  [ ? ? ? ? ? ? ? ]
static void
tsb (cpu_inst* cpux)
{
    printf ("tsb\n");

    // Compute operand base address
    cpux->amode[cpux->OP](cpux);
}


// TSX - Transfer Stack Pointer to X Index
//  [ S V B D I Z C ]
//  [ / . . . . / . ]
static void
tsx (cpu_inst* cpux)
{
    cpux->amode[cpux->OP](cpux);
    cpux->X = cpux->SP;

    HANDLE_SIGN_FLAG (cpux->X);
    HANDLE_ZERO_FLAG (cpux->X);
}


// TXA - Transfer X Index to Accumulator
//  [ S V B D I Z C ]
//  [ / . . . . / . ]
static void
txa (cpu_inst* cpux)
{
    cpux->amode[cpux->OP](cpux);
    cpux->A = cpux->X;

    HANDLE_SIGN_FLAG (cpux->A);
    HANDLE_ZERO_FLAG (cpux->A);
}


// TXS - Transfer X Index to Stack Pointer
//  [ S V B D I Z C ]
//  [ . . . . . . . ]
static void
txs (cpu_inst* cpux)
{
    cpux->amode[cpux->OP](cpux);
    cpux->SP = cpux->X;
}


// TYA - Transfer Y Index to Accumulator
//  [ S V B D I Z C ]
//  [ / . . . . / . ]
static void
tya (cpu_inst* cpux)
{
    cpux->amode[cpux->OP](cpux);
    cpux->A = cpux->Y;

    HANDLE_SIGN_FLAG (cpux->A);
    HANDLE_ZERO_FLAG (cpux->A);
}


// Tripple NOP
static void
top (cpu_inst* cpux)
{
    cpux->amode[cpux->OP](cpux);
    // Valid addressing mode are:
    //    Absolute      (3  cycles)
    //    Absolute, X   (3* cycles)

    // The Tripple NOP is 4* cycles
    // overall, so we need to do
    // a dummy read here to keep the
    // cycle count correct.
    MEM_READ (cpux->P0);
}

// Double NOP
static void
dop (cpu_inst* cpux)
{
    cpux->amode[cpux->OP](cpux);
    MEM_READ (cpux->P0);
}

static void
nmi (cpu_inst* cpux)
{
    // Save the PC to the Stack MSB first
    STACK_PUSH ((byte)(cpux->PC >> 8));
    STACK_PUSH ((byte)(cpux->PC & 0x00FF));

    // Save status register
    STACK_PUSH (cpux->S);

    // Grab jump address from NMI vector @ 0xFFFE
    cpux->PC = MEM_READ (0xFFFA)
             + (MEM_READ (0xFFFB) << 8);

}

/********************************************************************
 * E N G I N E     I N T E R F A C E S                              *
 ********************************************************************/

// Initializes the 6502 CPU engine look up tables
cpu_luts*
init_6502_engine ()
{
    /* Just used for shorthand */
    void (**opcode)(cpu_inst* cpux);
    void (**amode)(cpu_inst* cpux);
    int *cycles;

    /* This will serve a similar role as a "base class" */
    cpu_luts* luts = (cpu_luts*) malloc (sizeof(cpu_luts));

    /* Allocate look up tables */
    luts->opcode = malloc (sizeof(void(*)(cpu_inst* cpux)) * 256);
    luts->amode  = malloc (sizeof(void(*)(cpu_inst* cpux)) * 256);
    luts->cycles = (int*) malloc (sizeof(int) * 256);

    opcode = luts->opcode;
    amode  = luts->amode;
    cycles = luts->cycles;


    /* Initialize look up tables */
    opcode[0x00] = brk;  amode[0x00] = implied;     cycles[0x00] = 7;   // Cycle Correct
    opcode[0x01] = ora;  amode[0x01] = indirect_x;  cycles[0x01] = 6;   // Cycle Correct
    opcode[0x02] = nop;  amode[0x02] = implied;     cycles[0x02] = 0;   // (Should KILL CPU)
    opcode[0x03] = slo;  amode[0x03] = indirect_x;  cycles[0x03] = 8;   // Cycle Correct
    opcode[0x04] = dop;  amode[0x04] = zeropage;    cycles[0x04] = 3;   // Cycle Correct
    opcode[0x05] = ora;  amode[0x05] = zeropage;    cycles[0x05] = 3;   // Cycle Correct
    opcode[0x06] = asl;  amode[0x06] = zeropage;    cycles[0x06] = 5;   // Cycle Correct
    opcode[0x07] = slo;  amode[0x07] = zeropage;    cycles[0x07] = 5;   // Cycle Correct
    opcode[0x08] = php;  amode[0x08] = implied;     cycles[0x08] = 3;   // Cycle Correct
    opcode[0x09] = ora;  amode[0x09] = immediate;   cycles[0x09] = 2;   // Cycle Correct
    opcode[0x0A] = asla; amode[0x0A] = implied;     cycles[0x0A] = 2;   // Cycle Correct
    opcode[0x0B] = aac;  amode[0x0B] = immediate;   cycles[0x0B] = 2;   // Cycle Correct
    opcode[0x0C] = top;  amode[0x0C] = absolute;    cycles[0x0C] = 4;   // Cycle Correct
    opcode[0x0D] = ora;  amode[0x0D] = absolute;    cycles[0x0D] = 4;   // Cycle Correct
    opcode[0x0E] = asl;  amode[0x0E] = absolute;    cycles[0x0E] = 6;   // Cycle Correct
    opcode[0x0F] = slo;  amode[0x0F] = absolute;    cycles[0x0F] = 6;   // Cycle Correct

    opcode[0x10] = bpl;  amode[0x10] = relative;    cycles[0x10] = 2;   // Cycle Correct
    opcode[0x11] = ora;  amode[0x11] = ind_y_read;  cycles[0x11] = 5;   // Cycle Correct
    opcode[0x12] = nop;  amode[0x12] = implied;     cycles[0x12] = 0;   // (Should KILL CPU)
    opcode[0x13] = slo;  amode[0x13] = indirect_y;  cycles[0x13] = 8;   // Cycle Correct
    opcode[0x14] = dop;  amode[0x14] = zeropage_x;  cycles[0x14] = 4;   // Cycle Correct
    opcode[0x15] = ora;  amode[0x15] = zeropage_x;  cycles[0x15] = 4;   // Cycle Correct
    opcode[0x16] = asl;  amode[0x16] = zeropage_x;  cycles[0x16] = 6;   // Cycle Correct
    opcode[0x17] = slo;  amode[0x17] = zeropage_x;  cycles[0x17] = 6;   // Cycle Correct
    opcode[0x18] = clc;  amode[0x18] = implied;     cycles[0x18] = 2;   // Cycle Correct
    opcode[0x19] = ora;  amode[0x19] = absolute_y;  cycles[0x19] = 4;   // Cycle Correct
    opcode[0x1A] = nop;  amode[0x1A] = implied;     cycles[0x1A] = 2;   // Cycle Correct
    opcode[0x1B] = slo;  amode[0x1B] = absolute_y;  cycles[0x1B] = 2;   // Cycle Correct
    opcode[0x1C] = top;  amode[0x1C] = absolute_x;  cycles[0x1C] = 4;   // Cycle Correct
    opcode[0x1D] = ora;  amode[0x1D] = abs_x_read;  cycles[0x1D] = 4;   // Cycle Correct
    opcode[0x1E] = asl;  amode[0x1E] = absolute_x;  cycles[0x1E] = 7;   // Cycle Correct
    opcode[0x1F] = slo;  amode[0x1F] = absolute_x;  cycles[0x1F] = 7;   // Cycle Correct

    opcode[0x20] = jsr;  amode[0x20] = absolute;    cycles[0x20] = 6;   // Cycle Correct (out of order)
    opcode[0x21] = and;  amode[0x21] = indirect_x;  cycles[0x21] = 6;   // Cycle Correct
    opcode[0x22] = nop;  amode[0x22] = implied;     cycles[0x22] = 0;   // (Should KILL CPU)
    opcode[0x23] = rla;  amode[0x23] = indirect_x;  cycles[0x23] = 8;   // Cycle Correct
    opcode[0x24] = bit;  amode[0x24] = zeropage;    cycles[0x24] = 3;   // Cycle Correct
    opcode[0x25] = and;  amode[0x25] = zeropage;    cycles[0x25] = 3;   // Cycle Correct
    opcode[0x26] = rol;  amode[0x26] = zeropage;    cycles[0x26] = 5;   // Cycle Correct
    opcode[0x27] = rla;  amode[0x27] = zeropage;    cycles[0x27] = 5;   // Cycle Correct
    opcode[0x28] = plp;  amode[0x28] = implied;     cycles[0x28] = 4;   // Cycle Correct
    opcode[0x29] = and;  amode[0x29] = immediate;   cycles[0x29] = 2;   // Cycle Correct
    opcode[0x2A] = rola; amode[0x2A] = implied;     cycles[0x2A] = 2;   // Cycle Correct
    opcode[0x2B] = aac;  amode[0x2B] = immediate;   cycles[0x2B] = 2;   // Cycle Correct
    opcode[0x2C] = bit;  amode[0x2C] = absolute;    cycles[0x2C] = 4;   // Cycle Correct
    opcode[0x2D] = and;  amode[0x2D] = absolute;    cycles[0x2D] = 4;   // Cycle Correct
    opcode[0x2E] = rol;  amode[0x2E] = absolute;    cycles[0x2E] = 6;   // Cycle Correct
    opcode[0x2F] = rla;  amode[0x2F] = absolute;    cycles[0x2F] = 6;   // Cycle Correct

    opcode[0x30] = bmi;  amode[0x30] = relative;    cycles[0x30] = 2;   // Cycle Correct
    opcode[0x31] = and;  amode[0x31] = ind_y_read;  cycles[0x31] = 5;   // Cycle Correct
    opcode[0x32] = nop;  amode[0x32] = implied;     cycles[0x32] = 0;   // (Should KILL CPU)
    opcode[0x33] = rla;  amode[0x33] = indirect_y;  cycles[0x33] = 8;   // Cycle Correct
    opcode[0x34] = dop;  amode[0x34] = zeropage_x;  cycles[0x34] = 4;   // Cycle Correct
    opcode[0x35] = and;  amode[0x35] = zeropage_x;  cycles[0x35] = 4;   // Cycle Correct
    opcode[0x36] = rol;  amode[0x36] = zeropage_x;  cycles[0x36] = 6;   // Cycle Correct
    opcode[0x37] = rla;  amode[0x37] = zeropage_x;  cycles[0x37] = 6;   // Cycle Correct
    opcode[0x38] = sec;  amode[0x38] = implied;     cycles[0x38] = 2;   // Cycle Correct
    opcode[0x39] = and;  amode[0x39] = abs_y_read;  cycles[0x39] = 4;   // Cycle Correct
    opcode[0x3A] = nop;  amode[0x3A] = implied;     cycles[0x3A] = 2;   // Cycle Correct
    opcode[0x3B] = rla;  amode[0x3B] = absolute_y;  cycles[0x3B] = 7;   // Cycle Correct
    opcode[0x3C] = top;  amode[0x3C] = absolute_x;  cycles[0x3C] = 4;   // Cycle Correct
    opcode[0x3D] = and;  amode[0x3D] = abs_x_read;  cycles[0x3D] = 4;   // Cycle Correct
    opcode[0x3E] = rol;  amode[0x3E] = absolute_x;  cycles[0x3E] = 7;   // Cycle Correct
    opcode[0x3F] = rla;  amode[0x3F] = absolute_x;  cycles[0x3F] = 7;   // Cycle Correct

    opcode[0x40] = rti;  amode[0x40] = implied;     cycles[0x40] = 6;   // Cycle Correct
    opcode[0x41] = eor;  amode[0x41] = indirect_x;  cycles[0x41] = 6;   // Cycle Correct
    opcode[0x42] = nop;  amode[0x42] = implied;     cycles[0x42] = 0;   // (Should KILL CPU)
    opcode[0x43] = sre;  amode[0x43] = indirect_x;  cycles[0x43] = 8;   // Cycle Correct
    opcode[0x44] = dop;  amode[0x44] = zeropage;    cycles[0x44] = 3;   // Cycle Correct
    opcode[0x45] = eor;  amode[0x45] = zeropage;    cycles[0x45] = 3;   // Cycle Correct
    opcode[0x46] = lsr;  amode[0x46] = zeropage;    cycles[0x46] = 5;   // Cycle Correct
    opcode[0x47] = sre;  amode[0x47] = zeropage;    cycles[0x47] = 5;   // Cycle Correct
    opcode[0x48] = pha;  amode[0x48] = implied;     cycles[0x48] = 3;   // Cycle Correct
    opcode[0x49] = eor;  amode[0x49] = immediate;   cycles[0x49] = 2;   // Cycle Correct
    opcode[0x4A] = lsra; amode[0x4A] = implied;     cycles[0x4A] = 2;   // Cycle Correct
    opcode[0x4B] = asr;  amode[0x4B] = immediate;   cycles[0x4B] = 2;   // Cycle Correct
    opcode[0x4C] = jmp;  amode[0x4C] = absolute;    cycles[0x4C] = 3;   // Cycle Correct
    opcode[0x4D] = eor;  amode[0x4D] = absolute;    cycles[0x4D] = 4;   // Cycle Correct
    opcode[0x4E] = lsr;  amode[0x4E] = absolute;    cycles[0x4E] = 6;   // Cycle Correct
    opcode[0x4F] = sre;  amode[0x4F] = absolute;    cycles[0x4F] = 6;   // Cycle Correct

    opcode[0x50] = bvc;  amode[0x50] = relative;    cycles[0x50] = 2;   // Cycle Correct
    opcode[0x51] = eor;  amode[0x51] = ind_y_read;  cycles[0x51] = 5;   // Cycle Correct
    opcode[0x52] = nop;  amode[0x52] = implied;     cycles[0x52] = 0;   // (Should KILL CPU)
    opcode[0x53] = sre;  amode[0x53] = indirect_y;  cycles[0x53] = 8;   // Cycle Correct
    opcode[0x54] = dop;  amode[0x54] = zeropage_x;  cycles[0x54] = 4;   // Cycle Correct
    opcode[0x55] = eor;  amode[0x55] = zeropage_x;  cycles[0x55] = 4;   // Cycle Correct
    opcode[0x56] = lsr;  amode[0x56] = zeropage_x;  cycles[0x56] = 6;   // Cycle Correct
    opcode[0x57] = sre;  amode[0x57] = zeropage_x;  cycles[0x57] = 6;   // Cycle Correct
    opcode[0x58] = cli;  amode[0x58] = implied;     cycles[0x58] = 2;   // Cycle Correct
    opcode[0x59] = eor;  amode[0x59] = abs_y_read;  cycles[0x59] = 4;   // Cycle Correct
    opcode[0x5A] = nop;  amode[0x5A] = implied;     cycles[0x5A] = 2;   // Cycle Correct
    opcode[0x5B] = sre;  amode[0x5B] = absolute_y;  cycles[0x5B] = 7;   // Cycle Correct
    opcode[0x5C] = top;  amode[0x5C] = abs_x_read;  cycles[0x5C] = 4;   // Cycle Correct
    opcode[0x5D] = eor;  amode[0x5D] = abs_x_read;  cycles[0x5D] = 4;   // Cycle Correct
    opcode[0x5E] = lsr;  amode[0x5E] = absolute_x;  cycles[0x5E] = 7;   // Cycle Correct
    opcode[0x5F] = sre;  amode[0x5F] = absolute_x;  cycles[0x5F] = 7;   // Cycle Correct

    opcode[0x60] = rts;  amode[0x60] = implied;     cycles[0x60] = 6;   // Cycle Correct
    opcode[0x61] = adc;  amode[0x61] = indirect_x;  cycles[0x61] = 6;   // Cycle Correct
    opcode[0x62] = nop;  amode[0x62] = implied;     cycles[0x62] = 2;   // (Should KILL CPU)
    opcode[0x63] = rra;  amode[0x63] = indirect_x;  cycles[0x63] = 8;   // Cycle Correct
    opcode[0x64] = dop;  amode[0x64] = zeropage;    cycles[0x64] = 3;   // Cycle Correct
    opcode[0x65] = adc;  amode[0x65] = zeropage;    cycles[0x65] = 3;   // Cycle Correct
    opcode[0x66] = ror;  amode[0x66] = zeropage;    cycles[0x66] = 5;   // Cycle Correct
    opcode[0x67] = rra;  amode[0x67] = zeropage;    cycles[0x67] = 5;   // Cycle Correct
    opcode[0x68] = pla;  amode[0x68] = implied;     cycles[0x68] = 4;   // Cycle Correct
    opcode[0x69] = adc;  amode[0x69] = immediate;   cycles[0x69] = 2;   // Cycle Correct
    opcode[0x6A] = rora; amode[0x6A] = implied;     cycles[0x6A] = 2;   // Cycle Correct
    opcode[0x6B] = arr;  amode[0x6B] = immediate;   cycles[0x6B] = 2;   // Cycle Correct
    opcode[0x6C] = jmp;  amode[0x6C] = indirect;    cycles[0x6C] = 5;   // Cycle Correct
    opcode[0x6D] = adc;  amode[0x6D] = absolute;    cycles[0x6D] = 4;   // Cycle Correct
    opcode[0x6E] = ror;  amode[0x6E] = absolute;    cycles[0x6E] = 6;   // Cycle Correct
    opcode[0x6F] = rra;  amode[0x6F] = absolute;    cycles[0x6F] = 6;   // Cycle Correct

    opcode[0x70] = bvs;  amode[0x70] = relative;    cycles[0x70] = 2;   // Cycle Correct
    opcode[0x71] = adc;  amode[0x71] = ind_y_read;  cycles[0x71] = 5;   // Cycle Correct
    opcode[0x72] = nop;  amode[0x72] = implied;     cycles[0x72] = 0;   // (Should KILL CPU)
    opcode[0x73] = rra;  amode[0x73] = indirect_y;  cycles[0x73] = 8;   // Cycle Correct
    opcode[0x74] = dop;  amode[0x74] = zeropage_x;  cycles[0x74] = 4;   // Cycle Correct
    opcode[0x75] = adc;  amode[0x75] = zeropage_x;  cycles[0x75] = 4;   // Cycle Correct
    opcode[0x76] = ror;  amode[0x76] = zeropage_x;  cycles[0x76] = 6;   // Cycle Correct
    opcode[0x77] = rra;  amode[0x77] = zeropage_x;  cycles[0x77] = 6;   // Cycle Correct
    opcode[0x78] = sei;  amode[0x78] = implied;     cycles[0x78] = 2;   // Cycle Correct
    opcode[0x79] = adc;  amode[0x79] = abs_y_read;  cycles[0x79] = 4;   // Cycle Correct
    opcode[0x7A] = nop;  amode[0x7A] = implied;     cycles[0x7A] = 2;   // Cycle Correct
    opcode[0x7B] = rra;  amode[0x7B] = absolute_y;  cycles[0x7B] = 7;   // Cycle Correct
    opcode[0x7C] = top;  amode[0x7C] = absolute_x;  cycles[0x7C] = 4;   // Cycle Correct
    opcode[0x7D] = adc;  amode[0x7D] = abs_x_read;  cycles[0x7D] = 4;   // Cycle Correct
    opcode[0x7E] = ror;  amode[0x7E] = absolute_x;  cycles[0x7E] = 7;   // Cycle Correct
    opcode[0x7F] = rra;  amode[0x7F] = absolute_x;  cycles[0x7F] = 7;   // Cycle Correct

    opcode[0x80] = dop;  amode[0x80] = immediate;   cycles[0x80] = 2;   // Cycle Correct
    opcode[0x81] = sta;  amode[0x81] = indirect_x;  cycles[0x81] = 6;   // Cycle Correct
    opcode[0x82] = dop;  amode[0x82] = immediate;   cycles[0x82] = 2;   // Cycle Correct
    opcode[0x83] = sax;  amode[0x83] = indirect_x;  cycles[0x83] = 6;   // Cycle Correct
    opcode[0x84] = sty;  amode[0x84] = zeropage;    cycles[0x84] = 3;   // Cycle Correct
    opcode[0x85] = sta;  amode[0x85] = zeropage;    cycles[0x85] = 3;   // Cycle Correct
    opcode[0x86] = stx;  amode[0x86] = zeropage;    cycles[0x86] = 3;   // Cycle Correct
    opcode[0x87] = sax;  amode[0x87] = zeropage;    cycles[0x87] = 3;   // Cycle Correct
    opcode[0x88] = dey;  amode[0x88] = implied;     cycles[0x88] = 2;   // Cycle Correct
    opcode[0x89] = dop;  amode[0x89] = immediate;   cycles[0x89] = 2;   // Cycle Correct
    opcode[0x8A] = txa;  amode[0x8A] = implied;     cycles[0x8A] = 2;   // Cycle Correct
    opcode[0x8B] = dop;  amode[0x8B] = immediate;   cycles[0x8B] = 2;   // Cycle Correct
    opcode[0x8C] = sty;  amode[0x8C] = absolute;    cycles[0x8C] = 4;   // Cycle Correct
    opcode[0x8D] = sta;  amode[0x8D] = absolute;    cycles[0x8D] = 4;   // Cycle Correct
    opcode[0x8E] = stx;  amode[0x8E] = absolute;    cycles[0x8E] = 4;   // Cycle Correct
    opcode[0x8F] = sax;  amode[0x8F] = absolute;    cycles[0x8F] = 4;   // Cycle Correct

    opcode[0x90] = bcc;  amode[0x90] = relative;    cycles[0x90] = 2;   // Cycle Correct
    opcode[0x91] = sta;  amode[0x91] = indirect_y;  cycles[0x91] = 6;   // Cycle Correct
    opcode[0x92] = nop;  amode[0x92] = implied;     cycles[0x92] = 0;   // (Should KILL CPU)
    opcode[0x93] = dop;  amode[0x93] = indirect_y;  cycles[0x93] = 2;   // FIX THIS
    opcode[0x94] = sty;  amode[0x94] = zeropage_x;  cycles[0x94] = 4;   // Cycle Correct
    opcode[0x95] = sta;  amode[0x95] = zeropage_x;  cycles[0x95] = 4;   // Cycle Correct
    opcode[0x96] = stx;  amode[0x96] = zeropage_y;  cycles[0x96] = 4;   // Cycle Correct
    opcode[0x97] = sax;  amode[0x97] = zeropage_y;  cycles[0x97] = 4;   // Cycle Correct
    opcode[0x98] = tya;  amode[0x98] = implied;     cycles[0x98] = 2;   // Cycle Correct
    opcode[0x99] = sta;  amode[0x99] = absolute_y;  cycles[0x99] = 5;   // Cycle Correct
    opcode[0x9A] = txs;  amode[0x9A] = implied;     cycles[0x9A] = 2;   // Cycle Correct
    opcode[0x9B] = dop;  amode[0x9B] = absolute_y;  cycles[0x9B] = 2;   // FIX THIS
    opcode[0x9C] = stz;  amode[0x9C] = absolute_x;  cycles[0x9C] = 5;   // Incorrect illegal (SYA)
    opcode[0x9D] = sta;  amode[0x9D] = absolute_x;  cycles[0x9D] = 5;   // Cycle Correct
    opcode[0x9E] = stz;  amode[0x9E] = absolute_x;  cycles[0x9E] = 5;   // Incorrect illegal (SXA)
    opcode[0x9F] = dop;  amode[0x9F] = absolute_y;  cycles[0x9F] = 2;   // FIX THIS

    opcode[0xA0] = ldy;  amode[0xA0] = immediate;   cycles[0xA0] = 2;   // Cycle Correct
    opcode[0xA1] = lda;  amode[0xA1] = indirect_x;  cycles[0xA1] = 6;   // Cycle Correct
    opcode[0xA2] = ldx;  amode[0xA2] = immediate;   cycles[0xA2] = 2;   // Cycle Correct
    opcode[0xA3] = lax;  amode[0xA3] = indirect_x;  cycles[0xA3] = 6;   // Cycle Correct
    opcode[0xA4] = ldy;  amode[0xA4] = zeropage;    cycles[0xA4] = 3;   // Cycle Correct
    opcode[0xA5] = lda;  amode[0xA5] = zeropage;    cycles[0xA5] = 3;   // Cycle Correct
    opcode[0xA6] = ldx;  amode[0xA6] = zeropage;    cycles[0xA6] = 3;   // Cycle Correct
    opcode[0xA7] = lax;  amode[0xA7] = zeropage;    cycles[0xA7] = 3;   // Cycle Correct
    opcode[0xA8] = tay;  amode[0xA8] = implied;     cycles[0xA8] = 2;   // Cycle Correct
    opcode[0xA9] = lda;  amode[0xA9] = immediate;   cycles[0xA9] = 2;   // Cycle Correct
    opcode[0xAA] = tax;  amode[0xAA] = implied;     cycles[0xAA] = 2;   // Cycle Correct
    opcode[0xAB] = atx;  amode[0xAB] = immediate;   cycles[0xAB] = 2;   // Cycle Correct
    opcode[0xAC] = ldy;  amode[0xAC] = absolute;    cycles[0xAC] = 4;   // Cycle Correct
    opcode[0xAD] = lda;  amode[0xAD] = absolute;    cycles[0xAD] = 4;   // Cycle Correct
    opcode[0xAE] = ldx;  amode[0xAE] = absolute;    cycles[0xAE] = 4;   // Cycle Correct
    opcode[0xAF] = lax;  amode[0xAF] = absolute;    cycles[0xAF] = 4;   // Cycle Correct

    opcode[0xB0] = bcs;  amode[0xB0] = relative;    cycles[0xB0] = 2;   // Cycle Correct
    opcode[0xB1] = lda;  amode[0xB1] = ind_y_read;  cycles[0xB1] = 5;   // Cycle Correct
    opcode[0xB2] = nop;  amode[0xB2] = implied;   ; cycles[0xB2] = 0;   // (Shuold KILL CPU)
    opcode[0xB3] = lax;  amode[0xB3] = ind_y_read;  cycles[0xB3] = 2;   // Cycle Correct
    opcode[0xB4] = ldy;  amode[0xB4] = zeropage_x;  cycles[0xB4] = 4;   // Cycle Correct
    opcode[0xB5] = lda;  amode[0xB5] = zeropage_x;  cycles[0xB5] = 4;   // Cycle Correct
    opcode[0xB6] = ldx;  amode[0xB6] = zeropage_y;  cycles[0xB6] = 4;   // Cycle Correct
    opcode[0xB7] = lax;  amode[0xB7] = zeropage_y;  cycles[0xB7] = 4;   // Cycle Correct
    opcode[0xB8] = clv;  amode[0xB8] = implied;     cycles[0xB8] = 2;   // Cycle Correct
    opcode[0xB9] = lda;  amode[0xB9] = abs_y_read;  cycles[0xB9] = 4;   // Cycle Correct
    opcode[0xBA] = tsx;  amode[0xBA] = implied;     cycles[0xBA] = 2;   // Cycle Correct
    opcode[0xBB] = dop;  amode[0xBB] = absolute_y;  cycles[0xBB] = 2;   // FIX THIS
    opcode[0xBC] = ldy;  amode[0xBC] = abs_x_read;  cycles[0xBC] = 4;   // Cycle Correct
    opcode[0xBD] = lda;  amode[0xBD] = abs_x_read;  cycles[0xBD] = 4;   // Cycle Correct
    opcode[0xBE] = ldx;  amode[0xBE] = abs_y_read;  cycles[0xBE] = 4;   // Cycle Correct
    opcode[0xBF] = lax;  amode[0xBF] = abs_y_read;  cycles[0xBF] = 4;   // Cycle Correct

    opcode[0xC0] = cpy;  amode[0xC0] = immediate;   cycles[0xC0] = 2;   // Cycle Correct
    opcode[0xC1] = cmp;  amode[0xC1] = indirect_x;  cycles[0xC1] = 6;   // Cycle Correct
    opcode[0xC2] = dop;  amode[0xC2] = immediate;   cycles[0xC2] = 2;   // Cycle Correct
    opcode[0xC3] = dcp;  amode[0xC3] = indirect_x;  cycles[0xC3] = 8;   // Cycle Correct
    opcode[0xC4] = cpy;  amode[0xC4] = zeropage;    cycles[0xC4] = 3;   // Cycle Correct
    opcode[0xC5] = cmp;  amode[0xC5] = zeropage;    cycles[0xC5] = 3;   // Cycle Correct
    opcode[0xC6] = dec;  amode[0xC6] = zeropage;    cycles[0xC6] = 5;   // Cycle Correct
    opcode[0xC7] = dcp;  amode[0xC7] = zeropage;    cycles[0xC7] = 5;   // Cycle Correct
    opcode[0xC8] = iny;  amode[0xC8] = implied;     cycles[0xC8] = 2;   // Cycle Correct
    opcode[0xC9] = cmp;  amode[0xC9] = immediate;   cycles[0xC9] = 2;   // Cycle Correct
    opcode[0xCA] = dex;  amode[0xCA] = implied;     cycles[0xCA] = 2;   // Cycle Correct
    opcode[0xCB] = nop;  amode[0xCB] = immediate;   cycles[0xCB] = 2;   // Cycle Correct
    opcode[0xCC] = cpy;  amode[0xCC] = absolute;    cycles[0xCC] = 4;   // Cycle Correct
    opcode[0xCD] = cmp;  amode[0xCD] = absolute;    cycles[0xCD] = 4;   // Cycle Correct
    opcode[0xCE] = dec;  amode[0xCE] = absolute;    cycles[0xCE] = 6;   // Cycle Correct
    opcode[0xCF] = dcp;  amode[0xCF] = absolute;    cycles[0xCF] = 6;   // Cycle Correct

    opcode[0xD0] = bne;  amode[0xD0] = relative;    cycles[0xD0] = 2;   // Cycle Correct
    opcode[0xD1] = cmp;  amode[0xD1] = ind_y_read;  cycles[0xD1] = 5;   // Cycle Correct
    opcode[0xD2] = nop;  amode[0xD2] = implied;     cycles[0xD2] = 0;   // (Should KILL CPU)
    opcode[0xD3] = dcp;  amode[0xD3] = indirect_y;  cycles[0xD3] = 8;   // Cycle Correct
    opcode[0xD4] = dop;  amode[0xD4] = zeropage_x;  cycles[0xD4] = 4;   // Cycle Correct
    opcode[0xD5] = cmp;  amode[0xD5] = zeropage_x;  cycles[0xD5] = 4;   // Cycle Correct
    opcode[0xD6] = dec;  amode[0xD6] = zeropage_x;  cycles[0xD6] = 6;   // Cycle Correct
    opcode[0xD7] = dcp;  amode[0xD7] = zeropage_x;  cycles[0xD7] = 6;   // Cycle Correct
    opcode[0xD8] = cld;  amode[0xD8] = implied;     cycles[0xD8] = 2;   // Cycle Correct
    opcode[0xD9] = cmp;  amode[0xD9] = abs_y_read;  cycles[0xD9] = 4;   // Cycle Correct
    opcode[0xDA] = nop;  amode[0xDA] = implied;     cycles[0xDA] = 2;   // Cycle Correct
    opcode[0xDB] = dcp;  amode[0xDB] = absolute_y;  cycles[0xDB] = 7;   // Cycle Correct
    opcode[0xDC] = top;  amode[0xDC] = absolute_x;  cycles[0xDC] = 4;   // Cycle Correct
    opcode[0xDD] = cmp;  amode[0xDD] = abs_x_read;  cycles[0xDD] = 4;   // Cycle Correct
    opcode[0xDE] = dec;  amode[0xDE] = absolute_x;  cycles[0xDE] = 7;   // Cycle Correct
    opcode[0xDF] = dcp;  amode[0xDF] = absolute_x;  cycles[0xDF] = 7;   // Cycle Correct

    opcode[0xE0] = cpx;  amode[0xE0] = immediate;   cycles[0xE0] = 2;   // Cycle Correct
    opcode[0xE1] = sbc;  amode[0xE1] = indirect_x;  cycles[0xE1] = 6;   // Cycle Correct
    opcode[0xE2] = dop;  amode[0xE2] = immediate;   cycles[0xE2] = 2;   // Cycle Correct
    opcode[0xE3] = isb;  amode[0xE3] = indirect_x;  cycles[0xE3] = 8;   // Cycle Correct
    opcode[0xE4] = cpx;  amode[0xE4] = zeropage;    cycles[0xE4] = 3;   // Cycle Correct
    opcode[0xE5] = sbc;  amode[0xE5] = zeropage;    cycles[0xE5] = 3;   // Cycle Correct
    opcode[0xE6] = inc;  amode[0xE6] = zeropage;    cycles[0xE6] = 5;   // Cycle Correct
    opcode[0xE7] = isb;  amode[0xE7] = zeropage;    cycles[0xE7] = 5;   // Cycle Correct
    opcode[0xE8] = inx;  amode[0xE8] = implied;     cycles[0xE8] = 2;   // Cycle Correct
    opcode[0xE9] = sbc;  amode[0xE9] = immediate;   cycles[0xE9] = 2;   // Cycle Correct
    opcode[0xEA] = nop;  amode[0xEA] = implied;     cycles[0xEA] = 2;   // Cycle Correct
    opcode[0xEB] = sbc;  amode[0xEB] = immediate;   cycles[0xEB] = 2;   // Cycle Correct
    opcode[0xEC] = cpx;  amode[0xEC] = absolute;    cycles[0xEC] = 4;   // Cycle Correct
    opcode[0xED] = sbc;  amode[0xED] = absolute;    cycles[0xED] = 4;   // Cycle Correct
    opcode[0xEE] = inc;  amode[0xEE] = absolute;    cycles[0xEE] = 6;   // Cycle Correct
    opcode[0xEF] = isb;  amode[0xEF] = absolute;    cycles[0xEF] = 6;   // Cycle Correct

    opcode[0xF0] = beq;  amode[0xF0] = relative;    cycles[0xF0] = 2;   // Cycle Correct
    opcode[0xF1] = sbc;  amode[0xF1] = ind_y_read;  cycles[0xF1] = 5;   // Cycle Correct
    opcode[0xF2] = nop;  amode[0xF2] = implied;     cycles[0xF2] = 0;   // (Should KILL CPU)
    opcode[0xF3] = isb;  amode[0xF3] = indirect_y;  cycles[0xF3] = 8;   // Cycle Correct
    opcode[0xF4] = dop;  amode[0xF4] = zeropage_x;  cycles[0xF4] = 4;   // Cycle Correct
    opcode[0xF5] = sbc;  amode[0xF5] = zeropage_x;  cycles[0xF5] = 4;   // Cycle Correct
    opcode[0xF6] = inc;  amode[0xF6] = zeropage_x;  cycles[0xF6] = 6;   // Cycle Correct
    opcode[0xF7] = isb;  amode[0xF7] = zeropage_x;  cycles[0xF7] = 6;   // Cycle Correct
    opcode[0xF8] = sed;  amode[0xF8] = implied;     cycles[0xF8] = 2;   // Cycle Correct
    opcode[0xF9] = sbc;  amode[0xF9] = abs_y_read;  cycles[0xF9] = 4;   // Cycle Correct
    opcode[0xFA] = nop;  amode[0xFA] = implied;     cycles[0xFA] = 2;   // Cycle Correct
    opcode[0xFB] = isb;  amode[0xFB] = absolute_y;  cycles[0xFB] = 7;   // Cycle Correct
    opcode[0xFC] = top;  amode[0xFC] = absolute_x;  cycles[0xFC] = 4;   // Cycle Correct
    opcode[0xFD] = sbc;  amode[0xFD] = abs_x_read;  cycles[0xFD] = 4;   // Cycle Correct
    opcode[0xFE] = inc;  amode[0xFE] = absolute_x;  cycles[0xFE] = 7;   // Cycle Correct
    opcode[0xFF] = isb;  amode[0xFF] = absolute_x;  cycles[0xFF] = 7;   // Cycle Correct

    return luts;
}

void
unload_6502_engine (cpu_luts** luts)
{
    free ((*luts)->opcode);
    free ((*luts)->amode);
    free ((*luts)->cycles);
    free (*luts);
    *luts = 0;
}


// Allocates and initializes a 6502 CPU instance.
// The memory address of this new instance is returned.
cpu_inst*
make_cpu (cpu_luts* luts)
{
    /* Allocate a CPU instance for a 6502 CPU */
    cpu_inst* cpux = (cpu_inst*) malloc (sizeof(cpu_inst));

    /* Populate the registers with power-on values */
    cpux->PC = 0x0000;   // Program Counter
    cpux->SP = 0xFD;     // Stack Pointer
    cpux->A  = 0x00;     // Accumulator
    cpux->X  = 0x00;     // X Index Register
    cpux->Y  = 0x00;     // Y index Register
    cpux->S  = 0x24;     // Status Register

    cpux->OP = 0x00;     // Current Opcode

    cpux->P0 = 0x0000;   // Temp registers
    cpux->D0 = 0x00;     // (Used internally by opcodes)

    /* Reset extra cycles counter */
    cpux->xtra_cycles = 0;

    /* Caller responsible for allocating and setting */
    cpux->mmap = 0;

    /* Not a fan of this, but we attach the PPU to
     * the CPU.  This works out well in the code */
    cpux->ppux = 0;

    /* Attach look up tables to CPU instance */
    cpux->opcode = luts->opcode;
    cpux->amode  = luts->amode;
    cpux->cycles = luts->cycles;
    cpux->mapper = luts->mapper;

    /* Return the address of the allocated register file */
    return cpux;
}


void
destroy_cpu (cpu_inst** cpux)
{
    free (*cpux);
    *cpux = 0;
}

// Emulates a CPU reset
void
reset_cpu (cpu_inst* cpux)
{
    // Populate PC with value stored @ RESET vector in ROM
    cpux->PC = MEM_READ(0xFFFC) + (MEM_READ(0xFFFD) << 8);
}

// Runs the 6502 CPU for the specified number of cycles.
int
run_cpu (cpu_inst* cpux, int cycles)
{
    int save_cycles = cycles;
    ppu_inst* ppux = cpux->ppux;

    while (cycles > 0) {

        if (ppux->NMI) {
            nmi (cpux);
            ppux->NMI = 0;
        }

        // Fetch opcode from MEM & increment Program Counter
        cpux->OP = MEM_READ(cpux->PC++);

        // Execute opcode
        cpux->opcode[cpux->OP](cpux);

        // Update Memory Map
        cpux->mapper[cpux->mapper_id](cpux, 0);

        // Update CPU cycle limiter
        cycles -= cpux->cycles[cpux->OP]
                + cpux->xtra_cycles;

        // Reset xtra-cycles counter
        cpux->xtra_cycles ^= cpux->xtra_cycles;

        // Make sure BIT5 remains set
        SET_FLAG (FLAG_5);
    }

    return save_cycles - cycles;
}
