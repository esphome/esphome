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
  traits.set_supports_current_humidity(!current_humidity_id_.empty());
  traits.set_supports_target_humidity(!target_dehumidification_level_id_.empty());
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
    parent_->register_listener(
        current_temperature_id_, request_mod_, request_once_,
        [this](const EconetDatapoint &datapoint) {
          current_temperature = fahrenheit_to_celsius(datapoint.value_float);
          publish_state();
        },
        false, this->src_adr_);
  }
  if (!current_humidity_id_.empty()) {
    parent_->register_listener(
        current_humidity_id_, request_mod_, request_once_,
        [this](const EconetDatapoint &datapoint) {
          current_humidity = datapoint.value_float;
          publish_state();
        },
        false, this->src_adr_);
  }
  if (!target_dehumidification_level_id_.empty()) {
    parent_->register_listener(
        target_dehumidification_level_id_, request_mod_, request_once_,
        [this](const EconetDatapoint &datapoint) {
          target_humidity = datapoint.value_float;
          publish_state();
        },
        false, this->src_adr_);
  }
  if (!target_temperature_id_.empty()) {
    parent_->register_listener(
        target_temperature_id_, request_mod_, request_once_,
        [this](const EconetDatapoint &datapoint) {
          target_temperature = fahrenheit_to_celsius(datapoint.value_float);
          publish_state();
        },
        false, this->src_adr_);
  }
  if (!target_temperature_low_id_.empty()) {
    parent_->register_listener(
        target_temperature_low_id_, request_mod_, request_once_,
        [this](const EconetDatapoint &datapoint) {
          target_temperature_low = fahrenheit_to_celsius(datapoint.value_float);
          publish_state();
        },
        false, this->src_adr_);
  }
  if (!target_temperature_high_id_.empty()) {
    parent_->register_listener(
        target_temperature_high_id_, request_mod_, request_once_,
        [this](const EconetDatapoint &datapoint) {
          target_temperature_high = fahrenheit_to_celsius(datapoint.value_float);
          publish_state();
        },
        false, this->src_adr_);
  }
  if (!mode_id_.empty()) {
    parent_->register_listener(
        mode_id_, request_mod_, request_once_,
        [this](const EconetDatapoint &datapoint) {
          auto it = modes_.find(datapoint.value_enum);
          if (it == modes_.end()) {
            ESP_LOGW(TAG, "In modes of your yaml add a ClimateMode that corresponds to: %d: \"%s\"",
                     datapoint.value_enum, datapoint.value_string.c_str());
          } else {
            mode = it->second;
            publish_state();
          }
        },
        false, this->src_adr_);
  }
  if (!custom_preset_id_.empty()) {
    parent_->register_listener(
        custom_preset_id_, request_mod_, request_once_,
        [this](const EconetDatapoint &datapoint) {
          auto it = custom_presets_.find(datapoint.value_enum);
          if (it == custom_presets_.end()) {
            ESP_LOGW(TAG, "In custom_presets of your yaml add: %d: \"%s\"", datapoint.value_enum,
                     datapoint.value_string.c_str());
          } else {
            set_custom_preset_(it->second);
            publish_state();
          }
        },
        false, this->src_adr_);
  }
  if (!custom_fan_mode_id_.empty()) {
    parent_->register_listener(
        custom_fan_mode_id_, request_mod_, request_once_,
        [this](const EconetDatapoint &datapoint) {
          auto it = custom_fan_modes_.find(datapoint.value_enum);
          if (it == custom_fan_modes_.end()) {
            ESP_LOGW(TAG, "In custom_fan_modes of your yaml add: %d: \"%s\"", datapoint.value_enum,
                     datapoint.value_string.c_str());
          } else {
            fan_mode_ = it->second;
            if (follow_schedule_.has_value()) {
              if (follow_schedule_.value()) {
                set_custom_fan_mode_(fan_mode_);
                publish_state();
              }
            }
          }
        },
        false, this->src_adr_);
  }
  if (!custom_fan_mode_no_schedule_id_.empty()) {
    parent_->register_listener(
        custom_fan_mode_no_schedule_id_, request_mod_, request_once_,
        [this](const EconetDatapoint &datapoint) {
          auto it = custom_fan_modes_.find(datapoint.value_enum);
          if (it == custom_fan_modes_.end()) {
            ESP_LOGW(TAG, "In custom_fan_modes of your yaml add: %d: \"%s\"", datapoint.value_enum,
                     datapoint.value_string.c_str());
          } else {
            fan_mode_no_schedule_ = it->second;
            if (follow_schedule_.has_value()) {
              if (!follow_schedule_.value()) {
                set_custom_fan_mode_(fan_mode_no_schedule_);
                publish_state();
              }
            }
          }
        },
        false, this->src_adr_);
  }
  if (!follow_schedule_id_.empty()) {
    parent_->register_listener(
        follow_schedule_id_, request_mod_, request_once_,
        [this](const EconetDatapoint &datapoint) {
          ESP_LOGI(TAG, "MCU reported climate sensor %s is: %s", this->follow_schedule_id_.c_str(),
                   datapoint.value_string.c_str());
          follow_schedule_ = datapoint.value_enum > 0;
          if (follow_schedule_.value()) {
            if (fan_mode_ != "") {
              set_custom_fan_mode_(fan_mode_);
              publish_state();
            }
          } else {
            if (fan_mode_no_schedule_ != "") {
              set_custom_fan_mode_(fan_mode_no_schedule_);
              publish_state();
            }
          }
        },
        false, this->src_adr_);
  }
}

