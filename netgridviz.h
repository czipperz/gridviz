#ifndef NETGRIDVIZ_HEADER_GUARD
#define NETGRIDVIZ_HEADER_GUARD

#include <stdint.h>

/// This is an all in one header file for netgridviz.
///
/// To enable the implementation use: `#define NETGRIDVIZ_DEFINE`.

///////////////////////////////////////////////////////////////////////////////
// Header
///////////////////////////////////////////////////////////////////////////////

typedef struct netgridviz_context {
    uint16_t id;

    // READ-ONLY!!! You must call the setters to access these variables!!!
    uint8_t fg[3];
    uint8_t bg[3];
} netgridviz_context;

#define NETGRIDVIZ_DEFAULT_PORT 41088

/// Connect on the given port to the server.  Returns `0` on success, `-1` on failure.
int netgridviz_connect(int port);
/// Disconnect from the server.
void netgridviz_disconnect(void);

/// Make a new context.
netgridviz_context netgridviz_create_context(void);
netgridviz_context netgridviz_make_context(uint16_t id);

/// Set colors.
void netgridviz_set_fg(netgridviz_context* context, uint8_t r, uint8_t g, uint8_t b);
void netgridviz_set_bg(netgridviz_context* context, uint8_t r, uint8_t g, uint8_t b);

/// Send a charater.
void netgridviz_send_char(netgridviz_context* context, int64_t x, int64_t y, char ch);

#endif  // NETGRIDVIZ_HEADER_GUARD

///////////////////////////////////////////////////////////////////////////////
// Implementation
///////////////////////////////////////////////////////////////////////////////

#if defined(NETGRIDVIZ_DEFINE) || defined(NETGRIDVIZ_DEFINE_PROTOCOL)
#define GRIDVIZ_SET_FG 1
#define GRIDVIZ_SET_BG 2
#define GRIDVIZ_SEND_CHAR 3

netgridviz_context netgridviz_make_context(uint16_t id) {
    netgridviz_context context = {id};
    // White foreground.
    context.fg[0] = 0x00;
    context.fg[1] = 0x00;
    context.fg[2] = 0x00;
    // Black foreground.
    context.bg[1] = 0xff;
    context.bg[0] = 0xff;
    context.bg[2] = 0xff;
    return context;
}
#endif

#ifdef NETGRIDVIZ_DEFINE

#include <stdio.h>  // print error on lose connection

///////////////////////////////////////////////////////////////////////////////
// Module Code - connection
///////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////
// Xplat craziness
/////////////////////////////////////////////////

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

/////////////////////////////////////////////////
// Module Data
/////////////////////////////////////////////////

static SOCKET netgridviz_socket = INVALID_SOCKET;

#ifdef _WIN32
/// Winsock requires a global variable to store state.
static WSADATA netgridviz_winsock_global;
#endif

/////////////////////////////////////////////////
// Forward Declarations
/////////////////////////////////////////////////

static int netgridviz_winsock_start(void);
static int netgridviz_winsock_end(void);
static int netgridviz_make_non_blocking(SOCKET sock);

static int netgridviz_client_connect_sock(SOCKET sock, int port);
static int netgridviz_connect_timeout(SOCKET sock,
                                      const struct sockaddr* addr,
                                      socklen_t len,
                                      struct timeval* timeout);

/////////////////////////////////////////////////
// Module Code - connect to server
/////////////////////////////////////////////////

int netgridviz_connect(int port) {
    int result = netgridviz_winsock_start();
    if (result != 0)
        return -1;

    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        netgridviz_winsock_end();
        return -1;
    }

    result = netgridviz_client_connect_sock(sock, port);

    if (result < 0) {
        closesocket(sock);
        netgridviz_winsock_end();
        return -1;
    }

    netgridviz_socket = sock;

    return 0;
}

/////////////////////////////////////////////////
// Module Code - disconnect from server
/////////////////////////////////////////////////

void netgridviz_disconnect(void) {
    closesocket(netgridviz_socket);
    netgridviz_winsock_end();
}

/////////////////////////////////////////////////
// Module Code - connect to server on an open socket
/////////////////////////////////////////////////

static int netgridviz_client_connect_sock(SOCKET sock, int port) {
    int result = netgridviz_make_non_blocking(sock);
    if (result == SOCKET_ERROR)
        return -1;

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(port);

    struct timeval timeout = {0};
    timeout.tv_sec = 0;
    timeout.tv_usec = 500000;
    result =
        netgridviz_connect_timeout(sock, (struct sockaddr*)&address, sizeof(address), &timeout);
    if (result == SOCKET_ERROR)
        return -1;

    return 0;
}

/////////////////////////////////////////////////
// Module Code - connect syscall with timeout
/////////////////////////////////////////////////

