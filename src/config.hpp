#pragma once
namespace cfg {
constexpr unsigned int ACCEPT_TIMEOUT_MS = 250;
constexpr unsigned SOCKET_SR_TIMEOUT = 5000;
constexpr unsigned int DEFAULT_BUFLEN = 4096;
constexpr unsigned int MAX_CLIENTS = 16;
constexpr unsigned int SERVER_PORT = 14521;
constexpr unsigned int MAX_MESSAGE_LENGTH = 1e7;
constexpr unsigned int POLL_BLOCKING_TIMEOUT_MS = 2500;
};  // namespace cfg
