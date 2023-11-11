#pragma once

#include <string>
#include <map>
#include "ip_address.h"
#ifdef USE_MDNS
#include "esphome/components/mdns/mdns_component.h"
#endif
namespace esphome {
namespace network {

class Resolver {
 public:
  Resolver();
  Resolver(std::map<std::string, network::IPAddress> hosts);
  ~Resolver();
  network::IPAddress resolve(const std::string *hostname);

 protected:
  static void dns_found_callback(const char *name, const ip_addr_t *ipaddr, void *callback_arg);
  std::map<std::string, network::IPAddress> hosts_;
  network::IPAddress ip_;
  bool dns_resolved_{false};
  bool dns_resolve_error_{false};
};

extern Resolver *global_resolver;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace network
}  // namespace esphome
