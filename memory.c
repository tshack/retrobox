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

// file created: Sept 30th, 2010
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "6502_types.h"
#include "6502.h"
#include "2C02.h"
#include "memory.h"
#include "romreader.h"


inline void
do_dma (cpu_inst* cpux)
{
    int i;
    ppu_inst* ppux = cpux->ppux;
    byte cpu_base = *cpux->mmap[0x4014] << 8;

    // 2 cycles per byte xfer
    // 1 read & 1 write
    for (i=0; i<256; i++) {
        ppux->OAM[(ppux->OAMADDR + i) & 0xFF] = *cpux->mmap[cpu_base | i];
        run_ppu (ppux, 2);
    }
    run_ppu (ppux, 1);
}

// Swaps variable sized pages into the memory map.
// Perhaps better as a macro?
inline void
swap_in (
        byte **dest,
        unsigned int base_addr_dest,
        byte *src,
        unsigned int base_addr_src,
        unsigned int page_size
)
{
    unsigned int i;

    for (i=0; i < page_size; i++) {
        dest[base_addr_dest + i] = &src[base_addr_src + i];
//        printf ("%.4X: %.2X  -->  %.2X\n", base_addr_dest + i, *dest[base_addr_dest + i], src[base_addr_src + i]);
    }
}

// Swaps variable sized pages into the memory map.
// Perhaps better as a macro?
inline void
mirror (
        byte **mmap,
        unsigned int base_addr_dest,
        unsigned int base_addr_src,
        unsigned int page_size
)
{
    unsigned int i;

    for (i=0; i < page_size; i++) {
        mmap[base_addr_dest + i] = mmap[base_addr_src + i];
    }
}

/********************************************************************
 * M A P P E R S                                                    *
 ********************************************************************/
static void
mapper0 (cpu_inst* cpux, int init)
{
    // Mapper 0 is easy.
    //
    // The entire PRG-ROM is mapped to memory.  There is no
    // page swapping.  If the PRG-ROM is 2 banks (32KB) in size, then it
    // maps directly to the 2 banks worth of address space allocated
    // for PRG-ROM in the NES memory map. If the PRG-ROM is only 1 bank
    // (16KB) in size, then the address space for both the 1st 16KB bank
    // and the 2nd 16KB bank map to the same 16KB of data.  In other
    // words, in this scenario, they simply mirror one another.
    //
    // I think there is always only 8KB of Pattern Table data,
    // that contains both Name Table and Sprite patterns.  So,
    // in order for the Attribute Table & OAM palettes to work
    // correctly, we need to mirror CHR-ROM across both banks in
    // the PPU memory map.

    int i;
    nes_rom* romx = cpux->rom0;
    ppu_inst* ppux = cpux->ppux;

    if (init) {
        // Map PRG-ROM Pages
        if (romx->prg_rom_size == 1) {
            swap_in (cpux->mmap, 0x8000, romx->prg_rom, 0x0000, 16384);
            swap_in (cpux->mmap, 0xC000, romx->prg_rom, 0x0000, 16384);
        } else {
            swap_in (cpux->mmap, 0x8000, romx->prg_rom, 0x0000, 32768);
        }

        // Map CHR-ROM Pages
        swap_in (ppux->mmap, 0x0000, romx->chr_rom, 0x0000, 4096);
        swap_in (ppux->mmap, 0x1000, romx->chr_rom, 0x0000, 4096);

        // Update the PPU's full map mirror
        mirror (ppux->mmap, 0x4000, 0x0000, 16384);
        mirror (ppux->mmap, 0x8000, 0x0000, 16384);
        mirror (ppux->mmap, 0xC000, 0x0000, 16384);

        // Setup Name Table Mirroring (should do elsewhere?)
        if (romx->flg_mirroring == 0) {
            // horizontal mirroring
            mirror (ppux->mmap, 0x2400, 0x2000, 1024);
            mirror (ppux->mmap, 0x2C00, 0x2800, 1024);
        } else {
            // vertical mirroring
            mirror (ppux->mmap, 0x2800, 0x2000, 1024);
            mirror (ppux->mmap, 0x2C00, 0x2400, 1024);
        }

    } else {
        // Do nothing... static memory mapping
    }
}

