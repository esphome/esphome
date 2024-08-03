#pragma once

#include <string>
#include <vector>
#include <map>
#include "ip_address.h"
namespace esphome {
namespace network {

class Resolver {
 public:
  Resolver();
  explicit Resolver(std::map<std::string, network::IPAddress> hosts);

  network::IPAddress resolve(const std::string &hostname);

 protected:
  static void dns_found_callback(const char *name, const ip_addr_t *ipaddr, void *callback_arg);
  std::map<std::string, network::IPAddress> hosts_;
  network::IPAddress ip_;
  bool dns_resolved_{false};
  bool dns_resolve_error_{false};
  uint32_t connect_begin_;
};

extern Resolver *global_resolver;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace network
}  // namespace esphome
