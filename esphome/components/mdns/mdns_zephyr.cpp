#include "esphome/core/defines.h"
#if defined(USE_ZEPHYR) && defined(USE_MDNS)

#include "mdns_component.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome::mdns {

static const char *const TAG = "mdns.zephyr";

void MDNSComponent::setup() {
  ESP_LOGD(TAG, "Setting up mDNS for Zephyr...");
#ifdef USE_MDNS_STORE_SERVICES
#if defined(USE_API)
  get_mac_address_into_buffer(this->mac_address_);
  char *mac_ptr = this->mac_address_;
#else
  char *mac_ptr = nullptr;
#endif
  this->compile_records_(this->services_, mac_ptr, nullptr);
#else
  StaticVector<MDNSService, MDNS_SERVICE_COUNT> services;
  char *mac_ptr = nullptr;
  this->compile_records_(services, mac_ptr, nullptr);
#endif

  ESP_LOGI(TAG, "mDNS records compiled, waiting for network connectivity before registering services");
}

void MDNSComponent::on_shutdown() { ESP_LOGD(TAG, "Shutting down mDNS for Zephyr..."); }

}  // namespace esphome::mdns

#endif  // USE_ZEPHYR && USE_MDNS
