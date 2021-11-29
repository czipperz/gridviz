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

void close_font(Render_State* rend) {
    ZoneScoped;

    // Destroy existing caches.
    for (int ch = 0; ch < rend->caches.len; ch++) {
        Surface_Cache* cache = &rend->caches[ch];
        for (int i = 0; i < cache->surfaces.len; i++) {
            SDL_Surface* surface = cache->surfaces.get(i);
            SDL_FreeSurface(surface);
        }
        cache->code_points.len = 0;
        cache->surfaces.len = 0;
    }
    rend->caches.len = 0;

    // Close font.
    if (rend->font)
        TTF_CloseFont(rend->font);
}

bool open_font(Render_State* rend, const char* path, int font_size) {
    ZoneScoped;

    TTF_Font* font2 = TTF_OpenFont(path, font_size);
    if (!font2)
        return false;

    close_font(rend);
    rend->font = font2;
    rend->font_size = font_size;
    return true;
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

static int64_t compare_surface_cache(const Surface_Cache& left, const Surface_Cache& right) {
    return (int64_t)left.color - (int64_t)right.color;
}

static SDL_Surface* rasterize_code_point_cached(Render_State* rend,
                                                const char seq[5],
                                                SDL_Color color) {
    uint32_t code_point = unicode::utf8_code_point((const uint8_t*)seq);

    // Find the cache for this color.
    size_t index;
    uint32_t color32 = ((uint32_t)color.r << 16) | ((uint32_t)color.g << 8) |  //
                       ((uint32_t)color.b << 0);
    Surface_Cache fake_cache = {color32};
    if (!cz::binary_search(rend->caches.as_slice(), fake_cache, &index, compare_surface_cache)) {
        rend->caches.reserve(cz::heap_allocator(), 1);
        Surface_Cache cache = {};
        cache.color = color32;
        rend->caches.insert(index, cache);
    }

    // Check the cache.
    Surface_Cache* cache = &rend->caches[index];
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

bool render_code_point(Render_State* rend,
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
