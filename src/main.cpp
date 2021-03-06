#include <SDL.h>
#include <SDL_ttf.h>
#include <stdint.h>
#include <stdio.h>
#include <Tracy.hpp>
#include <cz/buffer_array.hpp>
#include <cz/date.hpp>
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

const int header_height = 40;

static int get_timeline_width(int window_width) {
    return window_width / 3;
}

static void render_timeline_line(Size_Cache* font,
                                 SDL_Surface* surface,
                                 SDL_Point* text_rect_start,
                                 SDL_Point* text_rect_end,
                                 SDL_Color bg,
                                 SDL_Color fg,
                                 cz::Str message,
                                 int mode) {
    int x = text_rect_start->x;
    int y = text_rect_start->y;
    int numchars = cz::max(1, (text_rect_end->x - text_rect_start->x) / font->font_width);
    int width = numchars * font->font_width;
    if (mode >= 0) {
        cz::Str prefix = (mode <= 1 ? "+ " : "  ");
        for (size_t i = 0; i < prefix.len; ++i) {
            if (x == width) {
                x = 0;
                y += font->font_height;
                text_rect_start->y = y;
            }

            char seq[5] = {(char)prefix[i]};
            (void)render_code_point(font, surface, x, y, bg, fg, seq);
            x += font->font_width;
        }
    }
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

static bool find_matching_stroke(cz::Slice<SDL_Rect> the_stroke_rects,
                                 SDL_Point point,
                                 size_t* index) {
    for (size_t i = 0; i < the_stroke_rects.len; ++i) {
        SDL_Rect rect = the_stroke_rects[i];
        if (SDL_PointInRect(&point, &rect)) {
            *index = i;
            return true;
        }
    }

    if (the_stroke_rects.len == 0)
        return false;

    // If flick up or down fast then recognize that.
    if (point.x < the_stroke_rects[0].y) {
        *index = 0;
        return true;
    }
    if (point.y > the_stroke_rects.last().y + the_stroke_rects.last().h) {
        *index = the_stroke_rects.len;
        return true;
    }

    return false;
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
    int wfc_font_size = 20;
    int header_font_size = 14;
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

    int dragging = 0;
    cz::Vector<SDL_Rect> the_stroke_rects = {};
    Run_Info* previously_selected_run = NULL;

    CZ_DEFER(the_stroke_rects.drop(cz::heap_allocator()));

    while (1) {
        uint32_t start_frame = SDL_GetTicks();

        Run_Info* the_run =
            (game.selected_run < game.runs.len ? &game.runs[game.selected_run] : NULL);

        if (previously_selected_run != the_run) {
            previously_selected_run = the_run;
            // Selection changed.
            dragging = 0;
            the_stroke_rects.len = 0;
        }

        for (SDL_Event event; SDL_PollEvent(&event);) {
            switch (event.type) {
            case SDL_QUIT:
                return 0;

            case SDL_MOUSEBUTTONDOWN:
                if (event.button.button == SDL_BUTTON_LEFT && the_run) {
                    int window_width;
                    SDL_GetWindowSize(window, &window_width, nullptr);
                    if (event.button.x > get_timeline_width(window_width)) {
                        dragging = 1;
                    } else {
                        // Select a new stroke.
                        SDL_Point point = {event.button.x, event.button.y};
                        (void)find_matching_stroke(the_stroke_rects, point,
                                                   &the_run->selected_stroke);
                        dragging = 2;
                    }
                }
                break;

            case SDL_MOUSEBUTTONUP:
                if (event.button.button == SDL_BUTTON_LEFT) {
                    dragging = 0;
                }
                break;

            case SDL_MOUSEMOTION:
                if (event.motion.state & SDL_BUTTON_LMASK) {
                    if (the_run && dragging == 1) {
                        // Panning.
                        the_run->off_x += event.motion.xrel;
                        the_run->off_y += event.motion.yrel;
                    } else if (the_run && dragging == 2) {
                        // Selecting stroke.
                        SDL_Point point = {event.motion.x, event.motion.y};
                        (void)find_matching_stroke(the_stroke_rects, point,
                                                   &the_run->selected_stroke);
                    }
                }
                break;

            case SDL_MOUSEWHEEL: {
                if (event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) {
                    event.wheel.y *= -1;
                    event.wheel.x *= -1;
                }

// On linux the horizontal scroll is flipped for some reason.
#ifndef _WIN32
                event.wheel.x *= -1;
#endif

                if (the_run) {
                    float old_zoom = the_run->zoom;
                    if (event.wheel.y < 0) {
                        // Scroll down - zoom out.
                        the_run->zoom /= 1.25f;
                    } else if (event.wheel.y > 0) {
                        // Scroll up - zoom in.
                        the_run->zoom *= 1.25f;
                    }
                    float new_zoom = the_run->zoom;
                    the_run->font_size = (int)(14 * the_run->zoom);

                    //
                    // Zoom around the mouse.  Note: the offsets are at the current zoom level.
                    //

                    // Get mouse position in the plane.
                    int mouse_x = 0, mouse_y = 0, window_width = 0;
                    SDL_GetMouseState(&mouse_x, &mouse_y);
                    SDL_GetWindowSize(window, &window_width, nullptr);
                    int64_t m2_x = mouse_x - get_timeline_width(window_width);
                    int64_t m2_y = mouse_y - header_height;

                    // Make the mouse the origin then zoom then revert.
                    the_run->off_x -= m2_x;
                    the_run->off_y -= m2_y;
                    the_run->off_x *= new_zoom / old_zoom;
                    the_run->off_y *= new_zoom / old_zoom;
                    the_run->off_x += m2_x;
                    the_run->off_y += m2_y;
                }
            } break;

            case SDL_KEYDOWN:
                if (event.key.keysym.sym == SDLK_ESCAPE)
                    return 0;

                // Set selected stroke.
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

                // Set selected run.
                if (event.key.keysym.sym == SDLK_LEFT) {
                    if (game.selected_run > 0)
                        game.selected_run--;
                    the_run =
                        (game.selected_run < game.runs.len ? &game.runs[game.selected_run] : NULL);
                    // Selection changed.
                    dragging = false;
                    the_stroke_rects.len = 0;
                }
                if (event.key.keysym.sym == SDLK_RIGHT) {
                    if (game.selected_run < game.runs.len)
                        game.selected_run++;
                    the_run =
                        (game.selected_run < game.runs.len ? &game.runs[game.selected_run] : NULL);
                    // Selection changed.
                    dragging = false;
                    the_stroke_rects.len = 0;
                }

                // Reset offset.
                if (event.key.keysym.sym == SDLK_0 && the_run) {
                    the_run->off_x = 10;
                    the_run->off_y = 10;
                }

                break;
            }
        }

        poll_network(net, &game);

        SDL_Surface* surface = SDL_GetWindowSurface(window);
        SDL_FillRect(surface, NULL, SDL_MapRGB(surface->format, 0xff, 0xff, 0xff));

        int timeline_width = get_timeline_width(surface->w);

        /////////////////////////////////////////
        // Header
        /////////////////////////////////////////
        {
            // Color constants.
            const SDL_Color bg = {0xbb, 0xbb, 0xbb};
            const SDL_Color fg = {0x00, 0x00, 0x00};
            const int hor_padding = 10;

            // Open font.
            Size_Cache* header_font =
                open_font(&rend, font_path, (int)(header_font_size * dpi_scale));
            if (!header_font) {
                fprintf(stderr, "TTF_OpenFont failed: %s\n", SDL_GetError());
                return 1;
            }

            // Clip and fill.
            SDL_Rect plane_rect = {0, 0, surface->w, header_height};
            SDL_SetClipRect(surface, &plane_rect);
            SDL_FillRect(surface, &plane_rect, SDL_MapRGB(surface->format, bg.r, bg.g, bg.b));

            // Draw bottom line.
            SDL_Rect bottom_line = {0, header_height - 1, surface->w, 1};
            SDL_FillRect(surface, &bottom_line, SDL_MapRGB(surface->format, 0x00, 0x00, 0x00));

            // Draw middle indicator.
            for (int iter = 0; iter < 3; ++iter) {
                Run_Info* iter_run = nullptr;
                int64_t index = (int64_t)game.selected_run + iter - 1;
                if (index >= 0 && index < game.runs.len)
                    iter_run = &game.runs[index];

                if (iter_run) {
                    cz::Date date = iter_run->start_time;

                    char buffer[20];
                    snprintf(buffer, sizeof(buffer), "%04d/%02d/%02d %02d:%02d:%02d", date.year,
                             date.month, date.day_of_month, date.hour, date.minute, date.second);
                    size_t buflen = sizeof(buffer) - 1;

                    int64_t x = (surface->w - hor_padding - header_font->font_width * buflen);
                    x = x * iter / 2;
                    int64_t y = 0;
                    for (size_t i = 0; i < buflen; ++i) {
                        char seq[5] = {(char)buffer[i]};
                        (void)render_code_point(header_font, surface, x, y, bg, fg, seq);
                        x += header_font->font_width;
                    }
                }
            }
        }

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

            SDL_Rect plane_rect = {timeline_width, header_height, surface->w - timeline_width,
                                   surface->h - header_height};
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
                        y += header_height;

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

            // Draw axes.
            SDL_Rect axis_x = {0, the_run->off_y, surface->w, 1};
            SDL_Rect axis_y = {the_run->off_x, 0, 1, surface->h};
            axis_x.x += timeline_width;
            axis_x.y += header_height;
            axis_y.x += timeline_width;
            axis_y.y += header_height;
            uint32_t axis_color32 = SDL_MapRGB(surface->format, 0x88, 0x88, 0x88);
            SDL_FillRect(surface, &axis_x, axis_color32);
            SDL_FillRect(surface, &axis_y, axis_color32);
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

            Size_Cache* menu_font = open_font(&rend, font_path, (int)(menu_font_size * dpi_scale));
            if (!menu_font) {
                fprintf(stderr, "TTF_OpenFont failed: %s\n", SDL_GetError());
                return 1;
            }

            SDL_Rect bar_rect = {0, header_height, timeline_width, surface->h - header_height};
            SDL_SetClipRect(surface, &bar_rect);

            // Gray background.
            SDL_FillRect(surface, &bar_rect, SDL_MapRGB(surface->format, bg.r, bg.g, bg.b));

            SDL_Rect rightbar = {bar_rect.w - 1, bar_rect.y, 1, bar_rect.h};
            SDL_FillRect(surface, &rightbar, SDL_MapRGB(surface->format, 0x00, 0x00, 0x00));

            SDL_Point text_rect_start = {bar_rect.x + padding, bar_rect.y + padding};
            SDL_Point text_rect_end = {bar_rect.w - padding, bar_rect.h - padding};

            // Draw title.
            render_timeline_line(menu_font, surface, &text_rect_start, &text_rect_end, bg,
                                 fg_applied, "Time line:", /*mode=*/-1);

            // Draw horizontal divider after the title.
            text_rect_start.y += 4;  // add some padding
            SDL_Rect horline = {bar_rect.x, text_rect_start.y, bar_rect.w, 2};
            SDL_FillRect(
                surface, &horline,
                SDL_MapRGB(surface->format, horline_color.r, horline_color.g, horline_color.b));
            text_rect_start.y += 2;  // account for horline
            text_rect_start.y += 4;  // add some padding

            the_stroke_rects.len = 0;
            for (size_t i = 0; i < the_run->strokes.len; ++i) {
                Stroke* stroke = &the_run->strokes[i];
                SDL_Color fg = fg_ignored;
                int mode = 2;
                if (i == the_run->selected_stroke ||
                    (i == the_run->selected_stroke - 1 && i == the_run->strokes.len - 1)) {
                    fg = fg_selected;
                    mode = 1;
                } else if (i < the_run->selected_stroke) {
                    fg = fg_applied;
                    mode = 0;
                }
                SDL_Rect stroke_rect = {text_rect_start.x, text_rect_start.y - 2,
                                        bar_rect.w - padding * 2, 0};

                render_timeline_line(menu_font, surface, &text_rect_start, &text_rect_end, bg, fg,
                                     stroke->title, mode);

                stroke_rect.h = text_rect_start.y - stroke_rect.y + 2 * 2;
                the_stroke_rects.reserve(cz::heap_allocator(), 1);
                the_stroke_rects.push(stroke_rect);

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

        /////////////////////////////////////////
        // Waiting for connection screen
        /////////////////////////////////////////
        if (!the_run) {
            const SDL_Color bg = {0xdd, 0xdd, 0xdd};
            const SDL_Color fg = {0x00, 0x00, 0x00};

            Size_Cache* menu_font = open_font(&rend, font_path, (int)(wfc_font_size * dpi_scale));
            if (!menu_font) {
                fprintf(stderr, "TTF_OpenFont failed: %s\n", SDL_GetError());
                return 1;
            }

            SDL_Rect plane_rect = {0, header_height, surface->w, surface->h - header_height};
            SDL_SetClipRect(surface, &plane_rect);

            cz::Str message1 = "WAITING FOR CONNECTION";
            cz::Str message2 = "...";

            // Draw bounding box.
            {
                int padding = 10;
                SDL_Rect rect = {
                    (surface->w - menu_font->font_width * (int)message1.len) / 2 - padding,
                    (surface->h - menu_font->font_height * 2) / 2 - padding,
                    menu_font->font_width * (int)message1.len + padding * 2,
                    menu_font->font_height * 2 + padding * 2,
                };
                SDL_FillRect(surface, &rect, SDL_MapRGB(surface->format, bg.r, bg.g, bg.b));
            }

            // Draw message1.
            {
                int64_t x = (surface->w - menu_font->font_width * message1.len) / 2;
                int64_t y = surface->h / 2 - menu_font->font_height;
                for (size_t i = 0; i < message1.len; ++i) {
                    char seq[5] = {(char)message1[i]};
                    (void)render_code_point(menu_font, surface, x, y, bg, fg, seq);
                    x += menu_font->font_width;
                }
            }

            // Draw message2.
            {
                int64_t x = (surface->w - menu_font->font_width * message2.len) / 2;
                int64_t y = surface->h / 2;
                int numticks = (SDL_GetTicks() % 2000 / 667 + 1);
                for (size_t i = 0; i < numticks; ++i) {
                    char seq[5] = {(char)message2[i]};
                    (void)render_code_point(menu_font, surface, x, y, bg, fg, seq);
                    x += menu_font->font_width;
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
