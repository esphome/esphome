#include "esphome/core/defines.h"
#if defined(USE_ZEPHYR) && defined(USE_MDNS)

#include "mdns_component.h"
#include "esphome/core/log.h"

namespace esphome::mdns {

static const char *const TAG = "mdns.zephyr";

void MDNSComponent::setup() { ESP_LOGW(TAG, "mDNS is not implemented for Zephyr"); }

void MDNSComponent::on_shutdown() {}

}  // namespace esphome::mdns

#endif  // USE_ZEPHYR && USE_MDNS
