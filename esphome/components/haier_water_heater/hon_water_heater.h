#pragma once

#include "hon_parser.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/water_heater/water_heater.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/component.h"

namespace esphome::haier_water_heater {

class HonWaterHeater : public Component, public water_heater::WaterHeater, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  water_heater::WaterHeaterCallInternal make_call() override;

  void set_communication_timeout(uint32_t value) { this->communication_timeout_ = value; }
  void set_primary_temperature_sensor(sensor::Sensor *value) { this->primary_sensor_ = value; }
  void set_secondary_temperature_sensor(sensor::Sensor *value) { this->secondary_sensor_ = value; }
  void set_target_temperature_sensor(sensor::Sensor *value) { this->target_sensor_ = value; }
  void set_estimated_heating_time_sensor(sensor::Sensor *value) { this->heating_time_sensor_ = value; }
  void set_available_water_sensor(sensor::Sensor *value) { this->water_sensor_ = value; }
  void set_communication_sensor(binary_sensor::BinarySensor *value) { this->communication_sensor_ = value; }
  void set_power_sensor(binary_sensor::BinarySensor *value) { this->power_sensor_ = value; }
  void set_heating_sensor(binary_sensor::BinarySensor *value) { this->heating_sensor_ = value; }
  void set_mode_sensor(text_sensor::TextSensor *value) { this->mode_sensor_ = value; }

 protected:
  static constexpr uint32_t NETWORK_STATUS_INTERVAL_ID = 0;
  static constexpr uint32_t STATUS_POLL_INTERVAL_ID = 1;
  static constexpr uint32_t NETWORK_STATUS_INTERVAL_MS = 500;
  static constexpr uint32_t POLL_INTERVAL_MS = 5000;
  static constexpr uint32_t RESPONSE_TIMEOUT_MS = 5000;         // a polled or controlled reply
  static constexpr uint32_t NETWORK_ACK_TIMEOUT_MS = 2000;      // the CONFIRM of a network status
  static constexpr uint32_t NETWORK_REFRESH_MS = 30000;         // resend the network status this often
  static constexpr uint32_t CONTROL_STATUS_MAX_AGE_MS = 15000;  // a control needs a status this fresh
  static constexpr uint32_t FRAME_GAP_MS = 300;                 // silence that ends a partial frame
  static constexpr uint8_t POLL_CYCLE = 12;                     // version and id are re-read once per cycle
  static constexpr uint8_t MAX_BYTES_PER_LOOP = 255;
  static constexpr uint8_t MIN_TARGET_TEMPERATURE = 35;
  static constexpr uint8_t MAX_TARGET_TEMPERATURE = 75;
  static constexpr uint8_t NETWORK_READY = 0x07;
  static constexpr uint8_t NETWORK_NOT_READY = 0x05;

  // The link carries one request at a time; this is the one whose answer is outstanding.
  enum class Awaiting : uint8_t { AWAITING_NONE, AWAITING_POLL, AWAITING_NETWORK, AWAITING_CONTROL };

  void control(const water_heater::WaterHeaterCall &call) override;
  water_heater::WaterHeaterTraits traits() override;
  void check_timeouts_(uint32_t now);
  void send_pending_(uint32_t now);
  void read_frames_(uint32_t now);
  void poll_status_();
  void update_network_status_();
  void send_frame_(water_heater_protocol::FrameType type, const uint8_t *payload = nullptr, size_t size = 0);
  void send_request_(uint32_t now, Awaiting kind, water_heater_protocol::FrameType type, const uint8_t *payload,
                     size_t size, water_heater_protocol::FrameType expected);
  void send_next_control_(uint32_t now);
  void process_frame_();
  void handle_frame_type_(water_heater_protocol::FrameType type);
  void confirm_control_(const water_heater_protocol::Status &status);
  void publish_status_(const water_heater_protocol::Status &status);

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
  uint32_t request_sent_{0};
  uint32_t network_sent_{0};
  uint32_t last_status_{0};
  uint32_t last_byte_{0};
  uint32_t communication_timeout_{20000};
  water_heater::WaterHeaterMode last_active_mode_{water_heater::WATER_HEATER_MODE_ELECTRIC};
  std::array<water_heater_protocol::Command, 4> commands_{};  // power, to Normal, mode, temperature at most
  water_heater_protocol::HonMode reported_mode_{water_heater_protocol::HonMode::HON_MODE_UNKNOWN};
  water_heater_protocol::FrameType expected_type_{water_heater_protocol::FrameType::FRAME_TYPE_STATUS};
  Awaiting awaiting_{Awaiting::AWAITING_NONE};
  uint8_t network_status_{NETWORK_NOT_READY};
  uint8_t command_count_{0};
  uint8_t command_index_{0};
  uint8_t poll_stage_{0};
  bool network_pending_{true};
  bool have_status_{false};
  bool pending_off_{false};
};

}  // namespace esphome::haier_water_heater
