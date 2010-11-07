// I think it should look something
// like this... not sure yet.  Not
// very hip to SDL, really.

// Use enumerated types for some of this

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
