#include "server.hpp"

#include <cz/binary_search.hpp>
#include <cz/format.hpp>
#include <cz/heap.hpp>

#define NETGRIDVIZ_DEFINE_PROTOCOL
#include "../netgridviz.h"

#include "event.hpp"

///////////////////////////////////////////////////////////////////////////////
// Xplat craziness
///////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#else
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#ifdef _WIN32
#define ssize_t int
#define socklen_t int
#define len_t int
#else
#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket close
#define len_t size_t
#endif

namespace gridviz {

static int actually_start_server(Network_State* net, int port);
static void actually_poll_server(Network_State* net, Game_State* game);

static int winsock_start(void);
static int winsock_end(void);
static int make_non_blocking(SOCKET socket);

///////////////////////////////////////////////////////////////////////////////
// Type Definitions
///////////////////////////////////////////////////////////////////////////////

struct Network_State {
    bool running;
    cz::String buffer;

    cz::Vector<netgridviz_context> contexts;
    bool reuse_first_stroke;

    SOCKET socket_server = INVALID_SOCKET;
    SOCKET socket_client = INVALID_SOCKET;
};

///////////////////////////////////////////////////////////////////////////////
// Module Code - initialization
///////////////////////////////////////////////////////////////////////////////

Network_State* start_networking(int port) {
    Network_State* net = cz::heap_allocator().alloc<Network_State>();
    *net = {};

    int result = actually_start_server(net, port);
    // TODO report result < 0 somehow???

    net->buffer.reserve(cz::heap_allocator(), 4096);
    return net;
}

static int actually_start_server(Network_State* net, int port) {
    struct sockaddr_in address;

    if (net->running)
        return 0;

    int result = winsock_start();
    if (result != 0)
        return -1;

    net->socket_server = socket(AF_INET, SOCK_STREAM, 0);
    if (net->socket_server == INVALID_SOCKET)
        goto error;

    // Reuse port to allow for quick restarting.
    {
        int opt = 1;
        result = setsockopt(net->socket_server, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt,
                            sizeof(opt));
        if (result < 0) {
            closesocket(net->socket_server);
            goto error;
        }
    }

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    result = bind(net->socket_server, (sockaddr*)&address, sizeof(address));
    if (result == SOCKET_ERROR) {
        closesocket(net->socket_server);
        goto error;
    }

    result = make_non_blocking(net->socket_server);
    if (result == SOCKET_ERROR) {
        closesocket(net->socket_server);
        goto error;
    }

    result = listen(net->socket_server, SOMAXCONN);
    if (result == SOCKET_ERROR) {
        closesocket(net->socket_server);
        goto error;
    }

    net->running = true;
    return 1;

error:
    net->socket_server = INVALID_SOCKET;
    return -1;
}

///////////////////////////////////////////////////////////////////////////////
// Module Code - cleanup
///////////////////////////////////////////////////////////////////////////////

void stop_networking(Network_State* net) {
    winsock_end();
    net->buffer.drop(cz::heap_allocator());
    cz::heap_allocator().dealloc(net);
}

///////////////////////////////////////////////////////////////////////////////
// Module Code - cleanup
///////////////////////////////////////////////////////////////////////////////

static size_t get_event_length(uint8_t type, cz::Str buffer);
static netgridviz_context* lookup_context(Network_State* net, uint16_t context_id);

void poll_network(Network_State* net, Game_State* game) {
    actually_poll_server(net, game);

    while (1) {
        if (net->buffer.len == 0)
            break;

        uint8_t type = 0;
        memcpy(&type, net->buffer.buffer, 1);

        size_t length = get_event_length(type, net->buffer);
        if (net->buffer.len < length)
            break;

        uint16_t context_id = 0;
        memcpy(&context_id, net->buffer.buffer + 1, 2);
        netgridviz_context* context = lookup_context(net, context_id);

        Run_Info* the_run = &game->runs.last();

        switch (type) {
        case GRIDVIZ_SET_FG:
            memcpy(&context->fg, net->buffer.buffer + 3, 3);
            break;
        case GRIDVIZ_SET_BG:
            memcpy(&context->bg, net->buffer.buffer + 3, 3);
            break;

        case GRIDVIZ_START_STROKE: {
            if (net->reuse_first_stroke) {
                net->reuse_first_stroke = false;
            } else {
                the_run->strokes.reserve(cz::heap_allocator(), 1);
                the_run->strokes.push({});

                // TODO pull out graphical stuff
                if (the_run->selected_stroke == the_run->strokes.len - 1)
                    the_run->selected_stroke = the_run->strokes.len;
            }

            cz::Str title;
            if (length == 5) {
                title = cz::format(cz::heap_allocator(), "Stroke ", the_run->strokes.len - 1);
            } else {
                title = net->buffer.slice(5, length).clone(cz::heap_allocator());
            }
            the_run->strokes.last().title = title;
        } break;

        case GRIDVIZ_SEND_CHAR: {
            net->reuse_first_stroke = false;

            int64_t x = 0, y = 0;
            char ch = 0;
            memcpy(&x, net->buffer.buffer + 3, sizeof(x));
            memcpy(&y, net->buffer.buffer + 11, sizeof(y));
            memcpy(&ch, net->buffer.buffer + 19, sizeof(ch));

            Event event = {};
            event.cp.type = EVENT_CHAR_POINT;
            memcpy(event.cp.fg, context->fg, sizeof(context->fg));
            memcpy(event.cp.bg, context->bg, sizeof(context->bg));
            event.cp.ch = ch;
            event.cp.x = x;
            event.cp.y = y;

            Stroke* stroke = &the_run->strokes.last();
            stroke->events.reserve(cz::heap_allocator(), 1);
            stroke->events.push(event);
        } break;

        default:
            CZ_PANIC("invalid message type");
            break;
        }

        net->buffer.remove_range(0, length);
    }
}

static size_t get_event_length(uint8_t type, cz::Str buffer) {
    switch (type) {
    case GRIDVIZ_SET_FG:
    case GRIDVIZ_SET_BG:
        return 6;
    case GRIDVIZ_START_STROKE: {
        if (buffer.len < 5)
            return 5;
        uint32_t title_len;
        memcpy(&title_len, buffer.buffer + 1, sizeof(title_len));
        return 5 + (size_t)title_len;
    }
    case GRIDVIZ_SEND_CHAR:
        return 20;
    default:
        CZ_PANIC("invalid message type");
        break;
    }
}

static int64_t compare_contexts(const netgridviz_context& left, const netgridviz_context& right) {
    return (int64_t)left.id - (int64_t)right.id;
}

static netgridviz_context* lookup_context(Network_State* net, uint16_t context_id) {
    // Use binary search because we want to handle crazy id number inputs without mallocing
    // a rediculous amount of memory.  Performance of this function isn't that important.
    size_t index;
    netgridviz_context fake_context = {};
    fake_context.id = context_id;
    if (!cz::binary_search(net->contexts.as_slice(), fake_context, &index, compare_contexts)) {
        net->contexts.reserve(cz::heap_allocator(), 1);
        net->contexts.insert(index, netgridviz_make_context(context_id));
    }

    return &net->contexts[index];
}

static void actually_poll_server(Network_State* net, Game_State* game) {
    if (!net->running) {
        return;
    }

    if (net->socket_client != INVALID_SOCKET) {
        // Reserve 2048 + 1 so the first time we get to 4k but we don't loop and bump to 8k.
        net->buffer.reserve(cz::heap_allocator(), 2049);

        ssize_t result =
            recv(net->socket_client, net->buffer.end(), (len_t)net->buffer.remaining(), 0);
        if (result > 0) {
            net->buffer.len += result;
        } else if (result == 0) {
            closesocket(net->socket_client);
            net->socket_client = INVALID_SOCKET;
        } else {
            // Ignore errors.
        }
    } else {
        SOCKET client = accept(net->socket_server, nullptr, nullptr);
        if (client == INVALID_SOCKET)
            return;

        int result = make_non_blocking(client);
        if (result == SOCKET_ERROR) {
            closesocket(client);
            return;
        }

        // Start a new client connection.
        net->socket_client = client;
        net->contexts.len = 0;
        net->reuse_first_stroke = true;

        // Create a new run and select it.
        Run_Info the_run = {};
        the_run.strokes.reserve(cz::heap_allocator(), 1);
        the_run.strokes.push({"Stroke 0"});
        // TODO pull out graphical stuff
        the_run.selected_stroke = 1;
        the_run.font_size = 14;
        game->runs.reserve(cz::heap_allocator(), 1);
        game->runs.push(the_run);
        game->selected_run = game->runs.len - 1;
    }
}

///////////////////////////////////////////////////////////////////////////////
// Utility
///////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32
/// Winsock requires a global variable to store state.
static WSADATA winsock_global;
#endif

static int winsock_start() {
#ifdef _WIN32
    WORD winsock_version = MAKEWORD(2, 2);
    return WSAStartup(winsock_version, &winsock_global);
#else
    return 0;
#endif
}

static int winsock_end() {
#ifdef _WIN32
    return WSACleanup();
#else
    return 0;
#endif
}

static int make_non_blocking(SOCKET socket) {
#ifdef _WIN32
    long cmd = FIONBIO;  // FIONBIO = File-IO-Non-Blocking-IO.
    u_long enabled = true;
    return ioctlsocket(socket, cmd, &enabled);
#else
    int flags = fcntl(socket, F_GETFL);
    if (flags < 0)
        return flags;
    return fcntl(socket, F_SETFL, flags | O_NONBLOCK);
#endif
}

}
