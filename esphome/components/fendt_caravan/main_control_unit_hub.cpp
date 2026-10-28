#include "main_control_unit_hub.h"

#ifdef USE_ESP32
namespace esphome::fendt_caravan {
ESPHOME_LOG_TAG(TAG, "FC.CU");

enum class Key : uint8_t {
  KEY_TEMP_IN,
  KEY_TEMP_OUT,
  KEY_WATER_LEVEL,
  KEY_LINE_EN,
  KEY_HS_KEY_STATE,
  KEY_FLOOR_HEATER_ON,
  KEY_SOFTWARE_VERSION,
};
static constexpr const char *KEY_NAMES[] = {
    "TEMP_IN", "TEMP_OUT", "WATER_LEVEL", "LINE_EN", "HS_KEY_STATE", "FLOOR_HEATER_ON", "SOFTWARE_VERSION",
};

void MainControlUnitHub::dump_config() {
  ESP_LOGCONFIG(TAG, "Fendt Control Unit");
  LOG_SWITCH(TAG, "  Main Switch", this->main_switch_switch_);
  LOG_SWITCH(TAG, "  All Lights Status", this->all_lights_switch_);
  LOG_SENSOR(TAG, "  Temp In", this->temp_in_sensor_);
  LOG_SENSOR(TAG, "  Temp Out", this->temp_out_sensor_);
  LOG_BINARY_SENSOR(TAG, "  Power Status", this->power_status_binary_sensor_);
  LOG_TEXT_SENSOR(TAG, "  Software Version", this->software_version_text_sensor_);
  LOG_SWITCH(TAG, "  Floor Heater", this->floor_heater_switch_);
}

void MainControlUnitHub::update() {
  if (this->temp_in_sensor_) {
    float state = this->temp_in_sensor_->state;
    this->temp_in_sensor_->publish_state(state);
  }
  if (this->temp_out_sensor_) {
    float state = this->temp_out_sensor_->state;
    this->temp_out_sensor_->publish_state(state);
  }
}

void MainControlUnitHub::write_state(switch_::Switch *sw, bool state) {
  if (sw != nullptr) {
    if (sw == this->main_switch_switch_) {
      bool current_state = this->hs_key_state_ > 0;
      if (current_state == state)
        return;
      if (state) {
        std::string cmd = Commands::update_run("HS_KEY_LONG");
        this->parent_->send_command(cmd);
      } else {
        std::string cmd = Commands::update_run("HS_KEY");
        this->parent_->send_command(cmd);
      }
    } else if (sw == this->all_lights_switch_) {
      bool current_state = this->hs_key_state_ == 2;
      if (current_state == state)
        return;
      std::string cmd = Commands::update_run("HS_KEY");
      this->parent_->send_command(cmd);
    } else if (sw == this->floor_heater_switch_) {
      std::string cmd = Commands::update_toggle("FLOOR_HEATER_ON");
      this->parent_->send_command(cmd);
    }
  }
}

bool MainControlUnitHub::decode(const char *key, const char *value) {
  size_t i = 0;
  while (i < std::size(KEY_NAMES) && strcmp(KEY_NAMES[i], key) != 0)
    i++;
  if (i == std::size(KEY_NAMES))
    return false;
  switch (static_cast<Key>(i)) {
    case Key::KEY_TEMP_IN: {
      float temp_in_state = DeviceDecoders::decode_float(value);
      if (this->temp_in_sensor_ != nullptr)
        this->temp_in_sensor_->state = temp_in_state;
      break;
    }
    case Key::KEY_TEMP_OUT: {
      float temp_out_state = DeviceDecoders::decode_float(value);
      if (this->temp_out_sensor_ != nullptr)
        this->temp_out_sensor_->state = temp_out_state;
      break;
    }
    case Key::KEY_WATER_LEVEL: {
      int water_level = DeviceDecoders::decode_int(value);
      if (this->water_level_sensor_ != nullptr)
        this->water_level_sensor_->publish_state(float(water_level * 100) / 4.0f);
      break;
    }
    case Key::KEY_LINE_EN: {
      if (this->power_status_binary_sensor_ != nullptr) {
        bool line_en = DeviceDecoders::decode_bool(value);
        this->power_status_binary_sensor_->publish_state(line_en);
      }
      break;
    }
    case Key::KEY_HS_KEY_STATE: {
      this->hs_key_state_ = DeviceDecoders::decode_int(value);
      if (this->main_switch_switch_)
        this->main_switch_switch_->publish_state(this->hs_key_state_ > 0);
      if (this->all_lights_switch_)
        this->all_lights_switch_->publish_state(this->hs_key_state_ == 2);
      break;
    }
    case Key::KEY_FLOOR_HEATER_ON: {
      if (this->floor_heater_switch_ != nullptr) {
        bool floor_heater_on = DeviceDecoders::decode_bool(value);
        this->floor_heater_switch_->publish_state(floor_heater_on);
      }
      break;
    }
    case Key::KEY_SOFTWARE_VERSION: {
      if (this->software_version_text_sensor_ != nullptr)
        this->software_version_text_sensor_->publish_state(value);
      break;
    }
    default:
      break;
  }
  return true;
}

}  // namespace esphome::fendt_caravan
#endif
