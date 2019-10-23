#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace text_sensor {

#define LOG_TEXT_SENSOR(prefix, type, obj) \
  if (obj != nullptr) { \
    ESP_LOGCONFIG(TAG, "%s%s '%s'", prefix, type, obj->get_name().c_str()); \
    if (!obj->get_icon().empty()) { \
      ESP_LOGCONFIG(TAG, "%s  Icon: '%s'", prefix, obj->get_icon().c_str()); \
    } \
    if (!obj->unique_id().empty()) { \
      ESP_LOGV(TAG, "%s  Unique ID: '%s'", prefix, obj->unique_id().c_str()); \
    } \
  }

class TextSensor : public Nameable {
 public:
  explicit TextSensor();
  explicit TextSensor(const std::string &name);

  void publish_state(std::string state);

  void set_icon(const std::string &icon);

  void add_on_state_callback(std::function<void(std::string)> callback);

  std::string state;

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  std::string get_icon();

  virtual std::string icon();

  virtual std::string unique_id();

  bool has_state();

 protected:
  uint32_t hash_base() override;

  CallbackManager<void(std::string)> callback_;
  optional<std::string> icon_;
  bool has_state_{false};
};

}  // namespace text_sensor
}  // namespace esphome
