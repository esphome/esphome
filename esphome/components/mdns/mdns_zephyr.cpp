#include "mdns_component.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

#ifdef USE_ZEPHYR

namespace esphome {
namespace mdns {

static const char *const TAG = "mdns.zephyr";

void MDNSComponent::setup() {
  ESP_LOGD(TAG, "Setting up mDNS for Zephyr...");
#ifdef USE_MDNS_STORE_SERVICES
  this->compile_records_(this->services_);
  const auto &services = this->services_;
#else
  StaticVector<MDNSService, MDNS_SERVICE_COUNT> services;
  this->compile_records_(services);
#endif

  ESP_LOGI(TAG, "mDNS records compiled, waiting for network connectivity before registering services");
}

void MDNSComponent::on_shutdown() {
  ESP_LOGD(TAG, "Shutting down mDNS for Zephyr...");
  // No specific shutdown actions needed for Zephyr mDNS
  // DNS-SD services are automatically unregistered when the application exits
}

}  // namespace mdns
}  // namespace esphome

#endif  // USE_ZEPHYR
