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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "2C02.h"
#include "display.h"
#include "memory.h"

// TODO:
// update_xscroll() and update_yscroll()
// are corrupting name table and palette
// data.  This is probably because PPUADDR
// is being used to write to VRAM
// during a screen update.  Why the program
// thinks writing to PPUDATA during a screen
// update remains a mystery.  In other words,
// uncommenting the below and switching to
// v1, which does all scroll tracking with
// PPUADDR itself, fails hard by trashing the
// PPU memory map.

//#define scroll_v1

ppu_inst*
make_ppu ()
{
    /* Allocate a PPU instance for a 2C02 PPU */
    ppu_inst* ppux = (ppu_inst*) malloc (sizeof(ppu_inst));

    ppux->mmap = 0;

    ppux->PPUCTRL     = 0x0;
    ppux->PPUMASK     = 0x0;
    ppux->PPUSTATUS   = 0x0;
    ppux->OAMADDR     = 0x0;
    ppux->OAMDATA     = 0x0;
    ppux->PPUADDR     = 0x0;
    ppux->PPUDATA     = 0x0;
    ppux->PPULATCH    = 0x0;
    ppux->FINESCROLL  = 0x0;

    ppux->scanline = -21;
    ppux->linecycle = 0;

    ppux->T0 = 0;
    ppux->T1 = 0;
    ppux->flipflop = 0;
    ppux->dodma = 0;

    ppux->OAM = 0;
    ppux->NMI = 0;

    /* Return the address of the allocated register file */
    return ppux;
}


static void
update_xscroll_v2 (ppu_inst* ppux)
{
    byte tmp;

    // update the fine x-scroll counter
    tmp = (ppux->FINESCROLL & 0x07) + 1;
    ppux->FINESCROLL = (0x07 & tmp);
    if (tmp & 0x08) {
        // Roll over fine scroll and increment course scroll
        ppux->FINESCROLL ^= ppux->FINESCROLL;

        // update the course x-scroll counter (bits 4-0)
        tmp = (ppux->SCROLL & 0x001F) + 1;   // new count
        ppux->SCROLL &= ~0x041F;             // clear old count & carry
        ppux->SCROLL |= (0x1F & tmp);        // set new count
        ppux->SCROLL ^= (0x20 & tmp) << 5;   // toggle nametable ?
    }
}


static void
update_yscroll_v2 (ppu_inst* ppux)
{
    byte tmp;

    // Get the fine y-scroll
    tmp = (ppux->SCROLL & 0x7000) >> 12;

    // Inrement fine y-scroll by 1
    tmp = (tmp & 0x07) + 1;
    ppux->SCROLL &= ~0x7000;
    ppux->SCROLL |= (0x07 & tmp) << 12;
    if (tmp & 0x08) {
        // we rolled over, clear fine y-scroll...
        ppux->SCROLL &= ~0x7000;

        // ...and increment course y-scroll
        tmp = ((ppux->SCROLL & 0x03E0) >> 5) + 1;   // new count
        if (tmp == 30) {
            // Reset course y-scroll
            tmp = 0;
            // ...and toggle name table
            ppux->SCROLL ^= 0x0800;
        }
        ppux->SCROLL &= ~0x0BE0;                    // clear old count & carry
        ppux->SCROLL |= (0x1F & tmp) << 5;          // set new count
    }
}


static void
update_xscroll (ppu_inst* ppux)
{
    byte tmp;

    // update the fine x-scroll counter
    tmp = (ppux->FINESCROLL & 0x07) + 1;
    ppux->FINESCROLL = (0x07 & tmp);
    if (tmp & 0x08) {
        // Roll over fine scroll and increment course scroll
        ppux->FINESCROLL ^= ppux->FINESCROLL;

        // update the course x-scroll counter (bits 4-0)
        tmp = (ppux->PPUADDR & 0x001F) + 1;   // new count
        ppux->PPUADDR &= ~0x041F;             // clear old count & carry
        ppux->PPUADDR |= (0x1F & tmp);        // set new count
        ppux->PPUADDR ^= (0x20 & tmp) << 5;   // toggle nametable ?
    }
}


static void
update_yscroll (ppu_inst* ppux)
{
    byte tmp;

    // Get the fine y-scroll
    tmp = (ppux->PPUADDR & 0x7000) >> 12;

    // Inrement fine y-scroll by 1
    tmp = (tmp & 0x07) + 1;
    ppux->PPUADDR &= ~0x7000;
    ppux->PPUADDR |= (0x07 & tmp) << 12;
    if (tmp & 0x08) {
        // we rolled over, clear fine y-scroll...
        ppux->PPUADDR &= ~0x7000;

        // ...and increment course y-scroll
        tmp = ((ppux->PPUADDR & 0x03E0) >> 5) + 1;   // new count
        if (tmp == 30) {
            // Reset course y-scroll
            tmp = 0;
            // ...and toggle name table
            ppux->PPUADDR ^= 0x0800;
        }
        ppux->PPUADDR &= ~0x0BE0;                    // clear old count & carry
        ppux->PPUADDR |= (0x1F & tmp) << 5;          // set new count
    }
}


