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
#ifndef _6502_h_
#define _6502_h_

#include "6502_types.h"
#include "romreader.h"
#include "2C02.h"

// A 6502 CPU instance.
typedef struct cpu_instance cpu_inst;
struct cpu_instance {

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

    /* Memory Mapper ID */
    byte mapper_id;

    /* Memory Maps */
    byte** mmap;

    /* Loaded NES ROM */
    nes_rom* rom0;

    /* Not a fan of this, but it works out well */
    ppu_inst* ppux;

    /* Look up tables */
    void (**opcode)(cpu_inst* cpux);
    void (**amode)(cpu_inst* cpux);
    int *cycles;
    void (**mapper)(cpu_inst* cpux, int init);
};

// Populated when the 6502 engine is initialized.
// Used to generate 6502 CPU instances.
typedef struct cpu_look_up_tables cpu_luts;
struct cpu_look_up_tables {

    /* Opcode LUT */
    void (**opcode)(cpu_inst* cpux);

    /* Addressing mode LUT */
    void (**amode)(cpu_inst* cpux);

    /* Clock cycle LUT */
    int *cycles;

    void (**mapper)(cpu_inst* cpux, int init);
};



#if defined __cplusplus
extern "C" {
#endif

/* Used to initialze the 6502 engine */
cpu_luts* init_6502_engine ();

/* Unloads the 6502 engine's look up tables */
void unload_6502_engine (cpu_luts** luts);

/* Spawns a virtual 6502 CPU */
cpu_inst* make_cpu (cpu_luts* luts);

/* Destroy's a virtual 6502 CPU */
void destroy_cpu (cpu_inst** cpux);

/* Reset a virtual 6502 CPU */
void reset_cpu (cpu_inst* cpux);

/* Runs a virtual 6502 CPU for a defined # of cycles */
int run_cpu (cpu_inst* cpux, int cycles);

#if defined __cplusplus
}
#endif

#endif
