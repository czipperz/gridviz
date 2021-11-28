#include "server.hpp"

#include <cz/heap.hpp>

namespace gridviz {

///////////////////////////////////////////////////////////////////////////////
// Type Definitions
///////////////////////////////////////////////////////////////////////////////

struct Network_State {
    cz::String buffer;
    cz::Vector<Event> backlog;
};

///////////////////////////////////////////////////////////////////////////////
// Module Code - initialization
///////////////////////////////////////////////////////////////////////////////

Network_State* start_networking() {
    Network_State* net = cz::heap_allocator().alloc<Network_State>();
    *net = {};

    net->buffer.reserve(cz::heap_allocator(), 4096);

    // TODO remove dummy events
    net->backlog.reserve(cz::heap_allocator(), 3);
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
        net->backlog.push(event);

        event = {};
        event.cp.type = EVENT_CHAR_POINT;
        memcpy(event.cp.fg, white, sizeof(white));
        memcpy(event.cp.bg, black, sizeof(black));
        event.cp.ch = 'B';
        event.cp.x = 2;
        event.cp.y = 1;
        net->backlog.push(event);

        event = {};
        event.cp.type = EVENT_CHAR_POINT;
        memcpy(event.cp.fg, white, sizeof(white));
        memcpy(event.cp.bg, black, sizeof(black));
        event.cp.ch = 'C';
        event.cp.x = 0;
        event.cp.y = 4;
        net->backlog.push(event);
    }

    return net;
}

///////////////////////////////////////////////////////////////////////////////
// Module Code - cleanup
///////////////////////////////////////////////////////////////////////////////

void stop_networking(Network_State* net) {
    net->buffer.drop(cz::heap_allocator());
    net->backlog.drop(cz::heap_allocator());
    cz::heap_allocator().dealloc(net);
}

///////////////////////////////////////////////////////////////////////////////
// Module Code - cleanup
///////////////////////////////////////////////////////////////////////////////

void poll_network(Network_State* net, cz::Vector<Event>* events) {
    events->reserve(cz::heap_allocator(), net->backlog.len);
    events->append(net->backlog);
    net->backlog.len = 0;
}

}
