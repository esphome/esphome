#pragma once
#include "esphome/core/component.h"
#include "esphome/components/api/custom_api_device.h"
#include "pid-controller.h"

namespace esphome {
namespace cv_controller {

using namespace std;
class HATemperatureDevice : public api::CustomAPIDevice {
 public:
  void setup(ControlParameters
                 pid);  // setup a thermostate radiator valve (low accuracy) or the muur thermostate (high accuracy)
  void loop();
  void print();
  void set_parameters(std::string entityid, bool high_accuracy);
  std::string getName();           // return the valve name
  float getSetpointTemperature();  // return the current setpoint (obsolete)
  float getCurrentTemperature();   // return the current temperature (obsolete)
  float getControlPercentage();    // return the PI output percentage for CH control
  float getPP();                   // return the pp value of the PI controller
  float getPI();                   // return the pi value of the PI controller
  float getPT();                   // return the pt value of the PI controller

  void setController(bool control_setting);

  std::string getEntityId();
  float deltaTemperature();
  void closeValve();
  void openValve();
  void normalValve();
  bool isHighAccuracy();
  bool isControlling();

 protected:
  const char *TAG = "temperatureDevices";

  bool debug_ = false;
  // int valve_id_;
  std::string entity_id_;   // entity id of the climate device of the radiator in HA
  std::string valve_name_;  // short name of the climate device
  float setpoint_temperature_;
  float current_temperature_;
  float delta_temperature_;

  bool is_high_accuracy_;
  bool is_controller_;
  PidController mPidController_;  // every valve will have it's own PI Controller

  bool isStateValid(std::string state);
  void onSetpointTemperatureChanged(std::string state);
  void onTemperatureChanged(std::string state);
};

}  // namespace cv_controller
}  // namespace esphome
