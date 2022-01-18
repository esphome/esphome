#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"

#ifdef USE_ESP32

#include <string>
#include <array>
#include <esp_gap_ble_api.h>
#include <esp_gattc_api.h>
#include <esp_bt_defs.h>

namespace esphome {
namespace ble_client {

namespace espbt = esphome::esp32_ble_tracker;

class BLEClient;
class BLEService;
class BLECharacteristic;

class BLEClientNode {
 public:
  virtual void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                   esp_ble_gattc_cb_param_t *param) = 0;
  virtual void loop(){};
  void set_address(uint64_t address) { address_ = address; }
  espbt::ESPBTClient *client;
  // This should be transitioned to Established once the node no longer needs
  // the services/descriptors/characteristics of the parent client. This will
  // allow some memory to be freed.
  espbt::ClientState node_state;

  BLEClient *parent() { return this->parent_; }
  void set_ble_client_parent(BLEClient *parent) { this->parent_ = parent; }

 protected:
  BLEClient *parent_;
  uint64_t address_;
};

class BLEDescriptor {
 public:
  espbt::ESPBTUUID uuid;
  uint16_t handle;

  BLECharacteristic *characteristic;
};

class BLECharacteristic {
 public:
  ~BLECharacteristic();
  espbt::ESPBTUUID uuid;
  uint16_t handle;
  esp_gatt_char_prop_t properties;
  std::vector<BLEDescriptor *> descriptors;
  void parse_descriptors();
  BLEDescriptor *get_descriptor(espbt::ESPBTUUID uuid);
  BLEDescriptor *get_descriptor(uint16_t uuid);
  void write_value(uint8_t *new_val, int16_t new_val_size);
  BLEService *service;
};

class BLEService {
 public:
  ~BLEService();
  espbt::ESPBTUUID uuid;
  uint16_t start_handle;
  uint16_t end_handle;
  std::vector<BLECharacteristic *> characteristics;
  BLEClient *client;
  void parse_characteristics();
  BLECharacteristic *get_characteristic(espbt::ESPBTUUID uuid);
  BLECharacteristic *get_characteristic(uint16_t uuid);
};

class BLEClient : public espbt::ESPBTClient, public Component {
 public:
  void setup() override;
  void dump_config() override;
  void loop() override;
  float get_setup_priority() const override;

  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;
  bool parse_device(const espbt::ESPBTDevice &device) override;
  void on_scan_end() override {}
  void connect() override;

  void set_address(uint64_t address) { this->address = address; }

  void set_enabled(bool enabled);

  void register_ble_node(BLEClientNode *node) {
    node->client = this;
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

  int gattc_if;
  esp_bd_addr_t remote_bda;
  uint16_t conn_id;
  uint64_t address;
  bool enabled;
  std::string address_str() const;

 protected:
  void set_states_(espbt::ClientState st) {
    this->set_state(st);
    for (auto &node : nodes_)
      node->node_state = st;
  }
  bool all_nodes_established_() {
    if (this->state() != espbt::ClientState::ESTABLISHED)
      return false;
    for (auto &node : nodes_)
      if (node->node_state != espbt::ClientState::ESTABLISHED)
        return false;
    return true;
  }

  std::vector<BLEClientNode *> nodes_;
  std::vector<BLEService *> services_;
};

}  // namespace ble_client
}  // namespace esphome

#endif
