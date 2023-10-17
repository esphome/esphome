#pragma once

#include <string>
#include <map>
#include "ip_address.h"
namespace esphome {
namespace network {

class Resolver {
 public:
  Resolver();
  Resolver(std::map<std::string, network::IPAddress>);
  ~Resolver();
  network::IPAddress resolve(std::string);

protected:
  std::map<std::string, network::IPAddress> hosts_;
};

extern Resolver *global_resolver;

}  // namespace network
}  // namespace esphome
