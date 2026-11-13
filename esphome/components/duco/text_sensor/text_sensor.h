#pragma once

#include "esphome/components/text_sensor/text_sensor.h"

#include "../duco.h"

namespace esphome::duco {

/// What a text sensor reads.
enum class DucoTextReading : uint8_t {
  DUCO_TEXT_READING_SERIAL_NUMBER,
  DUCO_TEXT_READING_VERSION,
  DUCO_TEXT_READING_ERROR,
  DUCO_TEXT_READING_UPTIME,
  DUCO_TEXT_READING_NODE_TYPE,
};

class DucoTextSensor : public DucoDevice, public text_sensor::TextSensor {
 public:
  /// address is the node address; a serial number at address 0 is the box itself.
  DucoTextSensor(DucoTextReading reading, uint8_t address) : reading_(reading), address_(address) {}

  void update() override;
  void receive_response(const DucoMessage &message) override;

 protected:
  void publish_serial_number_(const DucoMessage &message);
  void publish_version_(const DucoMessage &message);
  void publish_errors_(const DucoMessage &message);
  void publish_uptime_(const DucoMessage &message);

  DucoTextReading reading_;
  uint8_t address_;
};

}  // namespace esphome::duco