static void
mapper_null (cpu_inst* cpux, int init)
{
    printf ("Mapper not yet implemented.\nExiting...\n\n");
    exit (0);
}


/********************************************************************
 * E N G I N E     I N T E R F A C E S                              *
 ********************************************************************/

void
init_nes_memorymap (cpu_inst* cpux, ppu_inst* ppux)
{
    int i,j;
    byte *RAM;          /* CPU RAM                   */
    byte *SRAM;         /* CPU Save RAM              */
    byte *UNK;          /* UNKNOWN                   */
    byte *TABLES;       /* PPU Name/Attribute Tables */
    byte *PALETTES;     /* PPU Palettes              */
    byte *OAM;          /* Sprite RAM (OAM)          */

    // Piggyback the ppu onto the cpu
    cpux->ppux = ppux;

    /******************
     * CPU Memory Map *
     ******************/

    /* Setup RAMs and IO for 6502 Memory Map */
    RAM  = (byte*) malloc (2048 * sizeof(byte));
    SRAM = (byte*) malloc (8192 * sizeof(byte));
    UNK  = (byte*) malloc (8192 * sizeof(byte));   // Unknown address space

    /* Extra 0x8000 is for PRG-ROM shadow */
    cpux->mmap = (byte**) malloc (0x18000 * sizeof(byte*));

    memset (RAM, 0, 2048 * sizeof(byte));

    /* Basic 6502 Memory Map stuff   */
    /* Map RAM  into 0x0000 - 0x1FFF */
    /* Map SRAM into 0x6000 - 0x7FFF */
    swap_in (cpux->mmap, 0x0000, RAM, 0x0000, 2048);
    swap_in (cpux->mmap, 0x6000, SRAM, 0x0000, 8192);

    /* RAM Mirrors */
    /* 0x0800-0x0FFF mirrors 0x0000-0x07FF */
    /* 0x1000-0x17FF mirrors 0x0000-0x07FF */
    /* 0x1800-0x1FFF mirrors 0x0000-0x07FF */
    mirror (cpux->mmap, 0x0800, 0x0000, 2048);
    mirror (cpux->mmap, 0x1000, 0x0000, 2048);
    mirror (cpux->mmap, 0x1800, 0x0000, 2048);

    /* Temporary memory allocation for Expansion ROM range */
    /* 0x4000-0x401F is Mapper I/O? and 0x4020-0x6000 is Expansion ROM */
    swap_in (cpux->mmap, 0x4000, UNK, 0x0000, 8192);


    /******************
     * PPU Memory Map *
     ******************/
    ppux->mmap = (byte**) malloc (0x10000 * sizeof(byte*));

    /* For all Name Tables and Attribute Tables */
    TABLES   = (byte*) malloc (4096 * sizeof(byte));
    PALETTES = (byte*) malloc (  32 * sizeof(byte));
    OAM      = (byte*) malloc ( 256 * sizeof(byte));

    memset (TABLES  , 0, 4096 * sizeof(byte));
    memset (PALETTES, 0,   32 * sizeof(byte));
    memset (OAM     , 0,  256 * sizeof(byte));

    /* Just assign OAM. It doesn't live in the PPU memory map. */
    ppux->OAM = OAM;
    
    /* Basic 2C02 Memory Map stuff   */
    /* Map TABLES   into 0x2000 - 0x2FFF */
    /* Map PALETTES into 0x3F00 - 0x3F1F */
    swap_in (ppux->mmap, 0x2000, TABLES, 0x0000, 4096);
    swap_in (ppux->mmap, 0x3F00, PALETTES, 0x0000, 32);

    /* Setup mirrors in PPU memory map     */
    /* 0x3000-0x3EFF mirrors 0x2000-0x2EFF */
    /* 0x3F20-0x3FFF mirrors 0x3F00-0x3F1F */
    /* 0x4000-0x7FFF mirrors 0x0000-0x3FFF */
    /* 0x8000-0xCFFF mirrors 0x0000-0x3FFF */
    mirror (ppux->mmap, 0x3000, 0x2000, 3840);
    mirror (ppux->mmap, 0x3F20, 0x3F00, 32);
    mirror (ppux->mmap, 0x3F40, 0x3F00, 32);
    mirror (ppux->mmap, 0x3F60, 0x3F00, 32);
    mirror (ppux->mmap, 0x3F80, 0x3F00, 32);
    mirror (ppux->mmap, 0x3FA0, 0x3F00, 32);
    mirror (ppux->mmap, 0x3FC0, 0x3F00, 32);
    mirror (ppux->mmap, 0x3FE0, 0x3F00, 32);
    mirror (ppux->mmap, 0x4000, 0x0000, 16384);
    mirror (ppux->mmap, 0x8000, 0x0000, 16384);
    mirror (ppux->mmap, 0xC000, 0x0000, 16384);


    /******************
     * Memory Mappers *
     ******************/
    /* Allocate Memory Mappers function pointer table */
    cpux->mapper = malloc (sizeof(void(*)(cpu_inst* cpux, int init)) * 256);

    /* Initialize Memory Mappers */
    cpux->mapper[0] = mapper0;
    cpux->mapper[1] = mapper_null;
    cpux->mapper[2] = mapper0;
    cpux->mapper[3] = mapper_null;
    cpux->mapper[4] = mapper_null;
    cpux->mapper[5] = mapper_null;
    cpux->mapper[6] = mapper_null;
    cpux->mapper[7] = mapper_null;
    cpux->mapper[8] = mapper_null;
    cpux->mapper[9] = mapper_null;
    cpux->mapper[10] = mapper_null;
    cpux->mapper[11] = mapper_null;
    cpux->mapper[12] = mapper_null;
    cpux->mapper[15] = mapper_null;
    cpux->mapper[16] = mapper_null;
    cpux->mapper[17] = mapper_null;
    cpux->mapper[18] = mapper_null;
    cpux->mapper[19] = mapper_null;
    cpux->mapper[20] = mapper_null;
    cpux->mapper[21] = mapper_null;
    cpux->mapper[22] = mapper_null;
    cpux->mapper[23] = mapper_null;
    cpux->mapper[24] = mapper_null;
    cpux->mapper[25] = mapper_null;
    cpux->mapper[32] = mapper_null;
    cpux->mapper[33] = mapper_null;
    cpux->mapper[34] = mapper_null;
    cpux->mapper[64] = mapper_null;
    cpux->mapper[65] = mapper_null;
    cpux->mapper[66] = mapper_null;
    cpux->mapper[67] = mapper_null;
    cpux->mapper[68] = mapper_null;
    cpux->mapper[69] = mapper_null;
    cpux->mapper[71] = mapper_null;
    cpux->mapper[78] = mapper_null;
    cpux->mapper[91] = mapper_null;
}


