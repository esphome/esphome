#include "main_control_unit_hub.h"

#ifdef USE_ESP32
namespace esphome::fendt_caravan {
ESPHOME_LOG_TAG(TAG, "FC.CU");

void MainControlUnitHub::setup() {
  auto *network = new Variable<bool>("LINE_EN", DeviceDecoders::decode_bool);
  this->add_variable(network);

  auto *main_switch = new Variable<bool>("HS_EN", DeviceDecoders::decode_bool, Commands::update_toggle<bool>);
  this->add_variable(main_switch);

  auto *hs_key = new Variable<bool>("HS_KEY", nullptr, Commands::update_run);
  this->add_variable(hs_key);

  auto *hs_key_long = new Variable<bool>("HS_KEY_LONG", nullptr, Commands::update_run);
  this->add_variable(hs_key_long);

  auto *d_plus = new Variable<bool>("D_PLUS", DeviceDecoders::decode_bool);
  this->add_variable(d_plus);

  auto *battery_loading_status = new Variable<int>("IBAT_BAL", DeviceDecoders::decode_int);
  this->add_variable(battery_loading_status);

  auto *ac_active = new Variable<std::string>("AC_EN", [](const std::string &value) {
    const char *tmp[] = {"Enable", "Disable"};
    return DeviceDecoders::decode_bool_str(value, tmp);
  });
  this->add_variable(ac_active);

  auto *alarm_clock_active = new Variable<bool>("WAKE_EN", DeviceDecoders::decode_bool);
  this->add_variable(alarm_clock_active);

  auto *temp_in = new Variable<float>("TEMP_IN", DeviceDecoders::decode_temperature);
  this->add_variable(temp_in);

  auto *temp_out = new Variable<float>("TEMP_OUT", DeviceDecoders::decode_temperature);
  this->add_variable(temp_out);

  auto *battery_voltage = new Variable<float>("UBAT", DeviceDecoders::decode_voltage);
  this->add_variable(battery_voltage);

  auto *battery_voltage2 = new Variable<float>("UBATM", DeviceDecoders::decode_voltage);
  this->add_variable(battery_voltage2);

  auto *date = new Variable<time_t>("DATE", DeviceDecoders::decode_date);
  this->add_variable(date);

  auto *time = new Variable<time_t>("TIME", DeviceDecoders::decode_time);
  this->add_variable(time);

  auto *floor_heater =
      new Variable<bool>("FLOOR_HEATER_ON", DeviceDecoders::decode_bool, Commands::update_toggle<bool>);
  this->add_variable(floor_heater);

  auto *temp_in_offset = new Variable<int>("TEMP_IN_OFFSET", DeviceDecoders::decode_int);
  this->add_variable(temp_in_offset);

  auto *temp_out_offset = new Variable<int>("TEMP_OUT_OFFSET", DeviceDecoders::decode_int);
  this->add_variable(temp_out_offset);

  auto *software_version = new Variable<std::string>("SOFTWARE_VERSION", DeviceDecoders::decode_str);
  this->add_variable(software_version);

  auto *hs_key_state = new Variable<int>("HS_KEY_STATE", DeviceDecoders::decode_int);
  this->add_variable(hs_key_state);

  auto *th_error = new Variable<int>("TH_ERROR", DeviceDecoders::decode_int);
  this->add_variable(th_error);

  auto *trade_show = new Variable<int>("TRADE_SHOW", DeviceDecoders::decode_int);
  this->add_variable(trade_show);

  auto *therme_config = new Variable<int>("THERME_CONFIG", DeviceDecoders::decode_int);
  this->add_variable(therme_config);

  auto *floor_heater_config =
      new Variable<bool>("FLOOR_HEATER_CONFIG", DeviceDecoders::decode_bool, Commands::update_toggle<bool>);
  this->add_variable(floor_heater_config);

  auto *waste_water_heater_config = new Variable<int>("WASTE_WATER_HEATER_CONFIG", DeviceDecoders::decode_int);
  this->add_variable(waste_water_heater_config);

  auto *radio_config = new Variable<bool>("RADIO_CONFIG", DeviceDecoders::decode_bool);
  this->add_variable(radio_config);

  auto *water_level = new Variable<int>("WATER_LEVEL", DeviceDecoders::decode_int);
  this->add_variable(water_level);

  if (this->main_switch_switch_) {
    this->main_switch_switch_->add_on_state_callback([this, hs_key_state, hs_key_long, hs_key](bool state) {
      std::string cmd = "";
      bool current_state = hs_key_state->get_value() > 0;
      ESP_LOGD(TAG, "Main switch state changed. cs: %s, state: %s", ONOFF(current_state), ONOFF(state));
      if (current_state == state)
        return;
      if (state) {
        hs_key_long->set_value(true);
        cmd = hs_key_long->get_command();
      } else {
        hs_key->set_value(true);
        cmd = hs_key->get_command();
      }
      if (!cmd.empty()) {
        ESP_LOGD(TAG, "Main switch command:%s", cmd.c_str());
        this->parent_->send_command(cmd);
      }
    });
  }
  if (this->all_lights_switch_) {
    this->all_lights_switch_->add_on_state_callback([this, hs_key, hs_key_state](bool state) {
      std::string cmd = "";
      bool current_state = hs_key_state->get_value() == 2;
      if (current_state == state)
        return;
      ESP_LOGD(TAG, "Light switch state changed. cs: %s, state: %s", ONOFF(current_state), ONOFF(state));
      if (hs_key && hs_key_state) {
        cmd = hs_key->get_command();
      }
      if (!cmd.empty()) {
        ESP_LOGD(TAG, "All lights switch command:%s", cmd.c_str());
        this->parent_->send_command(cmd);
      }
    });
  }

  if (this->floor_heater_switch_) {
    this->floor_heater_switch_->add_on_state_callback([this, floor_heater](bool state) {
      if (floor_heater->get_value() != state) {
        floor_heater->set_value(state);
        this->parent_->send_command(floor_heater->get_command());
      }
    });
  }
}

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
    auto *temp_in = GET_VARIABLE(float, "TEMP_IN");
    if (temp_in && temp_in->is_active())
      this->temp_in_sensor_->publish_state(temp_in->get_value());
  }
  if (this->temp_out_sensor_) {
    auto *temp_out = GET_VARIABLE(float, "TEMP_OUT");
    if (temp_out && temp_out->is_active())
      this->temp_out_sensor_->publish_state(temp_out->get_value());
  }
}

