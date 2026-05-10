#include "mdns_component.h"

#ifdef USE_ZEPHYR

namespace esphome {
namespace mdns {

void MDNSComponent::setup() {}

void MDNSComponent::on_shutdown() {}

}  // namespace mdns
}  // namespace esphome

#endif  // USE_ZEPHYR
