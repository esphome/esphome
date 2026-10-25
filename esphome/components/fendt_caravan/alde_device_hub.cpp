#include "alde_device_hub.h"

#ifdef USE_ESP32
namespace esphome::fendt_caravan {
ESPHOME_LOG_TAG(TAG, "FC.alde");

void AldeDeviceHub::setup() {
  auto *alde_available = new Variable<bool>("HEATER_AVAILABLE", DeviceDecoders::decode_bool);
  this->add_variable(alde_available);

  auto *heater_status = new Variable<bool>("HEATER_ONOFF", DeviceDecoders::decode_bool, Commands::update_toggle<bool>);
  this->add_variable(heater_status);
  if (this->alde_heater_switch_) {
    this->alde_heater_switch_->add_on_state_callback([this, heater_status](bool state) {
      if (heater_status->get_value() != state) {
        heater_status->set_value(state);
        this->parent_->send_command(heater_status->get_command());
      }
    });
  }
  auto *heater_temp = new Variable<float>("HEATER_TEMP", DeviceDecoders::decode_temperature, Commands::update_temp_10);
  this->add_variable(heater_temp);
  if (this->alde_climate_) {
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

void AldeDeviceHub::decode(IVariable *variable) {
  if (variable->get_name() == "HEATER_AVAILABLE") {
    auto *var = static_cast<Variable<bool> *>(variable);
    if (this->alde_status_binary_sensor_)
      this->alde_status_binary_sensor_->publish_state(var->get_value());
  } else if (variable->get_name() == "HEATER_ONOFF") {
    auto *var = static_cast<Variable<bool> *>(variable);
    if (this->alde_heater_switch_)
      this->alde_heater_switch_->publish_state(var->get_value());
  } else if (variable->get_name() == "HEATER_TEMP") {
    auto *var = static_cast<Variable<float> *>(variable);
    if (this->alde_climate_) {
      this->alde_climate_->target_temperature = var->get_value();
      this->alde_climate_->publish_state();
    }
  } else if (variable->get_name() == "HEATER_WATER") {
    auto *var = static_cast<Variable<bool> *>(variable);
    if (this->alde_water_heater_switch_)
      this->alde_water_heater_switch_->publish_state(var->get_value());
  } else if (variable->get_name() == "HEATER_WATER_TEMP") {
    auto *var = static_cast<Variable<bool> *>(variable);
    if (this->alde_water_heater_boost_switch_)
      this->alde_water_heater_boost_switch_->publish_state(var->get_value());
  } else if (variable->get_name() == "HEATER_EL") {
    auto *var = static_cast<Variable<std::string> *>(variable);
    if (this->alde_heater_electricity_select_)
      this->alde_heater_electricity_select_->publish_state(var->get_value());
  } else if (variable->get_name() == "HEATER_GAS") {
    auto *var = static_cast<Variable<bool> *>(variable);
    if (this->alde_heater_gas_switch_)
      this->alde_heater_gas_switch_->publish_state(var->get_value());
  }
}

}  // namespace esphome::fendt_caravan
#endif
