#pragma once

#include <string>
#include <map>
#include "ip_address.h"
#include "esphome/components/mdns/mdns_component.h"
namespace esphome {
namespace network {

class Resolver {
 public:
  Resolver();
  Resolver(std::map<std::string, network::IPAddress>);
  ~Resolver();
  network::IPAddress resolve(std::string);

protected:
  static void dns_found_callback(const char *, const ip_addr_t *, void *);
  std::map<std::string, network::IPAddress> hosts_;
  network::IPAddress ip_;
  bool dns_resolved_{false};
  bool dns_resolve_error_{false};
};

extern Resolver *global_resolver;

}  // namespace network
}  // namespace esphome
