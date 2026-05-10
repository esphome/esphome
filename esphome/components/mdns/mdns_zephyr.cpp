#include "esphome/core/defines.h"
#if defined(USE_ZEPHYR) && defined(USE_MDNS)

#include "mdns_component.h"

namespace esphome::mdns {

void MDNSComponent::setup() {}

void MDNSComponent::on_shutdown() {}

}  // namespace esphome::mdns

#endif  // USE_ZEPHYR && USE_MDNS
