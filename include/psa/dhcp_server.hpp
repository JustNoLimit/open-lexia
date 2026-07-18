// Minimal DHCP server for the Wi-Fi Access Point. Without this, a phone or
// laptop that joins "Citroen-Diag" gets no IP address and can't reach the
// dashboard at 192.168.4.1 (cyw43_arch_enable_ap_mode() only brings up the
// AP link; it does not hand out addresses). Server-assigns-address only
// (no client-requested-IP negotiation, no lease expiry) — this AP serves at
// most a handful of diagnostic clients, never a real multi-host network.
#pragma once
#include <cstdint>
#include <cstddef>

#ifndef HOST_TEST
extern "C" {
#include "lwip/udp.h"
#include "lwip/ip4_addr.h"
}
#endif

namespace psa {

class DhcpServer {
public:
    static DhcpServer& instance() {
        static DhcpServer inst;
        return inst;
    }

#ifndef HOST_TEST
    // server_ip/netmask match the values cyw43_arch already assigned to the AP
    // netif (192.168.4.1 / 255.255.255.0 by default). Binds UDP port 67.
    bool init(const ip4_addr_t& server_ip, const ip4_addr_t& netmask);
#endif

private:
    DhcpServer() = default;

#ifndef HOST_TEST
    static void recvCallback(void* arg, struct udp_pcb* pcb, struct pbuf* p,
                              const ip_addr_t* addr, u16_t port);
    void handleRequest(struct pbuf* p);

    // Fixed lease pool: server_ip + 1 .. server_ip + kPoolSize, keyed by MAC.
    // A small fixed table beats a real lease table here — reconnects just
    // reuse the same slot (matched by MAC) or evict the oldest one.
    static constexpr size_t kPoolSize = 8;
    struct Lease {
        uint8_t mac[6] = {};
        bool in_use = false;
    };

    struct udp_pcb* pcb_ = nullptr;
    ip4_addr_t server_ip_{};
    ip4_addr_t netmask_{};
    Lease leases_[kPoolSize];
    size_t next_evict_ = 0;

    // Returns the offered/assigned IP (server_ip_ + 1 + slot) for this MAC,
    // allocating a free slot (or evicting the oldest) if not already leased.
    uint32_t leaseFor(const uint8_t* mac);
#endif
};

} // namespace psa
