#include "esphome/core/defines.h"
#if defined(USE_LIBRETINY) && defined(USE_MDNS)

#include <mDNS.h>

#include "mdns_arduino.h"

namespace esphome::mdns {

void MDNSComponent::setup() { this->setup_buffers_and_register_(register_arduino_mdns); }

void MDNSComponent::on_shutdown() {}

}  // namespace esphome::mdns

#endif
