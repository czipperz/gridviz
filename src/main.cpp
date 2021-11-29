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
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <shellscalingapi.h>
#endif

using namespace gridviz;

static int get_timeline_width(int window_width) {
    return window_width / 3;
}

static void render_timeline_line(Size_Cache* font,
                                 SDL_Surface* surface,
                                 SDL_Point* text_rect_start,
                                 SDL_Point* text_rect_end,
                                 SDL_Color bg,
                                 SDL_Color fg,
                                 cz::Str message) {
    int x = text_rect_start->x;
    int y = text_rect_start->y;
    int numchars = cz::max(1, (text_rect_end->x - text_rect_start->x) / font->font_width);
    int width = numchars * font->font_width;
    for (size_t i = 0; i < message.len; ++i) {
        if (x == width) {
            x = 0;
            y += font->font_height;
            text_rect_start->y = y;
        }

        char seq[5] = {(char)message[i]};
        (void)render_code_point(font, surface, x, y, bg, fg, seq);
        x += font->font_width;
    }
    text_rect_start->y += font->font_height;
}

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

    Font_State rend = {};
    Network_State* net = nullptr;
    Game_State game = {};

    int menu_font_size = 14;
    int port = 41088;

#ifdef _WIN32
    SetProcessDpiAwareness(PROCESS_SYSTEM_DPI_AWARE);
