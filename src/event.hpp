#pragma once

#include <stdint.h>
#include <cz/date.hpp>
#include <cz/str.hpp>
#include <cz/vector.hpp>

namespace gridviz {

enum Event_Type {
    EVENT_CHAR_POINT,
};

union Event {
    uint8_t type;
    struct {
        uint8_t type;
        uint8_t fg[3];
        uint8_t bg[3];
        uint8_t ch;
        int64_t x, y;
    } cp;
};

struct Stroke {
    cz::Str title;
    cz::Vector<Event> events;
};

struct Run_Info {
    cz::Vector<Stroke> strokes;
    // TODO pull out graphical stuff
    size_t selected_stroke;
    int64_t off_x;
    int64_t off_y;
    int font_size;
    cz::Date start_time;
    float zoom = 1.0f;
};

struct Game_State {
    cz::Vector<Run_Info> runs;
    size_t selected_run;
};

}
