#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uponor_smatrix/uponor_smatrix.h"
#include "esphome/core/component.h"

namespace esphome::uponor_smatrix {

class UponorSmatrixSensor final : public sensor::Sensor, public Component, public UponorSmatrixDevice {
  SUB_SENSOR(temperature)
  SUB_SENSOR(external_temperature)
  SUB_SENSOR(humidity)
  SUB_SENSOR(target_temperature)

 public:
  void dump_config() override;
  void loop() override;

 protected:
  void on_device_data(const UponorSmatrixData *data, size_t data_len) override;

  uint32_t last_data_;
  uint16_t eco_setback_value_raw_{0x0048};
  uint16_t heating_cooling_offset_raw_{0x0024};
  uint16_t target_temperature_raw_;
  bool eco_mode_;
  bool cooling_;
};

}  // namespace esphome::uponor_smatrix
