#pragma once

#include "esphome/components/esp32_ble/ble_uuid.h"
#include "esphome/components/bytebuffer/bytebuffer.h"

#ifdef USE_ESP32

#include <esp_gatt_defs.h>
#include <esp_gatts_api.h>
#include <span>
#include <functional>
#include <memory>

namespace esphome::esp32_ble_server {

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

  void set_value(const uint8_t *data, size_t length);
  void set_value(std::vector<uint8_t> &&buffer);
  void set_value(std::initializer_list<uint8_t> data);
  void set_value(ByteBuffer buffer) { this->set_value(buffer.get_data()); }

  void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);

  bool is_created() { return this->state_ == CREATED; }
  bool is_failed() { return this->state_ == FAILED; }

  /// Set the encryption each permitted direction needs: 0 none, 1 encrypted, 2 also an authenticated (MITM) key
  void require_encryption(uint8_t read_level, uint8_t write_level) {
    static constexpr esp_gatt_perm_t READ_PERMS[] = {ESP_GATT_PERM_READ, ESP_GATT_PERM_READ_ENCRYPTED,
                                                     ESP_GATT_PERM_READ_ENC_MITM};
    static constexpr esp_gatt_perm_t WRITE_PERMS[] = {ESP_GATT_PERM_WRITE, ESP_GATT_PERM_WRITE_ENCRYPTED,
                                                      ESP_GATT_PERM_WRITE_ENC_MITM};
    this->permissions_ = ((this->permissions_ & ESP_GATT_PERM_READ) != 0 ? READ_PERMS[read_level] : 0) |
                         ((this->permissions_ & ESP_GATT_PERM_WRITE) != 0 ? WRITE_PERMS[write_level] : 0);
  }

  // Direct callback registration - only allocates when callback is set
  void on_write(std::function<void(std::span<const uint8_t>, uint16_t)> &&callback) {
    this->on_write_callback_ =
        std::make_unique<std::function<void(std::span<const uint8_t>, uint16_t)>>(std::move(callback));
  }

 protected:
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

}  // namespace esphome::esp32_ble_server

#endif
