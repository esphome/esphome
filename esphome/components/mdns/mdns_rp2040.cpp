#include "esphome/core/defines.h"
#if defined(USE_RP2040) && defined(USE_MDNS)

#include <ESP8266mDNS.h>

#include "mdns_arduino.h"

namespace esphome::mdns {

void MDNSComponent::setup() { this->setup_buffers_and_register_(register_arduino_mdns); }

void MDNSComponent::loop() { MDNS.update(); }

void MDNSComponent::on_shutdown() {
  MDNS.close();
  delay(40);
}

}  // namespace esphome::mdns

#endif
