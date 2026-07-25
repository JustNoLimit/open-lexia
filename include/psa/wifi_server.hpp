// Wi-Fi HTTP + Server-Sent Events (SSE) server for wireless diagnostic dashboard.
// Configures Pico 2 W in Access Point mode and runs a lightweight lwIP raw TCP server.
// Reference: Phase 5 Extension of PSA CAN Interface Roadmap.
#pragma once
#include <cstdint>
#include <cstddef>
#include "psa/diag_shell.hpp"

// Forward-declare lwIP types only when building real firmware
#ifndef HOST_TEST
extern "C" {
#include "lwip/tcp.h"
}
#endif

namespace psa {

class WifiServer {
public:
    static WifiServer& instance() {
        static WifiServer inst;
        return inst;
    }

    // Initialize CYW43 in AP mode and bind port 80. Returns false on failure.
    bool init(DiagShell* shell);

    // Call from main loop when using poll architecture.
    void poll();

    // Route all stdio (printf) output to the SSE stream, line by line. This is
    // what makes the dashboard show diagnostic output: the shell uses printf, not
    // an explicit sink, so we capture stdout centrally. No-op on host builds.
    void initStdioCapture();

    // Broadcast a single log line to the connected SSE client (no trailing newline).
    void broadcastLog(const char* msg);

#ifndef HOST_TEST
    // Per-connection request/response state (request reassembly buffer + deferred
    // send bookkeeping). Defined in wifi_server.cpp. Declared public (rather than
    // forward-declared private) so the free helper functions in wifi_server.cpp's
    // anonymous namespace — allocConn/trySend/etc, which aren't WifiServer members
    // — can name the type; it's still opaque outside that one translation unit.
    struct HttpConn;
#endif

private:
    WifiServer() = default;

    DiagShell* shell_ = nullptr;

#ifndef HOST_TEST
    static err_t  acceptCallback(void* arg, struct tcp_pcb* newpcb, err_t err);
    static err_t  recvCallback(void* arg, struct tcp_pcb* tpcb, struct pbuf* p, err_t err);
    static void   errCallback(void* arg, err_t err);
    static err_t  pollCallback(void* arg, struct tcp_pcb* tpcb);
    static err_t  sentCallback(void* arg, struct tcp_pcb* tpcb, u16_t len);

    void handleHttpRequest(struct tcp_pcb* tpcb, HttpConn* conn);
    void registerSseClient(struct tcp_pcb* tpcb, HttpConn* conn);
    void unregisterSseClient(HttpConn* conn);

    struct tcp_pcb* server_pcb_    = nullptr;
    struct tcp_pcb* sse_client_pcb_ = nullptr;
#endif
};

} // namespace psa
