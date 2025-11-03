#include "bedjet_climate.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome {
namespace bedjet {

using namespace esphome::climate;

static const char *bedjet_fan_step_to_fan_mode(const uint8_t fan_step) {
  if (fan_step < BEDJET_FAN_SPEED_COUNT)
    return BEDJET_FAN_STEP_NAMES[fan_step];
  return nullptr;
}

static uint8_t bedjet_fan_speed_to_step(const char *fan_step_percent) {
  for (int i = 0; i < BEDJET_FAN_SPEED_COUNT; i++) {
    if (strcmp(BEDJET_FAN_STEP_NAMES[i], fan_step_percent) == 0) {
      return i;
    }
  }
  return -1;
}

static inline BedjetButton heat_button(BedjetHeatMode mode) {
  return mode == HEAT_MODE_EXTENDED ? BTN_EXTHT : BTN_HEAT;
}

std::string BedJetClimate::describe() { return "BedJet Climate"; }

void BedJetClimate::dump_config() {
  LOG_CLIMATE("", "BedJet Climate", this);
  auto traits = this->get_traits();

  ESP_LOGCONFIG(TAG, "  Supported modes:");
  for (auto mode : traits.get_supported_modes()) {
    ESP_LOGCONFIG(TAG, "   - %s", LOG_STR_ARG(climate_mode_to_string(mode)));
  }
  if (this->heating_mode_ == HEAT_MODE_EXTENDED) {
    ESP_LOGCONFIG(TAG, "   - BedJet heating mode: EXT HT");
  } else {
    ESP_LOGCONFIG(TAG, "   - BedJet heating mode: HEAT");
  }

  ESP_LOGCONFIG(TAG, "  Supported fan modes:");
  for (const auto &mode : traits.get_supported_fan_modes()) {
    ESP_LOGCONFIG(TAG, "   - %s", LOG_STR_ARG(climate_fan_mode_to_string(mode)));
  }
  for (const auto &mode : traits.get_supported_custom_fan_modes()) {
    ESP_LOGCONFIG(TAG, "   - %s (c)", mode);
  }

  ESP_LOGCONFIG(TAG, "  Supported presets:");
  for (auto preset : traits.get_supported_presets()) {
    ESP_LOGCONFIG(TAG, "   - %s", LOG_STR_ARG(climate_preset_to_string(preset)));
  }
  for (const auto &preset : traits.get_supported_custom_presets()) {
    ESP_LOGCONFIG(TAG, "   - %s (c)", preset);
  }
}

void BedJetClimate::setup() {
  // restore set points
  auto restore = this->restore_state_();
  if (restore.has_value()) {
    ESP_LOGI(TAG, "Restored previous saved state.");
    restore->apply(this);
  } else {
    // Initial status is unknown until we connect
    this->reset_state_();
  }
}

/** Resets states to defaults. */
void BedJetClimate::reset_state_() {
  this->mode = CLIMATE_MODE_OFF;
  this->action = CLIMATE_ACTION_IDLE;
  this->target_temperature = NAN;
  this->current_temperature = NAN;
  this->preset.reset();
  this->clear_custom_preset_();
  this->publish_state();
}

void BedJetClimate::loop() {
  // This component is controlled via the parent BedJetHub
  // Empty loop not needed, disable to save CPU cycles
  this->disable_loop();
}

void BedJetClimate::control(const ClimateCall &call) {
  ESP_LOGD(TAG, "Received BedJetClimate::control");
  if (!this->parent_->is_connected()) {
    ESP_LOGW(TAG, "Not connected, cannot handle control call yet.");
    return;
  }

  if (call.get_mode().has_value()) {
    ClimateMode mode = *call.get_mode();
    bool button_result;
    switch (mode) {
      case CLIMATE_MODE_OFF:
        button_result = this->parent_->button_off();
        break;
      case CLIMATE_MODE_HEAT:
        button_result = this->parent_->send_button(heat_button(this->heating_mode_));
        break;
      case CLIMATE_MODE_FAN_ONLY:
        button_result = this->parent_->button_cool();
        break;
      case CLIMATE_MODE_DRY:
        button_result = this->parent_->button_dry();
        break;
      default:
        ESP_LOGW(TAG, "Unsupported mode: %d", mode);
        return;
    }

    if (button_result) {
      this->mode = mode;
      // We're using (custom) preset for Turbo, EXT HT, & M1-3 presets, so changing climate mode will clear those
      this->clear_custom_preset_();
      this->preset.reset();
    }
  }

  if (call.get_target_temperature().has_value()) {
    auto target_temp = *call.get_target_temperature();
    auto result = this->parent_->set_target_temp(target_temp);

    if (result) {
      this->target_temperature = target_temp;
    }
  }

  if (call.get_preset().has_value()) {
    ClimatePreset preset = *call.get_preset();
    bool result;

    if (preset == CLIMATE_PRESET_BOOST) {
      // We use BOOST preset for TURBO mode, which is a short-lived/high-heat mode.
      result = this->parent_->button_turbo();

      if (result) {
        this->mode = CLIMATE_MODE_HEAT;
        this->set_preset_(CLIMATE_PRESET_BOOST);
      }
    } else if (preset == CLIMATE_PRESET_NONE && this->preset.has_value()) {
      if (this->mode == CLIMATE_MODE_HEAT && this->preset == CLIMATE_PRESET_BOOST) {
        // We were in heat mode with Boost preset, and now preset is set to None, so revert to normal heat.
        result = this->parent_->send_button(heat_button(this->heating_mode_));
        if (result) {
          this->preset.reset();
          this->clear_custom_preset_();
        }
      } else {
        ESP_LOGD(TAG, "Ignoring preset '%s' call; with current mode '%s' and preset '%s'",
                 LOG_STR_ARG(climate_preset_to_string(preset)), LOG_STR_ARG(climate_mode_to_string(this->mode)),
                 LOG_STR_ARG(climate_preset_to_string(this->preset.value_or(CLIMATE_PRESET_NONE))));
      }
    } else {
      ESP_LOGW(TAG, "Unsupported preset: %d", preset);
      return;
    }
  } else if (call.has_custom_preset()) {
    const char *preset = call.get_custom_preset();
    bool result;

    if (strcmp(preset, "M1") == 0) {
      result = this->parent_->button_memory1();
    } else if (strcmp(preset, "M2") == 0) {
      result = this->parent_->button_memory2();
    } else if (strcmp(preset, "M3") == 0) {
      result = this->parent_->button_memory3();
    } else if (strcmp(preset, "LTD HT") == 0) {
      result = this->parent_->button_heat();
    } else if (strcmp(preset, "EXT HT") == 0) {
      result = this->parent_->button_ext_heat();
    } else {
      ESP_LOGW(TAG, "Unsupported preset: %s", preset);
      return;
    }

    if (result) {
      this->set_custom_preset_(preset);
    }
  }

  if (call.get_fan_mode().has_value()) {
    // Climate fan mode only supports low/med/high, but the BedJet supports 5-100% increments.
    // We can still support a ClimateCall that requests low/med/high, and just translate it to a step increment here.
    auto fan_mode = *call.get_fan_mode();
    bool result;
    if (fan_mode == CLIMATE_FAN_LOW) {
      result = this->parent_->set_fan_speed(20);
    } else if (fan_mode == CLIMATE_FAN_MEDIUM) {
      result = this->parent_->set_fan_speed(50);
    } else if (fan_mode == CLIMATE_FAN_HIGH) {
      result = this->parent_->set_fan_speed(75);
    } else {
      ESP_LOGW(TAG, "[%s] Unsupported fan mode: %s", this->get_name().c_str(),
               LOG_STR_ARG(climate_fan_mode_to_string(fan_mode)));
      return;
    }

    if (result) {
      this->set_fan_mode_(fan_mode);
    }
  } else if (call.has_custom_fan_mode()) {
    const char *fan_mode = call.get_custom_fan_mode();
    auto fan_index = bedjet_fan_speed_to_step(fan_mode);
    if (fan_index <= 19) {
      ESP_LOGV(TAG, "[%s] Converted fan mode %s to bedjet fan step %d", this->get_name().c_str(), fan_mode, fan_index);
      bool result = this->parent_->set_fan_index(fan_index);
      if (result) {
        this->set_custom_fan_mode_(fan_mode);
      }
    }
  }
}

void BedJetClimate::on_bedjet_state(bool is_ready) {}

void BedJetClimate::on_status(const BedjetStatusPacket *data) {
  ESP_LOGV(TAG, "[%s] Handling on_status with data=%p", this->get_name().c_str(), (void *) data);

  auto converted_temp = bedjet_temp_to_c(data->target_temp_step);
  if (converted_temp > 0)
    this->target_temperature = converted_temp;

  if (this->temperature_source_ == TEMPERATURE_SOURCE_OUTLET) {
    converted_temp = bedjet_temp_to_c(data->actual_temp_step);
  } else {
    converted_temp = bedjet_temp_to_c(data->ambient_temp_step);
  }
  if (converted_temp > 0) {
    this->current_temperature = converted_temp;
  }

  const auto *fan_mode_name = bedjet_fan_step_to_fan_mode(data->fan_step);
  if (fan_mode_name != nullptr) {
    this->set_custom_fan_mode_(fan_mode_name);
  }

  // TODO: Get biorhythm data to determine which preset (M1-3) is running, if any.
  switch (data->mode) {
    case MODE_WAIT:  // Biorhythm "wait" step: device is idle
    case MODE_STANDBY:
      this->mode = CLIMATE_MODE_OFF;
      this->action = CLIMATE_ACTION_IDLE;
      this->fan_mode = CLIMATE_FAN_OFF;
      this->clear_custom_preset_();
      this->preset.reset();
      break;

    case MODE_HEAT:
      this->mode = CLIMATE_MODE_HEAT;
      this->action = CLIMATE_ACTION_HEATING;
      this->preset.reset();
      if (this->heating_mode_ == HEAT_MODE_EXTENDED) {
        this->set_custom_preset_("LTD HT");
      } else {
        this->clear_custom_preset_();
      }
      break;

    case MODE_EXTHT:
      this->mode = CLIMATE_MODE_HEAT;
      this->action = CLIMATE_ACTION_HEATING;
      this->preset.reset();
      if (this->heating_mode_ == HEAT_MODE_EXTENDED) {
        this->clear_custom_preset_();
      } else {
        this->set_custom_preset_("EXT HT");
      }
      break;

    case MODE_COOL:
      this->mode = CLIMATE_MODE_FAN_ONLY;
      this->action = CLIMATE_ACTION_COOLING;
      this->clear_custom_preset_();
      this->preset.reset();
      break;

    case MODE_DRY:
      this->mode = CLIMATE_MODE_DRY;
      this->action = CLIMATE_ACTION_DRYING;
      this->clear_custom_preset_();
      this->preset.reset();
      break;

    case MODE_TURBO:
      this->set_preset_(CLIMATE_PRESET_BOOST);
      this->mode = CLIMATE_MODE_HEAT;
      this->action = CLIMATE_ACTION_HEATING;
      break;

    default:
      ESP_LOGW(TAG, "[%s] Unexpected mode: 0x%02X", this->get_name().c_str(), data->mode);
      break;
  }

  ESP_LOGV(TAG, "[%s] After on_status, new mode=%s", this->get_name().c_str(),
           LOG_STR_ARG(climate_mode_to_string(this->mode)));
  // FIXME: compare new state to previous state.
  this->publish_state();
}

/** Attempts to update the climate device from the last received BedjetStatusPacket.
 *
 * This will be called from #on_status() when the parent dispatches new status packets,
 * and from #update() when the polling interval is triggered.
 *
 * @return `true` if the status has been applied; `false` if there is nothing to apply.
 */
bool BedJetClimate::update_status_() {
  if (!this->parent_->is_connected())
    return false;
  if (!this->parent_->has_status())
    return false;

  auto *status = this->parent_->get_status_packet();

  if (status == nullptr)
    return false;

  this->on_status(status);

  if (this->is_valid_()) {
    // TODO: only if state changed?
    this->publish_state();
    this->status_clear_warning();
    return true;
  }

  return false;
}

void BedJetClimate::update() {
  ESP_LOGD(TAG, "[%s] update()", this->get_name().c_str());
  // TODO: if the hub component is already polling, do we also need to include polling?
  //  We're already going to get on_status() at the hub's polling interval.
  auto result = this->update_status_();
  ESP_LOGD(TAG, "[%s] update_status result=%s", this->get_name().c_str(), result ? "true" : "false");
}

}  // namespace bedjet
}  // namespace esphome

#endif