#endif

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    CZ_DEFER(SDL_Quit());

    float dpi_scale = 1.0f;
    {
        const float dpi_default = 96.0f;
        float dpi = 0;
        if (SDL_GetDisplayDPI(0, &dpi, NULL, NULL) == 0)
            dpi_scale = dpi / dpi_default;
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

    SDL_Window* window = SDL_CreateWindow(
        "gridviz", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, (int)(800 * dpi_scale),
        (int)(800 * dpi_scale), SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return 1;
    }
    CZ_DEFER(SDL_DestroyWindow(window));

    net = start_networking(port);

    bool dragging = false;
    Run_Info* previously_selected_run = NULL;

    while (1) {
        uint32_t start_frame = SDL_GetTicks();

        Run_Info* the_run =
            (game.selected_run < game.runs.len ? &game.runs[game.selected_run] : NULL);

        if (previously_selected_run != the_run) {
            previously_selected_run = the_run;
            // Selection changed.
            dragging = false;
        }

        for (SDL_Event event; SDL_PollEvent(&event);) {
            switch (event.type) {
            case SDL_QUIT:
                return 0;

            case SDL_MOUSEBUTTONDOWN:
                if (event.button.button == SDL_BUTTON_LEFT && the_run) {
                    int window_width, window_height;
                    SDL_GetWindowSize(window, &window_width, &window_height);
                    if (event.button.x > get_timeline_width(window_width)) {
                        dragging = true;
                    } else {
#if 0
                        SDL_Rect bar_rect = {0, window_height - bottom_height, window_width,
                                             bottom_height};
                        SDL_Rect slider_rect = {bar_rect.x + 20, bar_rect.y + 50, bar_rect.w - 40,
                                                30};
                        SDL_Point point = {event.button.x, event.button.y};
                        if (SDL_PointInRect(&point, &slider_rect)) {
                            double event_width =
                                (double)slider_rect.w / (double)the_run->strokes.len;
                            the_run->selected_stroke =
                                (size_t)((point.x - slider_rect.x) / event_width + 0.5);
                        }
#endif
                    }
                }
                break;

            case SDL_MOUSEBUTTONUP:
                if (event.button.button == SDL_BUTTON_LEFT) {
                    dragging = false;
                }
                break;

            case SDL_MOUSEMOTION:
                // Dragging with left button.
                if ((event.motion.state & SDL_BUTTON_LMASK) && the_run && dragging) {
                    the_run->off_x += event.motion.xrel;
                    the_run->off_y += event.motion.yrel;
                }
                break;

            case SDL_KEYDOWN:
                if (event.key.keysym.sym == SDLK_ESCAPE)
                    return 0;
                if (event.key.keysym.sym == SDLK_UP && the_run) {
                    if (the_run->selected_stroke >= the_run->strokes.len &&
                        the_run->strokes.len > 0) {
                        the_run->selected_stroke--;
                    }
                    if (the_run->selected_stroke > 0)
                        the_run->selected_stroke--;
                }
                if (event.key.keysym.sym == SDLK_DOWN && the_run) {
                    if (the_run->selected_stroke < the_run->strokes.len)
                        the_run->selected_stroke++;
                }
                break;
            }
        }

        poll_network(net, &game);

        SDL_Surface* surface = SDL_GetWindowSurface(window);
        SDL_FillRect(surface, NULL, SDL_MapRGB(surface->format, 0xff, 0xff, 0xff));

        Size_Cache* menu_font = open_font(&rend, font_path, (int)(menu_font_size * dpi_scale));
        if (!menu_font) {
            fprintf(stderr, "TTF_OpenFont failed: %s\n", SDL_GetError());
            return 1;
        }

        int timeline_width = get_timeline_width(surface->w);

        /////////////////////////////////////////
        // Main plane
        /////////////////////////////////////////
        if (the_run) {
            Size_Cache* run_font =
                open_font(&rend, font_path, (int)(the_run->font_size * dpi_scale));
            if (!run_font) {
                fprintf(stderr, "TTF_OpenFont failed: %s\n", SDL_GetError());
                return 1;
            }

            SDL_Rect plane_rect = {timeline_width, 0, surface->w - timeline_width, surface->h};
            SDL_SetClipRect(surface, &plane_rect);

            for (size_t s = 0; s < cz::min(the_run->strokes.len, the_run->selected_stroke + 1);
                 ++s) {
                Stroke* stroke = &the_run->strokes[s];
                for (size_t i = 0; i < stroke->events.len; ++i) {
                    Event& event = stroke->events[i];
                    switch (event.type) {
                    case EVENT_CHAR_POINT: {
                        int64_t x = event.cp.x * run_font->font_width + the_run->off_x;
                        int64_t y = event.cp.y * run_font->font_height + the_run->off_y;
                        x += timeline_width;

                        SDL_Color bg = {event.cp.bg[0], event.cp.bg[1], event.cp.bg[2]};
                        SDL_Color fg = {event.cp.fg[0], event.cp.fg[1], event.cp.fg[2]};

                        char seq[5] = {(char)event.cp.ch};
                        (void)render_code_point(run_font, surface, x, y, bg, fg, seq);
                    } break;

                    default:
                        CZ_DEBUG_ASSERT(false);  // Ignore in release mode.
                        break;
                    }
                }
            }
        }

        /////////////////////////////////////////
        // Timeline
        /////////////////////////////////////////
        if (the_run) {
            // Color constants.
            const SDL_Color bg = {0xdd, 0xdd, 0xdd};
            const SDL_Color fg_selected = {0x00, 0x00, 0xd7};
            const SDL_Color fg_applied = {0x00, 0x00, 0x00};
            const SDL_Color fg_ignored = {0x44, 0x44, 0x44};
            const SDL_Color horline_color = {0x44, 0x44, 0x44};
            const int padding = 8;

            SDL_Rect bar_rect = {0, 0, timeline_width, surface->h};
            SDL_SetClipRect(surface, &bar_rect);

            // Gray background.
            SDL_FillRect(surface, &bar_rect, SDL_MapRGB(surface->format, bg.r, bg.g, bg.b));

            SDL_Point text_rect_start = {bar_rect.x + padding, bar_rect.y + padding};
            SDL_Point text_rect_end = {bar_rect.w - padding, bar_rect.h - padding};

            // Draw title.
            render_timeline_line(menu_font, surface, &text_rect_start, &text_rect_end, bg,
                                 fg_applied, "Time line:");

            // Draw horizontal divider after the title.
            text_rect_start.y += 4;  // add some padding
            SDL_Rect horline = {bar_rect.x, text_rect_start.y, bar_rect.w, 2};
            SDL_FillRect(
                surface, &horline,
                SDL_MapRGB(surface->format, horline_color.r, horline_color.g, horline_color.b));
            text_rect_start.y += 2;  // account for horline
            text_rect_start.y += 4;  // add some padding

            for (size_t i = 0; i < the_run->strokes.len; ++i) {
                Stroke* stroke = &the_run->strokes[i];
                SDL_Color fg = fg_ignored;
                if (i == the_run->selected_stroke ||
                    (i == the_run->selected_stroke - 1 && i == the_run->strokes.len - 1)) {
                    fg = fg_selected;
                } else if (i < the_run->selected_stroke) {
                    fg = fg_applied;
                }
                render_timeline_line(menu_font, surface, &text_rect_start, &text_rect_end, bg, fg,
                                     stroke->title);

                // Draw horizontal divider after the title.
                text_rect_start.y += 2;  // add some padding
                SDL_Rect horline = {bar_rect.x + padding, text_rect_start.y,
                                    bar_rect.w - 2 * padding, 1};
                SDL_FillRect(
                    surface, &horline,
                    SDL_MapRGB(surface->format, horline_color.r, horline_color.g, horline_color.b));
                text_rect_start.y += 1;  // account for horline
                text_rect_start.y += 2;  // add some padding
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