// Provides an abstraction for reading from memory
inline byte
read_mem (word address, cpu_inst* cpux)
{
    byte** mmap = cpux->mmap;
    ppu_inst* ppux = cpux->ppux;

    run_ppu (ppux, 3);

    // RAM, Stack, Zero Page
    if (address < 0x2000) {
        return *mmap[address];
    }

    // I/O Block Reads
    else if ((address >= 0x2000) && (address < 0x4000)) {
        // Because the I/O register map is highly mirrored 
        switch (address % 8)
        {
        case 0x00:  // PPUCTRL
        case 0x01:  // PPUMASK
        case 0x03:  // OAMADDR
        case 0x04:  // OAMDATA  (only Micromachines reads this?)
        case 0x05:  // PPUSCROLL
        case 0x06:  // PPUADDR
            break;

        // PPUSTATUS
        case 0x02:
            ppux->T1 = ppux->PPUSTATUS;
            ppux->PPUSTATUS &= ~0x80;
            return ppux->T1;
            break;


        // PPUDATA
        case 0x07:
            ppux->T1 = *ppux->mmap[ppux->PPUADDR];

            if ((ppux->PPUCTRL & 0x04)) {
                ppux->PPUADDR += 32;
            } else {
                ppux->PPUADDR++;
            }
            return ppux->T1;
            break;
        }
    }

    // Expansions ROM, SRAM
    else if ((address >= 0x4000) && (address < 0x8000)) {
        return *mmap[address];
    }

    // PRG-ROM
    else {
        return *mmap[address];
    }
}


