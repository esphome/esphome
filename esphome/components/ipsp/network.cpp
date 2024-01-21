#include "network.h"
#include <zephyr/net/net_if.h>


/* Define my IP address where to expect messages */
#define MY_IP6ADDR { { { 0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 0, \
			 0, 0, 0, 0, 0, 0, 0, 0x1 } } }

static struct in6_addr in6addr_my = MY_IP6ADDR;

namespace esphome {
namespace ipsp {

void Network::setup() {
	if (net_addr_pton(AF_INET6,
			  CONFIG_NET_CONFIG_MY_IPV6_ADDR,
			  &in6addr_my) < 0) {
		// LOG_ERR("Invalid IPv6 address %s",
		// 	CONFIG_NET_CONFIG_MY_IPV6_ADDR);
	}

	do {
		struct net_if_addr *ifaddr;

		ifaddr = net_if_ipv6_addr_add(net_if_get_default(),
					      &in6addr_my, NET_ADDR_MANUAL, 0);
	} while (0);
}

}  // namespace shell
}  // namespace esphome
