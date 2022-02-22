#pragma once
#include "config.hpp"
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#endif
#include <string>

///
/// Common API for network functions, intended to hide platform specific
/// implementation details, such as windows typedefs.
///
namespace network {
#ifdef _WIN32
typedef SOCKET socket_t;
constexpr socket_t BAD_SOCKET = INVALID_SOCKET;
typedef char* recv_buf_t;
typedef const char* send_buf_t;
#elif __linux__
typedef void* recv_buf_t;
typedef const void* send_buf_t;
typedef int socket_t;
constexpr socket_t BAD_SOCKET = -1;
constexpr socket_t SOCKET_ERROR = -1;
#else
#error Not implemented
#endif

enum socket_error { ERR_OK = 0, ERR_TIMEOUT = 1, ERR_UNKNOWN = 2 };

///
/// Initialize network library. On WIN32, this calls WSAStartup.
///
struct Init {
  Init();
#ifdef _WIN32
  WSADATA data;
#endif
};

///
/// Wait and accept incoming connections on a socket. On success, return ERR_OK
/// and store result socket in dest. In case of no connections within specified
/// timeout (milliseconds), return ERR_TIMEOUT. Otherwise return ERR_UNKNOWN.
///
socket_error accept_connection(socket_t s, socket_t* dest, long int timeout);
socket_error bind(socket_t s, const sockaddr* addr, int namelen);
socket_error connect(socket_t s, const sockaddr* name, int namelen);
void getaddrinfo(const std::string host, std::string port, addrinfo* hints,
                 addrinfo** result);
void closesocket(socket_t s);
socket_error listen(socket_t s, int backlog);
ssize_t recv(socket_t s, void* buffer, const size_t length, int flags);
ssize_t send(socket_t s, const void* buffer, const size_t length, int flags);
int setsockopt(socket_t s, int level, int optname, const char* optval,
               int optlen);
///
/// Create socket from given addrinfo. If timeout is greater than zero, then it
/// will be set as send/recv timeout (milliseconds).
///
socket_t socket(addrinfo* ptr,
                const unsigned long timeout = cfg::SOCKET_SR_TIMEOUT);
};  // namespace network