void EconetClimate::control(const climate::ClimateCall &call) {
  if (call.get_target_temperature().has_value() && !target_temperature_id_.empty()) {
    parent_->set_float_datapoint_value(target_temperature_id_,
                                       celsius_to_fahrenheit(call.get_target_temperature().value()), this->src_adr_);
  }
  if (call.get_target_temperature_low().has_value() && !target_temperature_low_id_.empty()) {
    parent_->set_float_datapoint_value(
        target_temperature_low_id_, celsius_to_fahrenheit(call.get_target_temperature_low().value()), this->src_adr_);
  }
  if (call.get_target_temperature_high().has_value() && !target_temperature_high_id_.empty()) {
    parent_->set_float_datapoint_value(
        target_temperature_high_id_, celsius_to_fahrenheit(call.get_target_temperature_high().value()), this->src_adr_);
  }
  if (call.get_mode().has_value() && !mode_id_.empty()) {
    climate::ClimateMode mode = call.get_mode().value();
    auto it = std::find_if(modes_.begin(), modes_.end(),
                           [&mode](const std::pair<uint8_t, climate::ClimateMode> &p) { return p.second == mode; });
    if (it != modes_.end()) {
      parent_->set_enum_datapoint_value(mode_id_, it->first, this->src_adr_);
    }
  }
  if (call.get_custom_preset().has_value() && !custom_preset_id_.empty()) {
    const std::string &preset = call.get_custom_preset().value();
    auto it = std::find_if(custom_presets_.begin(), custom_presets_.end(),
                           [&preset](const std::pair<uint8_t, std::string> &p) { return p.second == preset; });
    if (it != custom_presets_.end()) {
      parent_->set_enum_datapoint_value(custom_preset_id_, it->first, this->src_adr_);
    }
  }
  if (call.get_custom_fan_mode().has_value() && !custom_fan_mode_id_.empty()) {
    const std::string &fan_mode = call.get_custom_fan_mode().value();
    auto it = std::find_if(custom_fan_modes_.begin(), custom_fan_modes_.end(),
                           [&fan_mode](const std::pair<uint8_t, std::string> &p) { return p.second == fan_mode; });
    if (it != custom_fan_modes_.end()) {
      if (follow_schedule_.has_value()) {
        if (follow_schedule_.value()) {
          parent_->set_enum_datapoint_value(custom_fan_mode_id_, it->first, this->src_adr_);
        } else {
          parent_->set_enum_datapoint_value(custom_fan_mode_no_schedule_id_, it->first, this->src_adr_);
        }
      }
    }
  }
  if (call.get_target_humidity().has_value() && !target_dehumidification_level_id_.empty()) {
    parent_->set_float_datapoint_value(target_dehumidification_level_id_, call.get_target_humidity().value(),
                                       this->src_adr_);
  }
}

}  // namespace econet
}  // namespace esphome
