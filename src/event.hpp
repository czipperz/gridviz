#pragma once

#include <stdint.h>
#include <cz/str.hpp>

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

}
