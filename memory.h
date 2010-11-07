/************************************************************
 *    Filename: memory.h                                    *
 *      Author: James A. Shackleford (retro.box@tshack.net) *
 *        Date: Sept 30th, 2010                             *
 *     License: See COPYRIGHT.TXT                           *
 ************************************************************/
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
