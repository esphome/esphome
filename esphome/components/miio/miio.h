#pragma once

#include <cinttypes>
#include <vector>
#include <deque>
#include <unordered_map>
#include <unordered_set>
#include <tuple>

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/components/uart/uart.h"

#ifdef USE_TIME
#include "esphome/components/time/real_time_clock.h"
#include "esphome/core/time.h"
#endif

namespace esphome {
namespace miio {

using prop_t = std::tuple<uint8_t, uint8_t>;

enum class MiioPropertyType : uint8_t {
  BOOLEAN = 0x01,
  NUMBER = 0x02,
  ENUM = 0x03,
};

struct MiioPropertyValue {
  MiioPropertyType type;
  std::string value;
  union {
    bool value_bool;
    float value_numeric;
    uint8_t value_enum;
  };
};

struct MiioPropertyHash {
  std::size_t operator()(const std::tuple<uint8_t, uint8_t> &t) const {
    auto a = std::get<0>(t);
    auto b = std::get<1>(t);
    return (a << 8) + b;
  }
};

class Miio : public Component, public uart::UARTDevice {
 public:
  float get_setup_priority() const override { return setup_priority::LATE; }
  void setup() override;
  void loop() override;
  void dump_config() override;
  void register_listener(prop_t property, MiioPropertyType type,
                         const std::function<void(const MiioPropertyValue &)> &fn);
  void set_property(prop_t property, std::string value);
#ifdef USE_TIME
  void set_time_id(time::RealTimeClock *time_id) { this->time_id_ = time_id; }
#endif

 protected:
  void handle_char_(uint8_t c);
  void process_command_queue_();
  void process_command_raw_(const std::vector<std::string> &tokens);
  void process_properties_change_(const std::vector<std::string> &tokens);
  void process_result_(const std::vector<std::string> &tokens);
  void process_down_request_();
  void apply_property_(prop_t prop, const std::string &value);
  optional<prop_t> parse_property_specifier_(const std::string &cluster, const std::string &key);
  optional<MiioPropertyValue> parse_property_value_(MiioPropertyType type, const std::string &value);
  const char *get_network_state_();
  void mcu_reply_(const std::string &message);
  void mcu_reply_(const char *message);
  void mcu_reply_ok_();

  bool awaiting_for_get_properties_result_{false};
#ifdef USE_TIME
  optional<time::RealTimeClock *> time_id_{};
#endif
  std::unordered_map<prop_t, std::tuple<MiioPropertyType, std::function<void(const MiioPropertyValue &)>>,
                     MiioPropertyHash>
      listeners_;
  std::vector<std::string> rx_message_;
  std::deque<std::vector<std::string>> mcu_message_queue_;
  std::unordered_map<prop_t, std::string, MiioPropertyHash> mcu_pending_changes_;
  std::unordered_set<prop_t, MiioPropertyHash> mcu_query_;
  std::unordered_map<std::string, std::function<void(const std::vector<std::string> &)>> mcu_handlers_;
};

}  // namespace miio
}  // namespace esphome
