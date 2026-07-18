// Wi-Fi HTTP + Server-Sent Events (SSE) server implementation.
#include "psa/wifi_server.hpp"
#include <cstdio>
#include <cstring>
#include "psa/dashboard_assets.h"
#include "psa/dhcp_server.hpp"

#ifndef HOST_TEST
#include "pico/cyw43_arch.h"
#include "pico/stdio/driver.h"
#include "lwip/tcp.h"
#include "lwip/pbuf.h"

namespace {

// Line buffer for the stdout->SSE bridge. printf output arrives in arbitrary
// chunks (the scan report writes one ECU row across several printf calls); we
// accumulate until '\n' and forward whole lines so the dashboard's line-oriented
// SSE parser sees complete messages.
char   g_line_buf[256];
size_t g_line_len = 0;

void sse_flush_line() {
    if (g_line_len == 0) return;
    g_line_buf[g_line_len] = '\0';
    psa::WifiServer::instance().broadcastLog(g_line_buf);
    g_line_len = 0;
}

void sse_out_chars(const char* buf, int len) {
    for (int i = 0; i < len; ++i) {
        char c = buf[i];
        if (c == '\r') continue;
        if (c == '\n') { sse_flush_line(); continue; }
        if (g_line_len < sizeof(g_line_buf) - 1) g_line_buf[g_line_len++] = c;
        else sse_flush_line();          // over-long line: flush what we have
    }
}

stdio_driver_t g_sse_stdio_driver = {
    .out_chars = sse_out_chars,
    .out_flush = sse_flush_line,
    .in_chars  = nullptr,
};

void urlDecode(const char* src, char* dest) {
    while (*src) {
        if (*src == '%') {
            int val;
            // Parse 2 hex characters
            char hex[3] = { src[1], src[2], '\0' };
            if (std::sscanf(hex, "%x", &val) == 1) {
                *dest++ = static_cast<char>(val);
                src += 3;
            } else {
                *dest++ = *src++;
            }
        } else if (*src == '+') {
            *dest++ = ' ';
            src++;
        } else {
            *dest++ = *src++;
        }
    }
    *dest = '\0';
}

} // namespace

