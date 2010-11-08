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

// file created: Sept 30th, 2010
#ifndef _memory_h_
#define _memory_h_

#include "6502.h"
#include "2C02.h"
#include "6502_types.h"
#include "romreader.h"

#if defined __cplusplus
extern "C" {
#endif

/* Builds the mapper function pointer look-up table */
//void init_memorymap (cpu_inst* cpux);
void init_nes_memorymap (cpu_inst* cpux, ppu_inst* ppux);

/* Read a byte from memory (CPU) */
inline byte read_mem (word address, cpu_inst* cpux);

/* Write a byte to memory (CPU) */
inline void write_mem (byte data, word address, cpu_inst* cpux);

/* Read a byte from memory (PPU) */
inline byte read_mem_generic (word address, byte **mmap);

/* Write a byte to memory (PPU) */
inline void write_mem_generic (byte data, word address, byte **mmap);

#if defined __cplusplus
}
#endif

#endif
