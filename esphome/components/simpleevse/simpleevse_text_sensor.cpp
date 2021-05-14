#include "simpleevse_text_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace simpleevse {

void SimpleEvseTextSensors::update(bool running, const std::array<uint16_t, COUNT_STATUS_REGISTER> &status_register)
{
  if (running) {
    if (this->vehicle_state_sensor_){
      switch (status_register[REGISTER_VEHICLE_STATE]) {
        case VehicleState::VEHICLE_UNKNOWN:
          this->vehicle_state_sensor_->publish_state("unknown");
          break;
        case VehicleState::VEHICLE_READY:
          this->vehicle_state_sensor_->publish_state("ready");
          break;
        case VehicleState::VEHICLE_EV_PRESENT:
          this->vehicle_state_sensor_->publish_state("EV is present");
          break;
        case VehicleState::VEHICLE_CHARGING:
          this->vehicle_state_sensor_->publish_state("charging");
          break;
        case VEHICLE_CHARGING_WITH_VENT:
          this->vehicle_state_sensor_->publish_state("charging with ventilation");
          break;
        default:
          this->vehicle_state_sensor_->publish_state("?");
          ESP_LOGW(TAG, "Invalid state received: %d", status_register[REGISTER_VEHICLE_STATE]);
          break;
      }
    }

    if (this->evse_state_sensor_) {
      switch (status_register[REGISTER_EVSE_STATE]) {
        case 0:
          this->evse_state_sensor_->publish_state("unknown");
          break;
        case 1:
          this->evse_state_sensor_->publish_state("steady 12V");
          break;
        case 2:
          this->evse_state_sensor_->publish_state("PWM");
          break;
        case 3:
          this->evse_state_sensor_->publish_state("OFF");
          break;
        default:
          this->evse_state_sensor_->publish_state("?");
          ESP_LOGW(TAG, "Invalid state received: %d", status_register[REGISTER_EVSE_STATE]);
          break;        
      }
    }
  }
}

}  // namespace simpleevse
}  // namespace esphome

