#pragma once

#include "ble_descriptor.h"
#include "esphome/components/esp32_ble/ble_uuid.h"

#include <vector>

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

class BLEService;

class BLECharacteristic {
 public:
  BLECharacteristic(ESPBTUUID uuid, uint32_t properties);

  void set_value(const uint8_t *data, size_t length);
  void set_value(std::vector<uint8_t> value);
  void set_value(const std::string &value);
  void set_value(uint8_t &data);
  void set_value(uint16_t &data);
  void set_value(uint32_t &data);
  void set_value(int &data);
  void set_value(float &data);
  void set_value(double &data);
  void set_value(bool &data);

  void set_broadcast_property(bool value);
  void set_indicate_property(bool value);
  void set_notify_property(bool value);
  void set_read_property(bool value);
  void set_write_property(bool value);
  void set_write_no_response_property(bool value);

  void notify(bool notification = true);

  void do_create(BLEService *service);
  void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);

  void on_write(const std::function<void(const std::vector<uint8_t> &)> &&func) { this->on_write_ = func; }

  void add_descriptor(BLEDescriptor *descriptor);

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

 protected:
  bool write_event_{false};
  BLEService *service_;
  ESPBTUUID uuid_;
  esp_gatt_char_prop_t properties_;
  uint16_t handle_{0xFFFF};

  uint16_t value_read_offset_{0};
  std::vector<uint8_t> value_;
  SemaphoreHandle_t set_value_lock_;

  std::vector<BLEDescriptor *> descriptors_;

  std::function<void(const std::vector<uint8_t> &)> on_write_;

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
