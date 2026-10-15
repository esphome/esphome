#include "main_control_unit_hub.h"

#ifdef USE_ESP32
#include "decoders.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <cstring>
#include <iterator>

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
};
static constexpr const char *KEY_NAMES[] = {
    "TEMP_IN", "TEMP_OUT", "WATER_LEVEL", "LINE_EN", "HS_KEY_STATE", "FLOOR_HEATER_ON", "SOFTWARE_VERSION",
};
static constexpr int WATER_LEVEL_STEPS = 4;    // the panel reports quarters of a tank
static constexpr int HS_KEY_STATE_LIGHTS = 2;  // 0 off, 1 on, 2 on with all lights

void FendtSwitch::write_state(bool state) { this->parent_->write_switch(this, state); }

void MainControlUnitHub::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Main Control Unit\n"
                "  Address: %s\n"
                "  Handshake: %s",
                this->link_->parent()->address_str(), this->link_->get_handshake());
  LOG_SENSOR("  ", "Temperature In", this->temp_in_sensor_);
  LOG_SENSOR("  ", "Temperature Out", this->temp_out_sensor_);
  LOG_SENSOR("  ", "Water Level", this->water_level_sensor_);
  LOG_BINARY_SENSOR("  ", "Power Status", this->power_status_binary_sensor_);
  LOG_TEXT_SENSOR("  ", "Software Version", this->software_version_text_sensor_);
  LOG_SWITCH("  ", "Main Switch", this->main_switch_);
  LOG_SWITCH("  ", "All Lights", this->all_lights_switch_);
  LOG_SWITCH("  ", "Floor Heater", this->floor_heater_switch_);
}

void MainControlUnitHub::handle_report(const char *key, char *value) {
  size_t i = 0;
  while (i < std::size(KEY_NAMES) && strcmp(KEY_NAMES[i], key) != 0)
    i++;
  if (i == std::size(KEY_NAMES))
    return;
  float number = 0;
  int count = 0;
  bool flag = false;
  bool ok = true;
  switch (static_cast<Key>(i)) {
    case Key::KEY_TEMP_IN:
      ok = parse_panel_float(value, number);
      if (ok && this->temp_in_sensor_ != nullptr)
        this->temp_in_sensor_->publish_state(number);
      break;
    case Key::KEY_TEMP_OUT:
      ok = parse_panel_float(value, number);
      if (ok && this->temp_out_sensor_ != nullptr)
        this->temp_out_sensor_->publish_state(number);
      break;
    case Key::KEY_WATER_LEVEL:
      ok = parse_panel_int(value, count) && count >= 0 && count <= WATER_LEVEL_STEPS;
      if (ok && this->water_level_sensor_ != nullptr)
        this->water_level_sensor_->publish_state(count * (100.0f / WATER_LEVEL_STEPS));
      break;
    case Key::KEY_LINE_EN:
      ok = parse_panel_bool(value, flag);
      if (ok && this->power_status_binary_sensor_ != nullptr)
        this->power_status_binary_sensor_->publish_state(flag);
      break;
    case Key::KEY_HS_KEY_STATE:
      ok = parse_panel_int(value, count) && count >= 0 && count <= HS_KEY_STATE_LIGHTS;
      if (!ok)
        break;
      this->send_(this->hs_key_, this->hs_key_.on_report(count, App.get_loop_component_start_time()));
      if (this->main_switch_ != nullptr)
        this->main_switch_->publish_state(count > 0);
      if (this->all_lights_switch_ != nullptr)
        this->all_lights_switch_->publish_state(count == HS_KEY_STATE_LIGHTS);
      break;
    case Key::KEY_FLOOR_HEATER_ON:
      // An unreadable value must not count as off: it would send the toggle again
      ok = parse_panel_bool(value, flag);
      if (!ok)
        break;
      this->send_(this->floor_heater_, this->floor_heater_.on_report(flag, App.get_loop_component_start_time()));
      if (this->floor_heater_switch_ != nullptr)
        this->floor_heater_switch_->publish_state(flag);
      break;
    case Key::KEY_SOFTWARE_VERSION:
      if (this->software_version_text_sensor_ != nullptr)
        this->software_version_text_sensor_->publish_state(value);
      break;
  }
  if (!ok) {
    ESP_LOGW(TAG, "Unreadable %s: %s", key, value);
  }
}

void MainControlUnitHub::write_switch(FendtSwitch *sw, bool state) {
  // A press only makes sense against the panel's own state: the heater command is a toggle, and the key does
  // different things depending on the unit's state
  const bool heater = sw == this->floor_heater_switch_;
  PanelControl &control = heater ? this->floor_heater_ : this->hs_key_;
  control.expire(App.get_loop_component_start_time());
  const int8_t current = control.current();
  if (current < 0) {
    ESP_LOGW(TAG, "Panel state not reported yet, press skipped");
    return;
  }
  int8_t target;
  if (heater || sw == this->main_switch_) {
    // Turning on a running unit keeps its lights as they are
    target = state ? std::max<int8_t>(current, 1) : 0;
  } else {
    if (current == 0) {
      ESP_LOGW(TAG, "Unit is off, a key press would turn it on instead of the lights");
      return;
    }
    target = state ? HS_KEY_STATE_LIGHTS : 1;
  }
  this->send_(control, control.request(target));
}

void MainControlUnitHub::on_command_done(const char *command, bool sent) {
  const uint32_t now = App.get_loop_component_start_time();
  PanelControl *const controls[] = {&this->hs_key_, &this->floor_heater_};
  for (PanelControl *control : controls) {
    if (control->sends(command))
      control->on_command_done(sent, now);
  }
}

void MainControlUnitHub::send_(PanelControl &control, const char *command) {
  if (command != nullptr)
    control.on_queued(this->link_->send_command(command));
}

}  // namespace esphome::fendt_caravan
#endif
