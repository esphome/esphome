#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/components/climate/climate_traits.h"
#include "econet_climate.h"

using namespace esphome;

namespace esphome {
namespace econet {

namespace {

float fahrenheit_to_celsius(float f) { return (f - 32) * 5 / 9; }
float celsius_to_fahrenheit(float c) { return c * 9 / 5 + 32; }

template<class K, class V> std::set<V> map_values_as_set(std::map<K, V> map) {
  std::set<V> v;
  std::transform(map.begin(), map.end(), std::inserter(v, v.end()), [](const std::pair<K, V> &p) { return p.second; });
  return v;
}

}  // namespace

static const char *const TAG = "econet.climate";

void EconetClimate::dump_config() {
  LOG_CLIMATE("", "Econet Climate", this);
  dump_traits_(TAG);
}

climate::ClimateTraits EconetClimate::traits() {
  auto traits = climate::ClimateTraits();
  traits.set_supports_current_temperature(!current_temperature_id_.empty());
  traits.set_supports_two_point_target_temperature(!target_temperature_high_id_.empty());
  if (!mode_id_.empty()) {
    traits.set_supported_modes(map_values_as_set(modes_));
  }
  if (!custom_preset_id_.empty()) {
    traits.set_supported_custom_presets(map_values_as_set(custom_presets_));
  }
  if (!custom_fan_mode_id_.empty()) {
    traits.set_supported_custom_fan_modes(map_values_as_set(custom_fan_modes_));
  }
  return traits;
}

void EconetClimate::setup() {
  if (!current_temperature_id_.empty()) {
    parent_->register_listener(current_temperature_id_, request_mod_, request_once_,
                               [this](const EconetDatapoint &datapoint) {
                                 current_temperature = fahrenheit_to_celsius(datapoint.value_float);
                                 publish_state();
                               });
  }
  if (!target_temperature_id_.empty()) {
    parent_->register_listener(target_temperature_id_, request_mod_, request_once_,
                               [this](const EconetDatapoint &datapoint) {
                                 target_temperature = fahrenheit_to_celsius(datapoint.value_float);
                                 publish_state();
                               });
  }
  if (!target_temperature_low_id_.empty()) {
    parent_->register_listener(target_temperature_low_id_, request_mod_, request_once_,
                               [this](const EconetDatapoint &datapoint) {
                                 target_temperature_low = fahrenheit_to_celsius(datapoint.value_float);
                                 publish_state();
                               });
  }
  if (!target_temperature_high_id_.empty()) {
    parent_->register_listener(target_temperature_high_id_, request_mod_, request_once_,
                               [this](const EconetDatapoint &datapoint) {
                                 target_temperature_high = fahrenheit_to_celsius(datapoint.value_float);
                                 publish_state();
                               });
  }
  if (!mode_id_.empty()) {
    parent_->register_listener(mode_id_, request_mod_, request_once_, [this](const EconetDatapoint &datapoint) {
      auto it = modes_.find(datapoint.value_enum);
      if (it == modes_.end()) {
        ESP_LOGW(TAG, "In modes of your yaml add a ClimateMode that corresponds to: %d: \"%s\"", datapoint.value_enum,
                 datapoint.value_string.c_str());
      } else {
        mode = it->second;
        publish_state();
      }
    });
  }
  if (!custom_preset_id_.empty()) {
    parent_->register_listener(custom_preset_id_, request_mod_, request_once_,
                               [this](const EconetDatapoint &datapoint) {
                                 auto it = custom_presets_.find(datapoint.value_enum);
                                 if (it == custom_presets_.end()) {
                                   ESP_LOGW(TAG, "In custom_presets of your yaml add: %d: \"%s\"", datapoint.value_enum,
                                            datapoint.value_string.c_str());
                                 } else {
                                   set_custom_preset_(it->second);
                                   publish_state();
                                 }
                               });
  }
  if (!custom_fan_mode_id_.empty()) {
    parent_->register_listener(custom_fan_mode_id_, request_mod_, request_once_,
                               [this](const EconetDatapoint &datapoint) {
                                 auto it = custom_fan_modes_.find(datapoint.value_enum);
                                 if (it == custom_fan_modes_.end()) {
                                   ESP_LOGW(TAG, "In custom_fan_modes of your yaml add: %d: \"%s\"",
                                            datapoint.value_enum, datapoint.value_string.c_str());
                                 } else {
                                   set_custom_fan_mode_(it->second);
                                   publish_state();
                                 }
                               });
  }
}

void EconetClimate::control(const climate::ClimateCall &call) {
  if (call.get_target_temperature().has_value() && !target_temperature_id_.empty()) {
    parent_->set_float_datapoint_value(target_temperature_id_,
                                       celsius_to_fahrenheit(call.get_target_temperature().value()));
  }
  if (call.get_target_temperature_low().has_value() && !target_temperature_low_id_.empty()) {
    parent_->set_float_datapoint_value(target_temperature_low_id_,
                                       celsius_to_fahrenheit(call.get_target_temperature_low().value()));
  }
  if (call.get_target_temperature_high().has_value() && !target_temperature_high_id_.empty()) {
    parent_->set_float_datapoint_value(target_temperature_high_id_,
                                       celsius_to_fahrenheit(call.get_target_temperature_high().value()));
  }
  if (call.get_mode().has_value() && !mode_id_.empty()) {
    climate::ClimateMode mode = call.get_mode().value();
    auto it = std::find_if(modes_.begin(), modes_.end(),
                           [&mode](const std::pair<uint8_t, climate::ClimateMode> &p) { return p.second == mode; });
    if (it != modes_.end()) {
      parent_->set_enum_datapoint_value(mode_id_, it->first);
    }
  }
  if (call.get_custom_preset().has_value() && !custom_preset_id_.empty()) {
    const std::string &preset = call.get_custom_preset().value();
    auto it = std::find_if(custom_presets_.begin(), custom_presets_.end(),
                           [&preset](const std::pair<uint8_t, std::string> &p) { return p.second == preset; });
    if (it != custom_presets_.end()) {
      parent_->set_enum_datapoint_value(custom_preset_id_, it->first);
    }
  }
  if (call.get_custom_fan_mode().has_value() && !custom_fan_mode_id_.empty()) {
    const std::string &fan_mode = call.get_custom_fan_mode().value();
    auto it = std::find_if(custom_fan_modes_.begin(), custom_fan_modes_.end(),
                           [&fan_mode](const std::pair<uint8_t, std::string> &p) { return p.second == fan_mode; });
    if (it != custom_fan_modes_.end()) {
      parent_->set_enum_datapoint_value(custom_fan_mode_id_, it->first);
    }
  }
}

}  // namespace econet
}  // namespace esphome
