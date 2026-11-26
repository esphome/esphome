#pragma once

#include <utility>

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome::aht10 {

enum AHT10Variant { AHT10, AHT20 };

class AHT10Component final : public PollingComponent, public i2c::I2CDevice {
  SUB_SENSOR(temperature)
  SUB_SENSOR(humidity)

 public:
  void setup() override;
  void update() override;
  void dump_config() override;
  void set_variant(AHT10Variant variant) { this->variant_ = variant; }

 protected:
  AHT10Variant variant_{};
  unsigned read_count_{};
  void read_data_();
  void restart_read_();
  uint32_t start_time_{};
};

}  // namespace esphome::aht10
