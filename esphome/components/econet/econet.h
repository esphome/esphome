#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/uart/uart.h"
#include <map>
#include <vector>
#include <set>

namespace esphome {
namespace econet {

enum ModelType { MODEL_TYPE_TANKLESS = 0, MODEL_TYPE_HEATPUMP = 1, MODEL_TYPE_HVAC = 2 };

class ReadRequest {
 public:
  uint32_t dst_adr;
  uint8_t dst_bus;

  uint32_t src_adr;
  uint8_t src_bus;

  bool awaiting_res = false;

  std::vector<std::string> obj_names;
};

enum class EconetDatapointType : uint8_t {
  FLOAT = 0,
  TEXT = 1,
  ENUM_TEXT = 2,
  RAW = 4,
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
      return lhs.value_string == rhs.value_string;
    case EconetDatapointType::ENUM_TEXT:
      return lhs.value_enum == rhs.value_enum;
    case EconetDatapointType::RAW:
      return lhs.value_raw == rhs.value_raw;
  }
  return false;
}

struct EconetDatapointListener {
  std::string datapoint_id;
  std::function<void(EconetDatapoint)> on_datapoint;
};

class Econet : public Component, public uart::UARTDevice {
 public:
  void loop() override;
  void dump_config() override;

  void set_model_type(ModelType model_type) { model_type_ = model_type; }
  ModelType get_model_type() { return model_type_; }

  void set_update_interval(uint32_t interval_millis) { update_interval_millis_ = interval_millis; }

  void set_float_datapoint_value(const std::string &datapoint_id, float value);
  void set_enum_datapoint_value(const std::string &datapoint_id, uint8_t value);

  void register_listener(const std::string &datapoint_id, const std::function<void(EconetDatapoint)> &func,
                         bool is_raw_datapoint = false);

 protected:
  ModelType model_type_;
  uint32_t update_interval_millis_{30000};
  std::vector<EconetDatapointListener> listeners_;
  ReadRequest read_req_;
  void set_datapoint_(const std::string &datapoint_id, const EconetDatapoint &value);
  void send_datapoint_(const std::string &datapoint_id, const EconetDatapoint &value, bool skip_update_state = false);

  void make_request_();
  void read_buffer_(int bytes_available);
  void parse_message_(bool is_tx);
  void parse_rx_message_();
  void parse_tx_message_();

  void transmit_message_(uint32_t dst_adr, uint32_t src_adr, uint8_t command, const std::vector<uint8_t> &data);
  void request_strings_(uint32_t dst_adr, uint32_t src_adr, const std::vector<std::string> &objects);
  void write_value_(uint32_t dst_adr, uint32_t src_adr, const std::string &object, EconetDatapointType type,
                    float value);

  std::set<std::string> datapoint_ids_{};
  std::map<std::string, EconetDatapoint> datapoints_{};
  std::map<std::string, EconetDatapoint> pending_writes_{};
  std::map<std::string, EconetDatapoint> pending_confirmation_writes_{};

  uint32_t last_request_{0};
  uint32_t last_read_data_{0};
  std::vector<uint8_t> rx_message_;
  std::vector<uint8_t> tx_message_;

  uint32_t dst_adr_ = 0;
};

class EconetClient {
 public:
  void set_econet_parent(Econet *parent) { this->parent_ = parent; }

 protected:
  Econet *parent_;
};

}  // namespace econet
}  // namespace esphome
