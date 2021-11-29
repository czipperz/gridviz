#pragma once

#include <stdint.h>
#include <cz/str.hpp>
#include <cz/vector.hpp>

namespace gridviz {

enum Event_Type {
    EVENT_CHAR_POINT,
    EVENT_KEY_FRAME,
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
    struct {
        uint8_t type;
        uint64_t padding;
        cz::Str message;
    } kf;
};

struct Stroke {
    cz::Str title;
    cz::Vector<Event> events;
};

struct Run_Info {
    cz::Vector<Stroke> strokes;
    size_t selected_stroke;
    int64_t off_x;
    int64_t off_y;
};

struct Game_State {
    cz::Vector<Run_Info> runs;
    size_t selected_run;
};

}