namespace psa {

bool WifiServer::init(DiagShell* shell) {
    shell_ = shell;

    // Initialize CYW43 in Access Point mode
    if (cyw43_arch_init()) {
        printf("[WIFI] Failed to initialize cyw43\n");
        return false;
    }

    cyw43_arch_enable_ap_mode("Citroen-Diag", "12345678", CYW43_AUTH_WPA2_AES_PSK);
    printf("[WIFI] Access Point \"Citroen-Diag\" started. IP: 192.168.4.1\n");

    // Without this, a joining phone/laptop gets no address (enable_ap_mode only
    // brings up the AP link) and can't reach the dashboard at 192.168.4.1.
    ip4_addr_t ap_ip, ap_mask;
    IP4_ADDR(&ap_ip, 192, 168, 4, 1);
    IP4_ADDR(&ap_mask, 255, 255, 255, 0);
    if (DhcpServer::instance().init(ap_ip, ap_mask)) {
        printf("[WIFI] DHCP server started (pool 192.168.4.2-192.168.4.9).\n");
    } else {
        printf("[WIFI] Warning: DHCP server failed to start; clients may need a static IP.\n");
    }

    // Create listening PCB on port 80
    server_pcb_ = tcp_new_ip_type(IPADDR_TYPE_ANY);
    if (!server_pcb_) return false;

    err_t err = tcp_bind(server_pcb_, IP_ANY_TYPE, 80);
    if (err != ERR_OK) return false;

    server_pcb_ = tcp_listen_with_backlog(server_pcb_, 4);
    if (!server_pcb_) return false;

    tcp_arg(server_pcb_, this);
    tcp_accept(server_pcb_, acceptCallback);

    return true;
}

void WifiServer::poll() {
    // Under threadsafe background architecture, lwip runs in the background.
    // Nothing mandatory here, cyw43_arch_poll() can be called if using poll architecture.
}

void WifiServer::initStdioCapture() {
    stdio_set_driver_enabled(&g_sse_stdio_driver, true);
}

void WifiServer::broadcastLog(const char* msg) {
    if (sse_client_pcb_ == nullptr) return;

    char buffer[512];
    int len = std::snprintf(buffer, sizeof(buffer), "data: %s\n\n", msg);
    if (len > 0) {
        tcp_write(sse_client_pcb_, buffer, len, TCP_WRITE_FLAG_COPY);
        tcp_output(sse_client_pcb_);
    }
}

err_t WifiServer::acceptCallback(void* arg, struct tcp_pcb* newpcb, err_t err) {
    if (err != ERR_OK || newpcb == nullptr) return ERR_VAL;
    
    WifiServer* server = static_cast<WifiServer*>(arg);
    tcp_arg(newpcb, server);
    tcp_recv(newpcb, recvCallback);
    tcp_err(newpcb, errCallback);
    tcp_poll(newpcb, pollCallback, 4); // poll every 2 seconds

    return ERR_OK;
}

err_t WifiServer::recvCallback(void* arg, struct tcp_pcb* tpcb, struct pbuf* p, err_t err) {
    WifiServer* server = static_cast<WifiServer*>(arg);

    if (err != ERR_OK) {
        if (p) pbuf_free(p);
        return err;
    }

    if (p == nullptr) {
        // Connection closed by client
        server->unregisterSseClient(tpcb);
        tcp_close(tpcb);
        return ERR_OK;
    }

    // Extract request string (assume fits in first pbuf packet)
    char req[512];
    size_t copy_len = p->tot_len < sizeof(req) - 1 ? p->tot_len : sizeof(req) - 1;
    pbuf_copy_partial(p, req, copy_len, 0);
    req[copy_len] = '\0';

    pbuf_free(p);

    server->handleHttpRequest(tpcb, req);
    return ERR_OK;
}

void WifiServer::errCallback(void* arg, err_t err) {
    // Connection aborted, clean up if it was our SSE client
    WifiServer* server = static_cast<WifiServer*>(arg);
    (void)server;
    (void)err;
}

err_t WifiServer::pollCallback(void* arg, struct tcp_pcb* tpcb) {
    WifiServer* server = static_cast<WifiServer*>(arg);
    // Keep-alive or close if idle, but for SSE client we want to keep it alive
    if (tpcb != server->sse_client_pcb_) {
        // Close non-persistent HTTP client connections
        tcp_close(tpcb);
    }
    return ERR_OK;
}

void WifiServer::handleHttpRequest(struct tcp_pcb* tpcb, const char* req) {
    // Quick routing
    if (std::strncmp(req, "GET / ", 6) == 0 || std::strncmp(req, "GET /index.html", 15) == 0) {
        // Serve dashboard.html (gzipped)
        char header[256];
        int header_len = std::snprintf(header, sizeof(header),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html; charset=utf-8\r\n"
            "Content-Encoding: gzip\r\n"
            "Content-Length: %zu\r\n"
            "Connection: close\r\n\r\n",
            assets::kHtmlCompLen);

        tcp_write(tpcb, header, header_len, TCP_WRITE_FLAG_COPY);
        tcp_write(tpcb, assets::kHtmlData, assets::kHtmlCompLen, 0);
        tcp_output(tpcb);
    }
    else if (std::strncmp(req, "GET /dashboard.css", 18) == 0) {
        char header[256];
        int header_len = std::snprintf(header, sizeof(header),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/css; charset=utf-8\r\n"
            "Content-Encoding: gzip\r\n"
            "Content-Length: %zu\r\n"
            "Connection: close\r\n\r\n",
            assets::kCssCompLen);

        tcp_write(tpcb, header, header_len, TCP_WRITE_FLAG_COPY);
        tcp_write(tpcb, assets::kCssData, assets::kCssCompLen, 0);
        tcp_output(tpcb);
    }
    else if (std::strncmp(req, "GET /dashboard.js", 17) == 0) {
        char header[256];
        int header_len = std::snprintf(header, sizeof(header),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/javascript; charset=utf-8\r\n"
            "Content-Encoding: gzip\r\n"
            "Content-Length: %zu\r\n"
            "Connection: close\r\n\r\n",
            assets::kJsCompLen);

        tcp_write(tpcb, header, header_len, TCP_WRITE_FLAG_COPY);
        tcp_write(tpcb, assets::kJsData, assets::kJsCompLen, 0);
        tcp_output(tpcb);
    }
    else if (std::strncmp(req, "GET /api/stream", 15) == 0) {
        registerSseClient(tpcb);
    }
    else if (std::strncmp(req, "GET /api/cmd", 12) == 0) {
        // Extract command parameter from e.g. "GET /api/cmd?val=connect+BMF HTTP/1.1"
        const char* val_start = std::strstr(req, "?val=");
        if (val_start) {
            val_start += 5;
            const char* val_end = std::strchr(val_start, ' ');
            if (val_end) {
                char raw_cmd[256];
                size_t len = val_end - val_start;
                if (len < sizeof(raw_cmd)) {
                    std::memcpy(raw_cmd, val_start, len);
                    raw_cmd[len] = '\0';
                    
                    char cmd[256];
                    urlDecode(raw_cmd, cmd);

                    if (shell_) {
                        shell_->feedCommandLine(cmd);
                    }
                }
            }
        }
        
        const char* response = 
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 2\r\n"
            "Connection: close\r\n\r\n"
            "OK";
        tcp_write(tpcb, response, std::strlen(response), TCP_WRITE_FLAG_COPY);
        tcp_output(tpcb);
    }
    else if (std::strncmp(req, "GET /api/data", 13) == 0) {
        char response[384];
        int rlen;
        if (std::strstr(req, "type=vehicle")) {
            rlen = std::snprintf(response, sizeof(response),
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: application/json\r\n"
                "Connection: close\r\n\r\n"
                "{\"vin\":\"---\",\"model\":\"Citroen C5 Mk1 FL\",\"year\":\"2006\"}");
        } else {
            // Reflect the real shell state so the UI can sync on load, not just via SSE.
            bool connected = shell_ && shell_->isConnected();
            rlen = std::snprintf(response, sizeof(response),
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: application/json\r\n"
                "Connection: close\r\n\r\n"
                "{\"connected\":%s,\"ecu\":\"%s\",\"unlocked\":%s,\"scan_active\":%s}",
                connected ? "true" : "false",
                shell_ ? shell_->ecuFamily() : "none",
                (shell_ && shell_->isUnlocked()) ? "true" : "false",
                (shell_ && shell_->isScanning()) ? "true" : "false");
        }
        if (rlen > 0) {
            tcp_write(tpcb, response, rlen, TCP_WRITE_FLAG_COPY);
            tcp_output(tpcb);
        }
    }
    else {
        const char* response = 
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Length: 0\r\n"
            "Connection: close\r\n\r\n";
        tcp_write(tpcb, response, std::strlen(response), TCP_WRITE_FLAG_COPY);
        tcp_output(tpcb);
    }
}

void WifiServer::registerSseClient(struct tcp_pcb* tpcb) {
    unregisterSseClient(sse_client_pcb_); // disconnect older client if any
    
    sse_client_pcb_ = tpcb;
    
    const char* header = 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/event-stream\r\n"
        "Cache-Control: no-cache\r\n"
        "Connection: keep-alive\r\n\r\n";
        
    tcp_write(tpcb, header, std::strlen(header), TCP_WRITE_FLAG_COPY);
    tcp_output(tpcb);
    printf("[WIFI] Diagnostic client registered over SSE.\n");
}

void WifiServer::unregisterSseClient(struct tcp_pcb* tpcb) {
    if (tpcb && tpcb == sse_client_pcb_) {
        sse_client_pcb_ = nullptr;
        printf("[WIFI] Diagnostic client disconnected.\n");
    }
}

} // namespace psa

#else
// Mock implementation to keep host tests compiling without Pico SDK
namespace psa {
bool WifiServer::init(DiagShell* shell) { (void)shell; return true; }
void WifiServer::poll() {}
void WifiServer::initStdioCapture() {}
void WifiServer::broadcastLog(const char* msg) { (void)msg; }
} // namespace psa
#endif
