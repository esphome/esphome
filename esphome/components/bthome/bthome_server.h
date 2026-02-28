#pragma once
#include <array>
#include <cstddef>
#include <span>

#include "bthome_decoder.h"
#include "bthome_encoder.h"
#include "bthome_local_sensor.h"
#include "helpers.h"
#include "esphome/components/esp32_ble/ble.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

#ifdef USE_BTHOME_SERVER

#ifndef CONFIG_ESP_HOSTED_ENABLE_BT_BLUEDROID
#include <esp_bt.h>
#endif
#include <esp_bt_main.h>
#include <esp_gap_ble_api.h>
#include "esp_bt_device.h"
#ifdef USE_BTHOME_DECRYPTION
#include "bthome_encryption.h"
#endif

namespace esphome {
namespace bthome {
namespace server {

static constexpr size_t BLE_FLAGS_SIZE = 3;       // [02 01 06]
static constexpr size_t BLE_SVC_HEADER_SIZE = 4;  // [LL 16 D2 FC]
static constexpr size_t BLE_ADV_MAX_SIZE = 31;
static constexpr uint8_t BTHOME_VERSION_2 = 0x02;

// Base class with most implementation (non-template)
class BTHomeServerBase : public Component, public esp32_ble::GAPEventHandler {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override {
    return -100;
    return setup_priority::AFTER_BLUETOOTH;
  }

#ifdef USE_BTHOME_DECRYPTION
  void set_encryption_key(std::initializer_list<uint8_t> key) {
    EncryptionKey k{};
    std::copy(key.begin(), key.end(), k.begin());
    this->encryption_key_ = k;
  }
#endif

  void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) override;

  // Virtual methods for derived class to manage sensors
  virtual void set_local_sensor(size_t index, BTHomeLocalBase *sensor) = 0;
  virtual std::span<BTHomeLocalBase *> get_local_sensors() = 0;

 protected:
  void on_advertise_();
  void send_frame_();
  void advertise_immediate_(BTHomeObjectType type);

  size_t next_sensor_index_{0};
  BTHomeEncoder encoder_;
  bool advertising_{false};
  esp_ble_adv_params_t ble_adv_params_{};
  MacAddress local_mac_;
  uint8_t adv_buffer_[BLE_ADV_MAX_SIZE]{};

#ifdef USE_BTHOME_DECRYPTION
  optional<EncryptionKey> encryption_key_;
  uint32_t encryption_counter_{0};
#endif
};

// Template class - only holds the sensor array
template<size_t N> class BTHomeServer : public BTHomeServerBase {
 public:
  void set_local_sensor(size_t index, BTHomeLocalBase *sensor) override { this->local_sensors_[index] = sensor; }

  std::span<BTHomeLocalBase *> get_local_sensors() override {
    return std::span<BTHomeLocalBase *>(this->local_sensors_.data(), N);
  }

 private:
  std::array<BTHomeLocalBase *, N> local_sensors_{};
};

}  // namespace server
}  // namespace bthome
}  // namespace esphome

#endif  // USE_BTHOME_SERVER

#endif  // USE_ESP32
