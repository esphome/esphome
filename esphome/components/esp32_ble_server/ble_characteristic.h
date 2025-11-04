#pragma once

#include "ble_descriptor.h"
#include "esphome/components/esp32_ble/ble_uuid.h"
#include "esphome/components/bytebuffer/bytebuffer.h"

#include <vector>
#include <span>
#include <functional>
#include <memory>

#ifdef USE_ESP32

#include <esp_gap_ble_api.h>
#include <esp_gatt_defs.h>
#include <esp_gattc_api.h>
#include <esp_gatts_api.h>
#include <esp_bt_defs.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace esphome {
namespace esp32_ble_server {

using namespace esp32_ble;
using namespace bytebuffer;

class BLEService;

class BLECharacteristic {
 public:
  BLECharacteristic(ESPBTUUID uuid, uint32_t properties);
  ~BLECharacteristic();

  void set_value(ByteBuffer buffer);
  void set_value(std::vector<uint8_t> &&buffer);
  void set_value(std::initializer_list<uint8_t> data);
  void set_value(const std::string &buffer);

  void set_broadcast_property(bool value);
  void set_indicate_property(bool value);
  void set_notify_property(bool value);
  void set_read_property(bool value);
  void set_write_property(bool value);
  void set_write_no_response_property(bool value);

  void notify();

  void do_create(BLEService *service);
  void do_delete() { this->clients_to_notify_.clear(); }
  void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);

  void add_descriptor(BLEDescriptor *descriptor);
  void remove_descriptor(BLEDescriptor *descriptor);

  BLEService *get_service() { return this->service_; }
  ESPBTUUID get_uuid() { return this->uuid_; }
  std::vector<uint8_t> &get_value() { return this->value_; }

  static const uint32_t PROPERTY_READ = 1 << 0;
  static const uint32_t PROPERTY_WRITE = 1 << 1;
  static const uint32_t PROPERTY_NOTIFY = 1 << 2;
  static const uint32_t PROPERTY_BROADCAST = 1 << 3;
  static const uint32_t PROPERTY_INDICATE = 1 << 4;
  static const uint32_t PROPERTY_WRITE_NR = 1 << 5;

  bool is_created();
  bool is_failed();

  // Direct callback registration - only allocates when callback is set
  void on_write(std::function<void(std::span<const uint8_t>, uint16_t)> &&callback) {
    this->on_write_callback_ =
        std::make_unique<std::function<void(std::span<const uint8_t>, uint16_t)>>(std::move(callback));
  }
  void on_read(std::function<void(uint16_t)> &&callback) {
    this->on_read_callback_ = std::make_unique<std::function<void(uint16_t)>>(std::move(callback));
  }

 protected:
  bool write_event_{false};
  BLEService *service_{};
  ESPBTUUID uuid_;
  esp_gatt_char_prop_t properties_;
  uint16_t handle_{0xFFFF};

  uint16_t value_read_offset_{0};
  std::vector<uint8_t> value_;
  SemaphoreHandle_t set_value_lock_;

  std::vector<BLEDescriptor *> descriptors_;

  struct ClientNotificationEntry {
    uint16_t conn_id;
    bool indicate;  // true = indicate, false = notify
  };
  std::vector<ClientNotificationEntry> clients_to_notify_;

  void remove_client_from_notify_list_(uint16_t conn_id);
  ClientNotificationEntry *find_client_in_notify_list_(uint16_t conn_id);

  void set_property_bit_(esp_gatt_char_prop_t bit, bool value);

  std::unique_ptr<std::function<void(std::span<const uint8_t>, uint16_t)>> on_write_callback_;
  std::unique_ptr<std::function<void(uint16_t)>> on_read_callback_;

  esp_gatt_perm_t permissions_ = ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE;

  enum State : uint8_t {
    FAILED = 0x00,
    INIT,
    CREATING,
    CREATING_DEPENDENTS,
    CREATED,
  } state_{INIT};
};

}  // namespace esp32_ble_server
}  // namespace esphome

#endif
