#pragma once

#include "esphome/core/component.h"
#include "esphome/core/frame_reader.h"
#include "esphome/core/helpers.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"

namespace esphome::sds011 {

class SDS011Component final : public Component, public uart::UARTDevice {
  SUB_SENSOR(pm_2_5)
  SUB_SENSOR(pm_10_0)

 public:
  SDS011Component() = default;

  /// Manually set the rx-only mode. Defaults to false.
  void set_rx_mode_only(bool rx_mode_only) { this->rx_mode_only_ = rx_mode_only; }

  void setup() override;
  void dump_config() override;
  void loop() override;

  void set_update_interval_min(uint8_t update_interval_min);
  void set_working_state(bool working_state);

 protected:
  void sds011_write_command_(const uint8_t *command);
  uint8_t sds011_checksum_(const uint8_t *command_data, uint8_t length) const;
  bool check_frame_(const uint8_t *frame) const;
  void parse_data_(const uint8_t *frame);

  FrameReader<10, 0xAA, 0xC0> reader_;
  uint32_t last_transmission_{0};
  uint8_t update_interval_min_;

  bool rx_mode_only_;
};

}  // namespace esphome::sds011
