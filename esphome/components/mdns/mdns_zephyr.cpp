#include "esphome/core/defines.h"
#ifdef USE_MDNS
#ifdef USE_ZEPHYR

#include "mdns_component.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome {
namespace mdns {

static const char *const TAG = "mdns";

void MDNSComponent::setup() {
  ESP_LOGI(TAG, "MDNS setup for Zephyr - stub implementation");
  // TODO: Implement mDNS for Zephyr
  // Zephyr has net_dns_sd for DNS-SD/mDNS support
  // For now, this is a stub to allow linking

  // Compile the service records (required for OpenThread SRP)
  this->compile_records_(this->services_);
}

void MDNSComponent::on_shutdown() {
  // TODO: Cleanup mDNS resources
}

}  // namespace mdns
}  // namespace esphome

#endif  // USE_ZEPHYR
#endif  // USE_MDNS
