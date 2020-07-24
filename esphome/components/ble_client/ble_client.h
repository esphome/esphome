#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/sensor/sensor.h"

#ifdef ARDUINO_ARCH_ESP32

#include <string>
#include <array>
#include <esp_gap_ble_api.h>
#include <esp_gattc_api.h>
#include <esp_bt_defs.h>

namespace espbt = esphome::esp32_ble_tracker;

namespace esphome {
namespace ble_client {

class BLEClient;
class BLEService;
class BLECharacteristic;

class BLEClientNode {
 public:
  virtual void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                   esp_ble_gattc_cb_param_t *param) = 0;
  virtual void loop() = 0;
  void set_address(uint64_t address) { address_ = address; }
  uint64_t address_;
  espbt::ESPBTClient *client_;
  // This should be transitioned to Established once the node no longer needs
  // the services/descriptors/characteristics of the parent client. This will
  // allow some memory to be freed.
  espbt::ClientState node_state_;

  void set_ble_client_parent(BLEClient *parent) { this->parent_ = parent; }

 protected:
  BLEClient *parent_;
};

class BLEDescriptor {
 public:
  espbt::ESPBTUUID uuid_;
  uint16_t handle_;

  BLECharacteristic *characteristic_;
};

class BLECharacteristic {
 public:
  ~BLECharacteristic();
  espbt::ESPBTUUID uuid_;
  uint16_t handle_;
  esp_gatt_char_prop_t properties_;
  std::vector<BLEDescriptor *> descriptors_;
  void parse_descriptors();
  BLEDescriptor *get_descriptor(espbt::ESPBTUUID uuid);
  BLEDescriptor *get_descriptor(uint16_t uuid);

  BLEService *service_;
};

class BLEService {
 public:
  ~BLEService();
  espbt::ESPBTUUID uuid_;
  uint16_t start_handle_;
  uint16_t end_handle_;
  std::vector<BLECharacteristic *> characteristics_;
  BLEClient *client_;
  void parse_characteristics();
  BLECharacteristic *get_characteristic(espbt::ESPBTUUID uuid);
  BLECharacteristic *get_characteristic(uint16_t uuid);
};

class BLEClient : public espbt::ESPBTClient, public Component, public Nameable {
 public:
  void setup() override;
  void dump_config() override;
  void loop() override;

  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);
  bool parse_device(const espbt::ESPBTDevice &device) override;
  void on_scan_end() override {}
  void connect();

  void set_address(uint64_t address) { address_ = address; }

  void register_ble_node(BLEClientNode *node) {
    node->client_ = this;
    node->set_ble_client_parent(this);
    this->nodes_.push_back(node);
  }

  BLEService *get_service(espbt::ESPBTUUID uuid);
  BLEService *get_service(uint16_t uuid);
  BLECharacteristic *get_characteristic(espbt::ESPBTUUID service, espbt::ESPBTUUID chr);
  BLECharacteristic *get_characteristic(uint16_t service, uint16_t chr);
  BLEDescriptor *get_descriptor(espbt::ESPBTUUID service, espbt::ESPBTUUID chr, espbt::ESPBTUUID descr);
  BLEDescriptor *get_descriptor(uint16_t service, uint16_t chr, uint16_t descr);
  // Get the configuration descriptor for the given characteristic handle.
  BLEDescriptor *get_config_descriptor(uint16_t handle);

  float parse_char_value(uint8_t *value, uint16_t length);

  int gattc_if_;
  esp_bd_addr_t remote_bda_;
  uint16_t conn_id_;
  uint64_t address_;
  std::string address_str() const;

 protected:
  void set_states(espbt::ClientState st) {
    this->state_ = st;
    for (auto &node : nodes_)
      node->node_state_ = st;
  }
  bool all_nodes_established() {
    if (this->state_ != espbt::ClientState::Established)
      return false;
    for (auto &node : nodes_)
      if (node->node_state_ != espbt::ClientState::Established)
        return false;
    return true;
  }

  std::vector<BLEClientNode *> nodes_;
  std::vector<BLEService *> services_;

  uint32_t hash_base() override;
};

}  // namespace ble_client
}  // namespace esphome

#endif
