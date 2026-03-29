#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/sensirion_common/i2c_sensirion.h"

namespace esphome::sen6x {

class SEN6XComponent : public PollingComponent, public sensirion_common::SensirionI2CDevice {
  SUB_SENSOR(pm_1_0)
  SUB_SENSOR(pm_2_5)
  SUB_SENSOR(pm_4_0)
  SUB_SENSOR(pm_10_0)
  SUB_SENSOR(temperature)
  SUB_SENSOR(humidity)
  SUB_SENSOR(voc)
  SUB_SENSOR(nox)
  SUB_SENSOR(co2)
  SUB_SENSOR(hcho)

 public:
  float get_setup_priority() const override { return setup_priority::DATA; }
  void setup() override;
  void dump_config() override;
  void update() override;

  enum Sen6xType { SEN62, SEN63C, SEN65, SEN66, SEN68, SEN69C, UNKNOWN };

  void set_type(const std::string &type) { sen6x_type_ = infer_type_from_product_name_(type); }

 protected:
  Sen6xType infer_type_from_product_name_(const std::string &product_name);
  void poll_data_ready_();
  void read_measurements_();
  void parse_and_publish_measurements_();

  bool initialized_{false};
  std::string product_name_;
  Sen6xType sen6x_type_{UNKNOWN};
  std::string serial_number_;
  uint16_t read_cmd_{0};
  uint8_t firmware_version_major_{0};
  uint8_t firmware_version_minor_{0};
  uint8_t poll_retries_remaining_{0};
  uint8_t read_words_{0};
  bool startup_complete_{false};
};

}  // namespace esphome::sen6x
