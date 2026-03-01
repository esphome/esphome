#pragma once
#include <array>
#include <cstddef>
#include <functional>
#include <span>

#include "bthome_decoder.h"
#include "bthome_encoder.h"
#include "bthome_local_sensor.h"
#include "helpers.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/log.h"

#ifdef USE_BTHOME_SERVER

#ifdef USE_ESP32
#include "esphome/components/esp32_ble/ble.h"
#ifndef CONFIG_ESP_HOSTED_ENABLE_BT_BLUEDROID
#include <esp_bt.h>
#endif
#include <esp_bt_main.h>
#include <esp_gap_ble_api.h>
#include "esp_bt_device.h"
#endif  // USE_ESP32

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

// Abstract adapter interface for all BLE operations — platform agnostic
class IBLEAdapter {
 public:
  virtual ~IBLEAdapter() = default;

  // Returns local BLE MAC address (6 bytes), may be nullptr
  virtual MacAddressPtr get_local_mac() = 0;

  // Registers a callback invoked by the BLE cycling mechanism.
  // The bool argument indicates whether advertising is now active.
  virtual void register_advertise_callback(std::function<void(bool)> callback) = 0;

  // Pushes raw advertisement data to the BLE controller.
  // On ESP32 this triggers the DATA_RAW_SET_COMPLETE gap event which
  // subsequently calls start_advertising().
  virtual void config_adv_data_raw(const uint8_t *data, size_t len) = 0;

  // Starts BLE advertising.  Called from gap_event_handler on ESP32
  // after raw data has been configured.
  virtual void start_advertising() = 0;
};

// Base class with most implementation (non-template)
class BTHomeServerBase : public Component
#ifdef USE_ESP32
    ,
                         public esp32_ble::GAPEventHandler
#endif
{
 public:
  explicit BTHomeServerBase(IBLEAdapter *adapter = nullptr);

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_BLUETOOTH; }

#ifdef USE_BTHOME_DECRYPTION
  void set_encryption_key(std::initializer_list<uint8_t> key) {
    EncryptionKey k{};
    std::copy(key.begin(), key.end(), k.begin());
    this->encryption_key_ = k;
  }
#endif

#ifdef USE_ESP32
  void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) override;
#endif

  // Virtual methods for derived class to manage sensors
  virtual void set_local_sensor(size_t index, BTHomeLocalBase *sensor) = 0;
  virtual std::span<BTHomeLocalBase *> get_local_sensors() = 0;

 protected:
  void on_advertise_();
  void send_frame_();
  void advertise_immediate_(BTHomeObjectType type);

  IBLEAdapter *adapter_{nullptr};
  size_t next_sensor_index_{0};
  BTHomeEncoder encoder_;
  bool advertising_{false};
  MacAddress local_mac_;
  uint8_t adv_buffer_[BLE_ADV_MAX_SIZE]{};

#ifdef USE_BTHOME_DECRYPTION
  optional<EncryptionKey> encryption_key_;
  uint32_t encryption_counter_{0};
#endif
};

// Template class — only holds the sensor array
template<size_t N> class BTHomeServer : public BTHomeServerBase {
 public:
  explicit BTHomeServer(IBLEAdapter *adapter = nullptr) : BTHomeServerBase(adapter) {}

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
