#include "alde_device_hub.h"

#ifdef USE_ESP32
namespace esphome::fendt_caravan {
ESPHOME_LOG_TAG(TAG, "FC.alde");

enum class Key : uint8_t {
  KEY_HEATER_AVAILABLE = 0x10,
  KEY_HEATER_ONOFF,
  KEY_HEATER_TEMP,
  KEY_HEATER_WATER,
  KEY_HEATER_WATER_TEMP,
  KEY_HEATER_GAS,
};

static constexpr const char *KEY_NAMES[] = {
    "HEATER_AVAILABLE", "HEATER_ONOFF", "HEATER_TEMP", "HEATER_WATER", "HEATER_WATER_TEMP", "HEATER_GAS",
};

void AldeDeviceHub::setup() {
  /*   auto *alde_available = new Variable<bool>("HEATER_AVAILABLE", DeviceDecoders::decode_bool);
    this->add_variable(alde_available);

    auto *heater_status = new Variable<bool>("HEATER_ONOFF", DeviceDecoders::decode_bool,
    Commands::update_toggle<bool>); this->add_variable(heater_status); if (this->alde_heater_switch_) {
      this->alde_heater_switch_->add_on_state_callback([this, heater_status](bool state) {
        if (heater_status->get_value() != state) {
          heater_status->set_value(state);
          this->parent_->send_command(heater_status->get_command());
        }
      });
    }
    auto *heater_temp = new Variable<float>("HEATER_TEMP", DeviceDecoders::decode_temperature,
    Commands::update_temp_10); this->add_variable(heater_temp); if (this->alde_climate_) {
      this->alde_climate_->add_on_control_callback([this, heater_temp](climate::ClimateCall &call) {
        if (call.get_target_temperature().has_value()) {
          float target_temp = call.get_target_temperature().value();
          heater_temp->set_value(target_temp);
          this->parent_->send_command(heater_temp->get_command());
        }
      });
    }

    auto *heater_water = new Variable<bool>("HEATER_WATER", DeviceDecoders::decode_bool, Commands::update_toggle<bool>);
    this->add_variable(heater_water);
    if (this->alde_water_heater_switch_) {
      this->alde_water_heater_switch_->add_on_state_callback([this, heater_water](bool state) {
        if (heater_water->get_value() != state) {
          heater_water->set_value(state);
          this->parent_->send_command(heater_water->get_command());
        }
      });
    }

    auto *heater_water_temp = new Variable<bool>(
        "HEATER_WATER_TEMP", [](const std::string &data) { return DeviceDecoders::decode_temperature(data) == 65.0f; },
        Commands::update_toggle<bool>);
    this->add_variable(heater_water_temp);

    auto *heater_el =
        new Variable<std::string>("HEATER_EL", DeviceDecoders::decode_heater_el, Commands::update_heater_el);
    this->add_variable(heater_el);

    if (this->alde_heater_electricity_select_) {
      this->alde_heater_electricity_select_->add_on_state_callback([this, heater_el](size_t state) {
        ESP_LOGV(TAG, "Current: %s, sate: %d", heater_el->get_value().c_str(), state);
        auto index = this->alde_heater_electricity_select_->index_of(heater_el->get_value());
        if (index.has_value() && index.value() != state) {
          heater_el->set_value(this->alde_heater_electricity_select_->at(state).value());
          this->parent_->send_command(heater_el->get_command());
        }
      });
    }

    auto *heater_gas = new Variable<bool>("HEATER_GAS", DeviceDecoders::decode_bool, Commands::update_toggle<bool>);
    this->add_variable(heater_gas);
   */
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

void AldeDeviceHub::update() {}

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
      bool alde_heater_water_temp = DeviceDecoders::decode_bool(value);
      if (this->alde_water_heater_boost_switch_)
        this->alde_water_heater_boost_switch_->publish_state(alde_heater_water_temp);
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
