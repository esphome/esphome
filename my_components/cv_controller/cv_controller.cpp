#include "cv_controller.h"

namespace esphome {
namespace cv_controller {

void CVController::setup() {
  for (int i = 0; i < ha_temperature_devices_.size(); i++) {
    // setup and subscribe to the HA devices
    ha_temperature_devices_[i].setup(control_parameters_);
  }
  // Outside temperature from buienradar
  // TODO add selection of the outside temp sensor into YAML
  subscribe_homeassistant_state(&CVController::onCurrentOutsideTemperatureChanged, outside_temperature_sensor_name_);

  // subscribe to start and end time input boxes in HA
  subscribe_homeassistant_state(&CVController::onStartTimeChanged, start_input_id);
  subscribe_homeassistant_state(&CVController::onStopTimeChanged, stop_input_id);

  this->sensor_control_temperature_->set_accuracy_decimals(2);
  this->sensor_control_temperature_->set_unit_of_measurement("°C");
  this->sensor_control_temperature_->set_icon("mdi:thermometer");
  this->sensor_control_temperature_->set_device_class("temperature");

  stooklijn_.setup();

  start_ = clock();
  cv_status_ = OFF;  // start in the NORMAL position TODO set start position in the YAML
}

void CVController::loop() {
  for (int i = 0; i < ha_temperature_devices_.size(); i++) {
    ha_temperature_devices_[i].loop();  // this will calculate the new pid values for all devices
  }

  showInLoop();
}

void CVController::showInLoop() {
  if ((clock() - start_) > interval_) {
    start_ = clock();
    ESP_LOGI(TAG, "Running");
    if (debug_) {
      ESP_LOGI(TAG, "MAx temperature: %0.1f", max_temperature_);
    }
    update_status();
    print_status();
  }
}

void CVController::create_ha_temperature_device(std::string id, bool highaccuracy) {
  HATemperatureDevice hat;
  hat.set_parameters(id, highaccuracy);
  ha_temperature_devices_.push_back(hat);
}

void CVController::select_active_device() {
  // The first HA device is the default high accuracy controller (kamer muur thermostaat)
  // find the Ha Device with the largest temperature difference
  float temp_diff = 0.0;  // temperature difference per device
  int control_index = -1;
  for (int i = 0; i < ha_temperature_devices_.size(); i++) {
    float td = ha_temperature_devices_[i].deltaTemperature();
    if (td > temp_diff) {
      // this device has a larger positive temperature difference, so we have a heating request
      temp_diff = td;
      control_index = i;
    }
  }
  if (control_index < 0) {
    // We don't need heating so default use first temperature device (kamer muur thermostaat)
    control_index = 0;
  }

  if (control_index != 0) {
    // we selected a different than default controller
    if (!ha_temperature_devices_[control_index].isHighAccuracy()) {
      // the selected valve is low accuracy, it will change controller output in discrete steps
      // if  we still have a high control value for the kamer muur thermostaat we don't switch
      if (ha_temperature_devices_[0].getPT() > 50.0) {
        // we are still controlling the temperature with the kamer muur thermostate, no need to switch controllers
        control_index = 0;
      }
    }
  }

  if (control_index != selected_device_index_) {
    // need to change controller
    ha_temperature_devices_[selected_device_index_].setController(false);
    ha_temperature_devices_[control_index].setController(true);
    selected_device_index_ = control_index;
  }
}

void CVController::update_status() {
  float control_factor;
  select_active_device();  // first step is slection of the active device
  switch (cv_status_) {
    case OFF:
      if (get_time() > start_time_ && get_time() < stop_time_) {
        if (debug_) {
          ESP_LOGI(TAG, "Switching to the START cycle");
        }
        cv_status_ = START;
        sensor_cv_status_->publish_state("START");
      }
      control_temperature_ = 0.0;  // TODO need to check if this is the default control temperature
      break;
    case START:
      // get outside temperature, use stook lijn to get the maxium aanvoer temperature
      start_outside_temperature_ = current_outside_temperature_;
      max_temperature_ = stooklijn_.getAanvoerTemperature(start_outside_temperature_);
      if (debug_) {
        ESP_LOGI(TAG, "Setting Max temperature: %0.1f", max_temperature_);
      }
      // select the next status
      if (selected_device_index_ == 0 && ha_temperature_devices_[selected_device_index_].deltaTemperature() > 3.0) {
        // selected device = kamer muur thermostate and the temperature difference is large enough to start a adaption
        // cycle
        cv_status_ = ADAPT;
        sensor_cv_status_->publish_state("ADAPT");
        if (debug_) {
          ESP_LOGI(TAG, "Switching to the ADAPT cycle");
        }
      } else {
        cv_status_ = NORMAL;
        sensor_cv_status_->publish_state("NORMAL");
        if (debug_) {
          ESP_LOGI(TAG, "Switching to the NORMAL cycle");
        }
      }
      control_factor = ha_temperature_devices_[selected_device_index_].getControlPercentage();
      control_temperature_ = max_temperature_ * control_factor;
      break;
    case ADAPT:
      // after two hours we will check if we have to adapt the stooklijn
      if (get_time() > start_time_ + 2 * 60) {  // burn cycle of two hours
        // end of the adaptation phase
        if (selected_device_index_ == 0) {  // the controller is still the kamer muur thermostate
          adapt_stooklijn();
        }
        cv_status_ = NORMAL;
        sensor_cv_status_->publish_state("NORMAL");
        if (debug_) {
          ESP_LOGI(TAG, "Switching to the NORMAL cycle");
        }
      }
      control_factor = ha_temperature_devices_[selected_device_index_].getControlPercentage();
      control_temperature_ = max_temperature_ * control_factor;
      break;
    case NORMAL:
      if (get_time() > stop_time_) {
        // stop of the dail cycle
        cv_status_ = OFF;
        sensor_cv_status_->publish_state("OFF");
        if (debug_) {
          ESP_LOGI(TAG, "Switching to the OFF cycle");
        }
      }
      control_factor = ha_temperature_devices_[selected_device_index_].getControlPercentage();
      control_temperature_ = max_temperature_ * control_factor;
      break;
    default:
      break;
  }

  // publish control_temperature
  if (this->sensor_control_temperature_ != nullptr) {
    this->sensor_control_temperature_->publish_state(control_temperature_);
  }
  // publish controller name
  if (this->sensor_controller_name_ != nullptr) {
    this->sensor_controller_name_->publish_state(ha_temperature_devices_[selected_device_index_].getName());
  }
}

void CVController::adapt_stooklijn() {
  // end of ADAPT fase, check if we have to adapt the stooklijn curve
  float actual_pi = ha_temperature_devices_[selected_device_index_].getPI();
  float delta_temp =
      (actual_pi - control_parameters_.pi_target) / 2.5;  // transform the difference in pi to a temperature delta
  if (abs(delta_temp) > 2.0) {
    // we need to adapt the stooklijn
    stooklijn_.adaptStookLine(delta_temp, start_outside_temperature_);
    stooklijn_.save();  // save the new stooklijn in preference memory
  }
}

int CVController::get_time() {
  int minutus = 0;
  if (time_->now().is_valid()) {
    auto now = time_->now();  // esphome::ESPTime
    int hour = now.hour;
    int minute = now.minute;
    int second = now.second;
    if (debug_) {
      ESP_LOGI("my_component", "Current time: %02d:%02d:%02d", hour, minute, second);
    }
    return hour * 60 + minute;
  }
  return 0;  // when time is not available start with mid night time
}

void CVController::print_pid_parameters() {
  if (debug_) {
    ESP_LOGI(TAG, "kp %0.1f", control_parameters_.kp);
    ESP_LOGI(TAG, "kp_low %0.1f", control_parameters_.kp_low);
    ESP_LOGI(TAG, "ki %0.1f", control_parameters_.ki);
    ESP_LOGI(TAG, "pi_max %0.1f", control_parameters_.pi_max);
    ESP_LOGI(TAG, "control band %0.1f", control_parameters_.control_band);
    ESP_LOGI(TAG, "time interval %0.1f", control_parameters_.time_interval);
    ESP_LOGI(TAG, "start output %0.1f", control_parameters_.start_output);
  }
}
void CVController::print_status() {
  if (debug_) {
    switch (cv_status_) {
      case OFF:
        ESP_LOGI(TAG, "Status: OFF");
        break;
      case START:
        ESP_LOGI(TAG, "Status: START");
        break;
      case ADAPT:
        ESP_LOGI(TAG, "Status: ADAPT");
        break;
      case NORMAL:
        ESP_LOGI(TAG, "Status: NORMAL");
        break;

      default:
        break;
    }
  }
}
bool CVController::isStateValid(std::string state) {
  if (state.compare("unavailable") == 0 || state.compare("unknown") == 0) {
    // TODO implement a counter to check availability of HA?
    return false;
  } else {
    return true;
  }
}
void CVController::onCurrentOutsideTemperatureChanged(std::string state) {
  if (isStateValid(state)) {
    current_outside_temperature_ = atof(state.c_str());
  }
}
void CVController::onStartTimeChanged(std::string state) {
  if (isStateValid(state)) {
    int hour, minute;
    if (sscanf(state.c_str(), "%d:%d", &hour, &minute) == 2) {
      this->start_time_ = hour * 60 + minute;
      if (debug_) {
        ESP_LOGI(TAG, "Start time updated: %02d:%02d", hour, minute);
      }
    } else {
      if (debug_) {
        ESP_LOGI(TAG, "Invalid time format: %s", state.c_str());
      }
    }
  }
}
void CVController::onStopTimeChanged(std::string state) {
  if (isStateValid(state)) {
    int hour, minute;
    if (sscanf(state.c_str(), "%d:%d", &hour, &minute) == 2) {
      this->stop_time_ = hour * 60 + minute;
      if (debug_) {
        ESP_LOGI(TAG, "Stop time updated: %02d:%02d", hour, minute);
      }
    } else {
      if (debug_) {
        ESP_LOGI(TAG, "Invalid time format: %s", state.c_str());
      }
    }
  }
}

}  // namespace cv_controller
}  // namespace esphome
