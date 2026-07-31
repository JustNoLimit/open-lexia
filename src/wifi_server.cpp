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
            // Bounds-check before reading the two hex digits: src[1]/src[2] must
            // not be read past the string terminator (bare trailing '%' or '%A').
            if (src[1] != '\0' && src[2] != '\0') {
                int val;
                char hex[3] = { src[1], src[2], '\0' };
                if (std::sscanf(hex, "%x", &val) == 1) {
                    *dest++ = static_cast<char>(val);
                    src += 3;
                    continue;
                }
            }
            *dest++ = *src++; // trailing/invalid escape: emit '%' literally
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

// Per-connection state, hung off each pcb via tcp_arg(). Needed because lwIP's
// raw API gives callbacks no scratch space of their own: request bytes must be
// reassembled across recv() calls, and a large response write must be able to
// resume from tcp_sent() if it doesn't fit in one go.
struct WifiServer::HttpConn {
    bool             in_use   = false;
    struct tcp_pcb*  pcb      = nullptr;
    WifiServer*      server   = nullptr;

    // Inbound request accumulation, until CRLFCRLF or kReqCap is hit.
    char             req_buf[1024];
    size_t           req_len  = 0;

    // Outbound response: a small owned header (copied, since it's built into
    // this struct's own scratch space) followed by an optional external body
    // (e.g. a gzip'd dashboard asset — those arrays are static/const for the
    // program's lifetime, so no copy is needed and no lifetime risk on defer).
    char             hdr_buf[384];
    size_t           hdr_len  = 0, hdr_sent  = 0;
    const uint8_t*   body     = nullptr;
    size_t           body_len = 0, body_sent = 0;

    bool             is_sse         = false; // never auto-closed, never timed out
    bool             close_when_done = false;
};

namespace {

using HttpConn = WifiServer::HttpConn;

// ponytail: fixed pool, no malloc. One phone driving the dashboard needs at
// most a handful of connections at once (html+css+js+sse+the odd /api/cmd);
// a 5th concurrent connection is simply refused. Bump kMaxConns if that ever
// bites.
constexpr size_t kMaxConns = 4;
HttpConn g_conns[kMaxConns];

HttpConn* allocConn(struct tcp_pcb* pcb, WifiServer* server) {
    for (auto& c : g_conns) {
        if (!c.in_use) {
            c = HttpConn{};
            c.in_use = true;
            c.pcb = pcb;
            c.server = server;
            return &c;
        }
    }
    return nullptr; // pool exhausted; caller refuses the connection
}

HttpConn* findConn(struct tcp_pcb* pcb) {
    for (auto& c : g_conns) {
        if (c.in_use && c.pcb == pcb) return &c;
    }
    return nullptr;
}

void freeConn(HttpConn* c) {
    if (c) c->in_use = false;
}

// Clamp snprintf's return into hdr_buf: a negative (formatting error) becomes
// an empty header instead of wrapping to a huge size_t, and a would-have-been
// length past the buffer (truncation) is capped rather than read past the end.
size_t hdrLen(int n) {
    if (n <= 0) return 0;
    constexpr size_t cap = sizeof(HttpConn::hdr_buf) - 1;
    return static_cast<size_t>(n) < cap ? static_cast<size_t>(n) : cap;
}

// Push as much of the pending header+body as the TCP send window allows.
// Called right after a response is queued and again from tcp_sent()/poll if
// the first attempt didn't drain everything (ERR_MEM). Closes the connection
// once fully sent, unless it's the long-lived SSE stream.
void trySend(struct tcp_pcb* tpcb, HttpConn* c) {
    while (c->hdr_sent < c->hdr_len) {
        u16_t avail = tcp_sndbuf(tpcb);
        if (avail == 0) return; // window full; wait for tcp_sent()/poll
        size_t remaining = c->hdr_len - c->hdr_sent;
        size_t chunk = remaining < avail ? remaining : avail;
        err_t e = tcp_write(tpcb, c->hdr_buf + c->hdr_sent, static_cast<u16_t>(chunk), TCP_WRITE_FLAG_COPY);
        if (e == ERR_MEM) { tcp_output(tpcb); return; }
        if (e != ERR_OK) { freeConn(c); tcp_close(tpcb); return; }
        c->hdr_sent += chunk;
    }
    while (c->body_sent < c->body_len) {
        u16_t avail = tcp_sndbuf(tpcb);
        if (avail == 0) return;
        size_t remaining = c->body_len - c->body_sent;
        size_t chunk = remaining < avail ? remaining : avail;
        err_t e = tcp_write(tpcb, c->body + c->body_sent, static_cast<u16_t>(chunk), 0);
        if (e == ERR_MEM) { tcp_output(tpcb); return; }
        if (e != ERR_OK) { freeConn(c); tcp_close(tpcb); return; }
        c->body_sent += chunk;
    }
    tcp_output(tpcb);
    if (!c->is_sse && c->close_when_done) {
        freeConn(c);
        tcp_close(tpcb);
    }
}

} // namespace

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
    HttpConn* conn = allocConn(newpcb, server);
    if (!conn) {
        tcp_close(newpcb); // pool exhausted (see kMaxConns) — refuse this one
        return ERR_OK;
    }

    tcp_arg(newpcb, conn);
    tcp_recv(newpcb, recvCallback);
    tcp_err(newpcb, errCallback);
    tcp_sent(newpcb, sentCallback);
    tcp_poll(newpcb, pollCallback, 4); // poll every 2 seconds

    return ERR_OK;
}

