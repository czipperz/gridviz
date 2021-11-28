#include "server.hpp"

#include <cz/heap.hpp>

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
static void actually_poll_server(Network_State* net);

static int winsock_start(void);
static int winsock_end(void);
static int make_non_blocking(SOCKET socket);

///////////////////////////////////////////////////////////////////////////////
// Type Definitions
///////////////////////////////////////////////////////////////////////////////

struct Network_State {
    bool running;
    cz::String buffer;

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

void poll_network(Network_State* net, cz::Vector<Event>* events) {
    actually_poll_server(net);

    while (1) {
        if (net->buffer.len < sizeof(Event))
            break;
        net->buffer.remove_range(0, sizeof(Event));

        Event event;
        memcpy(&event, net->buffer.buffer, sizeof(Event));

        events->reserve(cz::heap_allocator(), 1);
        switch (event.type) {
        case EVENT_CHAR_POINT:
            events->push(event);
            break;
        case EVENT_KEY_FRAME:
            CZ_PANIC("todo");
            break;
        default:
            CZ_PANIC("invalid message type");
            break;
        }
    }
}

static void actually_poll_server(Network_State* net) {
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

        net->socket_client = client;
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
