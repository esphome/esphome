#pragma once

#include "esphome/components/gnss/gnss.h"
#include "esphome/components/uart/uart.h"
#include <TinyGPSPlus.h>

namespace esphome::nmea {

/// A receiver that sends NMEA 0183 sentences.
class NMEAGNSS final : public gnss::GNSSComponent, public uart::UARTDevice {
 public:
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  void dump_config() override;
  void loop() override;

 protected:
  void handle_message_();

  TinyGPSPlus tiny_gps_;
};

}  // namespace esphome::nmea
