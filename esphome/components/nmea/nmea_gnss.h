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

  TinyGPSPlus &get_tiny_gps() { return this->tiny_gps_; }

 protected:
  TinyGPSPlus tiny_gps_;
};

}  // namespace esphome::nmea
