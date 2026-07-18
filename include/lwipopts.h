// lwIP configuration for the Citroen C5 diagnostic interface (Pico 2 W / cyw43,
// NO_SYS=1 threadsafe-background architecture, raw callback API, AP mode HTTP+SSE
// server). Required whenever pico_cyw43_arch_lwip_* is linked — lwIP's opt.h
// includes this header and has no built-in defaults for the target platform.
#ifndef _LWIPOPTS_H
#define _LWIPOPTS_H

// no OS/RTOS backing lwIP; cyw43_arch drives the stack from IRQ + background poll
#define NO_SYS                      1
#define LWIP_SOCKET                 0
#define LWIP_NETCONN                0

// this project only uses the raw callback API (tcp.h / pbuf.h), not sockets
#define MEM_LIBC_MALLOC             0
#define MEM_ALIGNMENT               4
// lwIP's internal heap (tcp_seg/pbuf metadata for every queued write, plus
// TCP_WRITE_FLAG_COPY header copies) comes out of this, not TCP_SND_BUF —
// 4000 was too small once dashboard.js needed ~13 segments per write and
// starved later requests before earlier connections' memory was freed.
// RP2350 has 520KB SRAM; generous headroom here costs nothing meaningful.
#define MEM_SIZE                    32000
// Must be >= TCP_SND_QUEUELEN below (lwIP enforces this at compile time) —
// TCP_SND_QUEUELEN is ~65 at TCP_SND_BUF=16*TCP_MSS, so this needs headroom above that.
#define MEMP_NUM_TCP_SEG            68
#define MEMP_NUM_ARP_QUEUE          10
#define PBUF_POOL_SIZE              24

#define LWIP_ARP                    1
#define LWIP_ETHERNET               1
#define LWIP_ICMP                   1
#define LWIP_RAW                    1

#define TCP_MSS                     1460
#define TCP_WND                     (8 * TCP_MSS)
// Must exceed the largest gzipped dashboard asset (dashboard.js, ~18KB) — the
// server writes each asset in one tcp_write() call with no tcp_sent-driven
// resend, so anything bigger than TCP_SND_BUF silently fails to queue.
// ponytail: fixed headroom over today's JS size, bump again if dashboard.js grows past it.
#define TCP_SND_BUF                 (16 * TCP_MSS)
#define TCP_SND_QUEUELEN            ((4 * (TCP_SND_BUF) + (TCP_MSS - 1)) / (TCP_MSS))

#define LWIP_NETIF_STATUS_CALLBACK  1
#define LWIP_NETIF_LINK_CALLBACK    1
#define LWIP_NETIF_HOSTNAME         1
#define LWIP_NETIF_TX_SINGLE_PBUF   1

#define LWIP_TCP                    1
#define LWIP_UDP                    1
#define LWIP_DNS                    1
#define LWIP_DHCP                   1
#define DHCP_DOES_ARP_CHECK         0
#define LWIP_DHCP_DOES_ACD_CHECK    0

#define LWIP_TCP_KEEPALIVE          1
#define LWIP_CHKSUM_ALGORITHM       3

#define MEM_STATS                   0
#define SYS_STATS                   0
#define MEMP_STATS                  0
#define LINK_STATS                  0

#ifndef NDEBUG
#define LWIP_DEBUG                  1
#endif

#define LWIP_STATS                  0
#define LWIP_STATS_DISPLAY          0

#endif // _LWIPOPTS_H
