#include "render.hpp"

#include <SDL_image.h>
#include <Tracy.hpp>
#include <cz/binary_search.hpp>
#include <cz/format.hpp>
#include <cz/string.hpp>

#include "global.hpp"
#include "unicode.hpp"

#ifdef _WIN32
#include <SDL_syswm.h>
#endif

namespace gridviz {

///////////////////////////////////////////////////////////////////////////////
// Module Code - set icon
///////////////////////////////////////////////////////////////////////////////

void set_icon(SDL_Window* sdl_window) {
    ZoneScoped;

    // Try to set logo using Windows magic.  This results in
    // much higher definition on Windows so is preferred.
#ifdef _WIN32
    SDL_SysWMinfo wminfo;
    SDL_VERSION(&wminfo.version);
    if (SDL_GetWindowWMInfo(sdl_window, &wminfo) == 1) {
        HWND hwnd = wminfo.info.win.window;

        HINSTANCE handle = GetModuleHandle(nullptr);
        HICON icon = LoadIcon(handle, "IDI_MAIN_ICON");
        if (icon) {
            SetClassLongPtr(hwnd, GCLP_HICON, (LONG_PTR)icon);
            return;
        }
    }
#endif

    // Fallback to letting SDL do it.
    cz::String logo = cz::format(temp_allocator, program_directory, "logo.png");
    SDL_Surface* icon = IMG_Load(logo.buffer);
    if (icon) {
        SDL_SetWindowIcon(sdl_window, icon);
        SDL_FreeSurface(icon);
    }
}

///////////////////////////////////////////////////////////////////////////////
// Font manipulation
///////////////////////////////////////////////////////////////////////////////

Size_Cache* open_font(Font_State* rend, const char* path, int font_size) {
    size_t index;
    if (cz::binary_search(rend->font_sizes.as_slice(), font_size, &index))
        return &rend->by_size[index];

    ZoneScoped;

    Size_Cache size_cache = {};
    size_cache.font = TTF_OpenFont(path, font_size);
    if (!size_cache.font)
        return nullptr;

    size_cache.font_height = TTF_FontLineSkip(size_cache.font);
    size_cache.font_width = 10;
    TTF_GlyphMetrics(size_cache.font, ' ', nullptr, nullptr, nullptr, nullptr,
                     &size_cache.font_width);

    rend->font_sizes.reserve(cz::heap_allocator(), 1);
    rend->font_sizes.insert(index, font_size);
    rend->by_size.reserve(cz::heap_allocator(), 1);
    rend->by_size.insert(index, size_cache);
    return &rend->by_size[index];
}

///////////////////////////////////////////////////////////////////////////////
// Module Code - text cache manipulation
///////////////////////////////////////////////////////////////////////////////

static SDL_Surface* rasterize_code_point(const char* text,
                                         TTF_Font* font,
                                         int style,
                                         SDL_Color fgc) {
    ZoneScoped;
    TTF_SetFontStyle(font, style);
    return TTF_RenderUTF8_Blended(font, text, fgc);
}

static SDL_Surface* rasterize_code_point_cached(Size_Cache* rend,
                                                const char seq[5],
                                                SDL_Color color) {
    uint32_t code_point = unicode::utf8_code_point((const uint8_t*)seq);

    // Find the cache for this color.
    size_t index;
    uint32_t color32 = ((uint32_t)color.r << 16) | ((uint32_t)color.g << 8) |  //
                       ((uint32_t)color.b << 0);
    if (!cz::binary_search(rend->colors.as_slice(), color32, &index)) {
        rend->colors.reserve(cz::heap_allocator(), 1);
        rend->colors.insert(index, {});
        rend->by_color.reserve(cz::heap_allocator(), 1);
        rend->by_color.insert(index, {});
    }

    // Check the cache.
    Color_Cache* cache = &rend->by_color[index];
    if (cz::binary_search(cache->code_points.as_slice(), code_point, &index))
        return cache->surfaces[index];  // Cache hit.

    // Cache miss.  Rasterize and add to the cache.
    SDL_Surface* surface = rasterize_code_point(seq, rend->font, 0, color);
    CZ_ASSERT(surface);

    cache->code_points.reserve(cz::heap_allocator(), 1);
    cache->code_points.insert(index, code_point);
    cache->surfaces.reserve(cz::heap_allocator(), 1);
    cache->surfaces.insert(index, surface);

    return surface;
}

///////////////////////////////////////////////////////////////////////////////
// Module Code - render code point
///////////////////////////////////////////////////////////////////////////////

bool render_code_point(Size_Cache* rend,
                       SDL_Surface* window_surface,
                       int64_t px,
                       int64_t py,
                       SDL_Color background,
                       SDL_Color foreground,
                       const char seq_in[5]) {
    ZoneScoped;

    // Completely offscreen are ignored.
    if (px >= window_surface->w || py >= window_surface->h)
        return false;
    if (px + rend->font_width < 0 || py + rend->font_height < 0)
        return false;

    char seq[5];
    memcpy(seq, seq_in, sizeof(seq_in));

    // Little bit of input cleanup.
    if (cz::is_space(seq[0]))
        seq[0] = ' ';
    if (seq[0] == 0)
        seq[0] = 1;  // 0 would count as empty string which breaks rendering.

    SDL_Surface* s = rasterize_code_point_cached(rend, seq, foreground);

    SDL_Rect rect = {(int)px, (int)py, rend->font_width, rend->font_height};
    uint32_t bg32 = SDL_MapRGB(window_surface->format, background.r, background.g, background.b);
    SDL_FillRect(window_surface, &rect, bg32);

    SDL_Rect clip_character = {0, 0, rend->font_width, rend->font_height};
    SDL_BlitSurface(s, &clip_character, window_surface, &rect);

    return true;
}

}
