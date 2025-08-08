#pragma once

#include "ble_descriptor.h"
#include "esphome/components/esp32_ble/ble_uuid.h"
#include "esphome/components/event_emitter/event_emitter.h"
#include "esphome/components/bytebuffer/bytebuffer.h"

#include <vector>
#include <unordered_map>

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
using namespace event_emitter;

class BLEService;

namespace BLECharacteristicEvt {
enum VectorEvt {
  ON_WRITE,
};

enum EmptyEvt {
  ON_READ,
};
}  // namespace BLECharacteristicEvt

class BLECharacteristic : public EventEmitter<BLECharacteristicEvt::VectorEvt, std::vector<uint8_t>, uint16_t>,
                          public EventEmitter<BLECharacteristicEvt::EmptyEvt, uint16_t> {
 public:
  BLECharacteristic(ESPBTUUID uuid, uint32_t properties);
  ~BLECharacteristic();

  void set_value(ByteBuffer buffer);
  void set_value(const std::vector<uint8_t> &buffer);
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
  std::unordered_map<uint16_t, bool> clients_to_notify_;

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
