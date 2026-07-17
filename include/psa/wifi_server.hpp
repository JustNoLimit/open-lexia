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

private:
    WifiServer() = default;

    DiagShell* shell_ = nullptr;

#ifndef HOST_TEST
    static err_t  acceptCallback(void* arg, struct tcp_pcb* newpcb, err_t err);
    static err_t  recvCallback(void* arg, struct tcp_pcb* tpcb, struct pbuf* p, err_t err);
    static void   errCallback(void* arg, err_t err);
    static err_t  pollCallback(void* arg, struct tcp_pcb* tpcb);

    void handleHttpRequest(struct tcp_pcb* tpcb, const char* req);
    void registerSseClient(struct tcp_pcb* tpcb);
    void unregisterSseClient(struct tcp_pcb* tpcb);

    struct tcp_pcb* server_pcb_    = nullptr;
    struct tcp_pcb* sse_client_pcb_ = nullptr;
#endif
};

} // namespace psa
