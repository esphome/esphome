#include "fridge_device_hub.h"

#ifdef USE_ESP32

namespace esphome::fendt_caravan {
ESPHOME_LOG_TAG(TAG, "raw.fridge");

void FridgeDeviceHub::setup() {
  auto *fridge_status = new Variable<bool>("FRIDGE_AVAILABLE", DeviceDecoders::decode_bool);
  this->add_variable(fridge_status);

  auto *fridge_onoff = new Variable<bool>("FRIDGE_ON_OFF", DeviceDecoders::decode_bool, Commands::update_toggle<bool>);
  this->add_variable(fridge_onoff);
  if (this->fridge_status_switch_) {
    this->fridge_status_switch_->add_on_state_callback([this, fridge_onoff](bool state) {
      if (state != fridge_onoff->get_value()) {
        fridge_onoff->set_value(state);
        this->parent_->send_command(fridge_onoff->get_command());
      }
    });
  }

  std::vector<std::string> mode_list = {"Performance", "Quite", "Boost"};
  auto *fridge_mode = new Variable<std::string>(
      "FRIDGE_MODE",
      [mode_list](const std::string &value) {
        int decoded = DeviceDecoders::decode_int(value);
        if (decoded > 0)
          decoded -= 1;
        return mode_list.at(decoded);
      },
      [mode_list](const std::string &name, std::string value) {
        auto it = std::find(mode_list.begin(), mode_list.end(), value);
        if (it != mode_list.end()) {
          size_t index = std::distance(mode_list.begin(), it);
          if (index > 0)
            index += 1;
          return Commands::update_int(name, index);
        }
        return std::string("");
      });
  this->add_variable(fridge_mode);
  if (this->fridge_mode_select_) {
    this->fridge_mode_select_->add_on_state_callback([this, fridge_mode](size_t &state) {
      if (!this->fridge_mode_select_->at(state).has_value())
        return;
      std::string value = this->fridge_mode_select_->at(state).value();
      if (value != fridge_mode->get_value()) {
        fridge_mode->set_value(value);
        this->parent_->send_command(fridge_mode->get_command());
      }
    });
  }
  std::vector<std::string> source_list = {"Automatic", "Gas", "DirectCurrent", "AlternatingCurrent"};
  auto *fridge_source = new Variable<std::string>("FRIDGE_SOURCE", [source_list](const std::string &value) {
    return DeviceDecoders::decode_int_str(value, source_list);
  });
  this->add_variable(fridge_source);

  std::vector<std::string> type_list = {"None", "DometicAbsorberFridge", "HobbyCompressorRMVOC90",
                                        "DOMETICRC104Compressor", "DOMETIC_RUC"};
  auto *fridge_type = new Variable<std::string>("FRIDGE_TYPE", [type_list](const std::string &value) {
    return DeviceDecoders::decode_int_str(value, type_list);
  });
  this->add_variable(fridge_type);

  auto *fridge_temp = new Variable<std::string>(
      "FRIDGE_TEMP",
      [](const std::string &data) {
        uint16_t value = DeviceDecoders::decode_int(data);
        char buff[5] = {};
        int8_to_str(buff, value);
        return std::string(buff);
      },
      [](const std::string &name, std::string data) {
        uint16_t value = DeviceDecoders::decode_int(data);
        return Commands::update_int(name, value);
      });
  this->add_variable(fridge_temp);
  if (this->fridge_temperature_select_) {
    this->fridge_temperature_select_->add_on_state_callback([this, fridge_temp](size_t state) {
      if (!this->fridge_temperature_select_->at(state))
        return;
      std::string value = this->fridge_temperature_select_->at(state).value();
      if (fridge_temp->get_value() != value) {
        fridge_temp->set_value(value);
        this->parent_->send_command(fridge_temp->get_command());
      }
    });
  }
}

void FridgeDeviceHub::dump_config() {
  ESP_LOGCONFIG(TAG, " -Fendt Fridge Device-");
  LOG_BINARY_SENSOR(TAG, "  Fridge status", this->fridge_status_binary_sensor_);
  LOG_SWITCH(TAG, "  Fridge on/off", this->fridge_status_switch_);
  LOG_SELECT(TAG, "  Fridge Mode", this->fridge_mode_select_);
  LOG_TEXT_SENSOR(TAG, "  Fridge Source", this->fridge_power_source_text_sensor_);
  LOG_TEXT_SENSOR(TAG, "  Fridge Type", this->fridge_type_text_sensor_);
  LOG_SELECT(TAG, "  Fridge Temperature", this->fridge_temperature_select_);
}

void FridgeDeviceHub::decode(IVariable *variable) {
  if (variable->get_name() == "FRIDGE_AVAILABLE") {
    auto *var = static_cast<Variable<bool> *>(variable);
    if (this->fridge_status_binary_sensor_)
      this->fridge_status_binary_sensor_->publish_state(var->get_value());
  }
  if (variable->get_name() == "FRIDGE_ON_OFF") {
    auto *var = static_cast<Variable<bool> *>(variable);
    if (this->fridge_status_switch_ && this->fridge_status_switch_->state != var->get_value()) {
      this->fridge_status_switch_->publish_state(var->get_value());
    }
  }
  if (variable->get_name() == "FRIDGE_MODE") {
    auto *var = static_cast<Variable<std::string> *>(variable);
    if (this->fridge_mode_select_ && this->fridge_mode_select_->current_option() != var->get_value()) {
      this->fridge_mode_select_->publish_state(var->get_value());
    }
  }
  if (variable->get_name() == "FRIDGE_SOURCE") {
    auto *var = static_cast<Variable<std::string> *>(variable);
    if (this->fridge_power_source_text_sensor_ && this->fridge_power_source_text_sensor_->state != var->get_value()) {
      this->fridge_power_source_text_sensor_->publish_state(var->get_value());
    }
  }
  if (variable->get_name() == "FRIDGE_TYPE") {
    auto *var = static_cast<Variable<std::string> *>(variable);
    if (this->fridge_type_text_sensor_ && this->fridge_type_text_sensor_->state != var->get_value()) {
      this->fridge_type_text_sensor_->publish_state(var->get_value());
    }
  }
  if (variable->get_name() == "FRIDGE_TEMP") {
    auto *var = static_cast<Variable<std::string> *>(variable);
    if (this->fridge_temperature_select_ && this->fridge_temperature_select_->current_option() != var->get_value()) {
      this->fridge_temperature_select_->publish_state(var->get_value());
    }
  }
}

}  // namespace esphome::fendt_caravan
#endif