inline void
render_scanline (ppu_inst* ppux)
{
    // Fetch these things 32 times:
    //    1. Name Table byte for tile
    //    2. Attribute Table byte
    //    3. Pattern Table bitmap 0
    //    4. Pattern Table bitmap 1
    //    5. Evaluate y-coord of all 64 objects (4 cycles each)

    // Be sure to enforce (16-n) clock cycle timing for
    // correct Sprite 0 collisions.

    int i, j, x, y;
    byte tmp;
    byte nt_byte;        /* name table byte */
    byte at_byte;        /* attribute table byte */
    byte bitmap0;    
    byte bitmap1;
    word nt_base;
    word nt_addr;
    word at_addr;
    word bm_addr;
    word pal_addr;
    byte pal_bit0, pal_bit1, pal_bit23;
    disp_inst* displayx = ppux->displayx;

#if defined (scroll_v1)
    byte rough_x = ppux->PPUADDR & 0x001F;
    byte rough_y = (ppux->PPUADDR & 0x03E0) >> 5;
    byte fine_y  = (ppux->PPUADDR & 0x7000) >> 12;
#else
    byte rough_x = ppux->SCROLL & 0x001F;
    byte rough_y = (ppux->SCROLL & 0x03E0) >> 5;
    byte fine_y  = (ppux->SCROLL & 0x7000) >> 12;
#endif


    switch ((ppux->PPUADDR >> 10) & 0x03)
    {
    case 0:
        nt_base = 0x2000;
        break;
    case 1:
        nt_base = 0x2400;
        break;
    case 2:
        nt_base = 0x2800;
        break;
    case 3:
        nt_base = 0x2C00;
        break;
    }

    // physical screen coordinates
    x = ppux->linecycle;
    y = ppux->scanline;

    // render window coordinates
    i = (8*rough_x) + ppux->FINESCROLL;
    j = (8*rough_y) + fine_y;

    // build indices into name & attribute tables
    nt_addr = nt_base + ((32*(j/8)) + (i/8));
    at_addr = nt_base + 0x03C0 + (8*(j/32)) + (i/32);

    // name & attribute table lookups
    nt_byte = *ppux->mmap[nt_addr];
    at_byte = *ppux->mmap[at_addr];

    // base index into pattern table
    bm_addr = (nt_byte*16) + (j%8);
    bm_addr |= (0x10 & ppux->PPUCTRL) << 3;

    // pattern table lookups
    bitmap0 = *ppux->mmap[bm_addr + 0];
    bitmap1 = *ppux->mmap[bm_addr + 8];

    // extract pixel from upper & lower bitmap bytes
    pal_bit0 = ((0x80 >> (i%8)) & bitmap0) >> (7-(i%8));
    pal_bit1 = ((0x80 >> (i%8)) & bitmap1) >> (7-(i%8));

    // attribute table decode
    if ((j/16) % 2) {
        // C or D
        pal_bit23 = (0xF0 & at_byte) >> 4;
    } else {
        // A or B
        pal_bit23 = (0x0F & at_byte) >> 0;
    }
    if ((i/16) % 2) {
        pal_bit23 &= 0x03;    // A or C
    } else {
        pal_bit23 &= 0x0C ;   // B or D
        pal_bit23 = pal_bit23 >> 2;
    }

    // build address into palette table
    pal_addr = 0x3F00
             | (pal_bit0  << 0)
             | (pal_bit1  << 1)
             | (pal_bit23 << 2);

    // update the display
    displayx->pixels[256*y + x] = *ppux->mmap[pal_addr];
}


int
run_ppu (ppu_inst* ppux, int dcycles)
{
    byte tmp;

    while (dcycles > 0) {

        // VINT period
        if (ppux->scanline < -1) {

        }
        // The Dummy Scanline
        else if (ppux->scanline == -1) {
            // If background or sprite rendering is enabled, setup
            // the scroll parameters on cycle 304 of dummy scanline.
            if (ppux->linecycle == 304) {
                if (ppux->PPUMASK & 0x18) {
#if defined (scroll_v1)
                    ppux->PPUADDR = ppux->PPULATCH;
#else
                    ppux->SCROLL = ppux->PPULATCH;
#endif
                }
            }
        }

        // Display Scanlines
        else if ((ppux->scanline >= 0) && (ppux->scanline < 240)) {
            if ((ppux->linecycle >= 0) && (ppux->linecycle < 256)) {

                render_scanline (ppux);

#if defined (scroll_v1)
                update_xscroll (ppux);
#else
                update_xscroll_v2 (ppux);
#endif

            }
            else if ((ppux->linecycle >= 256) && (ppux->linecycle < 340)) {
                // Restore VRAM Address (undo changes made during scanline)
                //  --> Restore bits 10, 4, 3, 2, 1, 0 from latch
                if (ppux->linecycle == 256) {
                    ppux->PPUADDR |= (0x041F & ppux->PPULATCH);
                }

                // Load the sprite tiles to be rendered on the NEXT scanline.
                // To do this we do the following 8 times:
                //    1. Trash Name Table byte
                //    2. Trash Name Table byte
                //    3. Pattern Table bitmap 0 for sprite (for next scanline)
                //    4. Pattern Table bitmap 1 for sprite (for next scanline)
            }
        }

        // Update cycle counters
        ppux->linecycle++;
        dcycles--;

        if (ppux->linecycle > 341) {
            ppux->linecycle = 0;    // Current cycle within scanline (of 341)
            ppux->scanline++;

#if defined (scroll_v1)
            update_yscroll (ppux);
#else
            update_yscroll_v2 (ppux);
#endif

            // All Scanline Rendered!! (time to reset)
            if (ppux->scanline > 240) {
                ppux->scanline = -21;

                // issue an NMI on VBLANK?
                if (ppux->PPUCTRL & 0x80) {
                    ppux->NMI = 1;
//                    printf ("issuing NMI\n");
                }

                // indicate we are in VBLANK
                ppux->PPUSTATUS |= 0x80;

                update_display (ppux->displayx);
            }
        }

    } //while (cycles > 0)

    return dcycles;
}

