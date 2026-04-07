#include "uponor_smatrix_climate.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <cinttypes>

namespace esphome {
namespace uponor_smatrix {

static const char *const TAG = "uponor_smatrix.climate";

void UponorSmatrixClimate::dump_config() {
  LOG_CLIMATE("", "Uponor Smatrix Climate", this);
  ESP_LOGCONFIG(TAG, "  Device address: 0x%08" PRIX32, this->address_);
}

void UponorSmatrixClimate::loop() {
  const uint32_t now = App.get_loop_component_start_time();

  // Publish state after all update packets are processed
  if (this->last_data_ != 0 && (now - this->last_data_ > 100) && this->target_temperature_raw_ != 0) {
    // Apply offsets that are currently applied by the thermostat based on the current mode
    uint16_t temp_raw = this->target_temperature_raw_;
    if (this->mode == climate::CLIMATE_MODE_HEAT) {
      if (this->preset == climate::CLIMATE_PRESET_ECO)
        temp_raw -= this->eco_setback_value_raw_;
    } else if (this->mode == climate::CLIMATE_MODE_COOL) {
      temp_raw += this->heating_cooling_offset_raw_;
      if (this->preset == climate::CLIMATE_PRESET_ECO)
        temp_raw += this->eco_setback_value_raw_;
    }
    // Convert raw value to actual temperature shown on the thermostat
    float temp = raw_to_celsius(temp_raw);
    float step = this->get_traits().get_visual_target_temperature_step();
    this->target_temperature = roundf(temp / step) * step;
    this->publish_state();
    this->last_data_ = 0;
  }
}

climate::ClimateTraits UponorSmatrixClimate::traits() {
  auto traits = climate::ClimateTraits();
  traits.add_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE | climate::CLIMATE_SUPPORTS_CURRENT_HUMIDITY |
                           climate::CLIMATE_SUPPORTS_ACTION);
  traits.set_supported_modes({climate::CLIMATE_MODE_HEAT, climate::CLIMATE_MODE_COOL});
  traits.set_supported_presets({climate::CLIMATE_PRESET_ECO});
  traits.set_visual_min_temperature(this->min_temperature_);
  traits.set_visual_max_temperature(this->max_temperature_);
  traits.set_visual_current_temperature_step(0.1f);
  traits.set_visual_target_temperature_step(0.5f);
  return traits;
}

void UponorSmatrixClimate::control(const climate::ClimateCall &call) {
  auto temp = call.get_target_temperature();
  if (temp.has_value()) {
    // Convert temperature to raw value and remove offsets that are currently applied
    // by the thermostat based on the current mode
    uint16_t temp_raw = celsius_to_raw(*temp);
    if (this->mode == climate::CLIMATE_MODE_HEAT) {
      if (this->preset == climate::CLIMATE_PRESET_ECO)
        temp_raw += this->eco_setback_value_raw_;
    } else if (this->mode == climate::CLIMATE_MODE_COOL) {
      temp_raw -= this->heating_cooling_offset_raw_;
      if (this->preset == climate::CLIMATE_PRESET_ECO)
        temp_raw -= this->eco_setback_value_raw_;
    }
    // For unknown reasons, we need to send a null setpoint first for the thermostat to react
    UponorSmatrixData data[] = {{UPONOR_ID_TARGET_TEMP, 0}, {UPONOR_ID_TARGET_TEMP, temp_raw}};
    this->send(data, sizeof(data) / sizeof(data[0]));
  }
}

void UponorSmatrixClimate::on_device_data(const UponorSmatrixData *data, size_t data_len) {
  for (size_t i = 0; i < data_len; i++) {
    switch (data[i].id) {
      case UPONOR_ID_HEATING_COOLING_OFFSET:
        this->heating_cooling_offset_raw_ = data[i].value;
        break;
      case UPONOR_ID_TARGET_TEMP_MIN:
        this->min_temperature_ = raw_to_celsius(data[i].value);
        break;
      case UPONOR_ID_TARGET_TEMP_MAX:
        this->max_temperature_ = raw_to_celsius(data[i].value);
        break;
      case UPONOR_ID_TARGET_TEMP:
        // Ignore invalid values here as they are used by the controller to explicitely request the setpoint from a
        // thermostat
        if (data[i].value != UPONOR_INVALID_VALUE)
          this->target_temperature_raw_ = data[i].value;
        break;
      case UPONOR_ID_ECO_SETBACK:
        this->eco_setback_value_raw_ = data[i].value;
        break;
      case UPONOR_ID_DEMAND:
        if (data[i].value & 0x1000) {
          this->mode = climate::CLIMATE_MODE_COOL;
          this->action = (data[i].value & 0x0040) ? climate::CLIMATE_ACTION_COOLING : climate::CLIMATE_ACTION_IDLE;
        } else {
          this->mode = climate::CLIMATE_MODE_HEAT;
          this->action = (data[i].value & 0x0040) ? climate::CLIMATE_ACTION_HEATING : climate::CLIMATE_ACTION_IDLE;
        }
        break;
      case UPONOR_ID_MODE1:
        this->set_preset_((data[i].value & 0x0008) ? climate::CLIMATE_PRESET_ECO : climate::CLIMATE_PRESET_NONE);
        break;
      case UPONOR_ID_ROOM_TEMP:
        this->current_temperature = raw_to_celsius(data[i].value);
        break;
      case UPONOR_ID_HUMIDITY:
        this->current_humidity = data[i].value & 0x00FF;
        break;
    }
  }

  this->last_data_ = App.get_loop_component_start_time();
}

}  // namespace uponor_smatrix
}  // namespace esphome
