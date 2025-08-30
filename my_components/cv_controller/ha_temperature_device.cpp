#include "ha_temperature_device.h"

namespace esphome {
namespace cv_controller {

// public
void HATemperatureDevice::setup(ControlParameters pid) {
  // valve_id_ = id;
  is_controller_ = false;

  // create the entity shortname
  valve_name_.assign(entity_id_.substr(8));  // create short name by skipping the initial 8 characters: climate.

  // subscribe to the state event changes in HA
  subscribe_homeassistant_state(&HATemperatureDevice::onSetpointTemperatureChanged, entity_id_, "temperature");
  subscribe_homeassistant_state(&HATemperatureDevice::onTemperatureChanged, entity_id_, "current_temperature");

  // setup the PID controller
  mPidController_.setup(pid, is_high_accuracy_);
}

void HATemperatureDevice::loop() {
  // make sure we loop the Pid controller
  mPidController_.loop();
}

void HATemperatureDevice::print() { mPidController_.print(); }

void HATemperatureDevice::set_parameters(std::string entityid, bool high_accuracy) {
  entity_id_.assign(entityid);
  is_high_accuracy_ = high_accuracy;
}
std::string HATemperatureDevice::getName() { return valve_name_; }

float HATemperatureDevice::getSetpointTemperature() { return setpoint_temperature_; }

float HATemperatureDevice::getCurrentTemperature() { return current_temperature_; }

float HATemperatureDevice::getControlPercentage() { return mPidController_.getControlOutput(); }

float HATemperatureDevice::getPP() { return mPidController_.getPPvalue(); }

float HATemperatureDevice::getPI() { return mPidController_.getPIvalue(); }

float HATemperatureDevice::getPT() { return mPidController_.getPTvalue(); }

std::string HATemperatureDevice::getEntityId() { return entity_id_; }

void HATemperatureDevice::setController(bool controller_flag) { is_controller_ = controller_flag; }

float HATemperatureDevice::deltaTemperature() { return setpoint_temperature_ - current_temperature_; }

bool HATemperatureDevice::isHighAccuracy() { return is_high_accuracy_; }

bool HATemperatureDevice::isControlling() { return is_controller_; }

// protected
// HA frontend event changes
bool HATemperatureDevice::isStateValid(std::string state) {
  if (state.compare("unavailable") == 0 || state.compare("unknown") == 0) {
    // TODO implement a counter to check availability of HA?
    return false;
  } else {
    return true;
  }
}

void HATemperatureDevice::onSetpointTemperatureChanged(std::string state) {
  if (isStateValid(state)) {
    if (debug_) {
      ESP_LOGI(TAG, "Setpoint changed: %s", state.c_str());
    }
    setpoint_temperature_ = atof(state.c_str());
    mPidController_.setSetpoint(setpoint_temperature_);
  }
}

void HATemperatureDevice::onTemperatureChanged(std::string state) {
  if (isStateValid(state)) {
    if (debug_) {
      ESP_LOGI(TAG, "Temperature changed: %s", state.c_str());
    }
    current_temperature_ = atof(state.c_str());
    mPidController_.setCurrent(current_temperature_);
  }
}

}  // namespace cv_controller
}  // namespace esphome