err_t WifiServer::recvCallback(void* arg, struct tcp_pcb* tpcb, struct pbuf* p, err_t err) {
    HttpConn* conn = static_cast<HttpConn*>(arg);

    if (err != ERR_OK) {
        if (p) pbuf_free(p);
        return err;
    }

    if (p == nullptr) {
        // Connection closed by client
        conn->server->unregisterSseClient(conn);
        freeConn(conn);
        tcp_close(tpcb);
        return ERR_OK;
    }

    // Reassemble across TCP segments: append into the per-connection buffer
    // and only act once we've seen the header terminator. A request that
    // never completes within kReqCap gets rejected outright rather than
    // parsed truncated.
    size_t cap = sizeof(conn->req_buf) - 1;
    size_t space = conn->req_len < cap ? cap - conn->req_len : 0;
    size_t take = p->tot_len < space ? p->tot_len : space;
    if (take > 0) {
        pbuf_copy_partial(p, conn->req_buf + conn->req_len, take, 0);
        conn->req_len += take;
    }
    tcp_recved(tpcb, p->tot_len); // reopen the window regardless of what we kept
    pbuf_free(p);

    conn->req_buf[conn->req_len] = '\0';

    if (std::strstr(conn->req_buf, "\r\n\r\n") != nullptr) {
        conn->server->handleHttpRequest(tpcb, conn);
    } else if (conn->req_len >= cap) {
        conn->close_when_done = true;
        conn->hdr_len = hdrLen(std::snprintf(conn->hdr_buf, sizeof(conn->hdr_buf),
            "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\nConnection: close\r\n\r\n"));
        trySend(tpcb, conn);
    }
    // else: wait for more segments before parsing

    return ERR_OK;
}

void WifiServer::errCallback(void* arg, err_t err) {
    // lwIP has already freed tpcb by the time this fires — just drop our
    // bookkeeping for it so the pool slot (and sse_client_pcb_) don't leak.
    (void)err;
    HttpConn* conn = static_cast<HttpConn*>(arg);
    if (conn) {
        conn->server->unregisterSseClient(conn);
        freeConn(conn);
    }
}

err_t WifiServer::pollCallback(void* arg, struct tcp_pcb* tpcb) {
    HttpConn* conn = static_cast<HttpConn*>(arg);

    if (conn->is_sse) return ERR_OK; // the SSE stream is meant to stay open

    if (conn->hdr_sent < conn->hdr_len || conn->body_sent < conn->body_len) {
        trySend(tpcb, conn); // nudge a send that stalled on a full window
        return ERR_OK;
    }
    if (conn->req_len > 0 && conn->hdr_len == 0) {
        return ERR_OK; // still accumulating a multi-segment request
    }

    // Nothing in flight and no request pending: reclaim a connection the
    // client left dangling (opened but never sent/finished anything).
    freeConn(conn);
    tcp_close(tpcb);
    return ERR_OK;
}

err_t WifiServer::sentCallback(void* arg, struct tcp_pcb* tpcb, u16_t len) {
    (void)len;
    HttpConn* conn = static_cast<HttpConn*>(arg);
    trySend(tpcb, conn); // resume a write that previously hit ERR_MEM
    return ERR_OK;
}

