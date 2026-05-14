#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"

namespace esphome::elm327 {

// ELM327 OBD-II interpreter: https://www.elmelectronics.com/wp-content/uploads/2016/07/ELM327DS.pdf

class ELM327Component : public PollingComponent, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;
  void update() override;
  void dump_config() override;

  void set_engine_rpm_sensor(sensor::Sensor *s) { this->engine_rpm_ = s; }
  void set_vehicle_speed_sensor(sensor::Sensor *s) { this->vehicle_speed_ = s; }
  void set_coolant_temperature_sensor(sensor::Sensor *s) { this->coolant_temperature_ = s; }
  void set_engine_load_sensor(sensor::Sensor *s) { this->engine_load_ = s; }
  void set_throttle_position_sensor(sensor::Sensor *s) { this->throttle_position_ = s; }
  void set_intake_air_temperature_sensor(sensor::Sensor *s) { this->intake_air_temperature_ = s; }
  void set_maf_rate_sensor(sensor::Sensor *s) { this->maf_rate_ = s; }
  void set_fuel_level_sensor(sensor::Sensor *s) { this->fuel_level_ = s; }
  void set_battery_voltage_sensor(sensor::Sensor *s) { this->battery_voltage_ = s; }

 protected:
  enum class State : uint8_t {
    // Initialisation sequence — each state means "command sent, awaiting response"
    INIT_ATZ = 0,
    INIT_ATE0,
    INIT_ATL0,
    INIT_ATH0,
    INIT_ATAT1,
    INIT_ATSP0,
    // Normal operation
    IDLE,
    QUERYING,
  };

  void send_cmd_(const char *cmd);
  void on_response_();
  void send_next_query_();
  void publish_pid_(uint8_t sensor_index, const char *response);
  static bool parse_response_bytes(const char *response, uint8_t *data, uint8_t expected_bytes);

  // Response accumulator — cleared before each command, filled byte-by-byte in loop()
  char rx_buf_[64];
  size_t rx_pos_{0};

  State state_{State::INIT_ATZ};
  uint32_t cmd_sent_at_{0};

  // Which sensor slot to query next (round-robin through configured sensors)
  uint8_t sensor_index_{0};
  // Set by update() to kick off a new query cycle
  bool query_pending_{false};

  sensor::Sensor *engine_rpm_{nullptr};
  sensor::Sensor *vehicle_speed_{nullptr};
  sensor::Sensor *coolant_temperature_{nullptr};
  sensor::Sensor *engine_load_{nullptr};
  sensor::Sensor *throttle_position_{nullptr};
  sensor::Sensor *intake_air_temperature_{nullptr};
  sensor::Sensor *maf_rate_{nullptr};
  sensor::Sensor *fuel_level_{nullptr};
  sensor::Sensor *battery_voltage_{nullptr};
};

}  // namespace esphome::elm327
