#include "main_control_unit_hub.h"

#ifdef USE_ESP32
namespace esphome::fendt_caravan {
static const char *const TAG = "FC.CU";

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
    this->main_switch_switch_->add_on_state_callback([this](bool state) {
      std::string cmd = "";
      auto *hs_key_long = GET_VARIABLE(bool, "HS_KEY_LONG");
      auto *hs_key_state = GET_VARIABLE(int, "HS_KEY_STATE");
      bool current_state = hs_key_state->get_value() > 0;

      ESP_LOGV(TAG, "Main switch state changed. cs: %s", ONOFF(current_state));
      if (!(hs_key_long && hs_key_state))
        return;
      if (current_state == state)
        return;
      if (current_state) {
        hs_key_long->set_value(true);
        cmd = hs_key_long->get_command();
      } else {
        auto *hs_key = GET_VARIABLE(bool, "HS_KEY");
        hs_key->set_value(true);
        cmd = hs_key->get_command();
      }
      if (!cmd.empty()) {
        ESP_LOGV(TAG, "Main switch command:%s", cmd.c_str());
        this->parent_->send_command(cmd);
      }
    });
  }
  if (this->all_lights_switch_) {
    this->all_lights_switch_->add_on_state_callback([this](bool state) {
      std::string cmd = "";
      auto *hs_key = GET_VARIABLE(bool, "HS_KEY");
      auto *hs_key_state = GET_VARIABLE(int, "HS_KEY_STATE");
      bool current_state = hs_key_state->get_value() == 2;
      if (current_state == state)
        return;
      ESP_LOGV(TAG, "Light switch state changed. cs: %s", ONOFF(current_state));
      if (hs_key && hs_key_state) {
        cmd = hs_key->get_command();
      }
      if (!cmd.empty()) {
        ESP_LOGV(TAG, "All lights switch command:%s", cmd.c_str());
        this->parent_->send_command(cmd);
      }
    });
  }

  if (this->floor_heater_switch_) {
    this->floor_heater_switch_->add_on_state_callback([this](bool state) {
      std::string cmd = "";
      auto *floor_heater = GET_VARIABLE(bool, "FLOOR_HEATER_ON");
      if (floor_heater->get_value() == state)
        return;
      if (floor_heater) {
        floor_heater->set_value(state);
        cmd = floor_heater->get_command();
      }
      if (!cmd.empty()) {
        ESP_LOGV(TAG, "Floor heater switch command:%s", cmd.c_str());
        this->parent_->send_command(cmd);
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
  LOG_SENSOR(TAG, "  Water Level", this->water_level_sensor_);
  LOG_BINARY_SENSOR(TAG, "  Power Status", this->power_status_binary_sensor_);
  LOG_TEXT_SENSOR(TAG, "  Software Version", this->software_version_text_sensor_);
  LOG_SWITCH(TAG, "  Floor Heater", this->floor_heater_switch_);
}

void MainControlUnitHub::update() {
  ESP_LOGD(TAG, "Update Called");
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
  if (this->water_level_sensor_) {
    auto water_level = GET_VARIABLE(int, "WATER_LEVEL");
    ESP_LOGD(TAG, "Update called: WATER_LEVEL, %d", water_level->get_value());
    if (water_level && water_level->is_active()) {
      ESP_LOGD(TAG, "Update called: WATER_LEVEL, %d", water_level->get_value());
      this->water_level_sensor_->publish_state(water_level->get_value() * 100.0f / 4.0f);
    }
  }
}

bool MainControlUnitHub::decode(const std::string &name, const std::string &value) {
  bool ret = FendtCaravanHubBase::decode(name, value);
  if (name == "HS_KEY_STATE") {
    auto *hs_key_state = GET_VARIABLE(int, name);
    if (hs_key_state->is_active()) {
      if (this->main_switch_switch_)
        this->main_switch_switch_->publish_state(hs_key_state->get_value() > 0);
      if (this->all_lights_switch_)
        this->all_lights_switch_->publish_state(hs_key_state->get_value() == 2);
    }
  }
  if (name == "FLOOR_HEATER_ON" && this->floor_heater_switch_) {
    auto *floor_heater = GET_VARIABLE(bool, "FLOOR_HEATER_ON");
    if (floor_heater && floor_heater->is_active())
      this->floor_heater_switch_->publish_state(floor_heater->get_value());
  }

  if (name == "LINE_EN" && this->power_status_binary_sensor_) {
    auto *power_status = GET_VARIABLE(bool, "LINE_EN");
    if (power_status && power_status->is_active()) {
      this->power_status_binary_sensor_->publish_state(power_status->get_value());
    }
  }

  if (name == "SOFTWARE_VERSION" && this->software_version_text_sensor_) {
    auto *software_version = GET_VARIABLE(std::string, "SOFTWARE_VERSION");
    if (software_version && software_version->is_active())
      this->software_version_text_sensor_->publish_state(software_version->get_value());
  }
  return ret;
}
}  // namespace esphome::fendt_caravan
#endif
