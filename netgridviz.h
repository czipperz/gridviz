#ifndef NETGRIDVIZ_HEADER_GUARD
#define NETGRIDVIZ_HEADER_GUARD

#include <stdint.h>

/// This is an all in one header file for netgridviz.
///
/// To enable the implementation use: `#define NETGRIDVIZ_DEFINE`.

///////////////////////////////////////////////////////////////////////////////
// Header
///////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////
// Type Definitions
/////////////////////////////////////////////////

typedef struct netgridviz_context {
    uint16_t id;

    // READ-ONLY!!! You must call the setters to access these variables!!!
    uint8_t fg[3];
    uint8_t bg[3];
} netgridviz_context;

#define NETGRIDVIZ_DEFAULT_PORT 41088

/////////////////////////////////////////////////
// Connection
/////////////////////////////////////////////////

/// Connect on the given port to the server.  Returns `0` on success, `-1` on failure.
int netgridviz_connect(int port);
/// Disconnect from the server.
void netgridviz_disconnect(void);

/////////////////////////////////////////////////
// Context
/////////////////////////////////////////////////

/// Create a new context.  You can have multiple contexts and .
netgridviz_context netgridviz_create_context(void);
netgridviz_context netgridviz_make_context(uint16_t id);

/// Set colors.
void netgridviz_set_fg(netgridviz_context* context, uint8_t r, uint8_t g, uint8_t b);
void netgridviz_set_bg(netgridviz_context* context, uint8_t r, uint8_t g, uint8_t b);

/////////////////////////////////////////////////
// Draw Commands
/////////////////////////////////////////////////

/// Draw commands are issued as part of the current stroke.
/// If there is no stroke then creates a new stroke just for this command.

/// Start a stroke (a series of draw commands that are one "undo/redo"
/// unit).  Title is optional (null or empty string will count as no input).
void netgridviz_start_stroke(const char* title);
void netgridviz_end_stroke(void);

/// Draw a character.
void netgridviz_draw_char(netgridviz_context* context, int64_t x, int64_t y, char ch);

/// Draw a string.
void netgridviz_draw_string(netgridviz_context* context, int64_t x, int64_t y, const char* string);
void netgridviz_draw_fmt(netgridviz_context* context,
                         int64_t x,
                         int64_t y,
                         const char* format,
                         ...);
void netgridviz_draw_vfmt(netgridviz_context* context,
                          int64_t x,
                          int64_t y,
                          const char* format,
                          va_list args);

#endif  // NETGRIDVIZ_HEADER_GUARD

///////////////////////////////////////////////////////////////////////////////
// Implementation
///////////////////////////////////////////////////////////////////////////////

#if defined(NETGRIDVIZ_DEFINE) || defined(NETGRIDVIZ_DEFINE_PROTOCOL)
#define GRIDVIZ_SET_FG 1
#define GRIDVIZ_SET_BG 2
#define GRIDVIZ_START_STROKE 3
#define GRIDVIZ_SEND_CHAR 4

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

#include <stdio.h>   // print error on lose connection
#include <stdlib.h>  // malloc
#include <string.h>  // strlen

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
// Module Code - Utility
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
// Module Code - Context
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

/////////////////////////////////////////////////
// Module Code - Draw Commands
/////////////////////////////////////////////////

/// Note: using `uint8_t` instead of `bool` so C compliant.
static uint8_t netgridviz_has_stroke;

void netgridviz_start_stroke(const char* title) {
    netgridviz_has_stroke = 1;

    if (!title)
        title = "";

    // Truncate message to `UINT32_MAX`.
    size_t len_s = strlen(title);
    uint32_t len = ((uint64_t)len_s > (uint64_t)UINT32_MAX ? UINT32_MAX : (uint32_t)len_s);

    uint8_t message[5] = {GRIDVIZ_START_STROKE};
    memcpy(message + 1, &len, sizeof(len));

    ssize_t sent = netgridviz_send_raw(&message[0], sizeof(message));
    if (sent != sizeof(message)) {
        netgridviz_lose_connection();
        return;
    }

    // Send the title.
    if (len > 0) {
        ssize_t sent = netgridviz_send_raw(&title[0], len);
        if (sent != len)
            netgridviz_lose_connection();
    }
}

void netgridviz_end_stroke() {
    netgridviz_has_stroke = 0;
}

static void netgridviz_start_dummy_stroke(void) {
    uint8_t message[5] = {GRIDVIZ_START_STROKE};
    ssize_t sent = netgridviz_send_raw(&message[0], sizeof(message));
    if (sent != sizeof(message))
        netgridviz_lose_connection();
}

void netgridviz_draw_char(netgridviz_context* context, int64_t x, int64_t y, char ch) {
    if (netgridviz_socket == INVALID_SOCKET)
        return;

    if (!netgridviz_has_stroke)
        netgridviz_start_dummy_stroke();

    uint8_t message[20] = {GRIDVIZ_SEND_CHAR};
    memcpy(message + 1, &context->id, sizeof(context->id));
    memcpy(message + 3, &x, sizeof(x));
    memcpy(message + 11, &y, sizeof(y));
    memcpy(message + 19, &ch, sizeof(ch));

    ssize_t sent = netgridviz_send_raw(&message[0], sizeof(message));
    if (sent != sizeof(message))
        netgridviz_lose_connection();
}

void netgridviz_draw_string(netgridviz_context* context, int64_t x, int64_t y, const char* string) {
    uint8_t has_stroke = netgridviz_has_stroke;
    if (!has_stroke) {
        netgridviz_start_dummy_stroke();
        netgridviz_has_stroke = 1;
    }

    for (size_t i = 0; string[i] != '\0'; ++i) {
        netgridviz_draw_char(context, x, y, string[i]);
    }

    netgridviz_has_stroke = has_stroke;
}

void netgridviz_draw_fmt(netgridviz_context* context,
                         int64_t x,
                         int64_t y,
                         const char* format,
                         ...) {
    va_list args;
    va_start(args, format);
    netgridviz_draw_vfmt(context, x, y, format, args);
    va_end(args);
}

void netgridviz_draw_vfmt(netgridviz_context* context,
                          int64_t x,
                          int64_t y,
                          const char* format,
                          va_list args) {
    va_list args2;
    va_copy(args2, args);
    int result = snprintf(NULL, 0, format, args2);
    va_end(args2);

    if (result < 0)
        return;

    // Most of the time the message will fit in a 4k stack buffer.
    // If it doesn't then try to heap allocate a big enough buffer.
    // If heap allocation fails then fallback to the 4k stack buffer
    //     and truncate the result.
    if (result + 1 > 4096) {
        char* heap_buffer = (char*)malloc(result + 1);
        if (heap_buffer) {
            snprintf(heap_buffer, result + 1, format, args);
            netgridviz_draw_string(context, x, y, heap_buffer);
            free(heap_buffer);
            return;
        }
    }

    char stack_buffer[4096];
    snprintf(stack_buffer, sizeof(stack_buffer), format, args);
    netgridviz_draw_string(context, x, y, stack_buffer);
}

#endif  // NETGRIDVIZ_DEFINE
