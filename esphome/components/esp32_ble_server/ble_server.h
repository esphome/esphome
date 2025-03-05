#pragma once

#include "ble_service.h"
#include "ble_characteristic.h"

#include "esphome/components/esp32_ble/ble.h"
#include "esphome/components/esp32_ble/ble_uuid.h"
#include "esphome/components/bytebuffer/bytebuffer.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include <memory>
#include <vector>
#include <unordered_map>
#include <unordered_set>

#ifdef USE_ESP32

#include <esp_gatts_api.h>

namespace esphome {
namespace esp32_ble_server {

using namespace esp32_ble;
using namespace bytebuffer;

namespace BLEServerEvt {
enum EmptyEvt {
  ON_CONNECT,
  ON_DISCONNECT,
};
}  // namespace BLEServerEvt

class BLEServer : public Component,
                  public GATTsEventHandler,
                  public BLEStatusEventHandler,
                  public Parented<ESP32BLE>,
                  public EventEmitter<BLEServerEvt::EmptyEvt, uint16_t> {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;
  bool can_proceed() override;

  void teardown();
  bool is_running();

  void set_manufacturer_data(const std::vector<uint8_t> &data) {
    this->manufacturer_data_ = data;
    this->restart_advertising_();
  }

  BLEService *create_service(ESPBTUUID uuid, bool advertise = false, uint16_t num_handles = 15);
  void remove_service(ESPBTUUID uuid, uint8_t inst_id = 0);
  BLEService *get_service(ESPBTUUID uuid, uint8_t inst_id = 0);
  void enqueue_start_service(BLEService *service) { this->services_to_start_.push_back(service); }
  void set_device_information_service(BLEService *service) { this->device_information_service_ = service; }

  esp_gatt_if_t get_gatts_if() { return this->gatts_if_; }
  uint32_t get_connected_client_count() { return this->clients_.size(); }
  const std::unordered_set<uint16_t> &get_clients() { return this->clients_; }

  void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
                           esp_ble_gatts_cb_param_t *param) override;

  void ble_before_disabled_event_handler() override;

 protected:
  static std::string get_service_key(ESPBTUUID uuid, uint8_t inst_id);
  void restart_advertising_();

  void add_client_(uint16_t conn_id) { this->clients_.insert(conn_id); }
  void remove_client_(uint16_t conn_id) { this->clients_.erase(conn_id); }

  std::vector<uint8_t> manufacturer_data_{};
  esp_gatt_if_t gatts_if_{0};
  bool registered_{false};

  std::unordered_set<uint16_t> clients_;
  std::unordered_map<std::string, BLEService *> services_{};
  std::vector<BLEService *> services_to_start_{};
  BLEService *device_information_service_{};

  enum State : uint8_t {
    INIT = 0x00,
    REGISTERING,
    STARTING_SERVICE,
    RUNNING,
  } state_{INIT};
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern BLEServer *global_ble_server;

}  // namespace esp32_ble_server
}  // namespace esphome

#endif
