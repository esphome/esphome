#include "alde_device_sensor.h"

#ifdef USE_ESP32
namespace esphome {
namespace fendt_caravan {

void AldeDeviceSensor::setup() {
  ESP_LOGI(TAG, "Setup called");

  if (this->alde_available_text_sensor_) {
    auto alde_available =
        this->alde_available_text_sensor_->create_variable("HEATER_AVAILABLE", [](const std::string &value) {
          const char *tmp[] = {"Available", "Not available"};
          return DeviceDecoders::decode_bool_str(value, tmp);
        });
    this->add_variable(alde_available);
  }

  if (this->alde_heater_status_switch_) {
    auto heater_status = this->alde_heater_status_switch_->create_variable("HEATER_ONOFF", DeviceDecoders::decode_bool,
                                                                           Commands::update_toggle<bool>);
    this->add_variable(heater_status);
  }

  if (this->alde_heater_temperature_number_) {
    auto heater_temp = this->alde_heater_temperature_number_->create_variable(
        "HEATER_TEMP", DeviceDecoders::decode_temperature, Commands::update_temp_10);
    this->add_variable(heater_temp);
  }

  if (this->alde_heater_water_switch_) {
    auto heater_water = this->alde_heater_water_switch_->create_variable("HEATER_WATER", DeviceDecoders::decode_bool,
                                                                         Commands::update_toggle<bool>);
    this->add_variable(heater_water);
  }

  if (this->alde_heater_water_temperature_switch_) {
    auto heater_water_temp = this->alde_heater_water_temperature_switch_->create_variable(
        "HEATER_WATER_TEMP", [](const std::string &data) { return DeviceDecoders::decode_temperature(data) == 65.0f; },
        Commands::update_toggle<bool>);
    this->add_variable(heater_water_temp);
  }

  if (this->alde_heater_electric_select_) {
    auto heater_el = this->alde_heater_electric_select_->create_variable("HEATER_EL", DeviceDecoders::decode_heater_el,
                                                                         Commands::update_heater_el);
    this->add_variable(heater_el);
  }

  if (this->alde_heater_gas_switch_) {
    auto heater_gas = this->alde_heater_gas_switch_->create_variable("HEATER_GAS", DeviceDecoders::decode_bool,
                                                                     Commands::update_toggle<bool>);
    this->add_variable(heater_gas);
  }

  if (this->alde_heater_status_switch_)
    this->alde_heater_status_switch_->set_state_change_callback(
        std::bind(&AldeDeviceSensor::on_switch_state_change_, this, std::placeholders::_1, std::placeholders::_2));
  if (this->alde_heater_water_switch_)
    this->alde_heater_water_switch_->set_state_change_callback(
        std::bind(&AldeDeviceSensor::on_switch_state_change_, this, std::placeholders::_1, std::placeholders::_2));
  if (this->alde_heater_water_temperature_switch_)
    this->alde_heater_water_temperature_switch_->set_state_change_callback(
        std::bind(&AldeDeviceSensor::on_switch_state_change_, this, std::placeholders::_1, std::placeholders::_2));
  if (this->alde_heater_temperature_number_)
    this->alde_heater_temperature_number_->set_state_change_callback(
        std::bind(&AldeDeviceSensor::on_number_state_change_, this, std::placeholders::_1, std::placeholders::_2));
  if (this->alde_heater_electric_select_)
    this->alde_heater_electric_select_->set_state_change_callback(
        std::bind(&AldeDeviceSensor::on_select_state_change_, this, std::placeholders::_1, std::placeholders::_2));
  if (this->alde_heater_gas_switch_)
    this->alde_heater_gas_switch_->set_state_change_callback(
        std::bind(&AldeDeviceSensor::on_switch_state_change_, this, std::placeholders::_1, std::placeholders::_2));
}

void AldeDeviceSensor::dump_config() {
  ESP_LOGCONFIG(TAG, " -Fendt Alde Device-");
  LOG_TEXT_SENSOR(TAG, "  Alde Sensor", this->alde_available_text_sensor_);
  LOG_SWITCH(TAG, "  Alde Status Switch", this->alde_heater_status_switch_);
  LOG_NUMBER(TAG, "  Heater Temperature", this->alde_heater_temperature_number_);
  LOG_SWITCH(TAG, "  Heater Water", this->alde_heater_water_switch_);
  LOG_SWITCH(TAG, "  Water Temperature", this->alde_heater_water_temperature_switch_);
  LOG_SELECT(TAG, "  Heater Electric", this->alde_heater_electric_select_);
  LOG_SWITCH(TAG, "  Heater Gas", this->alde_heater_gas_switch_);
}

void AldeDeviceSensor::on_switch_state_change_(FendtSwitch *sw, bool state) {
  if (!sw->get_variable())
    return;
  std::string command = sw->get_variable()->get_command();
  if (!command.empty()) {
    ESP_LOGD(TAG, "Switch state changed command:%s", command.c_str());
    this->command_callback_.call(command);
  }
}

void AldeDeviceSensor::on_number_state_change_(FendtNumber *num, float state) {
  if (!num->get_variable())
    return;
  std::string command = num->get_variable()->get_command();
  if (!command.empty()) {
    ESP_LOGD(TAG, "Number state changed command:%s", command.c_str());
    this->command_callback_.call(command);
  }
}
void AldeDeviceSensor::on_select_state_change_(FendtSelect *sel, std::string state) {
  if (!sel->get_variable())
    return;
  std::string command = sel->get_variable()->get_command();
  if (!command.empty()) {
    ESP_LOGD(TAG, "Select state changed command:%s", command.c_str());
    this->command_callback_.call(command);
  }
}
}  // namespace fendt_caravan
}  // namespace esphome
#endif
