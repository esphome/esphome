#pragma once

#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/component.h"
#include "../sy6970.h"

namespace esphome::sy6970 {

class SY6970TextSensor : public PollingComponent {
 public:
  void set_parent(SY6970Component *parent) { this->parent_ = parent; }
  void set_bus_status_text_sensor(text_sensor::TextSensor *bus_status_text_sensor) {
    this->bus_status_text_sensor_ = bus_status_text_sensor;
  }
  void set_charge_status_text_sensor(text_sensor::TextSensor *charge_status_text_sensor) {
    this->charge_status_text_sensor_ = charge_status_text_sensor;
  }
  void set_ntc_status_text_sensor(text_sensor::TextSensor *ntc_status_text_sensor) {
    this->ntc_status_text_sensor_ = ntc_status_text_sensor;
  }

  void update() override;

 protected:
  SY6970Component *parent_{nullptr};
  text_sensor::TextSensor *bus_status_text_sensor_{nullptr};
  text_sensor::TextSensor *charge_status_text_sensor_{nullptr};
  text_sensor::TextSensor *ntc_status_text_sensor_{nullptr};
};

}  // namespace esphome::sy6970
