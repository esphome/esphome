#include "mitsubishi_cn105_climate.h"
#include "esphome/core/log.h"

namespace esphome::mitsubishi_cn105 {

static const char *const TAG = "mitsubishi_cn105.climate";

static constexpr std::array<std::pair<MitsubishiCN105::Mode, climate::ClimateMode>, 5> MODE_MAP{{
    {MitsubishiCN105::Mode::AUTO, climate::CLIMATE_MODE_AUTO},
    {MitsubishiCN105::Mode::HEAT, climate::CLIMATE_MODE_HEAT},
    {MitsubishiCN105::Mode::DRY, climate::CLIMATE_MODE_DRY},
    {MitsubishiCN105::Mode::COOL, climate::CLIMATE_MODE_COOL},
    {MitsubishiCN105::Mode::FAN_ONLY, climate::CLIMATE_MODE_FAN_ONLY},
}};

static constexpr std::array<std::pair<MitsubishiCN105::FanMode, climate::ClimateFanMode>, 6> FAN_MODE_MAP{{
    {MitsubishiCN105::FanMode::AUTO, climate::CLIMATE_FAN_AUTO},
    {MitsubishiCN105::FanMode::QUIET, climate::CLIMATE_FAN_QUIET},
    {MitsubishiCN105::FanMode::SPEED_1, climate::CLIMATE_FAN_LOW},
    {MitsubishiCN105::FanMode::SPEED_2, climate::CLIMATE_FAN_MEDIUM},
    {MitsubishiCN105::FanMode::SPEED_3, climate::CLIMATE_FAN_MIDDLE},
    {MitsubishiCN105::FanMode::SPEED_4, climate::CLIMATE_FAN_HIGH},
}};

template<typename Left, typename Right, std::size_t N>
static constexpr std::optional<Right> lookup(const std::array<std::pair<Left, Right>, N> &map, Left key) {
  for (const auto &entry : map) {
    if (entry.first == key) {
      return entry.second;
    }
  }
  return std::nullopt;
}

template<typename Left, typename Right, std::size_t N>
static constexpr std::optional<Left> lookup_reverse(const std::array<std::pair<Left, Right>, N> &map, Right key) {
  for (const auto &entry : map) {
    if (entry.second == key) {
      return entry.first;
    }
  }
  return std::nullopt;
}

template<typename Left, typename Right, std::size_t N>
static constexpr std::optional<Left> lookup_reverse(const std::array<std::pair<Left, Right>, N> &map,
                                                    const std::optional<Right> &key) {
  return key.has_value() ? lookup_reverse(map, *key) : std::nullopt;
}

void MitsubishiCN105Climate::dump_config() {
  LOG_CLIMATE("", "Mitsubishi CN105 Climate", this);

  const char *parity_str = this->parent_->get_parity() == uart::UART_CONFIG_PARITY_EVEN  ? "EVEN"
                           : this->parent_->get_parity() == uart::UART_CONFIG_PARITY_ODD ? "ODD"
                                                                                         : "NONE";

  ESP_LOGCONFIG(TAG,
                "  Update interval: %u ms\n"
                "  UART: baud_rate=%u data_bits=%u parity=%s stop_bits=%u",
                this->hp_.get_update_interval(), this->parent_->get_baud_rate(), this->parent_->get_data_bits(),
                parity_str, this->parent_->get_stop_bits());
}

void MitsubishiCN105Climate::setup() {
  this->hp_.set_connection_state_callback([this](bool connected) {
    if (connected) {
      this->status_clear_warning();
    } else {
      this->status_set_warning("No response from AC");
    }
  });

  this->hp_.init();
}

void MitsubishiCN105Climate::loop() {
  if (this->hp_.sync()) {
    this->apply_values_();
  }
}

climate::ClimateTraits MitsubishiCN105Climate::traits() {
  climate::ClimateTraits traits;

  traits.set_supported_modes({
      climate::CLIMATE_MODE_OFF,
      climate::CLIMATE_MODE_COOL,
      climate::CLIMATE_MODE_HEAT,
      climate::CLIMATE_MODE_DRY,
      climate::CLIMATE_MODE_FAN_ONLY,
      climate::CLIMATE_MODE_AUTO,
  });

  traits.set_supported_fan_modes({
      climate::CLIMATE_FAN_AUTO,
      climate::CLIMATE_FAN_QUIET,
      climate::CLIMATE_FAN_LOW,
      climate::CLIMATE_FAN_MEDIUM,
      climate::CLIMATE_FAN_MIDDLE,
      climate::CLIMATE_FAN_HIGH,
  });

  traits.add_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE);

  traits.set_visual_min_temperature(16.0f);
  traits.set_visual_max_temperature(31.5f);
  traits.set_visual_temperature_step(1.0f);
  traits.set_visual_current_temperature_step(0.5f);

  return traits;
}

void MitsubishiCN105Climate::control(const climate::ClimateCall &call) {
  if (const auto target_temperature = call.get_target_temperature()) {
    const auto adjusted = temperature_mapping_.to_mitsubishi(*target_temperature);
    this->hp_.set_target_temperature(adjusted);
  }

  if (const auto mode = call.get_mode()) {
    if (*mode == climate::CLIMATE_MODE_OFF) {
      this->hp_.set_power(false);
    } else if (const auto mapped = lookup_reverse(MODE_MAP, *mode)) {
      this->hp_.set_power(true);
      this->hp_.set_mode(*mapped);
    }
  }

  if (const auto fan_mode = lookup_reverse(FAN_MODE_MAP, call.get_fan_mode())) {
    this->hp_.set_fan_mode(*fan_mode);
  }

  if (this->hp_.is_status_initialized()) {
    this->apply_values_();
  }
}

void MitsubishiCN105Climate::apply_values_() {
  const auto status = this->hp_.status();
  bool is_valid = true;

  this->target_temperature = temperature_mapping_.from_mitsubishi(status.settings.target_temperature);
  this->current_temperature = temperature_mapping_.from_mitsubishi(status.room_temperature);

  if (status.settings.power_on) {
    if (const auto mapped = lookup(MODE_MAP, status.settings.mode)) {
      this->mode = *mapped;
    } else {
      ESP_LOGW(TAG, "Failed to map climate mode: %u", static_cast<uint8_t>(status.settings.mode));
      is_valid = false;
    }
  } else {
    this->mode = climate::CLIMATE_MODE_OFF;
  }

  if (const auto mapped = lookup(FAN_MODE_MAP, status.settings.fan_mode)) {
    this->fan_mode = mapped;
  } else {
    ESP_LOGW(TAG, "Failed to map fan mode: %u", static_cast<uint8_t>(status.settings.fan_mode));
    is_valid = false;
  }

  if (is_valid || this->has_state()) {
    this->publish_state();
  }
}

TemperatureMapping TemperatureMapping::identity() {
  return TemperatureMapping{
      .to_mitsubishi = [](float c) -> float { return c; },
      .from_mitsubishi = [](float c) -> float { return c; },
  };
}

TemperatureMapping TemperatureMapping::fahrenheit() {
  return TemperatureMapping{
      .to_mitsubishi = [](float c) -> float {
        const int f = std::clamp(static_cast<int>(std::round(c * 1.8f + 32.0f)), 61, 88);
        return 0.5f * (f - 28 + (f > 68) - (f < 68));
      },
      .from_mitsubishi = [](float c) -> float {
        const int mh = static_cast<int>(std::round(c * 2.0f));
        return (mh - 3 - (mh >= 40) - (mh > 40)) * (5.0f / 9.0f);
      },
  };
}

}  // namespace esphome::mitsubishi_cn105
