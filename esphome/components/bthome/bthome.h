#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/esp32_ble/ble.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"

#ifdef USE_ESP32

#ifndef CONFIG_ESP_HOSTED_ENABLE_BT_BLUEDROID
#include <esp_bt.h>
#endif
#include <esp_gap_ble_api.h>

#include <array>
#include <vector>

namespace esphome {
namespace bthome {

using namespace esp32_ble;

struct SensorMeasurement {
  sensor::Sensor *sensor;
  uint8_t object_id;
};

struct BinarySensorMeasurement {
  binary_sensor::BinarySensor *sensor;
  uint8_t object_id;
};

class BTHome : public Component, public GAPEventHandler, public Parented<ESP32BLE> {
 public:
  void setup() override;
  void dump_config() override;
  void loop() override;
  float get_setup_priority() const override;

  void set_min_interval(uint16_t val) { this->min_interval_ = val; }
  void set_max_interval(uint16_t val) { this->max_interval_ = val; }
  void set_tx_power(esp_power_level_t val) { this->tx_power_ = val; }

  void set_encryption_key(const std::vector<uint8_t> &key);
  void add_measurement(sensor::Sensor *sensor, uint8_t object_id);
  void add_binary_measurement(binary_sensor::BinarySensor *sensor, uint8_t object_id);

  void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) override;

 protected:
  void on_advertise_();
  void build_advertisement_data_();
  void encode_measurement_(std::vector<uint8_t> &data, uint8_t object_id, float value);
  void encode_binary_measurement_(std::vector<uint8_t> &data, uint8_t object_id, bool value);
  bool encrypt_payload_(const std::vector<uint8_t> &plaintext, std::vector<uint8_t> &ciphertext);

  std::vector<SensorMeasurement> measurements_;
  std::vector<BinarySensorMeasurement> binary_measurements_;

  uint16_t min_interval_{};
  uint16_t max_interval_{};
  esp_power_level_t tx_power_{};
  esp_ble_adv_params_t ble_adv_params_;
  bool advertising_{false};

  // Encryption
  bool encryption_enabled_{false};
  std::array<uint8_t, 16> encryption_key_{};
  uint32_t counter_{0};

  // Cached advertisement data
  std::vector<uint8_t> adv_data_;
  bool data_changed_{true};
};

}  // namespace bthome
}  // namespace esphome

#endif
