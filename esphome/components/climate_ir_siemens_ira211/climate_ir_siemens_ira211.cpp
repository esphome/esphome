#include "climate_ir_siemens_ira211.h"
#include "esphome/core/log.h"
#include <algorithm>
#include <cmath>

namespace esphome {
namespace climate_ir_siemens_ira211 {

using namespace remote_base;

static const char *const TAG = "climate.siemens_ira211";

// --- Mapping helpers ---

climate::ClimateMode SiemensIRA211Climate::active_climate_mode_() const {
  if (this->supports_heat_ && this->supports_cool_)
    return climate::CLIMATE_MODE_HEAT_COOL;
  if (this->supports_heat_)
    return climate::CLIMATE_MODE_HEAT;
  if (this->supports_cool_)
    return climate::CLIMATE_MODE_COOL;
  return climate::CLIMATE_MODE_FAN_ONLY;
}

climate::ClimateTraits SiemensIRA211Climate::traits() {
  auto traits = climate_ir::ClimateIR::traits();
  if (this->supports_heat_ && this->supports_cool_) {
    traits.set_supported_modes({climate::CLIMATE_MODE_OFF, climate::CLIMATE_MODE_HEAT_COOL});
  } else if (this->supports_heat_) {
    traits.set_supported_modes({climate::CLIMATE_MODE_OFF, climate::CLIMATE_MODE_HEAT});
  } else if (this->supports_cool_) {
    traits.set_supported_modes({climate::CLIMATE_MODE_OFF, climate::CLIMATE_MODE_COOL});
  } else {
    traits.set_supported_modes({climate::CLIMATE_MODE_OFF, climate::CLIMATE_MODE_FAN_ONLY});
  }
  return traits;
}

IRA211Mode SiemensIRA211Climate::climate_preset_to_ira211_mode_() const {
  if (this->preset.has_value()) {
    switch (*this->preset) {
      case climate::CLIMATE_PRESET_ECO:
        return IRA211Mode::TIMER;
      case climate::CLIMATE_PRESET_COMFORT:
      default:
        return IRA211Mode::COMFORT;
    }
  }
  return IRA211Mode::COMFORT;
}

IRA211Fan SiemensIRA211Climate::climate_fan_to_ira211_fan_() const {
  if (this->fan_mode.has_value()) {
    switch (*this->fan_mode) {
      case climate::CLIMATE_FAN_LOW:
        return IRA211Fan::FAN_LOW;
      case climate::CLIMATE_FAN_MEDIUM:
        return IRA211Fan::FAN_MEDIUM;
      case climate::CLIMATE_FAN_HIGH:
        return IRA211Fan::FAN_HIGH;
      case climate::CLIMATE_FAN_AUTO:
      default:
        return IRA211Fan::FAN_AUTO;
    }
  }
  return IRA211Fan::FAN_AUTO;
}

void SiemensIRA211Climate::ira211_mode_to_climate_preset_(IRA211Mode mode) {
  switch (mode) {
    case IRA211Mode::TIMER:
      this->preset = climate::CLIMATE_PRESET_ECO;
      break;
    case IRA211Mode::COMFORT:
    default:
      this->preset = climate::CLIMATE_PRESET_COMFORT;
      break;
  }
}

void SiemensIRA211Climate::ira211_fan_to_climate_fan_(IRA211Fan fan) {
  switch (fan) {
    case IRA211Fan::FAN_LOW:
      this->fan_mode = climate::CLIMATE_FAN_LOW;
      break;
    case IRA211Fan::FAN_MEDIUM:
      this->fan_mode = climate::CLIMATE_FAN_MEDIUM;
      break;
    case IRA211Fan::FAN_HIGH:
      this->fan_mode = climate::CLIMATE_FAN_HIGH;
      break;
    case IRA211Fan::FAN_AUTO:
    default:
      this->fan_mode = climate::CLIMATE_FAN_AUTO;
      break;
  }
}

// --- Transmit ---

void SiemensIRA211Climate::transmit_frame_(IRA211Command cmd, IRA211Mode mode, IRA211Fan fan) {
  // Round to nearest 0.5°C step and clamp to valid range
  float clamped = std::max(5.0f, std::min(35.0f, this->target_temperature));
  float rounded = std::round(clamped * 2.0f) / 2.0f;
  auto temp = static_cast<uint8_t>(rounded);
  uint8_t tenths = (rounded - temp) >= 0.25f ? 5 : 0;

  IRA211Data data;
  data.set_command(cmd);
  data.set_temperature(temp, tenths);
  data.set_mode(mode);
  data.set_fan(fan);
  data.finalize();

  auto transmit = this->transmitter_->transmit();
  IRA211Protocol().encode(transmit.get_data(), data);
  transmit.perform();
}

void SiemensIRA211Climate::transmit_state() {
  auto mode = this->climate_preset_to_ira211_mode_();
  auto fan = this->climate_fan_to_ira211_fan_();

  if (this->mode == climate::CLIMATE_MODE_OFF) {
    // Power off: send POWER with PROTECTION mode
    this->transmit_frame_(IRA211Command::POWER, IRA211Mode::PROTECTION, fan);
    this->is_on_ = false;
  } else {
    if (!this->is_on_) {
      // Turning on: send POWER first, then SYNC
      this->transmit_frame_(IRA211Command::POWER, mode, fan);
      this->is_on_ = true;
    }
    // Send full state via SYNC
    this->transmit_frame_(IRA211Command::SYNC, mode, fan);
  }
}

// --- Receive ---

bool SiemensIRA211Climate::on_receive(RemoteReceiveData data) {
  auto decoded = IRA211Protocol().decode(data);
  if (!decoded.has_value())
    return false;

  auto &frame = decoded.value();
  ESP_LOGD(TAG, "Received IRA211 command");

  switch (frame.get_command()) {
    case IRA211Command::SYNC:
    case IRA211Command::POWER: {
      // Full state frame
      if (frame.get_mode() == IRA211Mode::PROTECTION) {
        this->mode = climate::CLIMATE_MODE_OFF;
        this->is_on_ = false;
      } else {
        this->mode = this->active_climate_mode_();
        this->is_on_ = true;
        this->ira211_mode_to_climate_preset_(frame.get_mode());
      }
      this->target_temperature = frame.get_temperature() + frame.get_temp_tenths() / 10.0f;
      this->ira211_fan_to_climate_fan_(frame.get_fan());
      break;
    }
    case IRA211Command::TEMP_UP:
    case IRA211Command::TEMP_DOWN: {
      this->target_temperature = frame.get_temperature() + frame.get_temp_tenths() / 10.0f;
      break;
    }
    case IRA211Command::MODE: {
      if (frame.get_mode() == IRA211Mode::PROTECTION) {
        this->mode = climate::CLIMATE_MODE_OFF;
        this->is_on_ = false;
      } else {
        if (!this->is_on_) {
          this->mode = this->active_climate_mode_();
          this->is_on_ = true;
        }
        this->ira211_mode_to_climate_preset_(frame.get_mode());
      }
      break;
    }
    case IRA211Command::FAN: {
      this->ira211_fan_to_climate_fan_(frame.get_fan());
      break;
    }
    default:
      return false;
  }

  this->publish_state();
  return true;
}

}  // namespace climate_ir_siemens_ira211
}  // namespace esphome
