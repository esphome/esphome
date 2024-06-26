#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace gcja5 {

class GCJA5Component : public Component, public uart::UARTDevice {
 public:
  void setup() override;
  void dump_config() override;
  void loop() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_pm_1_0_sensor(sensor::Sensor *pm_1_0) { pm_1_0_sensor_ = pm_1_0; }
  void set_pm_2_5_sensor(sensor::Sensor *pm_2_5) { pm_2_5_sensor_ = pm_2_5; }
  void set_pm_10_0_sensor(sensor::Sensor *pm_10_0) { pm_10_0_sensor_ = pm_10_0; }

  void set_pmc_0_3_sensor(sensor::Sensor *pmc_0_3) { pmc_0_3_sensor_ = pmc_0_3; }
  void set_pmc_0_5_sensor(sensor::Sensor *pmc_0_5) { pmc_0_5_sensor_ = pmc_0_5; }
  void set_pmc_1_0_sensor(sensor::Sensor *pmc_1_0) { pmc_1_0_sensor_ = pmc_1_0; }
  void set_pmc_2_5_sensor(sensor::Sensor *pmc_2_5) { pmc_2_5_sensor_ = pmc_2_5; }
  void set_pmc_5_0_sensor(sensor::Sensor *pmc_5_0) { pmc_5_0_sensor_ = pmc_5_0; }
  void set_pmc_10_0_sensor(sensor::Sensor *pmc_10_0) { pmc_10_0_sensor_ = pmc_10_0; }

 protected:
  void parse_data_();
  bool calculate_checksum_();

  uint32_t get_32_bit_uint_(uint8_t start_index);
  uint16_t get_16_bit_uint_(uint8_t start_index);
  uint32_t last_transmission_{0};
  std::vector<uint8_t> rx_message_;

  bool have_good_data_{false};
  bool first_status_log_{false};
  sensor::Sensor *pm_1_0_sensor_{nullptr};
  sensor::Sensor *pm_2_5_sensor_{nullptr};
  sensor::Sensor *pm_10_0_sensor_{nullptr};

  sensor::Sensor *pmc_0_3_sensor_{nullptr};
  sensor::Sensor *pmc_0_5_sensor_{nullptr};
  sensor::Sensor *pmc_1_0_sensor_{nullptr};
  sensor::Sensor *pmc_2_5_sensor_{nullptr};
  sensor::Sensor *pmc_5_0_sensor_{nullptr};
  sensor::Sensor *pmc_10_0_sensor_{nullptr};
};

}  // namespace gcja5
}  // namespace esphome
