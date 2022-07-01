#include "debug_helpers.h"
#include "errors.hpp"
#include "network.hpp"
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

using namespace network;

constexpr int SOCK_ERR = SOCKET_ERROR;

Init::Init() {
#ifdef _WIN32
  static WSADATA data;
  WSAStartup(MAKEWORD(2, 2), &data);
#endif
}

socket_error network::accept_connection(socket_t s, socket_t* dest,
                                        long int timeout) {
  fd_set rfds;
  FD_ZERO(&rfds);
  FD_SET(s, &rfds);
  auto tsec = timeout / 1000;
  timeval tv{.tv_sec = tsec, .tv_usec = (timeout - tsec * 1000) * 1000};
  int res = select(1, &rfds, NULL, NULL, &tv);
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

void network::set_io_timeout(socket_t s, const unsigned long timeout) {
  auto* pt = reinterpret_cast<const char*>(&timeout);
  setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, pt, sizeof(timeout));
  setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, pt, sizeof(timeout));
  DPRINTF("sock=%d,timeout=%d\n", s, timeout);
}

// TODO: don't throw exceptions inside these
socket_t network::socket(addrinfo* ptr, const unsigned long timeout) {
  auto s = ::socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
  if (s == BAD_SOCKET) {
    throw yrclient::system_error("socket()");
  }
  if (timeout > 0) {
#ifdef _WIN32
    set_io_timeout(s, timeout);
#else
#error Not implemented
#endif
  }
  return s;
}

void network::getaddrinfo(const std::string host, std::string port,
                          addrinfo* hints, addrinfo** result) {
  int res;
  const char* p = (host.size() > 0) ? host.c_str() : NULL;
  if ((res =
           ::getaddrinfo(p, port.c_str(), reinterpret_cast<::addrinfo*>(hints),
                         reinterpret_cast<::addrinfo**>(result))) != 0) {
    throw yrclient::system_error("getaddrinfo()");
  }
}

void network::freeaddrinfo(addrinfo* info) {
  ::freeaddrinfo(reinterpret_cast<::addrinfo*>(info));
}

void network::closesocket(socket_t s) {
  DPRINTF("closing %d\n", s);
  if (::closesocket(s) == SOCK_ERR) {
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

int network::setsockopt(socket_t s, int level, int optname, const char* optval,
                        int optlen) {
  return ::setsockopt(s, level, optname, optval, optlen);
}