static int netgridviz_connect_timeout(SOCKET sock,
                                      const struct sockaddr* addr,
                                      socklen_t len,
                                      struct timeval* timeout) {
    int result = connect(sock, addr, len);
    if (result != SOCKET_ERROR)
        return 0;

#ifdef _WIN32
    int error = WSAGetLastError();
    if (error != WSAEWOULDBLOCK)
        return -1;
#else
    int error = errno;
    if (error != EINPROGRESS)
        return -1;
#endif

    fd_set set_write;
    FD_ZERO(&set_write);
    FD_SET(sock, &set_write);
    fd_set set_except;
    FD_ZERO(&set_except);
    FD_SET(sock, &set_except);

    result = select((int)(sock + 1), NULL, &set_write, &set_except, timeout);
    if (result <= 0)
        return -1;

    // Error connecting.
    if (FD_ISSET(sock, &set_except))
        return -1;

    // Success.
    return 0;
}

/////////////////////////////////////////////////
// Module Code - connect utility
/////////////////////////////////////////////////

static int netgridviz_winsock_start() {
#ifdef _WIN32
    WORD winsock_version = MAKEWORD(2, 2);
    return WSAStartup(winsock_version, &netgridviz_winsock_global);
#else
    return 0;
#endif
}

static int netgridviz_winsock_end() {
#ifdef _WIN32
    return WSACleanup();
#else
    return 0;
#endif
}

static int netgridviz_make_non_blocking(SOCKET socket) {
#ifdef _WIN32
    long cmd = FIONBIO;  // FIONBIO = File-IO-Non-Blocking-IO.
    u_long enabled = 1;
    return ioctlsocket(socket, cmd, &enabled);
#else
    int flags = fcntl(socket, F_GETFL);
    if (flags < 0)
        return flags;
    return fcntl(socket, F_SETFL, flags | O_NONBLOCK);
#endif
}

///////////////////////////////////////////////////////////////////////////////
// Module Code - contexts and messages
///////////////////////////////////////////////////////////////////////////////

static ssize_t netgridviz_send_raw(const void* buffer, size_t len) {
    return send(netgridviz_socket, (const char*)buffer, (len_t)len, 0);
}

static void netgridviz_lose_connection(void) {
    fprintf(stderr, "netgridviz: Connection to server lost\n");
    netgridviz_disconnect();
    netgridviz_socket = INVALID_SOCKET;
}

/////////////////////////////////////////////////
// Context manipulation
/////////////////////////////////////////////////

static uint16_t netgridviz_context_counter;

netgridviz_context netgridviz_create_context(void) {
    netgridviz_context_counter++;
    return netgridviz_make_context(netgridviz_context_counter);
}

void netgridviz_set_fg(netgridviz_context* context, uint8_t r, uint8_t g, uint8_t b) {
    if (netgridviz_socket == INVALID_SOCKET)
        return;

    context->fg[0] = r;
    context->fg[1] = g;
    context->fg[2] = b;

    uint8_t message[6] = {GRIDVIZ_SET_FG};
    memcpy(message + 1, &context->id, sizeof(context->id));
    memcpy(message + 3, &context->fg[0], sizeof(context->fg));

    ssize_t sent = netgridviz_send_raw(&message[0], sizeof(message));
    if (sent != sizeof(message))
        netgridviz_lose_connection();
}

void netgridviz_set_bg(netgridviz_context* context, uint8_t r, uint8_t g, uint8_t b) {
    if (netgridviz_socket == INVALID_SOCKET)
        return;

    context->bg[0] = r;
    context->bg[1] = g;
    context->bg[2] = b;

    uint8_t message[6] = {GRIDVIZ_SET_BG};
    memcpy(message + 1, &context->id, sizeof(context->id));
    memcpy(message + 3, &context->bg[0], sizeof(context->bg));

    ssize_t sent = netgridviz_send_raw(&message[0], sizeof(message));
    if (sent != sizeof(message))
        netgridviz_lose_connection();
}

void netgridviz_send_char(netgridviz_context* context, int64_t x, int64_t y, char ch) {
    if (netgridviz_socket == INVALID_SOCKET)
        return;

    uint8_t message[20] = {GRIDVIZ_SEND_CHAR};
    memcpy(message + 1, &context->id, sizeof(context->id));
    memcpy(message + 3, &x, sizeof(x));
    memcpy(message + 11, &y, sizeof(y));
    memcpy(message + 19, &ch, sizeof(ch));

    ssize_t sent = netgridviz_send_raw(&message[0], sizeof(message));
    if (sent != sizeof(message))
        netgridviz_lose_connection();
}

#endif  // NETGRIDVIZ_DEFINE
