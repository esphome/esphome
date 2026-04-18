#include "esphome/core/defines.h"
#if defined(USE_HOST) && defined(USE_MDNS)

#include "esphome/components/network/ip_address.h"
#include "esphome/components/network/util.h"
#include "esphome/core/log.h"
#include "mdns_component.h"

namespace esphome::mdns {

void MDNSComponent::setup() {
#ifdef USE_MDNS_STORE_SERVICES
#ifdef USE_API
  get_mac_address_into_buffer(this->mac_address_);
  char *mac_ptr = this->mac_address_;
#else
  char *mac_ptr = nullptr;
#endif
  this->compile_records_(this->services_, mac_ptr);
#endif
  // Host platform doesn't have actual mDNS implementation
}

void MDNSComponent::on_shutdown() {}

}  // namespace esphome::mdns

#endif
