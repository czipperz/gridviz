#include <SDL.h>
#include <SDL_ttf.h>
#include <stdint.h>
#include <stdio.h>
#include <Tracy.hpp>
#include <cz/buffer_array.hpp>
#include <cz/defer.hpp>
#include <cz/heap.hpp>
#include <cz/str.hpp>

#include "global.hpp"
#include "gridviz.hpp"
#include "render.hpp"

#ifdef _WIN32
#include <shellscalingapi.h>
#endif

using namespace gridviz;

int actual_main(int argc, char** argv) {
    ZoneScoped;

    cz::Buffer_Array temp_arena;
    temp_arena.init();
    temp_allocator = temp_arena.allocator();

    cz::Buffer_Array permanent_arena;
    permanent_arena.init();
    permanent_allocator = permanent_arena.allocator();

    set_program_name(argv[0]);
    set_program_directory();

    Render_State rend = {};
    rend.font_size = 14;

#ifdef _WIN32
    SetProcessDpiAwareness(PROCESS_SYSTEM_DPI_AWARE);
#endif

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    CZ_DEFER(SDL_Quit());

    rend.dpi_scale = 1.0f;
    {
        const float dpi_default = 96.0f;
        float dpi = 0;
        if (SDL_GetDisplayDPI(0, &dpi, NULL, NULL) == 0)
            rend.dpi_scale = dpi / dpi_default;
    }

    if (TTF_Init() < 0) {
        fprintf(stderr, "TTF_Init failed: %s\n", TTF_GetError());
        return 1;
    }
    CZ_DEFER(TTF_Quit());

#ifdef _WIN32
    const char* font_path = "C:/Windows/Fonts/MesloLGM-Regular.ttf";
#else
    const char* font_path = "/usr/share/fonts/TTF/MesloLGMDZ-Regular.ttf";
#endif

    rend.font = TTF_OpenFont(font_path, (int)(rend.font_size * rend.dpi_scale));
    if (!rend.font) {
        fprintf(stderr, "TTF_OpenFont failed: %s\n", SDL_GetError());
        return 1;
    }

    rend.font_height = TTF_FontLineSkip(rend.font);
    rend.font_width = 10;
    TTF_GlyphMetrics(rend.font, ' ', nullptr, nullptr, nullptr, nullptr, &rend.font_width);

    SDL_Window* window = SDL_CreateWindow(
        "MYPROJECT", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, (int)(800 * rend.dpi_scale),
        (int)(800 * rend.dpi_scale), SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return 1;
    }
    CZ_DEFER(SDL_DestroyWindow(window));

    cz::Vector<Event> events = {};

    // TODO remove dummy events
    events.reserve(cz::heap_allocator(), 3);
    {
        uint8_t white[] = {0xff, 0xff, 0xff};
        uint8_t black[] = {0x00, 0x00, 0x00};

        Event event = {};
        event.cp.type = EVENT_CHAR_POINT;
        memcpy(event.cp.fg, white, sizeof(white));
        memcpy(event.cp.bg, black, sizeof(black));
        event.cp.ch = 'A';
        event.cp.x = 0;
        event.cp.y = 0;
        events.push(event);

        event = {};
        event.cp.type = EVENT_CHAR_POINT;
        memcpy(event.cp.fg, white, sizeof(white));
        memcpy(event.cp.bg, black, sizeof(black));
        event.cp.ch = 'B';
        event.cp.x = 2;
        event.cp.y = 1;
        events.push(event);

        event = {};
        event.cp.type = EVENT_CHAR_POINT;
        memcpy(event.cp.fg, white, sizeof(white));
        memcpy(event.cp.bg, black, sizeof(black));
        event.cp.ch = 'C';
        event.cp.x = 0;
        event.cp.y = 4;
        events.push(event);
    }

    while (1) {
        uint32_t start_frame = SDL_GetTicks();

        for (SDL_Event event; SDL_PollEvent(&event);) {
            switch (event.type) {
            case SDL_QUIT:
                return 0;
            case SDL_MOUSEMOTION:
                // Dragging with left button.
                if (event.motion.state & SDL_BUTTON_LMASK) {
                    rend.off_x -= event.motion.xrel;
                    rend.off_y -= event.motion.yrel;
                }
                break;
            case SDL_KEYDOWN:
                if (event.key.keysym.sym == SDLK_ESCAPE)
                    return 0;
                break;
            }
        }

        SDL_Surface* surface = SDL_GetWindowSurface(window);
        SDL_FillRect(surface, NULL, SDL_MapRGB(surface->format, 0xff, 0xff, 0xff));

        for (size_t i = 0; i < events.len; ++i) {
            Event& event = events[i];
            switch (event.type) {
            case EVENT_CHAR_POINT: {
                int64_t x = event.cp.x * rend.font_width + rend.off_x;
                int64_t y = event.cp.y * rend.font_height + rend.off_y;

                uint32_t bg =
                    SDL_MapRGB(surface->format, event.cp.bg[0], event.cp.bg[1], event.cp.bg[2]);
                uint32_t fg = (((uint32_t)event.cp.fg[0] << 16) | ((uint32_t)event.cp.fg[1] << 8) |
                               ((uint32_t)event.cp.fg[2]));

                char seq[5] = {(char)event.cp.ch};
                (void)render_code_point(&rend, surface, event.cp.x * rend.font_width,
                                        event.cp.y * rend.font_height, bg, fg, seq);
            } break;

            case EVENT_KEY_FRAME:
                CZ_PANIC("todo");
                break;

            default:
                CZ_DEBUG_ASSERT(false);  // Ignore in release mode.
                break;
            }
        }

        SDL_Rect rect = {200, 200, 100, 100};
        SDL_FillRect(surface, &rect, SDL_MapRGB(surface->format, 0x4d, 0xe0, 0xf4));
        SDL_UpdateWindowSurface(window);

        const uint32_t frame_length = 1000 / 60;
        uint32_t wanted_end = start_frame + frame_length;
        uint32_t end_frame = SDL_GetTicks();
        if (wanted_end > end_frame) {
            SDL_Delay(wanted_end - end_frame);
        }
    }

    return 0;
}
