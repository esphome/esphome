#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/log.h"
#include "esphome/components/time/real_time_clock.h"
#include "esphome/components/api/custom_api_device.h"
#include "ha_temperature_device.h"
#include "stooklijn.h"

#include <vector>
#include <string>
#include <limits>
#include <chrono>

namespace esphome {
namespace cv_controller {

enum CVstatus { OFF, START, ADAPT, NORMAL };
class CVController : public esphome::Component, public api::CustomAPIDevice {
 public:
  void setup() override;
  void loop() override;

  // method to get the time
  void set_time(esphome::time::RealTimeClock *time) { this->time_ = time; }

  // method to get the outside temperature sensor
  void get_ha_outside_temp_sensor(std::string outside_temp) { this->outside_temperature_sensor_name_ = outside_temp; }

  // methods that will be called during the py setup fase
  void create_ha_temperature_device(std::string id, bool highaccuracy);
  void set_kp(float in) { control_parameters_.kp = in; }
  void set_kp_low(float in) { control_parameters_.kp_low = in; }
  void set_ki(float in) { control_parameters_.ki = in; }
  void set_pi_max(float in) { control_parameters_.pi_max = in; }
  void set_pi_target(float in) { control_parameters_.pi_target = in; }
  void set_control_band(float in) { control_parameters_.control_band = in; }
  void set_time_interval(int in) { control_parameters_.time_interval = in; }
  void set_start_output(float in) { control_parameters_.start_output = in; }
  void set_control_temperature_sensor(sensor::Sensor *sensor) { this->sensor_control_temperature_ = sensor; }

  void set_active_controller_name(text_sensor::TextSensor *sensor) { this->sensor_controller_name_ = sensor; }
  void set_cv_status(text_sensor::TextSensor *sensor) { this->sensor_cv_status_ = sensor; }

  void set_start_time_id(std::string id) { this->start_input_id = id; }
  void set_stop_time_id(std::string id) { this->stop_input_id = id; }

 protected:
  const char *TAG = "cv-controller";

  bool debug_ = true;
  esphome::time::RealTimeClock *time_{nullptr};
  std::string outside_temperature_sensor_name_;
  ControlParameters control_parameters_;
  std::vector<HATemperatureDevice> ha_temperature_devices_;
  Stooklijn stooklijn_;  // instantiate this class only once, due to preferences

  // Sensors
  sensor::Sensor *sensor_control_temperature_{nullptr};
  text_sensor::TextSensor *sensor_controller_name_{nullptr};
  text_sensor::TextSensor *sensor_cv_status_{nullptr};

  // start and end time for the daily cycle
  std::string start_input_id;
  std::string stop_input_id;
  int start_time_ = 7 * 60;  // start time in minutes after midnight
  int stop_time_ = 22 * 60;  // stop time in minutes after midnight

  CVstatus cv_status_;             // cv burn cycle status during the day
  int selected_device_index_ = 0;  // selected device, default kamer muur thermostaat
  float current_outside_temperature_;
  float control_temperature_;        // this temperature will be send as sensor data
  float max_temperature_;            // this is the maxium temperature based o stooklijn and outside temperature
  float start_outside_temperature_;  // outside temperature from buienradar at the start of the daily cycle

  long unsigned int start_;  // start time during loop
  int interval_ = 10000;     // loop interval

  void showInLoop();
  void select_active_device();
  void update_status();
  void adapt_stooklijn();
  int get_time();  // will get the current time in minutes from midnight
  void print_pid_parameters();
  void print_status();
  bool isStateValid(std::string state);
  void onCurrentOutsideTemperatureChanged(std::string state);
  void onStartTimeChanged(std::string state);
  void onStopTimeChanged(std::string state);
};

}  // namespace cv_controller
}  // namespace esphome
