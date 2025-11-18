#pragma once

#include "../dfrobot_c4002.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace dfrobot_c4002 {

class C4002BinarySensor : public C4002Listener, public Component, binary_sensor::BinarySensor {
 public:
  void setup() override {
    if (this->presence_bsensor_ != nullptr) {
      this->presence_bsensor_->publish_state(false);
    }
  }
  void set_presence_sensor(binary_sensor::BinarySensor *bsensor) { this->presence_bsensor_ = bsensor; };

  void on_target_state(bool presence) override {
    if (this->presence_bsensor_ != nullptr) {
      if (this->presence_bsensor_->state != presence)
        this->presence_bsensor_->publish_state(presence);
    }
  }

 protected:
  binary_sensor::BinarySensor *presence_bsensor_{nullptr};
};

}  // namespace dfrobot_C4002
}  // namespace esphome
