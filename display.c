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

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <SDL.h>
#include "display.h"

static void
set_pixel_rgb (SDL_Surface *surface, int idx, Uint8 r, Uint8 g, Uint8 b)
{
    // Cast SDL surface pixels and add address offset
    Uint32 *pixmem32 = (Uint32*) (surface->pixels + 4*idx);

    // Build an SDL RBG Map...
    Uint32 color = SDL_MapRGB (surface->format, r, g, b);

    // Write the color to the pixel 
    *pixmem32 = color;
}

static void
set_pixel_hv (disp_inst* displayx, int idx)
{
    SDL_Surface *surface = displayx->surface;
    Uint8 r, g, b;
    Uint32 *pixmem32;
    Uint32 color;
    unsigned int hv;

    hv = displayx->pixels[idx];

    if (hv < 0x40) {
        r = (displayx->palette[hv] & 0xFF0000) >> 16;
        g = (displayx->palette[hv] & 0x00FF00) >> 8;
        b = (displayx->palette[hv] & 0x0000FF) >> 0;
    } else {
        printf ("display.c: Pixel value outside of palette range!\n");
//        r = 0xDB; g = 0x2B; b = 0;
    }


    // Cast SDL surface pixels and add address offset
    pixmem32 = (Uint32*)(surface->pixels + 4*idx);

    // Build an SDL RBG Map...
    color = SDL_MapRGB (surface->format, r, g, b);

    // Write the color to the pixel 
    *pixmem32 = color;
}

// ----------

void
init_display ()
{
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0 ) {
        printf ("FATAL ERROR: Unable to initialize SDL!\nExiting...\n\n");
        exit (0);
    }
}


