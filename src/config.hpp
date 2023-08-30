#pragma once

#include "types.h"

#include <chrono>

namespace cfg {
namespace {
using namespace std::chrono_literals;
}

constexpr unsigned int ACCEPT_TIMEOUT_MS = 250;
constexpr unsigned SOCKET_SR_TIMEOUT = 5000;
constexpr unsigned int DEFAULT_BUFLEN = 4096;
constexpr unsigned int MAX_CLIENTS = 16;
constexpr unsigned int SERVER_PORT = 14521;
constexpr unsigned int MAX_MESSAGE_LENGTH = 1e7;
// How long a connection thread in InstrumentationService waits for item to
// appear in target queue
constexpr duration_t POLL_BLOCKING_TIMEOUT = 2.5s;
constexpr char SERVER_ADDRESS[] = "127.0.0.1";
constexpr char DLL_NAME[] = "libra2yrcpp.dll";
constexpr char INIT_NAME[] = "init_iservice";
constexpr unsigned int EVENT_BUFFER_SIZE = 600;
constexpr unsigned int RESULT_QUEUE_SIZE = 32U;
constexpr duration_t COMMAND_RESULTS_ACQUIRE_TIMEOUT = 5.0s;
// General purpose "maximum" timeout value to avoid overflow in wait_for() etc.
constexpr duration_t MAX_TIMEOUT = (60 * 60 * 24) * 1.0s;
constexpr duration_t WEBSOCKET_READ_TIMEOUT = 5.0s;
// Timeout for client when polling results from a queue.
constexpr duration_t POLL_RESULTS_TIMEOUT = 1.0s;
// Timeout for client to get ACK from service.
constexpr duration_t COMMAND_ACK_TIMEOUT = 0.25s;
constexpr char ALLOWED_HOSTS_REGEX[] = "0.0.0.0|127.0.0.1";
constexpr unsigned int PLACE_QUERY_MAX_LENGTH = 1024U;
};  // namespace cfg

#if defined(_M_X64) || defined(__amd64__)
#define RA2YRCPP_64
#else
#define RA2YRCPP_32
#endif
