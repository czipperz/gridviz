#ifndef NETGRIDVIZ_HEADER_GUARD
#define NETGRIDVIZ_HEADER_GUARD

/// This is an all in one header file for netgridviz.
///
/// To enable the implementation use: `#define NETGRIDVIZ_DEFINE`.

///////////////////////////////////////////////////////////////////////////////
// Header
///////////////////////////////////////////////////////////////////////////////

#define NETGRIDVIZ_DEFAULT_PORT 41088

/// Connect on the given port to the server.  Returns `0` on success, `-1` on failure.
int netgridviz_connect(int port);
/// Disconnect from the server.
void netgridviz_disconnect(void);

/// Send data to the server.  Returns `len` on success, `-1`
/// on failure, somewhere inbetween on partial transfer.
int netgridviz_send(const void* buffer, size_t len);

#endif  // NETGRIDVIZ_HEADER_GUARD

///////////////////////////////////////////////////////////////////////////////
// Implementation
///////////////////////////////////////////////////////////////////////////////

#ifdef NETGRIDVIZ_DEFINE

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

static SOCKET netgridviz_socket;

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
// Module Code - send
/////////////////////////////////////////////////

int netgridviz_send(const void* buffer, size_t len) {
    return send(netgridviz_socket, (const char*)buffer, (len_t)len, 0);
}

/////////////////////////////////////////////////
// Module Code - utility
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

#endif  // NETGRIDVIZ_DEFINE
