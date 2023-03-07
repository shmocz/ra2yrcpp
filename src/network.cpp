#include "network.hpp"

#include "errors.hpp"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#elif __linux__
#include <cerrno>
#include <netdb.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

using namespace network;

constexpr int SOCK_ERR = SOCKET_ERROR;

auto get_sock_opt(SOCK_OPT s) {
  std::array<decltype(SO_REUSEADDR), static_cast<unsigned>(SOCK_OPT::COUNT)> a =
      {SO_REUSEADDR, SO_SNDTIMEO, SO_RCVTIMEO, TCP_NODELAY};
  return a[static_cast<unsigned>(s)];
}

auto get_sock_level(SOCK_LEVEL s) {
  static constexpr std::array<decltype(SOL_SOCKET),
                              static_cast<unsigned>(SOCK_LEVEL::COUNT)>
      a = {SOL_SOCKET, IPPROTO_TCP};
  return a[static_cast<unsigned>(s)];
}

Init::Init() {
#ifdef _WIN32
  static WSADATA data;
  WSAStartup(MAKEWORD(2, 2), &data);
#endif
}

void network::Deinit() {
#ifdef _WIN32
  (void)WSACleanup();
#endif
}

socket_error network::accept_connection(socket_t s, socket_t* dest,
                                        long int timeout) {
  fd_set rfds;
  FD_ZERO(&rfds);
  FD_SET(s, &rfds);
  auto tsec = timeout / 1000;
  timeval tv{.tv_sec = tsec, .tv_usec = (timeout - tsec * 1000) * 1000};
  int res = select(s + 1, &rfds, NULL, NULL, &tv);
  if (res == SOCKET_ERROR) {
    return network::ERR_UNKNOWN;
  }
  if (res < 1) {
    return network::ERR_TIMEOUT;
  }
  if ((*dest = accept(s, NULL, NULL)) == INVALID_SOCKET) {
    return network::ERR_UNKNOWN;
  }
  return network::ERR_OK;
}

socket_error network::bind(socket_t s, const network::sockaddr* addr,
                           int namelen) {
  if (::bind(s, addr, namelen) == SOCK_ERR) {
    return network::ERR_UNKNOWN;
  }
  return network::ERR_OK;
}

socket_error network::connect(socket_t s, const network::sockaddr* name,
                              int namelen) {
  if (!::connect(s, name, namelen)) {
    return ERR_OK;
  }
  return ERR_UNKNOWN;
}

void network::set_io_timeout(socket_t s, const unsigned long timeout,
                             const bool nodelay) {
#ifdef _WIN32
  auto p = timeout;
  auto* pt = reinterpret_cast<const char*>(&p);
  // on linux, the input is astruct timeval
#elif __linux__
  auto tsec = timeout / 1000;
  timeval p{.tv_sec = tsec, .tv_usec = (timeout - tsec * 1000) * 1000};
  // timeval p{timeout, 0};
  auto* pt = reinterpret_cast<const char*>(&p);
#endif
  setsockopt(s, SOCK_LEVEL::L_SOL_SOCKET, SOCK_OPT::SNDTIMEO, pt, sizeof(p));
  setsockopt(s, SOCK_LEVEL::L_SOL_SOCKET, SOCK_OPT::RCVTIMEO, pt, sizeof(p));
  if (nodelay) {
    int e = 1;
    setsockopt(s, SOCK_LEVEL::L_IPPROTO_TCP, SOCK_OPT::NODELAY,
               reinterpret_cast<const char*>(&e), sizeof(e));
  }
}

// TODO: don't throw exceptions inside these
socket_t network::socket(addrinfo* ptr, const unsigned long timeout) {
  auto s = ::socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
  if (s == BAD_SOCKET) {
    throw yrclient::system_error("socket()");
  }
  if (timeout > 0) {
#if defined(_WIN32) || defined(__linux__)
    set_io_timeout(s, timeout);
#else
#error Not implemented
#endif
  }
  return s;
}

void network::getaddrinfo(const std::string host, std::string port,
                          addrinfo* hints, addrinfo** result) {
  const char* p = (host.size() > 0) ? host.c_str() : NULL;
  if (::getaddrinfo(p, port.c_str(), reinterpret_cast<addrinfo*>(hints),
                    reinterpret_cast<addrinfo**>(result)) != 0) {
    throw yrclient::system_error("getaddrinfo()");
  }
}

std::unique_ptr<addrinfo, void (*)(addrinfo*)> network::agetaddrinfo(
    const std::string host, std::string port, addrinfo* hints) {
  addrinfo* result = nullptr;
  getaddrinfo(host, port, hints, &result);
  return std::unique_ptr<addrinfo, void (*)(addrinfo*)>(
      result, [](addrinfo* a) { network::freeaddrinfo(a); });
}

void network::freeaddrinfo(addrinfo* info) { ::freeaddrinfo(info); }

void network::closesocket(socket_t s) {
#ifdef _WIN32
  if (::closesocket(s) == SOCK_ERR) {
#elif __linux__
  if (close(s) == SOCK_ERR) {
#else
#error Not implemented
#endif
    throw yrclient::system_error("closesocket()");
  }
}

socket_error network::listen(socket_t s, int backlog) {
  if (::listen(s, backlog) == SOCK_ERR) {
    return network::ERR_UNKNOWN;
  }
  return network::ERR_OK;
}

ssize_t network::recv(socket_t s, void* buffer, const size_t length,
                      int flags) {
  return ::recv(s, static_cast<recv_buf_t>(buffer), length, flags);
}

ssize_t network::send(socket_t s, const void* buffer, const size_t length,
                      int flags) {
  return ::send(s, static_cast<send_buf_t>(buffer), length, flags);
}

int network::setsockopt(socket_t s, SOCK_LEVEL level, SOCK_OPT optname,
                        const char* optval, int optlen) {
  int i = ::setsockopt(s, get_sock_level(level), get_sock_opt(optname), optval,
                       optlen);
  if (i == -1) {
    throw yrclient::system_error("setsockopt()");
  }
  return i;
}

int network::get_last_network_error() {
#ifdef _WIN32
  return WSAGetLastError();
#elif __linux__
  return errno;
#else
#error Not implemented
#endif
}

int network::shutdown(socket_t s, int how) {
#if defined(_WIN32) || defined(__linux__)
  return ::shutdown(s, how);
#else
#error Not implemented
#endif
}
