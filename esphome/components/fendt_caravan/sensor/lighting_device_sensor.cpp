#include "lighting_device_sensor.h"

namespace esphome {
namespace fendt_caravan {

void LightingDeviceSensor::setup() {
  if (this->light_sw0_switch_) {
    auto *sw = this->light_sw0_switch_->create_variable("LIGHT_SW0", DeviceDecoders::decode_int,
                                                        Commands::update_toggle<bool>);
    this->add_variable(sw);
    this->light_sw0_switch_->set_state_change_callback(
        std::bind(&LightingDeviceSensor::on_switch_state_changed_, this, std::placeholders::_1, std::placeholders::_2));
  }
  if (this->light_sw1_switch_) {
    auto *sw = this->light_sw1_switch_->create_variable("LIGHT_SW1", DeviceDecoders::decode_int,
                                                        Commands::update_toggle<bool>);
    this->add_variable(sw);
    this->light_sw1_switch_->set_state_change_callback(
        std::bind(&LightingDeviceSensor::on_switch_state_changed_, this, std::placeholders::_1, std::placeholders::_2));
  }
  if (this->light_sw2_switch_) {
    auto *sw = this->light_sw2_switch_->create_variable("LIGHT_SW2", DeviceDecoders::decode_int,
                                                        Commands::update_toggle<bool>);
    this->add_variable(sw);
    this->light_sw2_switch_->set_state_change_callback(
        std::bind(&LightingDeviceSensor::on_switch_state_changed_, this, std::placeholders::_1, std::placeholders::_2));
  }
  if (this->light_sw3_switch_) {
    auto *sw = this->light_sw3_switch_->create_variable("LIGHT_SW3", DeviceDecoders::decode_int,
                                                        Commands::update_toggle<bool>);
    this->add_variable(sw);
    this->light_sw3_switch_->set_state_change_callback(
        std::bind(&LightingDeviceSensor::on_switch_state_changed_, this, std::placeholders::_1, std::placeholders::_2));
  }

  if (this->light_dimsw0_light_output_) {
    auto *dimsw = this->create_variable_("LIGHT_DIM0", this->light_dimsw0_light_output_);
    this->add_variable(dimsw);
    this->light_dimsw0_light_output_->set_state_change_callback(std::bind(
        &LightingDeviceSensor::on_light_output_state_changed_, this, std::placeholders::_1, std::placeholders::_2));
  }

  if (this->light_dimsw1_light_output_) {
    auto *dimsw = this->create_variable_("LIGHT_DIM1", this->light_dimsw1_light_output_);
    this->add_variable(dimsw);
    this->light_dimsw1_light_output_->set_state_change_callback(std::bind(
        &LightingDeviceSensor::on_light_output_state_changed_, this, std::placeholders::_1, std::placeholders::_2));
  }
  if (this->light_dimsw2_light_output_) {
    auto *dimsw = this->create_variable_("LIGHT_DIM2", this->light_dimsw2_light_output_);
    this->add_variable(dimsw);
    this->light_dimsw2_light_output_->set_state_change_callback(std::bind(
        &LightingDeviceSensor::on_light_output_state_changed_, this, std::placeholders::_1, std::placeholders::_2));
  }
  if (this->light_dimsw3_light_output_) {
    auto *dimsw = this->create_variable_("LIGHT_DIM3", this->light_dimsw3_light_output_);
    this->add_variable(dimsw);
    this->light_dimsw3_light_output_->set_state_change_callback(std::bind(
        &LightingDeviceSensor::on_light_output_state_changed_, this, std::placeholders::_1, std::placeholders::_2));
  }
  if (this->light_dimsw4_light_output_) {
    auto *dimsw = this->create_variable_("LIGHT_DIM4", this->light_dimsw4_light_output_);
    this->add_variable(dimsw);
    this->light_dimsw4_light_output_->set_state_change_callback(std::bind(
        &LightingDeviceSensor::on_light_output_state_changed_, this, std::placeholders::_1, std::placeholders::_2));
  }
}

void LightingDeviceSensor::dump_config() {}

void LightingDeviceSensor::on_switch_state_changed_(FendtSwitch *sw, bool state) {
  if (!sw->get_variable())
    return;

  std::string command = "";
  command = sw->get_variable()->get_command();
  if (!command.empty()) {
    ESP_LOGD(this->tag_, "Select state changed command:%s", command.c_str());
    this->command_callback_.call(command);
  }
}
void LightingDeviceSensor::on_light_output_state_changed_(FendtLightOutput *lo, LampStateT state) {
  if (!lo->get_variable())
    return;
  std::string command = "";
  auto *variable = lo->get_variable();
  if (variable->get_value().status != state.status) {
    variable->set_value(state);
    command = variable->get_command();
  } else if (variable->get_value().state != state.state) {
    variable->set_value(state);
    command = variable->get_alt_command();
  }
  if (!command.empty()) {
    ESP_LOGV(this->tag_, "Lamp state changed. Command: %s", command.c_str());
    this->command_callback_.call(command);
  }
}
}  // namespace fendt_caravan
}  // namespace esphome
