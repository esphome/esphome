#include "mitsubishi_cn105_climate.h"

#include "esphome/core/log.h"

namespace esphome::mitsubishi_cn105 {

static const char *const TAG = "mitsubishi_cn105.climate";

static constexpr std::array MODE_MAP{
    std::pair{MitsubishiCN105::Mode::AUTO, climate::CLIMATE_MODE_HEAT_COOL},
    std::pair{MitsubishiCN105::Mode::HEAT, climate::CLIMATE_MODE_HEAT},
    std::pair{MitsubishiCN105::Mode::DRY, climate::CLIMATE_MODE_DRY},
    std::pair{MitsubishiCN105::Mode::COOL, climate::CLIMATE_MODE_COOL},
    std::pair{MitsubishiCN105::Mode::FAN_ONLY, climate::CLIMATE_MODE_FAN_ONLY},
};

static constexpr std::array FAN_MODE_MAP{
    std::pair{MitsubishiCN105::FanMode::AUTO, climate::CLIMATE_FAN_AUTO},
    std::pair{MitsubishiCN105::FanMode::QUIET, climate::CLIMATE_FAN_QUIET},
    std::pair{MitsubishiCN105::FanMode::SPEED_1, climate::CLIMATE_FAN_LOW},
    std::pair{MitsubishiCN105::FanMode::SPEED_2, climate::CLIMATE_FAN_MEDIUM},
    std::pair{MitsubishiCN105::FanMode::SPEED_3, climate::CLIMATE_FAN_MIDDLE},
    std::pair{MitsubishiCN105::FanMode::SPEED_4, climate::CLIMATE_FAN_HIGH},
};

template<typename A, typename B, std::size_t N>
static bool map_lookup(const std::array<std::pair<A, B>, N> &map, A key, B &out) {
  for (const auto &[from, to] : map) {
    if (from == key) {
      out = to;
      return true;
    }
  }
  return false;
}

template<typename Left, typename Right, std::size_t N>
static constexpr std::optional<Left> reverse_map_lookup(const std::array<std::pair<Left, Right>, N> &map, Right key) {
  for (const auto &entry : map) {
    if (entry.second == key) {
      return entry.first;
    }
  }
  return std::nullopt;
}

template<typename Left, typename Right, std::size_t N>
static constexpr std::optional<Left> reverse_map_lookup(const std::array<std::pair<Left, Right>, N> &map,
                                                        const std::optional<Right> &key) {
  return key.has_value() ? reverse_map_lookup(map, *key) : std::nullopt;
}

void MitsubishiCN105Climate::dump_config() {
  LOG_CLIMATE("", "Mitsubishi CN105 Climate", this);
  ESP_LOGCONFIG(TAG, "  Temperature unit: °%c",
                this->parent_->get_temperature_mapping().get_use_fahrenheit() ? 'F' : 'C');
}

void MitsubishiCN105Climate::setup() {
  this->parent_->add_on_status_callback([this]() { this->apply_values_(); });
  if (this->parent_->is_status_initialized()) {
    this->apply_values_();
  }
}

climate::ClimateTraits MitsubishiCN105Climate::traits() {
  climate::ClimateTraits traits;

  for (const auto &p : MODE_MAP) {
    traits.add_supported_mode(p.second);
  }

  for (const auto &p : FAN_MODE_MAP) {
    traits.add_supported_fan_mode(p.second);
  }

  traits.set_supported_swing_modes(this->swing_mode_manager_.supported_swing_modes());

  const bool use_fahrenheit = this->parent_->get_temperature_mapping().get_use_fahrenheit();
  traits.set_temperature_unit(use_fahrenheit ? TemperatureUnit::FAHRENHEIT : TemperatureUnit::CELSIUS);
  traits.set_visual_min_temperature(use_fahrenheit ? 61.0f : 16.0f);
  traits.set_visual_max_temperature(use_fahrenheit ? 88.0f : 31.0f);
  traits.set_visual_temperature_step(1.0f);

  if (this->parent_->is_telemetry_polling_enabled()) {
    traits.add_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE);
    traits.set_visual_current_temperature_step(use_fahrenheit ? 1.0f : 0.5f);
  }