// Called every time the CPU writes to memory
inline void
write_mem (byte data, word address, cpu_inst* cpux)
{
    int i;
    byte** mmap = cpux->mmap;
    ppu_inst* ppux = cpux->ppux;

    run_ppu (ppux, 3);

    // RAM, Stack, Zero Page
    if (address < 0x2000) {
        *mmap[address] = data;
    }

    // I/O Block Writes
    else if ((address >= 0x2000) && (address < 0x4000)) {

        // Last write to PPU I/O is held in PPUSTATUS
        ppux->PPUSTATUS |= (0x1F & data);

        // Because the I/O register map is highly mirrored 
        switch (address % 8)
        {
        // PPUCTRL
        case 0x00:
            ppux->PPUCTRL = data;

            // Lower 2 bits into B11-B10 of latch
            ppux->PPULATCH |= (0x03 & data) << 10;
            break;

        // PPUMASK
        case 0x01:
            ppux->PPUMASK = data;
            break;

        // PPUSTATUS
        case 0x02:
            // (cannot be written)
            break;

        // OAMADDR
        case 0x03:
            ppux->OAMADDR = data;
            break;

        // OAMDATA
        case 0x04:
            // (writes cause OAMADDR to increment)
            ppux->OAMDATA = data;
            ppux->OAM[ppux->OAMADDR++] = ppux->OAMDATA;
            break;

        // PPUSCROLL
        case 0x05:
            // (writes are two operations)
            if (ppux->flipflop) {
                // (2nd write - Vertical Scroll Offset)
                /* Lower 3 bits into B14-B12 of latch */
                ppux->PPULATCH |= (0x07 & data) << 12;
                /* Upper 5 bits into B9-B5 of latch */
                ppux->PPULATCH |= (data >> 3) << 5;
            } else {
                // (1st write - Horizontal Scroll Offset)
                /* Lower 3 bits define fine scroll */
                ppux->FINESCROLL = (0x07 & data);
                /* Upper 5 bits into B4-B0 of latch  */
                ppux->PPULATCH |= (data >> 3);

            }
            ppux->flipflop = !(ppux->flipflop);
            break;

        // PPUADDR
        case 0x06:
            // (writes are two operations)
            if (ppux->flipflop) {
                // 2nd write is lower byte
                ppux->PPULATCH |= data;
                ppux->PPUADDR = ppux->PPULATCH;
                ppux->SCROLL = ppux->PPULATCH;  // not sure about this
            } else {
                // 1st write is upper byte
                ppux->PPULATCH = (data << 8);
            }
            ppux->flipflop = !(ppux->flipflop);
            break;

        // PPUDATA
        case 0x07:
            ppux->PPUDATA = data;

            // Protect CHR-ROM from writes
            if (ppux->PPUADDR > 0x2000) {
                *ppux->mmap[ppux->PPUADDR] = ppux->PPUDATA;
            }

            if ((ppux->PPUCTRL & 0x04)) {
                ppux->PPUADDR += 32;
            } else {
                ppux->PPUADDR++;
            }
            break;
        }
    }

    // Expansions ROM, SRAM
    else if ((address >= 0x4000) && (address < 0x8000)) {
        *mmap[address] = data;

        // Sprite OAM DMA
        if (address == 0x4014) {
            // The PPU will take over and perform the DMA.  During this time
            // the PPU will continue to render (and the APU /should/ continue
            // to play sound), but the CPU will be cycle-stolen until the DMA
            // transfer is complete. Effectively, the CPU is "frozen in time"
            // during the DMA.
            do_dma (cpux);
        }
    }

    // PRG-ROM
    else {
        // The CPU will *write* to this PRG-ROM region 0x8000-0xFFFF when
        // communicating with certain Memory Mapper hardware.  So, we save
        // these writes to a shadow buffer instead of PRG-ROM.  To do this, we
        // extend the memory map by 32KB (32768 bytes).
        *mmap[address + 0x8000] = data;
    }
}


inline byte
read_mem_generic (word address, byte **mmap)
{
    return *mmap[address];
}


inline void
write_mem_generic (byte data, word address, byte **mmap)
{
    *mmap[address] = data;
}
