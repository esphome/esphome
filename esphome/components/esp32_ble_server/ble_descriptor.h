#pragma once

#include "esphome/components/esp32_ble/ble_uuid.h"
#include "esphome/components/bytebuffer/bytebuffer.h"

#ifdef USE_ESP32

#include <esp_gatt_defs.h>
#include <esp_gatts_api.h>
#include <span>
#include <functional>
#include <memory>

namespace esphome {
namespace esp32_ble_server {

using namespace esp32_ble;
using namespace bytebuffer;

class BLECharacteristic;

// Base class for BLE descriptors
class BLEDescriptor {
 public:
  BLEDescriptor(ESPBTUUID uuid, uint16_t max_len = 100, bool read = true, bool write = true);
  virtual ~BLEDescriptor();
  void do_create(BLECharacteristic *characteristic);
  ESPBTUUID get_uuid() const { return this->uuid_; }

  void set_value(std::vector<uint8_t> &&buffer);
  void set_value(std::initializer_list<uint8_t> data);
  void set_value(ByteBuffer buffer) { this->set_value(buffer.get_data()); }

  void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);

  bool is_created() { return this->state_ == CREATED; }
  bool is_failed() { return this->state_ == FAILED; }

  // Direct callback registration - only allocates when callback is set
  void on_write(std::function<void(std::span<const uint8_t>, uint16_t)> &&callback) {
    this->on_write_callback_ =
        std::make_unique<std::function<void(std::span<const uint8_t>, uint16_t)>>(std::move(callback));
  }

 protected:
  void set_value_impl_(const uint8_t *data, size_t length);

  BLECharacteristic *characteristic_{nullptr};
  ESPBTUUID uuid_;
  uint16_t handle_{0xFFFF};

  esp_attr_value_t value_{};

  std::unique_ptr<std::function<void(std::span<const uint8_t>, uint16_t)>> on_write_callback_;

  esp_gatt_perm_t permissions_{};

  enum State : uint8_t {
    FAILED = 0x00,
    INIT,
    CREATING,
    CREATED,
  } state_{INIT};
};

}  // namespace esp32_ble_server
}  // namespace esphome

#endif
