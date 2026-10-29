#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"

namespace esphome::gcja5 {

class GCJA5Component final : public Component, public uart::UARTDevice {
  SUB_SENSOR(pm_1_0)
  SUB_SENSOR(pm_2_5)
  SUB_SENSOR(pm_10_0)
  SUB_SENSOR(pmc_0_3)
  SUB_SENSOR(pmc_0_5)
  SUB_SENSOR(pmc_1_0)
  SUB_SENSOR(pmc_2_5)
  SUB_SENSOR(pmc_5_0)
  SUB_SENSOR(pmc_10_0)

 public:
  void dump_config() override;
  void loop() override;

 protected:
  void parse_data_();
  bool calculate_checksum_();

  uint16_t get_16_bit_uint_(uint8_t start_index) const {
    return encode_uint16(this->rx_message_[start_index + 1], this->rx_message_[start_index]);
  }
  uint32_t get_32_bit_uint_(uint8_t start_index) const {
    return encode_uint32(this->rx_message_[start_index + 3], this->rx_message_[start_index + 2],
                         this->rx_message_[start_index + 1], this->rx_message_[start_index]);
  }
  uint32_t last_transmission_{0};
  std::vector<uint8_t> rx_message_;

  bool have_good_data_{false};
  bool first_status_log_{false};
};

}  // namespace esphome::gcja5
