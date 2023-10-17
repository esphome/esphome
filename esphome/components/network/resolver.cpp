#include "resolver.h"
namespace esphome {
namespace network {


Resolver::Resolver() : hosts_{} { global_resolver = this; }
Resolver::Resolver(std::map<std::string, network::IPAddress> hosts) : hosts_(std::move(hosts)) { global_resolver = this; }
network::IPAddress Resolver::resolve(std::string hostname) {
  if (this->hosts_.count(hostname) > 0) {
    return this->hosts_[hostname];
  } else {
    // Resolve mdns
    // ... mdns ...
    // else
    // Resolve regualr dns
    // ... dns ...
  }
  return network::IPAddress();

}

Resolver *global_resolver = nullptr;

}  // namespace network
}  // namespace esphome
