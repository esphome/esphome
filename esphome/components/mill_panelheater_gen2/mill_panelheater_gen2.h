#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "esphome/components/climate/climate.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"

namespace esphome::mill_panelheater_gen2 {

class MillPanelHeaterGen2 : public climate::Climate, public Component, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;
  void control(const climate::ClimateCall &call) override;
  void dump_config() override;

  void set_power_sensor(sensor::Sensor *power_sensor) { this->power_sensor_ = power_sensor; }
  void set_rated_power(float rated_power) { this->rated_power_ = rated_power; }

 protected:
  climate::ClimateTraits traits() override;

  void send_power_command_(uint8_t mode_value);
  void send_temperature_command_(uint8_t temperature);

 private:
  static constexpr size_t RECEIVE_BUFFER_SIZE = 15;
  static constexpr size_t COMMAND_PAYLOAD_SIZE = 13;

  void receive_byte_();
  void log_frame_(const char *message, uint8_t last_byte) const;
  void reset_receive_state_();
  void start_receive_frame_();
  void publish_power_state_();
  void reset_communication_timeout_();
  void send_command_(std::array<uint8_t, COMMAND_PAYLOAD_SIZE> payload, size_t value_position, uint8_t value);
  static uint8_t checksum(const uint8_t *data, size_t length);

  static constexpr size_t FRAME_LENGTH_POS = 1;
  static constexpr size_t COMMAND_TYPE_POS = 4;
  static constexpr size_t TARGET_TEMP_POS = 6;
  static constexpr size_t CURRENT_TEMP_POS = 7;
  static constexpr size_t MODE_POS = 9;
  static constexpr size_t ACTION_POS = 11;
  static constexpr size_t POWER_COMMAND_VALUE_POS = 5;
  static constexpr size_t TEMPERATURE_COMMAND_VALUE_POS = 7;

  static constexpr uint8_t START_MARKER = 0x5A;
  static constexpr uint8_t END_MARKER = 0x5B;
  static constexpr uint8_t STATUS_COMMAND_TYPE = 0xC9;
  static constexpr uint8_t MIN_TARGET_TEMPERATURE = 5;
  static constexpr uint8_t MAX_TARGET_TEMPERATURE = 35;
  static constexpr uint8_t PROTOCOL_MODE_OFF = 0x00;
  static constexpr uint8_t PROTOCOL_MODE_HEAT = 0x01;
  static constexpr uint8_t PROTOCOL_ACTION_IDLE = 0x00;
  static constexpr uint8_t PROTOCOL_ACTION_HEATING = 0x01;

  static constexpr size_t FRAME_OVERHEAD_SIZE = 2;
  static constexpr size_t MIN_FRAME_LENGTH = 4;
  static constexpr size_t STATUS_FRAME_LENGTH = 17;
  static constexpr uint32_t RECEIVE_TIMEOUT_MS = 100;
  static constexpr uint32_t COMMUNICATION_TIMEOUT_MS = 150000;

  std::array<uint8_t, RECEIVE_BUFFER_SIZE> received_data_{};
  size_t received_length_{0};
  size_t expected_payload_length_{0};
  uint32_t last_receive_byte_time_{0};
  bool receive_in_progress_{false};
  bool new_data_{false};

  sensor::Sensor *power_sensor_{nullptr};
  float rated_power_{0.0f};
};

}  // namespace esphome::mill_panelheater_gen2
