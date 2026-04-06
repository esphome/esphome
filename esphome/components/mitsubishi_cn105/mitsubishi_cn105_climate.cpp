#include <cinttypes>
#include "mitsubishi_cn105_climate.h"
#include "esphome/core/log.h"

namespace esphome::mitsubishi_cn105 {

static const char *const TAG = "mitsubishi_cn105.climate";

static constexpr std::array MODE_MAP{
    std::pair{MitsubishiCN105::Mode::AUTO, climate::CLIMATE_MODE_AUTO},
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

void MitsubishiCN105Climate::dump_config() {
  LOG_CLIMATE("", "Mitsubishi CN105 Climate", this);
  if (this->hp_.is_room_temperature_enabled()) {
    ESP_LOGCONFIG(TAG, "  Current temperature min interval: %" PRIu32 " ms",
                  this->hp_.get_room_temperature_min_interval());
  } else {
    ESP_LOGCONFIG(TAG, "  Current temperature: disabled");
  }
  ESP_LOGCONFIG(TAG,
                "  Update interval: %" PRIu32 " ms\n"
                "  UART: baud_rate=%" PRIu32 " data_bits=%u parity=%s stop_bits=%u",
                this->hp_.get_update_interval(), this->parent_->get_baud_rate(), this->parent_->get_data_bits(),
                LOG_STR_ARG(parity_to_str(this->parent_->get_parity())), this->parent_->get_stop_bits());
}

void MitsubishiCN105Climate::setup() { this->hp_.initialize(); }

void MitsubishiCN105Climate::loop() {
  if (this->hp_.update()) {
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

  traits.set_visual_min_temperature(16.0f);
  traits.set_visual_max_temperature(31.0f);
  traits.set_visual_temperature_step(1.0f);

  if (this->hp_.is_room_temperature_enabled()) {
    traits.add_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE);
    traits.set_visual_current_temperature_step(0.5f);
  }

  return traits;
}

void MitsubishiCN105Climate::control(const climate::ClimateCall &call) {}

void MitsubishiCN105Climate::apply_values_() {
  const auto &status = this->hp_.status();

  this->target_temperature = status.target_temperature;

  if (this->hp_.is_room_temperature_enabled()) {
    this->current_temperature = status.room_temperature;
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

  this->publish_state();
}

}  // namespace esphome::mitsubishi_cn105
