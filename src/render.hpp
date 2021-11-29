#pragma once

#include <SDL.h>
#include <SDL_ttf.h>
#include <stdint.h>
#include <cz/vector.hpp>

namespace gridviz {

///////////////////////////////////////////////////////////////////////////////
// Type Definitions
///////////////////////////////////////////////////////////////////////////////

struct Color_Cache {
    cz::Vector<uint32_t> code_points;
    cz::Vector<SDL_Surface*> surfaces;
};

struct Size_Cache {
    TTF_Font* font;
    int font_width;
    int font_height;
    cz::Vector<uint32_t> colors;
    cz::Vector<Color_Cache> by_color;
};

struct Font_State {
    cz::Vector<int> font_sizes;
    cz::Vector<Size_Cache> by_size;
};

///////////////////////////////////////////////////////////////////////////////
// Function Declarations
///////////////////////////////////////////////////////////////////////////////

void set_icon(SDL_Window* sdl_window);

Size_Cache* open_font(Font_State* rend, const char* path, int font_size);

bool render_code_point(Size_Cache* rend,
                       SDL_Surface* window_surface,
                       int64_t px,
                       int64_t py,
                       SDL_Color background,
                       SDL_Color foreground,
                       const char seq_in[5]);

}
