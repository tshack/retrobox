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

#ifndef _display_h_
#define _display_h_

#include <SDL.h>

typedef struct disp_instance disp_inst;
struct disp_instance {

    /* SDL surface properties */
    int width;          // x-resolution
    int height;         // y-resolution
    int depth;          // bit depth
    int fullscreen;     // 0 = windowed, 1 = fullscreen
    int interp;         // 0 = nearest neighbor, 1 = ???, 2 = ???

    /* our SDL surface */
    SDL_Surface *surface;

    /* used to hold screen pixels.
     * This will always be of NES
     * resolution dimensions.  This
     * data will be interpolated if
     * the SDL surface is larger */
    unsigned int *pixels;

    /* HSV to RGB Palette LUT */
    unsigned int *palette;

};


#if defined __cplusplus
extern "C" {
#endif

    /* Initializes the SDL Video Sub-System */
    void init_display ();

    /* Create a display instance */
    disp_inst* make_display (int width, int height, int depth, int fullscreen, int interp);

    /* Copies our pixel data to the SDL Surface and flips the page */
    void update_display (disp_inst* displayx);

    /* Destroy display instance */
    void destroy_display (disp_inst* displayx);

    /* Unloads the SDL Video Sub-System */
    void unload_display ();

#if defined __cplusplus
}
#endif


#endif