void MainControlUnitHub::decode(IVariable *variable) {
  if (variable->get_name() == "HS_KEY_STATE") {
    auto *hs_key_state = static_cast<Variable<int> *>(variable);
    if (hs_key_state->is_active()) {
      if (this->main_switch_switch_)
        this->main_switch_switch_->publish_state(hs_key_state->get_value() > 0);
      if (this->all_lights_switch_)
        this->all_lights_switch_->publish_state(hs_key_state->get_value() == 2);
    }
  }
  if (variable->get_name() == "FLOOR_HEATER_ON" && this->floor_heater_switch_) {
    auto *floor_heater = static_cast<Variable<bool> *>(variable);
    this->floor_heater_switch_->publish_state(floor_heater->get_value());
  }

  if (variable->get_name() == "LINE_EN" && this->power_status_binary_sensor_) {
    auto *power_status = static_cast<Variable<bool> *>(variable);
    ;
    this->power_status_binary_sensor_->publish_state(power_status->get_value());
  }

  if (variable->get_name() == "SOFTWARE_VERSION" && this->software_version_text_sensor_) {
    auto *software_version = static_cast<Variable<std::string> *>(variable);
    this->software_version_text_sensor_->publish_state(software_version->get_value());
  }

  if (variable->get_name() == "WATER_LEVEL" && this->water_level_sensor_) {
    auto *water_level = static_cast<Variable<int> *>(variable);
    this->water_level_sensor_->publish_state(float(water_level->get_value() * 100) / 4.0f);
  }
}
}  // namespace esphome::fendt_caravan
#endif
