#include "mdns_component.h"

#ifdef USE_ZEPHYR

namespace esphome::mdns {

void MDNSComponent::setup() {}

void MDNSComponent::on_shutdown() {}

}  // namespace esphome::mdns

#endif  // USE_ZEPHYR
