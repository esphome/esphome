#include "resolver.h"
#include "lwip/dns.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace network {

static const char *const TAG = "resolver";

Resolver::Resolver() { global_resolver = this; }
Resolver::Resolver(std::map<std::string, network::IPAddress> hosts) : hosts_(std::move(hosts)) {
  global_resolver = this;
}
network::IPAddress Resolver::resolve(const std::string *hostname) {
  if (this->hosts_.count(*hostname) > 0) {
    return this->hosts_[*hostname];
  }
  network::IPAddress resolved_mdns = mdns::global_mdns->resolve(*hostname);
  if (resolved_mdns.is_set()) {
    return resolved_mdns;
  }
  ip_addr_t addr;
  err_t err =
      dns_gethostbyname_addrtype(hostname->c_str(), &addr, Resolver::dns_found_callback, this, LWIP_DNS_ADDRTYPE_IPV4);
  if (err == ERR_OK) {
    return network::IPAddress(&addr);
  }
  while (!this->dns_resolved_) {
    switch (err) {
      case ERR_OK: {
        // Got IP immediately
        this->dns_resolved_ = true;
        this->ip_ = network::IPAddress(&addr);
        return this->ip_;
      }
      case ERR_INPROGRESS: {
        // wait for callback
        ESP_LOGD(TAG, "Resolving IP address...");
        break;
      }
      default:
      case ERR_ARG: {
        // error
        ESP_LOGW(TAG, "Error resolving IP address: %d", err);
        break;
      }
    }
    // TODO: Add timeout
    delay_microseconds_safe(100);
  }
  return this->ip_;
}

void Resolver::dns_found_callback(const char *name, const ip_addr_t *ipaddr, void *callback_arg) {
  auto *a_this = (Resolver *) callback_arg;
  if (ipaddr == nullptr) {
    a_this->dns_resolve_error_ = true;
  } else {
    a_this->ip_ = network::IPAddress(ipaddr);
    a_this->dns_resolved_ = true;
  }
}

Resolver *global_resolver = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace network
}  // namespace esphome