disp_inst*
make_display (int width, int height, int depth, int fullscreen, int interp)
{
    disp_inst *displayx;
    SDL_Surface *surface;

    // Finally, set the window caption
    SDL_WM_SetCaption("retro.box", "retrobox");

    // Build our display instance
    displayx = (disp_inst*) malloc (sizeof(disp_inst));

    // Allocate SDL independent working pixel buffer
    displayx->pixels = (unsigned int*) malloc (256*240*sizeof(unsigned int));
    memset (displayx->pixels, 0, 256*240*sizeof(unsigned int));

    // Initialize our SDL display surface
    if (!(surface = SDL_SetVideoMode(width, height, depth, SDL_HWSURFACE))) {
        printf ("FATAL ERROR: Unable to set SDL Video Mode!\nExiting...\n\n");
        SDL_Quit();
        exit (0);
    }

    // Populate display struct with useful things...
    displayx->surface    = surface;
    displayx->width      = width;
    displayx->height     = height;
    displayx->depth      = depth;
    displayx->fullscreen = fullscreen;
    displayx->interp     = interp;

    // Finally, build the NES system palette
    displayx->palette = (unsigned int*) malloc (64 * sizeof(unsigned int));

    displayx->palette[0x00] = 0x757575;    displayx->palette[0x10] = 0xBCBCBC;
    displayx->palette[0x01] = 0x271B8F;    displayx->palette[0x11] = 0x0073EF;
    displayx->palette[0x02] = 0x0000AB;    displayx->palette[0x12] = 0x233BEF;
    displayx->palette[0x03] = 0x47009F;    displayx->palette[0x13] = 0x8300F3;
    displayx->palette[0x04] = 0x8F0077;    displayx->palette[0x14] = 0xBF00BF;
    displayx->palette[0x05] = 0xAB0013;    displayx->palette[0x15] = 0xE7005B;
    displayx->palette[0x06] = 0xA70000;    displayx->palette[0x16] = 0xDB2B00;
    displayx->palette[0x07] = 0x7F0B00;    displayx->palette[0x17] = 0xCB4F0F;
    displayx->palette[0x08] = 0x432F00;    displayx->palette[0x18] = 0x8B7300;
    displayx->palette[0x09] = 0x004700;    displayx->palette[0x19] = 0x009700;
    displayx->palette[0x0A] = 0x005100;    displayx->palette[0x1A] = 0x00AB00;
    displayx->palette[0x0B] = 0x003F17;    displayx->palette[0x1B] = 0x00933B;
    displayx->palette[0x0C] = 0x1B3F5F;    displayx->palette[0x1C] = 0x00838B;
    displayx->palette[0x0D] = 0x000000;    displayx->palette[0x1D] = 0x000000;
    displayx->palette[0x0E] = 0x000000;    displayx->palette[0x1E] = 0x000000;
    displayx->palette[0x0F] = 0x000000;    displayx->palette[0x1F] = 0x000000;

    displayx->palette[0x20] = 0xFFFFFF;    displayx->palette[0x30] = 0xFFFFFF;
    displayx->palette[0x21] = 0x3FBFFF;    displayx->palette[0x31] = 0xABE7FF;
    displayx->palette[0x22] = 0x5F97FF;    displayx->palette[0x32] = 0xC7D7FF;
    displayx->palette[0x23] = 0xA78BFD;    displayx->palette[0x33] = 0xD7CBFF;
    displayx->palette[0x24] = 0xF77BFF;    displayx->palette[0x34] = 0xFFC7FF;
    displayx->palette[0x25] = 0xFF77B7;    displayx->palette[0x35] = 0xFFC7DB;
    displayx->palette[0x26] = 0xFF7763;    displayx->palette[0x36] = 0xFFBFB3;
    displayx->palette[0x27] = 0xFF9B3B;    displayx->palette[0x37] = 0xFFDBAB;
    displayx->palette[0x28] = 0xF3BF3F;    displayx->palette[0x38] = 0xFFE7A3;
    displayx->palette[0x29] = 0x83D313;    displayx->palette[0x39] = 0xE3FFA3;
    displayx->palette[0x2A] = 0x4FDF4B;    displayx->palette[0x3A] = 0xABF3BF;
    displayx->palette[0x2B] = 0x58F898;    displayx->palette[0x3B] = 0xB3FFCF;
    displayx->palette[0x2C] = 0x00EBDB;    displayx->palette[0x3C] = 0x9FFFF3;
    displayx->palette[0x2D] = 0x000000;    displayx->palette[0x3D] = 0x000000;
    displayx->palette[0x2E] = 0x000000;    displayx->palette[0x3E] = 0x000000;
    displayx->palette[0x2F] = 0x000000;    displayx->palette[0x3F] = 0x000000;

    return displayx;
}


void
update_display (disp_inst* displayx)
{ 
    SDL_Surface* surface = displayx->surface;
    Uint8 h, v;
    int i;

    // Lock the SDL surface so we can manipulate its pixel data
    if (SDL_MUSTLOCK(surface)) {
        if (SDL_LockSurface(surface) < 0) {
            return;
        }
    }

    // Surface is rasterized differently depending
    // on the employed interpolation method
    switch (displayx->interp)
    {
        // Nearest Neighbor
        // (Right now this is direct 1-to-1 rendering)
        case 0:
            // Scan through all pixels in NES frame
            for (i=0; i<256*240; i++) {
                set_pixel_hv (displayx, i);
            }
            break;

        // 2xSAI
        case 1:
            break;

        // Cubic B-spline, perahps?  just for fun...
        case 2:
            break;
    }


    // Unlock SDL surface if it needed locking earlier
    if (SDL_MUSTLOCK(surface)) {
        SDL_UnlockSurface (surface);
    }

    // Update the screen with a page flip
    SDL_Flip (surface); 
}

void
destroy_display (disp_inst* displayx)
{
    free (displayx->palette);
    free (displayx->pixels);
    SDL_FreeSurface (displayx->surface);
    free (displayx);
    displayx = 0x0;
}

void
unload_display ()
{
    SDL_QuitSubSystem (SDL_INIT_VIDEO);
}

