#include <vector>
#include <utility>
#include "resolver.h"
#if !defined(USE_HOST)
#include "lwip/dns.h"
#endif
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/hal.h"
#include "esphome/core/defines.h"
#ifdef USE_MDNS
#include "esphome/components/mdns/mdns_component.h"
#endif

#ifdef ENABLE_IPV6
#define ESPHOME_DNS_ADDRTYPE LWIP_DNS_ADDRTYPE_IPV6_IPV4
#else
#define ESPHOME_DNS_ADDRTYPE LWIP_DNS_ADDRTYPE_IPV4
#endif

namespace esphome {
namespace network {

static const char *const TAG = "resolver";

Resolver::Resolver() { global_resolver = this; }
Resolver::Resolver(std::multimap<std::string, network::IPAddress> hosts) : hosts_(std::move(hosts)) {
  global_resolver = this;
}
// TODO(HeMan): resolve needs to return multiple IP addresses
std::vector<network::IPAddress> Resolver::resolve(const std::string &hostname) {
  if (this->hosts_.count(hostname) > 0) {
    std::vector<network::IPAddress> resolved;
    for (auto a = hosts_.find(hostname); a != hosts_.end(); a++) {
      resolved.push_back(a->second);
      ESP_LOGVV(TAG, "Found %s in hosts section", hostname.c_str());
    }
    return resolved;
  }
#ifdef USE_MDNS
  ESP_LOGV(TAG, "Looking for %s with mDNS", hostname.c_str());
  std::vector<network::IPAddress> resolved_mdns = mdns::global_mdns->resolve(hostname);
  if (!resolved_mdns.empty()) {
    ESP_LOGVV(TAG, "Found %s in mDNS", hostname.c_str());
    return resolved_mdns;
  }
#endif
  ip_addr_t addr;
  ESP_LOGVV(TAG, "Resolving %s", hostname.c_str());
#if !defined(USE_HOST)
  err_t err =
      dns_gethostbyname_addrtype(hostname.c_str(), &addr, Resolver::dns_found_callback, this, ESPHOME_DNS_ADDRTYPE);
  if (err == ERR_OK) {
    return {network::IPAddress(&addr)};
  }
  this->connect_begin_ = millis();
  while (!this->dns_resolved_ && !this->dns_resolve_error_ && (millis() - this->connect_begin_ < 2000)) {
    switch (err) {
      case ERR_OK: {
        // Got IP immediately
        ESP_LOGVV(TAG, "Found %s in DNS", hostname.c_str());
        this->dns_resolved_ = true;
        this->ip_ = network::IPAddress(&addr);
        return {this->ip_};
      }
      case ERR_INPROGRESS: {
        // wait for callback
        ESP_LOGVV(TAG, "Resolving IP address...");
        break;
      }
      default:
      case ERR_ARG: {
        // error
        ESP_LOGW(TAG, "Error resolving IP address: %d", err);
        break;
      }
    }
    delay_microseconds_safe(100);
  }
  if (this->dns_resolve_error_) {
    ESP_LOGV(TAG, "Error resolving IP address");
  }
  if (!this->dns_resolved_) {
    ESP_LOGVV(TAG, "Not resolved");
  }
#endif
  return {this->ip_};
}

void Resolver::dns_found_callback(const char *name, const ip_addr_t *ipaddr, void *callback_arg) {
  auto *a_this = reinterpret_cast<Resolver *>(callback_arg);
  if (ipaddr == nullptr) {
    a_this->dns_resolve_error_ = true;
  } else {
    ESP_LOGVV(TAG, "Found %s in DNS", name);
    a_this->ip_ = network::IPAddress(ipaddr);
    a_this->dns_resolved_ = true;
  }
}

Resolver *global_resolver = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace network
}  // namespace esphome
