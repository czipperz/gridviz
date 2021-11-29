#include <SDL.h>
#include <SDL_ttf.h>
#include <stdint.h>
#include <stdio.h>
#include <Tracy.hpp>
#include <cz/buffer_array.hpp>
#include <cz/defer.hpp>
#include <cz/heap.hpp>
#include <cz/str.hpp>

#include "event.hpp"
#include "global.hpp"
#include "render.hpp"
#include "server.hpp"

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
    Network_State* net = nullptr;

    rend.font_size = 14;
    int port = 41088;

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
        "gridviz", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, (int)(800 * rend.dpi_scale),
        (int)(800 * rend.dpi_scale), SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return 1;
    }
    CZ_DEFER(SDL_DestroyWindow(window));

    cz::Vector<Event> events = {};
    net = start_networking(port);
    size_t selected_event = events.len;

    while (1) {
        uint32_t start_frame = SDL_GetTicks();

        for (SDL_Event event; SDL_PollEvent(&event);) {
            switch (event.type) {
            case SDL_QUIT:
                return 0;

            case SDL_MOUSEMOTION:
                // Dragging with left button.
                if (event.motion.state & SDL_BUTTON_LMASK) {
                    rend.off_x += event.motion.xrel;
                    rend.off_y += event.motion.yrel;
                }
                break;

            case SDL_KEYDOWN:
                if (event.key.keysym.sym == SDLK_ESCAPE)
                    return 0;
                if (event.key.keysym.sym == SDLK_LEFT) {
                    if (selected_event > 0)
                        selected_event--;
                }
                if (event.key.keysym.sym == SDLK_RIGHT) {
                    if (selected_event < events.len)
                        selected_event++;
                }
                break;
            }
        }

        bool advance = (selected_event == events.len);
        poll_network(net, &events);
        if (advance)
            selected_event = events.len;

        SDL_Surface* surface = SDL_GetWindowSurface(window);
        SDL_FillRect(surface, NULL, SDL_MapRGB(surface->format, 0xff, 0xff, 0xff));

        int bottom_height = 200;

        /////////////////////////////////////////
        // Main plane
        /////////////////////////////////////////
        {
            SDL_Rect plane_rect = {0, 0, surface->w, surface->h - bottom_height};
            SDL_SetClipRect(surface, &plane_rect);

            for (size_t i = 0; i < selected_event; ++i) {
                Event& event = events[i];
                switch (event.type) {
                case EVENT_CHAR_POINT: {
                    int64_t x = event.cp.x * rend.font_width + rend.off_x;
                    int64_t y = event.cp.y * rend.font_height + rend.off_y;

                    SDL_Color bg = {event.cp.bg[0], event.cp.bg[1], event.cp.bg[2]};
                    SDL_Color fg = {event.cp.fg[0], event.cp.fg[1], event.cp.fg[2]};

                    char seq[5] = {(char)event.cp.ch};
                    (void)render_code_point(&rend, surface, x, y, bg, fg, seq);
                } break;

                case EVENT_KEY_FRAME:
                    CZ_PANIC("todo");
                    break;

                default:
                    CZ_DEBUG_ASSERT(false);  // Ignore in release mode.
                    break;
                }
            }
        }

        /////////////////////////////////////////
        // Timeline
        /////////////////////////////////////////
        {
            SDL_Rect bar_rect = {0, surface->h - bottom_height, surface->w, bottom_height};
            SDL_SetClipRect(surface, &bar_rect);

            SDL_Color bg = {0xdd, 0xdd, 0xdd};

            // Gray background.
            SDL_FillRect(surface, &bar_rect, SDL_MapRGB(surface->format, bg.r, bg.g, bg.b));

            // Green slider.  TODO change color based on if future or past.
            SDL_Rect slider_rect = {bar_rect.x + 20, bar_rect.y + 120, bar_rect.w - 40, 60};
            SDL_FillRect(surface, &slider_rect, SDL_MapRGB(surface->format, 0x00, 0xff, 0x00));

            // Draw title.
            {
                SDL_Color fg = {0x00, 0x00, 0x00};
                int x = bar_rect.x + 20;
                int y = bar_rect.y + 20;
                cz::Str message = "Time line:";
                for (size_t i = 0; i < message.len; ++i) {
                    char seq[5] = {(char)message[i]};
                    (void)render_code_point(&rend, surface, x, y, bg, fg, seq);
                    x += rend.font_width;
                }
            }

            // Draw vertical bars for each event.
            {
                SDL_Color color = {0x00, 0x00, 0x00};
                uint32_t color32 = SDL_MapRGB(surface->format, color.r, color.g, color.b);
                double event_width = (double)slider_rect.w / (double)events.len;
                double x = slider_rect.x;
                for (size_t i = 0; i < events.len + 1; ++i) {
                    SDL_Rect line_rect = {x, slider_rect.y, 1, slider_rect.h};
                    SDL_FillRect(surface, &line_rect, color32);
                    x += event_width;
                }
            }
        }

        SDL_SetClipRect(surface, nullptr);
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
