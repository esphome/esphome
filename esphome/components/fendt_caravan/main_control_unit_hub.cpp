#include "main_control_unit_hub.h"

#ifdef USE_ESP32
namespace esphome::fendt_caravan {
ESPHOME_LOG_TAG(TAG, "fendt_caravan.mcu");

enum class Key : uint8_t {
  KEY_TEMP_IN,
  KEY_TEMP_OUT,
  KEY_WATER_LEVEL,
  KEY_LINE_EN,
  KEY_HS_KEY_STATE,
  KEY_FLOOR_HEATER_ON,
  KEY_SOFTWARE_VERSION,
  KEY_PANEL_VERSION,
};
static constexpr const char *KEY_NAMES[] = {"TEMP_IN",      "TEMP_OUT",        "WATER_LEVEL",      "LINE_EN",
                                            "HS_KEY_STATE", "FLOOR_HEATER_ON", "SOFTWARE_VERSION", "PANEL_VERSION"};

void MainControlUnitHub::dump_config() {
  ESP_LOGCONFIG(TAG, "Fendt Control Unit");
  LOG_UPDATE_INTERVAL(this);
  LOG_SWITCH(TAG, "  Main Switch", this->main_switch_switch_);
  LOG_SWITCH(TAG, "  All Lights Status", this->all_lights_switch_);
  LOG_SENSOR(TAG, "  Temp In", this->temp_in_sensor_);
  LOG_SENSOR(TAG, "  Temp Out", this->temp_out_sensor_);
  LOG_SENSOR(TAG, "  Water Level", this->water_level_sensor_);
  LOG_BINARY_SENSOR(TAG, "  Power Status", this->power_status_binary_sensor_);
  LOG_TEXT_SENSOR(TAG, "  Software Version", this->software_version_text_sensor_);
  LOG_SWITCH(TAG, "  Floor Heater", this->floor_heater_switch_);
  LOG_TEXT_SENSOR(TAG, "  Panel Version", this->panel_version_text_sensor_);
}

// The panel reports temperatures many times a second; publish only the latest one once per update interval
void MainControlUnitHub::update() {
  const float temp_in = this->temp_in_;
  const float temp_out = this->temp_out_;
  this->temp_in_ = NAN;
  this->temp_out_ = NAN;
  if (!std::isnan(temp_in))
    PUBLISH_STATE(this->temp_in_sensor_, temp_in);
  if (!std::isnan(temp_out))
    PUBLISH_STATE(this->temp_out_sensor_, temp_out);
}

void MainControlUnitHub::write_state(switch_::Switch *sw, bool state) {
  ESP_LOGD(TAG, "Write state called for switch: %p, state: %s", sw, TRUEFALSE(state));
  if (sw == nullptr)
    return;
  char cmd[COMMAND_MAX];
  if (sw == this->main_switch_switch_) {
    if ((this->hs_key_state_ > 0) == state)
      return;
    this->parent_->send_command(Commands::update_run(cmd, state ? "HS_KEY_LONG" : "HS_KEY"));
  } else if (sw == this->all_lights_switch_) {
    if ((this->hs_key_state_ == 2) == state)
      return;
    this->parent_->send_command(Commands::update_run(cmd, "HS_KEY"));
  } else if (sw == this->floor_heater_switch_) {
    if (!sw->has_state() || (sw->state != sw->is_inverted()) == state)
      return;
    this->parent_->send_command(Commands::update_toggle(cmd, "FLOOR_HEATER_ON"));
  }
}

bool MainControlUnitHub::decode(const char *key, const char *value) {
  size_t i = 0;
  while (i < std::size(KEY_NAMES) && strcmp(KEY_NAMES[i], key) != 0)
    i++;
  if (i == std::size(KEY_NAMES))
    return false;
  ESP_LOGV(TAG, "Handled key: %s, value: %s", key, value);
  switch (static_cast<Key>(i)) {
    case Key::KEY_TEMP_IN: {
      this->temp_in_ = DeviceDecoders::decode_float(value);
      break;
    }
    case Key::KEY_TEMP_OUT: {
      this->temp_out_ = DeviceDecoders::decode_float(value);
      break;
    }
    case Key::KEY_WATER_LEVEL: {
      int level;
      if (DeviceDecoders::decode_int(value, level))
        PUBLISH_STATE(this->water_level_sensor_, float(level * 100) / 4.0f);
      break;
    }
    case Key::KEY_LINE_EN: {
      PUBLISH_STATE(this->power_status_binary_sensor_, DeviceDecoders::decode_bool(value));
      break;
    }
    case Key::KEY_HS_KEY_STATE: {
      if (!DeviceDecoders::decode_int(value, this->hs_key_state_))
        break;
      PUBLISH_STATE(this->main_switch_switch_, this->hs_key_state_ > 0);
      PUBLISH_STATE(this->all_lights_switch_, this->hs_key_state_ == 2);
      break;
    }
    case Key::KEY_FLOOR_HEATER_ON: {
      PUBLISH_STATE(this->floor_heater_switch_, DeviceDecoders::decode_bool(value));
      break;
    }
    case Key::KEY_SOFTWARE_VERSION: {
      PUBLISH_STATE(this->software_version_text_sensor_, value);
      break;
    }
    case Key::KEY_PANEL_VERSION: {
      PUBLISH_STATE(this->panel_version_text_sensor_, value);
      break;
    }
    default:
      break;
  }
  return true;
}

}  // namespace esphome::fendt_caravan
#endif
