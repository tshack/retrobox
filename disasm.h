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
#ifndef _disasm_h_
#define _disasm_h_

#include "6502.h"
#include "6502_types.h"

// Populated when the disassembly engine is initialized.
typedef struct disasm_look_up_tables disasm_luts;
struct disasm_look_up_tables {

    /* Opcode LUT */
    void (**opcode)();

    /* Addressing mode LUT */
    void (**amode)();

    /* Clock cycle LUT */
    int *cycles;
};

// A disassembler instance.
typedef struct disasm_instance disasm_inst;
struct disasm_instance {

    /* External Registers */
    word PC;    /* Program Counter */
    byte SP;    /* Stack Pointer   */
    byte A;     /* Accumulator     */
    byte X;     /* X Index         */
    byte Y;     /* Y Index         */
    byte S;     /* Status          */

    byte OP;    /* Current Opcode  */

    /* Internal Registers */
    word P0;    /* Temp Register   */
    byte D0;    /* Temp Register   */

    /* Extra Cycles Counter */
    byte xtra_cycles;

    /* Memory images */
    byte** mmap;
    byte* ROM;
    byte* RAM;

    /* Look up tables */
    void (**opcode)(disasm_inst* dax);
    void (**amode)(disasm_inst* dax);
    int *cycles;
};


#if defined __cplusplus
extern "C" {
#endif

disasm_luts* init_disasm_engine ();
void unload_disasm_engine (disasm_luts** luts);

disasm_inst* make_disasm (disasm_luts* luts, cpu_inst* cpux);
void destroy_disasm (disasm_inst** dax);

void disasm (cpu_inst* cpux, disasm_inst* dax);


#if defined __cplusplus
}
#endif

#endif
