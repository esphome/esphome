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

}  // namespace

static const char *const TAG = "econet.climate";

void EconetClimate::dump_config() {
  LOG_CLIMATE("", "Econet Climate", this);
  this->dump_traits_(TAG);
}

climate::ClimateTraits EconetClimate::traits() {
  ModelType model_type = this->parent_->get_model_type();
  auto traits = climate::ClimateTraits();

  traits.set_supports_action(false);

  if (model_type == MODEL_TYPE_HVAC) {
    traits.set_supported_modes({climate::CLIMATE_MODE_OFF, climate::CLIMATE_MODE_COOL, climate::CLIMATE_MODE_HEAT,
                                climate::CLIMATE_MODE_HEAT_COOL, climate::CLIMATE_MODE_FAN_ONLY});
  } else {
    traits.set_supported_modes({climate::CLIMATE_MODE_OFF, climate::CLIMATE_MODE_AUTO});
  }

  if (model_type == MODEL_TYPE_HEATPUMP) {
    traits.set_supported_custom_presets({"Off", "Eco Mode", "Heat Pump", "High Demand", "Electric", "Vacation"});
  } else if (model_type == MODEL_TYPE_ELECTRIC_TANK) {
    traits.set_supported_custom_presets({"Energy Saver", "Performance"});
  }
  traits.set_supports_current_temperature(true);
  if (model_type == MODEL_TYPE_HVAC) {
    traits.set_supported_custom_fan_modes({"Automatic", "Speed 1 (Low)", "Speed 2 (Medium Low)", "Speed 3 (Medium)",
                                           "Speed 4 (Medium High)", "Speed 5 (High)"});

    traits.set_supports_two_point_target_temperature(true);
  } else {
    traits.set_visual_min_temperature(43.3333);
    traits.set_visual_max_temperature(60);

    traits.set_supports_two_point_target_temperature(false);
  }
  traits.set_visual_temperature_step(1.0f);

  return traits;
}

void EconetClimate::setup() {
  ModelType model_type = this->parent_->get_model_type();
  if (model_type == MODEL_TYPE_HVAC) {
    this->parent_->register_listener("HEATSETP", [this](const EconetDatapoint &datapoint) {
      this->target_temperature_low = fahrenheit_to_celsius(datapoint.value_float);
      this->publish_state();
    });
    this->parent_->register_listener("COOLSETP", [this](const EconetDatapoint &datapoint) {
      this->target_temperature_high = fahrenheit_to_celsius(datapoint.value_float);
      this->publish_state();
    });
    this->parent_->register_listener("SPT_STAT", [this](const EconetDatapoint &datapoint) {
      this->current_temperature = fahrenheit_to_celsius(datapoint.value_float);
      this->publish_state();
    });
  } else {
    this->parent_->register_listener("WHTRSETP", [this](const EconetDatapoint &datapoint) {
      this->target_temperature = fahrenheit_to_celsius(datapoint.value_float);
      this->publish_state();
    });
    this->parent_->register_listener(
        (model_type == MODEL_TYPE_HEATPUMP || model_type == MODEL_TYPE_ELECTRIC_TANK) ? "UPHTRTMP" : "TEMP_OUT",
        [this](const EconetDatapoint &datapoint) {
          this->current_temperature = fahrenheit_to_celsius(datapoint.value_float);
          this->publish_state();
        });
  }
  if (model_type == MODEL_TYPE_HVAC) {
    this->parent_->register_listener("STATMODE", [this](const EconetDatapoint &datapoint) {
      switch (datapoint.value_enum) {
        case 0:
          this->mode = climate::CLIMATE_MODE_HEAT;
          break;
        case 1:
          this->mode = climate::CLIMATE_MODE_COOL;
          break;
        case 2:
          this->mode = climate::CLIMATE_MODE_HEAT_COOL;
          break;
        case 3:
          this->mode = climate::CLIMATE_MODE_FAN_ONLY;
          break;
        case 4:
          this->mode = climate::CLIMATE_MODE_OFF;
          break;
      }
      this->publish_state();
    });
    this->parent_->register_listener("STATNFAN", [this](const EconetDatapoint &datapoint) {
      switch (datapoint.value_enum) {
        case 0:
          this->set_custom_fan_mode_("Automatic");
          break;
        case 1:
          this->set_custom_fan_mode_("Speed 1 (Low)");
          break;
        case 2:
          this->set_custom_fan_mode_("Speed 2 (Medium Low)");
          break;
        case 3:
          this->set_custom_fan_mode_("Speed 3 (Medium)");
          break;
        case 4:
          this->set_custom_fan_mode_("Speed 4 (Medium High)");
          break;
        case 5:
          this->set_custom_fan_mode_("Speed 5 (High)");
          break;
      }
      this->publish_state();
    });
  } else {
    this->parent_->register_listener("WHTRENAB", [this](const EconetDatapoint &datapoint) {
      if (datapoint.value_enum == 1) {
        this->mode = climate::CLIMATE_MODE_AUTO;
      } else {
        this->mode = climate::CLIMATE_MODE_OFF;
      }
      this->publish_state();
    });
  }
  if (model_type == MODEL_TYPE_HEATPUMP) {
    this->parent_->register_listener("WHTRCNFG", [this](const EconetDatapoint &datapoint) {
      switch (datapoint.value_enum) {
        case 0:
          this->set_custom_preset_("Off");
          break;
        case 1:
          this->set_custom_preset_("Eco Mode");
          break;
        case 2:
          this->set_custom_preset_("Heat Pump");
          break;
        case 3:
          this->set_custom_preset_("High Demand");
          break;
        case 4:
          this->set_custom_preset_("Electric");
          break;
        case 5:
          this->set_custom_preset_("Vacation");
          break;
      }
      this->publish_state();
    });
  } else if (model_type == MODEL_TYPE_ELECTRIC_TANK) {
    this->parent_->register_listener("WHTRCNFG", [this](const EconetDatapoint &datapoint) {
      switch (datapoint.value_enum) {
        case 0:
          this->set_custom_preset_("Energy Saver");
          break;
        case 1:
          this->set_custom_preset_("Performance");
          break;
      }
      this->publish_state();
    });
  }
}

