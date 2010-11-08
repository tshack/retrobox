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

#include <SDL.h>
#include "6502.h"
#include "display.h"

int
main (int argc, char* argv[])
{
    SDL_Event event;

    cpu_luts *cluts;        /* CPU Engine LUTs */
    cpu_inst *cpu0;         /* CPU Instance 0  */

    ppu_inst *ppu0;         /* PPU Instance 0 */
    disp_inst* display0;    /* NTSC Display */
    nes_rom* rom0;          /* Nintendo ROM Dump  */


    /* open rom from command line */
    if (argc > 1) {
        rom0 = read_rom (argv[1]);
    } else {
        printf ("No input ROM specified.\n\n");
        exit (0);
    }

    /* bring up some Video */
    init_display ();
    display0 = make_display (
                  256,        // width
                  240,        // height
                  32,         // bit depth
                  0,          // 0 = windowed, 1 = fullscreen
                  0           // interpolation mode
              );

    /* get 6502 running & all memory mapped up */
    cluts = init_6502_engine ();
    cpu0 = make_cpu (cluts);
    ppu0 = make_ppu ();
    init_nes_memorymap (cpu0, ppu0);
    ppu0->displayx = display0;

    /* setup mapper and run its init routine */
    cpu0->mapper_id = rom0->mapper;
    cpu0->rom0 = rom0;
    cpu0->mapper[cpu0->mapper_id](cpu0, 1);
    reset_cpu (cpu0);

    // Main event loop... 
    for (;;) {
        SDL_PollEvent (&event);

        run_cpu (cpu0, 1);

        if (event.type == SDL_QUIT) {
            break;
        }
    }

    // Destroy display instance
    destroy_display (display0);

    // Unload Video Sub-System
    unload_display ();

    return 0;
}

