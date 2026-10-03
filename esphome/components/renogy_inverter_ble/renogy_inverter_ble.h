#pragma once

#include <array>
#include <vector>
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/sensor/sensor.h"

#ifdef USE_ESP32
#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include <esp_gattc_api.h>
namespace espbt = esphome::esp32_ble_tracker;
#endif

namespace esphome::renogy_inverter_ble {

// Renogy inverter BLE GATT (RIV1230PU). Notify char 0xFFF1 (svc 0xFFF0), write char 0xFFD1
// (svc 0xFFD0), init char 0xFFD4 (svc 0xFFD0). Modbus device id 0x20, function 0x03.
class RenogyInverterBle :
#ifdef USE_ESP32
    public ble_client::BLEClientNode,
#endif
    public PollingComponent {
 public:
  void setup() override;
  void dump_config() override;
  void update() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

#ifdef USE_ESP32
  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;
#endif

  void set_ac_input_voltage_sensor(sensor::Sensor *s) { this->ac_input_voltage_sensor_ = s; }
  void set_ac_output_voltage_sensor(sensor::Sensor *s) { this->ac_output_voltage_sensor_ = s; }
  void set_ac_output_current_sensor(sensor::Sensor *s) { this->ac_output_current_sensor_ = s; }
  void set_ac_output_frequency_sensor(sensor::Sensor *s) { this->ac_output_frequency_sensor_ = s; }
  void set_input_frequency_sensor(sensor::Sensor *s) { this->input_frequency_sensor_ = s; }
  void set_battery_voltage_sensor(sensor::Sensor *s) { this->battery_voltage_sensor_ = s; }
  void set_temperature_sensor(sensor::Sensor *s) { this->temperature_sensor_ = s; }
  void set_load_current_sensor(sensor::Sensor *s) { this->load_current_sensor_ = s; }
  void set_load_active_power_sensor(sensor::Sensor *s) { this->load_active_power_sensor_ = s; }
  void set_load_apparent_power_sensor(sensor::Sensor *s) { this->load_apparent_power_sensor_ = s; }

 protected:
#ifdef USE_ESP32
  // Read cycle: IDLE → (update) INIT read(0xFFD4) → MAIN read(4000) → LOAD read(4408) → IDLE.
  enum class State : uint8_t { IDLE, INIT, MAIN, LOAD };

  void start_cycle_();
  void read_register_(uint16_t start_register, uint16_t word_count);
  void on_frame_complete_();
  void reset_frame_();
  // Cancel the in-flight read cycle (watchdog + load timers), drop the partial frame, go IDLE.
  void abort_cycle_();

  bool established_{false};
  State state_{State::IDLE};
  uint16_t notify_handle_{0};
  uint16_t write_handle_{0};
  uint16_t init_handle_{0};
  // Reassembly buffer for the Modbus response (arrives in MTU-sized notification chunks).
  std::vector<uint8_t> frame_;
  uint16_t expected_len_{0};
#endif

  sensor::Sensor *ac_input_voltage_sensor_{nullptr};
  sensor::Sensor *ac_output_voltage_sensor_{nullptr};
  sensor::Sensor *ac_output_current_sensor_{nullptr};
  sensor::Sensor *ac_output_frequency_sensor_{nullptr};
  sensor::Sensor *input_frequency_sensor_{nullptr};
  sensor::Sensor *battery_voltage_sensor_{nullptr};
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *load_current_sensor_{nullptr};
  sensor::Sensor *load_active_power_sensor_{nullptr};
  sensor::Sensor *load_apparent_power_sensor_{nullptr};
};

}  // namespace esphome::renogy_inverter_ble