void EconetClimate::control(const climate::ClimateCall &call) {
  ModelType model_type = this->parent_->get_model_type();
  if (call.get_target_temperature_low().has_value()) {
    this->parent_->set_float_datapoint_value("HEATSETP",
                                             celsius_to_fahrenheit(call.get_target_temperature_low().value()));
  }

  if (call.get_target_temperature_high().has_value()) {
    this->parent_->set_float_datapoint_value("COOLSETP",
                                             celsius_to_fahrenheit(call.get_target_temperature_high().value()));
  }

  if (call.get_target_temperature().has_value()) {
    this->parent_->set_float_datapoint_value("WHTRSETP", celsius_to_fahrenheit(call.get_target_temperature().value()));
  }

  if (call.get_mode().has_value()) {
    climate::ClimateMode climate_mode = call.get_mode().value();
    if (model_type == MODEL_TYPE_HVAC) {
      uint8_t new_mode = 0;

      switch (climate_mode) {
        case climate::CLIMATE_MODE_HEAT_COOL:
          new_mode = 2;
          break;
        case climate::CLIMATE_MODE_HEAT:
          new_mode = 0;
          break;
        case climate::CLIMATE_MODE_COOL:
          new_mode = 1;
          break;
        case climate::CLIMATE_MODE_FAN_ONLY:
          new_mode = 3;
          break;
        case climate::CLIMATE_MODE_OFF:
          new_mode = 4;
          break;
        default:
          new_mode = 4;
      }
      ESP_LOGI("econet", "Raw Mode is %d", climate_mode);
      ESP_LOGI("econet", "Lets change the mode to %d", new_mode);
      this->parent_->set_enum_datapoint_value("STATMODE", new_mode);
    } else {
      bool new_mode = climate_mode != climate::CLIMATE_MODE_OFF;
      ESP_LOGI("econet", "Raw Mode is %d", climate_mode);
      ESP_LOGI("econet", "Lets change the mode to %d", new_mode);
      this->parent_->set_enum_datapoint_value("WHTRENAB", new_mode);
    }
  }

  if (call.get_custom_fan_mode().has_value()) {
    const std::string &fan_mode = call.get_custom_fan_mode().value();
    int new_fan_mode = 0;
    if (fan_mode == "Automatic") {
      new_fan_mode = 0;
    } else if (fan_mode == "Speed 1 (Low)") {
      new_fan_mode = 1;
    } else if (fan_mode == "Speed 2 (Medium Low)") {
      new_fan_mode = 2;
    } else if (fan_mode == "Speed 3 (Medium)") {
      new_fan_mode = 3;
    } else if (fan_mode == "Speed 4 (Medium High)") {
      new_fan_mode = 4;
    } else if (fan_mode == "Speed 5 (High)") {
      new_fan_mode = 5;
    }
    this->parent_->set_enum_datapoint_value("STATNFAN", new_fan_mode);
  }

  if (call.get_custom_preset().has_value()) {
    const std::string &preset = call.get_custom_preset().value();

    ESP_LOGI("econet", "Set custom preset: %s", preset.c_str());

    int8_t new_mode = -1;

    if (model_type == MODEL_TYPE_HEATPUMP) {
      if (preset == "Off") {
        new_mode = 0;
      } else if (preset == "Eco Mode") {
        new_mode = 1;
      } else if (preset == "Heat Pump") {
        new_mode = 2;
      } else if (preset == "High Demand") {
        new_mode = 3;
      } else if (preset == "Electric") {
        new_mode = 4;
      } else if (preset == "Vacation") {
        new_mode = 5;
      }
    } else if (model_type == MODEL_TYPE_ELECTRIC_TANK) {
      if (preset == "Energy Saver") {
        new_mode = 0;
      } else if (preset == "Performance") {
        new_mode = 1;
      }
    }

    if (new_mode != -1) {
      this->parent_->set_enum_datapoint_value("WHTRCNFG", new_mode);
    }
  }
}

}  // namespace econet
}  // namespace esphome
