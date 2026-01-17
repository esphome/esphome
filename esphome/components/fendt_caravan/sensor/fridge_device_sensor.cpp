#ifdef USE_ESP32
#include "fridge_device_sensor.h"
#include <array>
#include <vector>

namespace esphome {
namespace fendt_caravan {

void FridgeDeviceSensor::setup() {
  ESP_LOGV(TAG, "Fridge Setup Called");

  if (this->fridge_available_text_sensor_) {
    auto available =
        this->fridge_available_text_sensor_->create_variable("FRIDGE_AVAILABLE", [](const std::string &value) {
          const char *tmp[] = {"Available", "Not available"};
          return DeviceDecoders::decode_bool_str(value, tmp);
        });
    this->add_variable(available);
  }
  if (this->fridge_status_switch_) {
    auto status = this->fridge_status_switch_->create_variable("FRIDGE_ON_OFF", DeviceDecoders::decode_bool,
                                                               Commands::update_toggle<bool>);
    this->add_variable(status);
    this->fridge_status_switch_->set_state_change_callback(
        std::bind(&FridgeDeviceSensor::on_switch_state_change, this, std::placeholders::_1, std::placeholders::_2));
  }
  if (this->fridge_mode_select_) {
    std::vector<std::string> list = {"Performance", "", "Quite", "Boost"};
    auto mode = this->fridge_mode_select_->create_variable(
        "FRIDGE_MODE", [list](const std::string &value) { return DeviceDecoders::decode_int_str(value, list); },
        [list](const std::string &name, std::string value) {
          auto it = std::find(list.begin(), list.end(), value);
          if (it != list.end()) {
            size_t index = std::distance(list.begin(), it);
            return Commands::update_int(name, index);
          }
          return std::string("");
        });
    this->add_variable(mode);
    this->fridge_mode_select_->set_state_change_callback(
        std::bind(&FridgeDeviceSensor::on_select_state_change, this, std::placeholders::_1, std::placeholders::_2));
  }
  if (this->fridge_source_text_sensor_) {
    std::vector<std::string> list = {"Automatic", "Gas", "DirectCurrent", "AlternatingCurrent"};
    auto source = this->fridge_source_text_sensor_->create_variable(
        "FRIDGE_SOURCE", [list](const std::string &value) { return DeviceDecoders::decode_int_str(value, list); });
    this->add_variable(source);
  }
  if (this->fridge_type_text_sensor_) {
    std::vector<std::string> list = {"None", "DometicAbsorberFridge", "HobbyCompressorRMVOC90",
                                     "DOMETICRC104Compressor", "DOMETIC_RUC"};
    auto ftype = this->fridge_type_text_sensor_->create_variable(
        "FRIDGE_TYPE", [list](const std::string &value) { return DeviceDecoders::decode_int_str(value, list); });
    this->add_variable(ftype);
  }
  if (this->fridge_temperature_number_) {
    auto temp = this->fridge_temperature_number_->create_variable("FRIDGE_TEMP", DeviceDecoders::decode_int,
                                                                  Commands::update_int);
    this->add_variable(temp);
    this->fridge_temperature_number_->set_state_change_callback(
        std::bind(&FridgeDeviceSensor::on_number_state_change, this, std::placeholders::_1, std::placeholders::_2));
  }
}

void FridgeDeviceSensor::dump_config() {
  ESP_LOGCONFIG(TAG, " -Fendt Fridge Device-");
  LOG_TEXT_SENSOR(TAG, "  Fridge Available", this->fridge_available_text_sensor_);
  LOG_SWITCH(TAG, "  Fridge Status", this->fridge_status_switch_);
  LOG_SELECT(TAG, "  Fridge Mode", this->fridge_mode_select_);
  LOG_TEXT_SENSOR(TAG, "  Fridge Source", this->fridge_source_text_sensor_);
  LOG_TEXT_SENSOR(TAG, "  Fridge Type", this->fridge_type_text_sensor_);
  LOG_NUMBER(TAG, "  Fridge Temperature", this->fridge_temperature_number_);
}

void FridgeDeviceSensor::on_switch_state_change(FendtSwitch *sw, bool state) {
  std::string command = sw->get_variable()->get_command();
  if (!command.empty()) {
    ESP_LOGV(TAG, "Switch state changed command:%s", command.c_str());
    this->command_callback_.call(command);
  }
}

void FridgeDeviceSensor::on_number_state_change(FendtNumber *num, float state) {
  std::string command = num->get_variable()->get_command();
  if (!command.empty()) {
    ESP_LOGV(TAG, "Number state changed command:%s", command.c_str());
    this->command_callback_.call(command);
  }
}

void FridgeDeviceSensor::on_select_state_change(FendtSelect *sel, std::string state) {
  std::string command = sel->get_variable()->get_command();
  if (!command.empty()) {
    ESP_LOGV(TAG, "Select state changed command:%s", command.c_str());
    this->command_callback_.call(command);
  }
}

}  // namespace fendt_caravan
}  // namespace esphome
#endif
