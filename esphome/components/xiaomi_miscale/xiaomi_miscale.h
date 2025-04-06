#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/xiaomi_ble/xiaomi_ble.h"

#include <set>
#include <vector>

#ifdef USE_ESP32

namespace esphome {
namespace xiaomi_miscale {

enum Model {
  MODEL_V1,
  MODEL_V2,
  MODEL_S400,
};

struct ParseResult {
  enum Model model;
  optional<float> weight;
  optional<float> impedance;
  optional<float> impedance_low;
  optional<uint8_t> heart_rate;
  optional<uint8_t> profile_id;
  optional<uint32_t> timestamp;
};

class XiaomiMiscale : public Component, public esp32_ble_tracker::ESPBTDeviceListener {
 public:
  void set_address(uint64_t address) { address_ = address; };
  void set_bindkey(const std::string &bindkey);

  bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override;
  void dump_config() override;
  void set_weight(sensor::Sensor *weight) { weight_ = weight; }
  void set_impedance(sensor::Sensor *impedance) { impedance_ = impedance; }
  void set_impedance_low(sensor::Sensor *impedance_low) { impedance_low_ = impedance_low; }
  void set_heart_rate(sensor::Sensor *heart_rate) { heart_rate_ = heart_rate; }
  void set_timestamp(sensor::Sensor *timestamp) { timestamp_ = timestamp; }
  void set_profile_id(sensor::Sensor *profile_id) { profile_id_ = profile_id; }
  void set_clear_impedance(bool clear_impedance) { clear_impedance_ = clear_impedance; }
  void set_allow_any_profile_id(bool allowed) { allow_any_profile_id_ = allowed; }
  void add_allowed_profile_id(uint8_t allowed_id) { allowed_profile_ids_.insert(allowed_id); }

 protected:
  uint64_t address_;
  uint8_t bindkey_[16];

  sensor::Sensor *weight_{nullptr};
  sensor::Sensor *impedance_{nullptr};
  sensor::Sensor *heart_rate_{nullptr};
  sensor::Sensor *impedance_low_{nullptr};
  sensor::Sensor *timestamp_{nullptr};
  sensor::Sensor *profile_id_{nullptr};
  bool allow_any_profile_id_ = true;
  std::set<uint8_t> allowed_profile_ids_;
  bool clear_impedance_{false};

  optional<ParseResult> parse_header_(const esp32_ble_tracker::ServiceData &service_data);
  bool parse_message_(const std::vector<uint8_t> &message, ParseResult &result);
  bool parse_message_v1_(const std::vector<uint8_t> &message, ParseResult &result);
  bool parse_message_v2_(const std::vector<uint8_t> &message, ParseResult &result);
  bool parse_message_s400_(const std::vector<uint8_t> &message, ParseResult &result);
  std::string get_model_name_(Model model);
  bool report_results_(const optional<ParseResult> &result, const std::string &address);
};

}  // namespace xiaomi_miscale
}  // namespace esphome

#endif
