#pragma once

#include <SDL.h>
#include <SDL_ttf.h>
#include <stdint.h>
#include <cz/vector.hpp>

namespace gridviz {

///////////////////////////////////////////////////////////////////////////////
// Type Definitions
///////////////////////////////////////////////////////////////////////////////

struct Surface_Cache {
    uint32_t color;
    cz::Vector<uint32_t> code_points;
    cz::Vector<SDL_Surface*> surfaces;
};

struct Render_State {
    float dpi_scale;
    int font_width;
    int font_height;
    TTF_Font* font;
    int font_size;
    cz::Vector<Surface_Cache> caches;
    int64_t off_x;
    int64_t off_y;

    void drop();
};

///////////////////////////////////////////////////////////////////////////////
// Function Declarations
///////////////////////////////////////////////////////////////////////////////

void set_icon(SDL_Window* sdl_window);

bool open_font(Render_State* rend, const char* path, int font_size);

bool render_code_point(Render_State* rend,
                       SDL_Surface* window_surface,
                       int64_t px,
                       int64_t py,
                       SDL_Color background,
                       SDL_Color foreground,
                       const char seq_in[5]);

}
