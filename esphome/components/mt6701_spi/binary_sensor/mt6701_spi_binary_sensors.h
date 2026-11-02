#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/mt6701_spi/mt6701_spi.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

namespace esphome::mt6701_spi {

// Reads the encoder at its own interval and publishes the status flags of each frame
class MT6701SPIBinarySensors : public Component, public Parented<MT6701SPIComponent> {
  SUB_BINARY_SENSOR(push_button)
  SUB_BINARY_SENSOR(track_loss)

 public:
  void set_update_interval(uint32_t update_interval) { this->update_interval_ = update_interval; }
  void loop() override;
  void dump_config() override;

 protected:
  uint32_t update_interval_{50};
  uint32_t last_read_{0};
};

}  // namespace esphome::mt6701_spi
