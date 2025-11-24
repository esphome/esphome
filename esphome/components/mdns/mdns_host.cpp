#include "esphome/core/defines.h"
#if defined(USE_HOST) && defined(USE_MDNS)

#include "esphome/components/network/ip_address.h"
#include "esphome/components/network/util.h"
#include "esphome/core/log.h"
#include "mdns_component.h"

namespace esphome::mdns {

void MDNSComponent::setup() {
#ifdef USE_API
  char mac_address[MAC_ADDRESS_BUFFER_SIZE];
  get_mac_address_into_buffer(std::span<char, MAC_ADDRESS_BUFFER_SIZE>(mac_address));
#else
  char *mac_address = nullptr;
#endif

  this->on_setup_(mac_address);
  // Host platform doesn't have actual mDNS implementation
}

void MDNSComponent::on_shutdown() {}

}  // namespace esphome::mdns

#endif
