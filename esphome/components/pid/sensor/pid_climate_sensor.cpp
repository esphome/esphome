#include "pid_climate_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace pid {

static const char *TAG = "pid.sensor";

void PIDClimateSensor::setup() {
  this->parent_->add_on_state_callback([this]() {
    this->update_from_parent_();
  });
  this->update_from_parent_();
}

}  // namespace pid
}  // namespace esphome
