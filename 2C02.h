/*******************************************************
 *    Filename: 2C02.h                                 *
 *      Author: James A. Shackleford (tshack@ieee.org) *
 *        Date: Sept 30th, 2010                        *
 *     License: GPL                                    *
 *******************************************************/
#ifndef _2C02_h_
#define _2C02_h_

#include "6502_types.h"
#include "display.h"

// NOTE:
// wo = write only, ro = read only, rw = read/write

// A 2C02 PPU Instance
typedef struct ppu_instance ppu_inst;
struct ppu_instance {

    // PPU needs access to its own memory map
    // (where all the tables live) as well as
    // the I/O block of the CPU memory map
    // (where all the PPU status and control
    // registers live).
    byte** mmap;

    // PPU needs access to the display
    disp_inst* displayx;

    /* Registers */
    // Because most operations require multiple
    // writes to PPU registers crossing several
    // CPU cycles, PPU register values from the
    // I/O block need to be saved in PPU internal
    // memory via latches.  Collectively, can be
    // thought of as the PPU state definition.
    byte PPUCTRL;       /*     Control (wo)  */
    byte PPUMASK;       /*        Mask (wo)  */
    byte PPUSTATUS;     /*      Status (ro)  */
    byte OAMADDR;       /* OAM Address (wo)  */
    byte OAMDATA;       /*    OAM Data (rw)  */
    byte PPUDATA;       /*    PPU Data (rw)  */
    word PPUADDR;       /* a.k.a Loopy_V     */
    word PPULATCH;      /* a.k.a Loopy_T     */
    byte FINESCROLL;    /* a.k.a Loopy_X     */
//    byte PPUSCROLL;     /*      Scroll (wo)  */
    
    /* Contains Sprite States */
    byte *OAM;

    /* NMI (VBLANK) */
    byte NMI;

    /* Set when DMA in progress */
    byte dodma;

    /* scroll stuff */
    word SCROLL;

    /* Internal (tmp) Registers */
    byte flipflop;
    byte latch;    // set by reading PPUSTATUS
    word T0;
    byte T1;

    /* Pixel/state Tracking */
    int scanline;       /* Current scanline          */
    int linecycle;      /* PPU cycle within scanline */
};

#if defined __cplusplus
extern "C" {
#endif

ppu_inst* make_ppu ();
int run_ppu (ppu_inst* ppux, int dcycles);

#if defined __cplusplus
}
#endif

#endif

