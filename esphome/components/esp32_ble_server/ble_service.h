#pragma once

#include "ble_characteristic.h"
#include "esphome/components/esp32_ble/ble_uuid.h"

#include <vector>

#ifdef USE_ESP32

#include <esp_bt_defs.h>
#include <esp_gap_ble_api.h>
#include <esp_gatt_defs.h>
#include <esp_gattc_api.h>
#include <esp_gatts_api.h>

namespace esphome {
namespace esp32_ble_server {

class BLEServer;

using namespace esp32_ble;

class BLEService {
 public:
  BLEService(ESPBTUUID uuid, uint16_t num_handles, uint8_t inst_id, bool advertise);
  ~BLEService();
  BLECharacteristic *get_characteristic(ESPBTUUID uuid);
  BLECharacteristic *get_characteristic(uint16_t uuid);

  BLECharacteristic *create_characteristic(const std::string &uuid, esp_gatt_char_prop_t properties);
  BLECharacteristic *create_characteristic(uint16_t uuid, esp_gatt_char_prop_t properties);
  BLECharacteristic *create_characteristic(ESPBTUUID uuid, esp_gatt_char_prop_t properties);

  ESPBTUUID get_uuid() { return this->uuid_; }
  uint8_t get_inst_id() { return this->inst_id_; }
  BLECharacteristic *get_last_created_characteristic() { return this->last_created_characteristic_; }
  uint16_t get_handle() { return this->handle_; }

  BLEServer *get_server() { return this->server_; }

  void do_create(BLEServer *server);
  void do_delete();
  void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);

  void start();
  void stop();

  bool is_failed();
  bool is_created() { return this->state_ == CREATED; }
  bool is_running() { return this->state_ == RUNNING; }
  bool is_starting() { return this->state_ == STARTING; }
  bool is_deleted() { return this->state_ == DELETED; }

 protected:
  std::vector<BLECharacteristic *> characteristics_;
  BLECharacteristic *last_created_characteristic_{nullptr};
  uint32_t created_characteristic_count_{0};
  BLEServer *server_ = nullptr;
  ESPBTUUID uuid_;
  uint16_t num_handles_;
  uint16_t handle_{0xFFFF};
  uint8_t inst_id_;
  bool advertise_{false};
  bool should_start_{false};

  bool do_create_characteristics_();
  void stop_();

  enum State : uint8_t {
    FAILED = 0x00,
    INIT,
    CREATING,
    CREATED,
    STARTING,
    RUNNING,
    STOPPING,
    STOPPED,
    DELETING,
    DELETED,
  } state_{INIT};
};

}  // namespace esp32_ble_server
}  // namespace esphome

#endif
