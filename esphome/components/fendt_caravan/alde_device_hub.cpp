#include "alde_device_hub.h"

#ifdef USE_ESP32
namespace esphome::fendt_caravan {
ESPHOME_LOG_TAG(TAG, "FC.alde");

enum class Key : uint8_t {
  KEY_HEATER_AVAILABLE,
  KEY_HEATER_ONOFF,
  KEY_HEATER_TEMP,
  KEY_HEATER_WATER,
  KEY_HEATER_WATER_TEMP,
  KEY_HEATER_EL,
  KEY_HEATER_GAS,
};

static constexpr const char *KEY_NAMES[] = {
    "HEATER_AVAILABLE", "HEATER_ONOFF", "HEATER_TEMP", "HEATER_WATER", "HEATER_WATER_TEMP", "HEATER_EL", "HEATER_GAS",
};

void AldeDeviceHub::setup() {
  if (this->alde_climate_) {
    this->alde_climate_->add_on_control_callback([this](climate::ClimateCall &call) {
      if (call.get_target_temperature().has_value()) {
        float target_temp = call.get_target_temperature().value();
        this->parent_->send_command(Commands::update_temp_10("HEATER_TEMP", target_temp));
      }
    });
  }
  if (this->alde_heater_electricity_select_) {
    this->alde_heater_electricity_select_->add_on_state_callback([this](size_t state) {
      ESP_LOGV(TAG, "Heater electricity select state: %d", state);
      std::string cmd =
          Commands::update_heater_el("HEATER_EL", this->alde_heater_electricity_select_->at(state).value());
      this->parent_->send_command(cmd);
    });
  }
}

void AldeDeviceHub::dump_config() {
  ESP_LOGCONFIG(TAG, " -Fendt Alde Device-");
  LOG_BINARY_SENSOR(TAG, "  Alde Status", this->alde_status_binary_sensor_);
  LOG_CLIMATE(TAG, "  Alde climate", this->alde_climate_);
  LOG_SWITCH(TAG, "  Alde heater switch", this->alde_heater_switch_);
  LOG_SWITCH(TAG, "  Alde heater water", this->alde_water_heater_switch_);
  LOG_SWITCH(TAG, "  Alde heater Water boost", this->alde_water_heater_boost_switch_);
  LOG_SELECT(TAG, "  Heater Electric", this->alde_heater_electricity_select_);
  LOG_SWITCH(TAG, "  Heater Gas", this->alde_heater_gas_switch_);
}

void AldeDeviceHub::write_state(switch_::Switch *sw, bool state) {
  ESP_LOGD(TAG, "Write state called for switch: %p, state: %s", sw, TRUEFALSE(state));
  if (sw == this->alde_heater_switch_) {
    std::string cmd = Commands::update_toggle("HEATER_ONOFF");
    this->parent_->send_command(cmd);
  } else if (sw == this->alde_water_heater_switch_) {
    std::string cmd = Commands::update_toggle("HEATER_WATER");
    this->parent_->send_command(cmd);
  } else if (sw == this->alde_water_heater_boost_switch_) {
    std::string cmd = Commands::update_toggle("HEATER_WATER_TEMP");
    this->parent_->send_command(cmd);
  } else if (sw == this->alde_heater_gas_switch_) {
    std::string cmd = Commands::update_run("HEATER_GAS");
    this->parent_->send_command(cmd);
  }
}

bool AldeDeviceHub::decode(const char *key, const char *value) {
  size_t i = 0;
  while (i < std::size(KEY_NAMES) && strcmp(KEY_NAMES[i], key) != 0)
    i++;
  if (i == std::size(KEY_NAMES))
    return false;
  Key key_value = static_cast<Key>(i);
  switch (key_value) {
    case Key::KEY_HEATER_AVAILABLE: {
      bool alde_status = DeviceDecoders::decode_bool(value);
      if (this->alde_status_binary_sensor_)
        this->alde_status_binary_sensor_->publish_state(alde_status);
      break;
    }
    case Key::KEY_HEATER_ONOFF: {
      bool alde_heater_onoff = DeviceDecoders::decode_bool(value);
      if (this->alde_heater_switch_)
        this->alde_heater_switch_->publish_state(alde_heater_onoff);
      break;
    }
    case Key::KEY_HEATER_TEMP: {
      float alde_heater_temp = DeviceDecoders::decode_float(value);
      if (this->alde_climate_) {
        this->alde_climate_->target_temperature = alde_heater_temp;
        this->alde_climate_->publish_state();
      }
      break;
    }
    case Key::KEY_HEATER_WATER: {
      bool alde_heater_water = DeviceDecoders::decode_bool(value);
      if (this->alde_water_heater_switch_)
        this->alde_water_heater_switch_->publish_state(alde_heater_water);
      break;
    }
    case Key::KEY_HEATER_WATER_TEMP: {
      float alde_heater_water_temp = DeviceDecoders::decode_float(value);
      if (this->alde_water_heater_boost_switch_)
        this->alde_water_heater_boost_switch_->publish_state(alde_heater_water_temp == 65.0f);
      break;
    }
    case Key::KEY_HEATER_EL: {
      if (this->alde_heater_electricity_select_)
        this->alde_heater_electricity_select_->publish_state(value);
      break;
    }
    case Key::KEY_HEATER_GAS: {
      bool alde_heater_gas = DeviceDecoders::decode_bool(value);
      if (this->alde_heater_gas_switch_)
        this->alde_heater_gas_switch_->publish_state(alde_heater_gas);
      break;
    }
    default:
      break;
  }
  return true;
}

}  // namespace esphome::fendt_caravan
#endif
