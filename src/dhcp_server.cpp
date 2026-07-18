// Minimal DHCP server implementation. See dhcp_server.hpp for why this exists.
#include "psa/dhcp_server.hpp"

#ifndef HOST_TEST
#include <cstring>
extern "C" {
#include "lwip/pbuf.h"
#include "lwip/def.h"
#include "lwip/prot/dhcp.h"
}

namespace psa {

bool DhcpServer::init(const ip4_addr_t& server_ip, const ip4_addr_t& netmask) {
    server_ip_ = server_ip;
    netmask_ = netmask;

    pcb_ = udp_new();
    if (!pcb_) return false;

    ip_addr_t bind_ip = *reinterpret_cast<const ip_addr_t*>(&server_ip_);
    if (udp_bind(pcb_, &bind_ip, 67) != ERR_OK) {
        udp_remove(pcb_);
        pcb_ = nullptr;
        return false;
    }
    udp_recv(pcb_, recvCallback, this);
    return true;
}

void DhcpServer::recvCallback(void* arg, struct udp_pcb* /*pcb*/, struct pbuf* p,
                               const ip_addr_t* /*addr*/, u16_t /*port*/) {
    static_cast<DhcpServer*>(arg)->handleRequest(p);
    pbuf_free(p);
}

uint32_t DhcpServer::leaseFor(const uint8_t* mac) {
    for (size_t i = 0; i < kPoolSize; ++i) {
        if (leases_[i].in_use && std::memcmp(leases_[i].mac, mac, 6) == 0) {
            return lwip_htonl(lwip_ntohl(server_ip_.addr) + 1 + i);
        }
    }
    for (size_t i = 0; i < kPoolSize; ++i) {
        if (!leases_[i].in_use) {
            leases_[i].in_use = true;
            std::memcpy(leases_[i].mac, mac, 6);
            return lwip_htonl(lwip_ntohl(server_ip_.addr) + 1 + i);
        }
    }
    // pool exhausted: evict round-robin (fine for a handful of diagnostic clients)
    size_t i = next_evict_;
    next_evict_ = (next_evict_ + 1) % kPoolSize;
    leases_[i].in_use = true;
    std::memcpy(leases_[i].mac, mac, 6);
    return lwip_htonl(lwip_ntohl(server_ip_.addr) + 1 + i);
}

void DhcpServer::handleRequest(struct pbuf* p) {
    if (p->tot_len < DHCP_OPTIONS_OFS + 1) return;

    uint8_t buf[576];
    uint16_t recv_len = (p->tot_len < sizeof(buf)) ? p->tot_len : sizeof(buf);
    pbuf_copy_partial(p, buf, recv_len, 0);

    if (buf[0] != DHCP_BOOTREQUEST) return;
    uint32_t cookie;
    std::memcpy(&cookie, buf + 236, 4);
    if (cookie != PP_HTONL(DHCP_MAGIC_COOKIE)) return;

    // Walk the TLV options looking for message type (53); ignore anything else
    // (parameter request lists, vendor class, hostname, ...) — we don't need them.
    uint8_t msg_type = 0;
    size_t i = DHCP_OPTIONS_OFS;
    while (i < recv_len) {
        uint8_t code = buf[i];
        if (code == DHCP_OPTION_END) break;
        if (code == DHCP_OPTION_PAD) { i++; continue; }
        if (i + 1 >= recv_len) break;
        uint8_t len = buf[i + 1];
        if (i + 2 + len > recv_len) break;
        if (code == DHCP_OPTION_MESSAGE_TYPE && len >= 1) msg_type = buf[i + 2];
        i += 2 + len;
    }
    if (msg_type != DHCP_DISCOVER && msg_type != DHCP_REQUEST) return;

    uint8_t mac[6];
    std::memcpy(mac, buf + 28, 6); // chaddr starts right after op..giaddr (28 bytes)
    uint32_t offered_ip = leaseFor(mac);

    struct dhcp_msg reply;
    std::memset(&reply, 0, sizeof(reply));
    reply.op = DHCP_BOOTREPLY;
    reply.htype = 1;
    reply.hlen = 6;
    std::memcpy(&reply.xid, buf + 4, 4);   // pass through verbatim
    std::memcpy(&reply.flags, buf + 8, 2); // pass through verbatim
    reply.yiaddr.addr = offered_ip;
    reply.siaddr.addr = server_ip_.addr;
    std::memcpy(reply.chaddr, mac, 6);
    reply.cookie = PP_HTONL(DHCP_MAGIC_COOKIE);

    uint8_t* opt = reply.options;
    size_t n = 0;
    uint8_t reply_type = (msg_type == DHCP_DISCOVER) ? DHCP_OFFER : DHCP_ACK;
    opt[n++] = DHCP_OPTION_MESSAGE_TYPE; opt[n++] = 1; opt[n++] = reply_type;
    opt[n++] = DHCP_OPTION_SERVER_ID; opt[n++] = 4;
    std::memcpy(opt + n, &server_ip_.addr, 4); n += 4;
    uint32_t lease_secs = PP_HTONL(86400);
    opt[n++] = DHCP_OPTION_LEASE_TIME; opt[n++] = 4;
    std::memcpy(opt + n, &lease_secs, 4); n += 4;
    opt[n++] = DHCP_OPTION_SUBNET_MASK; opt[n++] = 4;
    std::memcpy(opt + n, &netmask_.addr, 4); n += 4;
    opt[n++] = DHCP_OPTION_ROUTER; opt[n++] = 4;
    std::memcpy(opt + n, &server_ip_.addr, 4); n += 4;
    opt[n++] = DHCP_OPTION_END;

    size_t send_len = DHCP_OPTIONS_OFS + n;
    struct pbuf* out = pbuf_alloc(PBUF_TRANSPORT, static_cast<u16_t>(send_len), PBUF_RAM);
    if (!out) return;
    std::memcpy(out->payload, &reply, send_len);
    udp_sendto(pcb_, out, IP_ADDR_BROADCAST, 68);
    pbuf_free(out);
}

} // namespace psa

#endif // HOST_TEST
