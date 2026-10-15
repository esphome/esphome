#pragma once

#include "hon_parser.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/water_heater/water_heater.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/component.h"

namespace esphome::haier {

class HonWaterHeater : public Component, public water_heater::WaterHeater, public uart::UARTDevice {
 public:
  void set_primary_temperature_sensor(sensor::Sensor *value) { this->primary_sensor_ = value; }
  void set_secondary_temperature_sensor(sensor::Sensor *value) { this->secondary_sensor_ = value; }
  void setup() override;
  void loop() override;
  void dump_config() override;
  water_heater::WaterHeaterCallInternal make_call() override;
  void set_target_temperature_sensor(sensor::Sensor *value) { this->target_sensor_ = value; }
  void set_estimated_heating_time_sensor(sensor::Sensor *value) { this->heating_time_sensor_ = value; }
  void set_available_water_sensor(sensor::Sensor *value) { this->water_sensor_ = value; }
  void set_communication_timeout(uint32_t value) { this->communication_timeout_ = value; }
  void set_communication_sensor(binary_sensor::BinarySensor *value) { this->communication_sensor_ = value; }
  void set_power_sensor(binary_sensor::BinarySensor *value) { this->power_sensor_ = value; }
  void set_heating_sensor(binary_sensor::BinarySensor *value) { this->heating_sensor_ = value; }
  void set_mode_sensor(text_sensor::TextSensor *value) { this->mode_sensor_ = value; }

 protected:
  static constexpr uint32_t NETWORK_STATUS_INTERVAL_ID = 0;
  static constexpr uint32_t STATUS_POLL_INTERVAL_ID = 1;

  void control(const water_heater::WaterHeaterCall &call) override;
  water_heater::WaterHeaterTraits traits() override;
  void process_frame_();
  void poll_status_();
  void expire_poll_(uint32_t now);
  void send_frame_(uint8_t type, const uint8_t *payload = nullptr, size_t size = 0);
  void send_next_control_();
  void update_network_status_();
  // Widest members first, so the narrow flags pack together at the end.
  water_heater_protocol::FrameParser parser_;
  sensor::Sensor *primary_sensor_{nullptr};
  sensor::Sensor *secondary_sensor_{nullptr};
  sensor::Sensor *target_sensor_{nullptr};
  sensor::Sensor *heating_time_sensor_{nullptr};
  sensor::Sensor *water_sensor_{nullptr};
  binary_sensor::BinarySensor *communication_sensor_{nullptr};
  binary_sensor::BinarySensor *power_sensor_{nullptr};
  binary_sensor::BinarySensor *heating_sensor_{nullptr};
  text_sensor::TextSensor *mode_sensor_{nullptr};
  uint32_t network_sent_{0};
  uint32_t last_status_{0};
  uint32_t communication_timeout_{20000};
  uint32_t control_sent_{0};
  uint32_t poll_sent_{0};
  uint32_t last_byte_{0};
  water_heater::WaterHeaterMode last_active_mode_{water_heater::WATER_HEATER_MODE_ELECTRIC};
  std::array<std::array<uint8_t, 4>, 4> commands_{};
  water_heater_protocol::HonMode reported_mode_{water_heater_protocol::HonMode::HON_MODE_UNKNOWN};
  uint8_t network_status_{0x05};
  uint8_t command_count_{0};
  uint8_t command_index_{0};
  uint8_t poll_stage_{0};
  uint8_t expected_type_{0};
  bool network_pending_{true};
  bool waiting_network_{false};
  bool have_status_{false};
  bool waiting_control_{false};
  bool pending_off_{false};
  bool waiting_poll_{false};
};

}  // namespace esphome::haier
