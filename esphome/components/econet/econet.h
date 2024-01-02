#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/api/custom_api_device.h"
#include "esphome/components/uart/uart.h"
#include <map>
#include <vector>
#include <set>

namespace esphome {
namespace econet {

static const uint8_t MAX_REQUEST_MODS = 16;

class ReadRequest {
 public:
  uint32_t dst_adr;
  uint8_t dst_bus;

  uint32_t src_adr;
  uint8_t src_bus;

  bool awaiting_res;

  uint8_t type;
  std::vector<std::string> obj_names;
};

struct EconetDatapointID {
  std::string name;
  uint32_t address;
};
inline bool operator==(const EconetDatapointID &lhs, const EconetDatapointID &rhs) {
  return lhs.name == rhs.name && lhs.address == rhs.address;
}
inline bool operator<(const EconetDatapointID &lhs, const EconetDatapointID &rhs) {
  return (lhs.name < rhs.name) || ((lhs.name == rhs.name) && (lhs.address < rhs.address));
}

enum class EconetDatapointType : uint8_t {
  FLOAT = 0,
  TEXT = 1,
  ENUM_TEXT = 2,
  RAW = 4,
  UNSUPPORTED = 21,
};

struct EconetDatapoint {
  EconetDatapointType type;
  union {
    float value_float;
    uint8_t value_enum;
  };
  std::string value_string;
  std::vector<uint8_t> value_raw;
};
inline bool operator==(const EconetDatapoint &lhs, const EconetDatapoint &rhs) {
  if (lhs.type != rhs.type) {
    return false;
  }
  switch (lhs.type) {
    case EconetDatapointType::FLOAT:
      return lhs.value_float == rhs.value_float;
    case EconetDatapointType::TEXT:
      return lhs.value_enum == rhs.value_enum && lhs.value_string == rhs.value_string;
    case EconetDatapointType::ENUM_TEXT:
      return lhs.value_enum == rhs.value_enum;
    case EconetDatapointType::RAW:
      return lhs.value_raw == rhs.value_raw;
    case EconetDatapointType::UNSUPPORTED:
      return false;
  }
  return false;
}

struct EconetDatapointListener {
  EconetDatapointID datapoint_id;
  std::function<void(EconetDatapoint)> on_datapoint;
};

class Econet : public Component, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  void set_src_address(uint32_t address) { src_adr_ = address; }
  void set_dst_address(uint32_t address) { dst_adr_ = address; }
  void set_request_mod_addresses(std::vector<uint8_t> request_mods, std::vector<uint32_t> addresses) {
    for (auto i = 0; i < MAX_REQUEST_MODS; i++) {
      request_mod_addresses_[i] = 0;
    }
    for (auto i = 0; i < request_mods.size(); i++) {
      request_mod_addresses_[request_mods[i]] = addresses[i];
    }
  }
  void set_request_mod_update_intervals(std::vector<uint8_t> request_mods, std::vector<uint32_t> update_intervals) {
    for (auto i = 0; i < request_mods.size(); i++) {
      request_mod_update_interval_millis_map_[request_mods[i]] = update_intervals[i];
    }
    update_intervals_();
  }
  void set_update_interval(uint32_t interval_millis) {
    update_interval_millis_ = interval_millis;
    update_intervals_();
  }
  void set_flow_control_pin(GPIOPin *flow_control_pin) { this->flow_control_pin_ = flow_control_pin; }

  void set_float_datapoint_value(const std::string &datapoint_id, float value, uint32_t address = 0);
  void set_enum_datapoint_value(const std::string &datapoint_id, uint8_t value, uint32_t address = 0);

  void register_listener(const std::string &datapoint_id, int8_t request_mod, bool request_once,
                         const std::function<void(EconetDatapoint)> &func, bool is_raw_datapoint = false,
                         uint32_t src_adr = 0);

  void homeassistant_read(const std::string &datapoint_id, uint32_t address = 0);
  void homeassistant_write(const std::string &datapoint_id, uint8_t value);
  void homeassistant_write(const std::string &datapoint_id, float value);

 protected:
  uint32_t update_interval_millis_{30000};
  std::vector<uint32_t> request_mod_addresses_ = std::vector<uint32_t>(MAX_REQUEST_MODS, 0);
  std::map<uint8_t, uint32_t> request_mod_update_interval_millis_map_;
  std::vector<uint32_t> request_mod_update_interval_millis_ =
      std::vector<uint32_t>(MAX_REQUEST_MODS, update_interval_millis_);
  uint32_t min_update_interval_millis_{update_interval_millis_};
  uint32_t min_delay_between_read_requests_{update_interval_millis_};

  std::vector<EconetDatapointListener> listeners_;
  ReadRequest read_req_;
  void set_datapoint_(const EconetDatapointID &datapoint_id, const EconetDatapoint &value);
  void send_datapoint_(const EconetDatapointID &datapoint_id, const EconetDatapoint &value);

  void make_request_();
  void read_buffer_(int bytes_available);
  void parse_message_(bool is_tx);
  void parse_rx_message_();
  void parse_tx_message_();
  void handle_response_(const EconetDatapointID &datapoint_id, const uint8_t *p, uint8_t len);

  void transmit_message_(uint8_t command, const std::vector<uint8_t> &data, uint32_t dst_adr = 0, uint32_t src_adr = 0);
  void request_strings_();
  void write_value_(const std::string &object, EconetDatapointType type, float value, uint32_t address = 0);

  void update_intervals_() {
    min_update_interval_millis_ = update_interval_millis_;
    for (auto i = 0; i < MAX_REQUEST_MODS; i++) {
      request_mod_update_interval_millis_[i] = update_interval_millis_;
    }
    for (auto &kv : this->request_mod_update_interval_millis_map_) {
      request_mod_update_interval_millis_[kv.first] = kv.second;
      min_update_interval_millis_ = std::min(min_update_interval_millis_, kv.second);
    }
  }

  std::vector<std::set<std::string>> request_datapoint_ids_ = std::vector<std::set<std::string>>(MAX_REQUEST_MODS);
  std::vector<uint32_t> request_mod_last_requested_ = std::vector<uint32_t>{MAX_REQUEST_MODS, 0};
  std::set<uint8_t> request_mods_;
  std::set<EconetDatapointID> raw_datapoint_ids_;
  std::set<EconetDatapointID> request_once_datapoint_ids_;
  std::map<EconetDatapointID, EconetDatapoint> datapoints_;
  std::map<EconetDatapointID, EconetDatapoint> pending_writes_;
  std::queue<EconetDatapointID> datapoint_ids_for_read_service_;

  uint32_t loop_now_{0};
  uint32_t last_request_{0};
  uint32_t last_read_request_{0};
  uint32_t last_read_data_{0};
  std::vector<uint8_t> rx_message_;
  std::vector<uint8_t> tx_message_;

  uint32_t src_adr_{0};
  uint32_t dst_adr_{0};
  GPIOPin *flow_control_pin_{nullptr};
  esphome::api::CustomAPIDevice capi_;
};

class EconetClient {
 public:
  void set_econet_parent(Econet *parent) { this->parent_ = parent; }
  void set_request_mod(int8_t request_mod) { this->request_mod_ = request_mod; }
  void set_request_once(bool request_once) { this->request_once_ = request_once; }
  void set_src_adr(uint32_t src_adr) { this->src_adr_ = src_adr; }

 protected:
  Econet *parent_;
  int8_t request_mod_{0};
  bool request_once_{false};
  uint32_t src_adr_{0};
};

}  // namespace econet
}  // namespace esphome
