#pragma once

#include <chrono>
#include <list>
#include <utility>

#include "esphome/components/climate/climate.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/number/number.h"
#include "esphome/components/select/select.h"
#include "esphome/components/uart/uart.h"
#include "mitsubishi_protocol.h"

namespace esphome {
namespace climate_mitsubishi {

enum class ResponseType {
  NO_RESPONSE,
  INVALID,
  UNKNOWN,
  CONNECT_SUCCESS,
  SET_SUCCESS,
  SETTINGS,
  ROOM_TEMP,
  STATUS,
  SENSORS
};

enum class RequestState { CONSTRUCTED, QUEUED, WRITING, WAITING, READING };

struct RequestSlot {
  uint8_t request_packet_[mitsubishi_protocol::PACKET_LEN];

  RequestState state_;
  ResponseType response_type_;
  ResponseType expected_response_type_;

  std::chrono::steady_clock::time_point transmit_timestamp_;
};

class ClimateMitsubishi : public esphome::Component,
                          public esphome::climate::Climate,
                          public esphome::uart::UARTDevice {
 public:
  ClimateMitsubishi();

  void setup() override;
  void loop() override;
  void control(const esphome::climate::ClimateCall &call) override;
  void set_vertical_airflow_direction(const std::string &direction);

  void set_inject_enable(bool enable);

  size_t read_array(uint8_t *data, size_t len) noexcept {
    return esphome::uart::UARTDevice::read_array(data, len) ? len : 0;
  };
  size_t available() noexcept { return esphome::uart::UARTDevice::available(); };
  void write_array(const uint8_t *data, size_t len) noexcept { esphome::uart::UARTDevice::write_array(data, len); };
  void flush() noexcept { esphome::uart::UARTDevice::flush(); }
  inline int read() { return esphome::uart::UARTDevice::read(); };

  void set_compressor_frequency_sensor(esphome::sensor::Sensor *sensor);
  void set_fan_velocity_sensor(esphome::sensor::Sensor *sensor);
  void set_conflicted_sensor(esphome::binary_sensor::BinarySensor *sensor);
  void set_preheat_sensor(esphome::binary_sensor::BinarySensor *sensor);
  void set_control_temperature_sensor(esphome::sensor::Sensor *sensor);

  void set_vertical_airflow_select(esphome::select::Select *select);

  void set_remote_temperature_number(esphome::number::Number *number);
  void inject_temperature(float temperature);
  void set_temperature_offset(float offset);
  void disable_injection();

 protected:
  esphome::climate::ClimateTraits traits() override;

  esphome::climate::ClimateMode mode_to_climate_mode_(uint8_t mode);
  std::string fan_to_custom_fan_mode_(uint8_t fan);
  std::string vertical_vane_to_vertical_airflow_select_(uint8_t vertical_vane);
  uint8_t climate_mode_to_mode_(esphome::climate::ClimateMode mode);
  uint8_t custom_fan_mode_to_fan_(const std::string &fan_mode);
  uint8_t vertical_airflow_select_to_vertical_vane_(const std::string &swing_mode);
  int convert_fan_velocity_(uint8_t velocity);

  float room_temp_to_celsius_(uint8_t temp);
  float temp_05_to_celsius_(uint8_t temp);
  float setting_temp_to_celsius_(uint8_t temp);
  uint8_t celsius_to_temp_05_(float celsius);
  uint8_t celsius_to_setting_temp_(float celsius);

  ResponseType read_packet_();

  RequestSlot *new_request_slot_();
  void prepare_request_slot_(RequestSlot *slot);

  void request_info_(uint8_t type);

  uint8_t checksum_(const uint8_t *packet, size_t len);

  esphome::sensor::Sensor *compressor_frequency_sensor_;
  esphome::sensor::Sensor *fan_velocity_sensor_;
  esphome::binary_sensor::BinarySensor *conflicted_sensor_;
  esphome::binary_sensor::BinarySensor *preheat_sensor_;
  esphome::sensor::Sensor *control_temperature_sensor_;

  esphome::switch_::Switch *inject_enable_switch_;
  esphome::number::Number *remote_temperature_number_;
  esphome::select::Select *vertical_airflow_select_;

  esphome::climate::ClimateTraits traits_;

  bool power_;
  bool high_precision_temp_setting_;
  bool connected_;
  bool inject_enable_;
  std::chrono::steady_clock::time_point last_connect_attempt_timestamp_;
  std::chrono::steady_clock::time_point last_status_request_timestamp_;
  std::chrono::steady_clock::time_point last_settings_request_timestamp_;
  std::list<RequestSlot *> pending_requests_;
  RequestSlot active_request_;
  int status_rotation_;
  float last_control_temperature_;
  float temperature_offset_;
};

class ClimateMitsubishiInjectEnableSwitch : public esphome::Component, public esphome::switch_::Switch {
 public:
  void write_state(bool state) override;
  void set_climate(ClimateMitsubishi *climate);

 protected:
  ClimateMitsubishi *climate_;
};

class ClimateMitsubishiRemoteTemperatureNumber : public esphome::Component, public esphome::number::Number {
 public:
  void control(float value) override;
  void set_climate(ClimateMitsubishi *climate);

 protected:
  ClimateMitsubishi *climate_;
};

class ClimateMitsubishiTemperatureOffsetNumber : public esphome::Component, public esphome::number::Number {
 public:
  ClimateMitsubishiTemperatureOffsetNumber();
  void control(float value) override;
  void set_climate(ClimateMitsubishi *climate);

 protected:
  ClimateMitsubishi *climate_;
};

class ClimateMitsubishiVerticalAirflowSelect : public esphome::Component, public esphome::select::Select {
 public:
  void set_climate(ClimateMitsubishi *climate);
  void control(const std::string &value) override;

 protected:
  ClimateMitsubishi *climate_;
};

}  // namespace climate_mitsubishi
}  // namespace esphome
