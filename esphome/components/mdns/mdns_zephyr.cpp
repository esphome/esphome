#include "esphome/core/defines.h"
#if defined(USE_ZEPHYR) && defined(USE_MDNS)

#include "mdns_component.h"
#include "esphome/core/log.h"

namespace esphome::mdns {

static const char *const TAG = "mdns.zephyr";

static void register_zephyr(MDNSComponent *mdns, StaticVector<MDNSService, MDNS_SERVICE_COUNT> &services) {
  ESP_LOGD(TAG, "mDNS records compiled");
}

void MDNSComponent::setup() { this->setup_buffers_and_register_(register_zephyr); }

void MDNSComponent::on_shutdown() {}

}  // namespace esphome::mdns

#endif  // USE_ZEPHYR && USE_MDNS
