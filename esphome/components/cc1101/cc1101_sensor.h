#pragma once

#include "esphome/core/defines.h"

#ifdef USE_SENSOR

#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"
#include "cc1101.h"

namespace esphome::cc1101 {

class CC1101Sensor : public sensor::Sensor, public Component, public Parented<CC1101Component>, public CC1101Listener {
 public:
  enum CC1101SensorType {
    RSSI,
    LQI,
  };

  void set_type(CC1101SensorType type) { type_ = type; }

  void setup() override {
    if (this->parent_ != nullptr) {
      this->parent_->register_listener(this);
    }
  }

  void on_packet(const std::vector<uint8_t> &packet, float freq_offset, float rssi, uint8_t lqi) override {
    if (this->type_ == RSSI) {
      this->publish_state(rssi);
    } else if (this->type_ == LQI) {
      this->publish_state(lqi);
    }
  }

 protected:
  CC1101SensorType type_;
};

}  // namespace esphome::cc1101

#endif
