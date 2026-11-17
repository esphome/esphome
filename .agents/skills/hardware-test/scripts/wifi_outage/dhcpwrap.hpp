#pragma once
// Counts DHCP messages the station's lwIP sends (dhcp.c calls these across objects, so --wrap sees them).
#include <lwip/udp.h>
#include <lwip/pbuf.h>
#include <lwip/netif.h>
extern "C" {
volatile uint32_t g_dhcp_disc = 0, g_dhcp_req = 0, g_dhcp_rel = 0, g_dhcp_rel_up = 0;
err_t __real_udp_sendto_if(struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *dst_ip, u16_t dst_port,
                           struct netif *netif);
err_t __real_udp_sendto_if_src(struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *dst_ip, u16_t dst_port,
                               struct netif *netif, const ip_addr_t *src_ip);
static void dhcpwrap_count(struct pbuf *p, u16_t dst_port, struct netif *netif) {
  if (dst_port != 67 || p == nullptr || p->len < 243)
    return;
  const uint8_t *b = (const uint8_t *) p->payload;
  for (uint16_t i = 240; i + 2 < p->len;) {
    uint8_t opt = b[i];
    if (opt == 255)
      break;
    if (opt == 0) {
      i++;
      continue;
    }
    if (opt == 53) {
      uint8_t t = b[i + 2];
      if (t == 1)
        g_dhcp_disc++;
      else if (t == 3)
        g_dhcp_req++;
      else if (t == 7) {
        g_dhcp_rel++;
        if (netif != nullptr && netif_is_up(netif) && netif_is_link_up(netif))
          g_dhcp_rel_up++;
      }
      break;
    }
    i += 2 + b[i + 1];
  }
}
err_t __wrap_udp_sendto_if(struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *dst_ip, u16_t dst_port,
                           struct netif *netif) {
  dhcpwrap_count(p, dst_port, netif);
  return __real_udp_sendto_if(pcb, p, dst_ip, dst_port, netif);
}
err_t __wrap_udp_sendto_if_src(struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *dst_ip, u16_t dst_port,
                               struct netif *netif, const ip_addr_t *src_ip) {
  dhcpwrap_count(p, dst_port, netif);
  return __real_udp_sendto_if_src(pcb, p, dst_ip, dst_port, netif, src_ip);
}
}
