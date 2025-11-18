#include "c4002_switch.h"
#include "esphome/core/log.h"
namespace esphome {
namespace dfrobot_c4002 {

void C4002Switch1::write_state(bool state) {
  bool send_flag = false;
  if (this->parent_) {
    if( state  == true ){
      send_flag = this->parent_->setOutLed(eLedOn);
    }else{
      send_flag = this->parent_->setOutLed(eLedOff);
    }
    if(send_flag){
      this->publish_state(state);
    }else{
      ESP_LOGW("C4002Switch1", "led cmd send data failed");
    }
  }
}

void C4002Switch2::write_state(bool state) {
  bool send_flag = false;
  if (this->parent_) {
    if( state  == true ){
      send_flag = this->parent_->setRunLed(eLedOn);
    }else{
      send_flag = this->parent_->setRunLed(eLedOff);
    }
    if(send_flag){
      this->publish_state(state);
    }else{
      ESP_LOGW("C4002Switch2", "led cmd send data failed");
    }
  }
}

void C4002SwitchFactoryReset::write_state(bool state) {
  if (this->parent_) {
    if (state == true) {
      bool send_flag = this->parent_->factoryReset();

      ESP_LOGW("C4002SwitchFactoryReset", "bool: %d", send_flag);

      if (send_flag) {
        this->publish_state(true);
        this->set_timeout(2000, [this]() {  // 2000ms = 2秒
          this->publish_state(false);
          ESP_LOGD("C4002SwitchFactoryReset", "Factory reset completed, switch auto-reset to OFF");
        });
      } else {
        ESP_LOGW("C4002SwitchFactoryReset", "Factory reset command failed");
        this->publish_state(false);  // Reset switch to off even on failure
      }
    }else{
      this->publish_state(false);  // Ensure switch is off
    }
  }
}

void C4002SwitchEnvironmentalCalibration::write_state(bool state) {
  if (this->parent_) {
    if (state == true) {
      //ESP_LOGW("C4002SwitchEnvironmentalCalibration ", "Start environmental calibration");
      this->parent_->startEnvCalibration(3,15);// (delayTime,contTime)
      this->publish_state(true);

      this->set_timeout(18000, [this]() {  // 18000ms = 18秒
        this->publish_state(false);
        ESP_LOGD("C4002SwitchEnvironmentalCalibration", "Environmental calibration completed, switch auto-reset to OFF");
      });


    } else {
      this->publish_state(false);  // Ensure switch is off
    }
  }
}

}  // namespace dfrobot_c4002
}  // namespace esphome