  return traits;
}

void MitsubishiCN105Climate::control(const climate::ClimateCall &call) {
  if (const auto target_temperature = call.get_target_temperature()) {
    this->parent_->set_target_temperature(this->parent_->get_temperature_mapping().to_mitsubishi(*target_temperature));
  }

  if (const auto mode = call.get_mode()) {
    if (*mode == climate::CLIMATE_MODE_OFF) {
      this->parent_->set_power(false);
    } else if (const auto mapped = reverse_map_lookup(MODE_MAP, *mode)) {
      this->parent_->set_power(true);
      this->parent_->set_mode(*mapped);
    }
  }

  if (const auto fan_mode = reverse_map_lookup(FAN_MODE_MAP, call.get_fan_mode())) {
    this->parent_->set_fan_mode(*fan_mode);
  }

  if (const auto swing_mode = call.get_swing_mode()) {
    if (const auto vane = this->swing_mode_manager_.vane_from(*swing_mode)) {
      this->parent_->set_vane_mode(*vane);
    }
    if (const auto wide = this->swing_mode_manager_.wide_vane_from(*swing_mode)) {
      this->parent_->set_wide_vane_mode(*wide);
    }
  }

  this->parent_->publish_status();
}

void MitsubishiCN105Climate::apply_values_() {
  const auto &status = this->parent_->status();

  this->target_temperature = this->parent_->get_temperature_mapping().from_mitsubishi(status.target_temperature);

  if (this->parent_->is_telemetry_polling_enabled()) {
    this->current_temperature = this->parent_->get_temperature_mapping().from_mitsubishi(status.room_temperature);
  }

  if (status.power_on) {
    if (!map_lookup(MODE_MAP, status.mode, this->mode)) {
      ESP_LOGD(TAG, "Unable to map mode");
    }
  } else {
    this->mode = climate::CLIMATE_MODE_OFF;
  }

  climate::ClimateFanMode fan_mode;
  if (map_lookup(FAN_MODE_MAP, status.fan_mode, fan_mode)) {
    this->fan_mode = fan_mode;
  } else {
    ESP_LOGD(TAG, "Unable to map fan mode");
  }

  if (const auto swing_mode =
          this->swing_mode_manager_.update_and_get_swing_mode(status.vane_mode, status.wide_vane_mode)) {
    this->swing_mode = *swing_mode;
  }

  this->publish_state();
}

void MitsubishiCN105Climate::set_supported_swing_mode(climate::ClimateSwingMode mode) {
  climate::ClimateSwingModeMask supported_swing_modes;
  switch (mode) {
    case climate::CLIMATE_SWING_VERTICAL:
      supported_swing_modes.insert(climate::CLIMATE_SWING_OFF);
      supported_swing_modes.insert(climate::CLIMATE_SWING_VERTICAL);
      break;

    case climate::CLIMATE_SWING_HORIZONTAL:
      supported_swing_modes.insert(climate::CLIMATE_SWING_OFF);
      supported_swing_modes.insert(climate::CLIMATE_SWING_HORIZONTAL);
      break;

    case climate::CLIMATE_SWING_BOTH:
      supported_swing_modes.insert(climate::CLIMATE_SWING_OFF);
      supported_swing_modes.insert(climate::CLIMATE_SWING_VERTICAL);
      supported_swing_modes.insert(climate::CLIMATE_SWING_HORIZONTAL);
      supported_swing_modes.insert(climate::CLIMATE_SWING_BOTH);
      break;

    case climate::CLIMATE_SWING_OFF:
    default:
      break;
  }
  this->swing_mode_manager_.set_supported_swing_modes(supported_swing_modes);
}

}  // namespace esphome::mitsubishi_cn105