void WifiServer::handleHttpRequest(struct tcp_pcb* tpcb, HttpConn* conn) {
    const char* req = conn->req_buf;
    conn->close_when_done = true; // every branch here is a one-shot response

    // Quick routing
    if (std::strncmp(req, "GET / ", 6) == 0 || std::strncmp(req, "GET /index.html", 15) == 0) {
        // Serve dashboard.html (gzipped)
        conn->hdr_len = hdrLen(std::snprintf(conn->hdr_buf, sizeof(conn->hdr_buf),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html; charset=utf-8\r\n"
            "Content-Encoding: gzip\r\n"
            "Content-Length: %zu\r\n"
            "Connection: close\r\n\r\n",
            assets::kHtmlCompLen));
        conn->body = assets::kHtmlData;
        conn->body_len = assets::kHtmlCompLen;
    }
    else if (std::strncmp(req, "GET /dashboard.css", 18) == 0) {
        conn->hdr_len = hdrLen(std::snprintf(conn->hdr_buf, sizeof(conn->hdr_buf),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/css; charset=utf-8\r\n"
            "Content-Encoding: gzip\r\n"
            "Content-Length: %zu\r\n"
            "Connection: close\r\n\r\n",
            assets::kCssCompLen));
        conn->body = assets::kCssData;
        conn->body_len = assets::kCssCompLen;
    }
    else if (std::strncmp(req, "GET /dashboard.js", 17) == 0) {
        conn->hdr_len = hdrLen(std::snprintf(conn->hdr_buf, sizeof(conn->hdr_buf),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/javascript; charset=utf-8\r\n"
            "Content-Encoding: gzip\r\n"
            "Content-Length: %zu\r\n"
            "Connection: close\r\n\r\n",
            assets::kJsCompLen));
        conn->body = assets::kJsData;
        conn->body_len = assets::kJsCompLen;
    }
    else if (std::strncmp(req, "GET /api/stream", 15) == 0) {
        registerSseClient(tpcb, conn); // owns close_when_done/is_sse itself
        return;
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

        conn->hdr_len = hdrLen(std::snprintf(conn->hdr_buf, sizeof(conn->hdr_buf),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 2\r\n"
            "Connection: close\r\n\r\n"
            "OK"));
    }
    else if (std::strncmp(req, "GET /api/data", 13) == 0) {
        if (std::strstr(req, "type=vehicle")) {
            conn->hdr_len = hdrLen(std::snprintf(conn->hdr_buf, sizeof(conn->hdr_buf),
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: application/json\r\n"
                "Connection: close\r\n\r\n"
                "{\"vin\":\"---\",\"model\":\"Citroen C5 Mk1 FL\",\"year\":\"2006\"}"));
        } else {
            // Reflect the real shell state so the UI can sync on load, not just via SSE.
            bool connected = shell_ && shell_->isConnected();
            conn->hdr_len = hdrLen(std::snprintf(conn->hdr_buf, sizeof(conn->hdr_buf),
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: application/json\r\n"
                "Connection: close\r\n\r\n"
                "{\"connected\":%s,\"ecu\":\"%s\",\"unlocked\":%s,\"scan_active\":%s,\"rw\":%s}",
                connected ? "true" : "false",
                shell_ ? shell_->ecuFamily() : "none",
                (shell_ && shell_->isUnlocked()) ? "true" : "false",
                (shell_ && shell_->isScanning()) ? "true" : "false",
                (shell_ && shell_->rwEnabled()) ? "true" : "false"));
        }
    }
    else {
        conn->hdr_len = hdrLen(std::snprintf(conn->hdr_buf, sizeof(conn->hdr_buf),
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Length: 0\r\n"
            "Connection: close\r\n\r\n"));
    }

    trySend(tpcb, conn);
}

void WifiServer::registerSseClient(struct tcp_pcb* tpcb, HttpConn* conn) {
    if (sse_client_pcb_) {
        // A second browser tab opened /api/stream: close the stale client now
        // instead of leaking its pcb until the next poll tick notices it's no
        // longer sse_client_pcb_.
        struct tcp_pcb* old_pcb = sse_client_pcb_;
        sse_client_pcb_ = nullptr;
        HttpConn* old = findConn(old_pcb);
        freeConn(old);
        tcp_close(old_pcb);
    }

    sse_client_pcb_ = tpcb;
    conn->is_sse = true;
    conn->close_when_done = false;

    conn->hdr_len = hdrLen(std::snprintf(conn->hdr_buf, sizeof(conn->hdr_buf),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/event-stream\r\n"
        "Cache-Control: no-cache\r\n"
        "Connection: keep-alive\r\n\r\n"));
    trySend(tpcb, conn);
}

void WifiServer::unregisterSseClient(HttpConn* conn) {
    if (conn && conn->is_sse && conn->pcb == sse_client_pcb_) {
        sse_client_pcb_ = nullptr;
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
