#pragma once
#include "config.hpp"
#include "errors.hpp"

#include <array>
#include <memory>
#ifdef __MINGW32__
#include <_bsd_types.h>
#include <psdk_inc/_ip_types.h>
#endif
#include <string>

///
/// Common API for network functions, intended to hide platform specific
/// implementation details, such as windows typedefs.
///

#ifdef _MSC_VER
typedef int ssize_t;
#endif

struct sockaddr;
struct addrinfo;

namespace network {

enum class SOCK_TYPE : std::int32_t { SOCK_STREAM = 0, SOCK_DGRAM, COUNT };
enum class SOCK_OPT : std::int32_t {
  REUSEADDR = 0,
  SNDTIMEO,
  RCVTIMEO,
  NODELAY,
  COUNT
};
enum class SOCK_LEVEL : std::int32_t { L_SOL_SOCKET = 0, L_IPPROTO_TCP, COUNT };

#ifdef _WIN32
typedef unsigned int socket_t;
#define INVALID_SOCKET (socket_t)(~0)
#ifndef _WINSOCKAPI_
// wsock2.h
#define AF_UNSPEC 0
#define SOCK_STREAM 1
#define AF_INET 2
#define SOMAXCONN 0x7fffffff
constexpr int ETIMEOUT = 10060;
constexpr int SO_REUSEADDR = 0x0004;
// ws2tcpip.h
#define AI_PASSIVE 0x00000001
#endif
constexpr socket_t BAD_SOCKET = INVALID_SOCKET;
typedef char* recv_buf_t;
typedef const char* send_buf_t;
typedef ::sockaddr sockaddr;

// typedef ::addrinfo addrinfo;
#elif __linux__
typedef int socket_t;
#define INVALID_SOCKET (socket_t)(~0)
typedef unsigned int DWORD;
typedef ::sockaddr sockaddr;
typedef void* recv_buf_t;
typedef const void* send_buf_t;
typedef int socket_t;

#define SOMAXCONN 0x7fffffff
constexpr int ETIMEOUT = 10060;
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
};

void Deinit();

///
/// Wait and accept incoming connections on a socket. On success, return ERR_OK
/// and store result socket in dest. In case of no connections within specified
/// timeout (milliseconds), return ERR_TIMEOUT. Otherwise return ERR_UNKNOWN.
///
socket_error accept_connection(socket_t s, socket_t* dest, long int timeout);
socket_error bind(socket_t s, const sockaddr* addr, int namelen);
socket_error connect(socket_t s, const network::sockaddr* name, int namelen);
void getaddrinfo(const std::string host, std::string port, addrinfo* hints,
                 addrinfo** result);
std::unique_ptr<addrinfo, void (*)(addrinfo*)> agetaddrinfo(
    const std::string host, std::string port, addrinfo* hints);
void freeaddrinfo(addrinfo* info);
void closesocket(socket_t s);
socket_error listen(socket_t s, int backlog);
ssize_t recv(socket_t s, void* buffer, const size_t length, int flags);
ssize_t send(socket_t s, const void* buffer, const size_t length, int flags);
int setsockopt(socket_t s, SOCK_LEVEL, SOCK_OPT optname, const char* optval,
               int optlen);
///
/// Set timeout for send/receive, in milliseconds.
///
void set_io_timeout(socket_t s, const unsigned long timeout,
                    const bool nodelay = true);
///
/// Create socket from given addrinfo. If timeout is greater than zero, then it
/// will be set as send/recv timeout (milliseconds).
///
socket_t socket(addrinfo* ptr,
                const unsigned long timeout = cfg::SOCKET_SR_TIMEOUT);
int get_last_network_error();
int shutdown(socket_t s, int how);
};  // namespace network
