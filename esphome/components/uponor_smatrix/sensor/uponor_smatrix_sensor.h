#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uponor_smatrix/uponor_smatrix.h"

namespace esphome::uponor_smatrix {

class UponorSmatrixSensor final : public sensor::Sensor, public UponorSmatrixDevice {
  SUB_SENSOR(temperature)
  SUB_SENSOR(external_temperature)
  SUB_SENSOR(humidity)
  SUB_SENSOR(target_temperature)

 public:
  void dump_child_config() override;

 protected:
  void on_device_data(const UponorSmatrixData *data, size_t data_len) override;
};

}  // namespace esphome::uponor_smatrix
