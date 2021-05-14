#pragma once

#include "esphome/components/sensor/sensor.h"
#include "simplesevse.h"

namespace esphome {
namespace simpleevse {

class SimpleEvseSensors : public UpdateListener {
  public:
    explicit SimpleEvseSensors(SimpleEvseComponent *parent) : parent_(parent) { parent->add_observer(this); }

    /* setter methods for sensor configuration */
    void set_set_charge_current_sensor(sensor::Sensor *set_charge_current) { set_charge_current_ = set_charge_current; }
    void set_actual_charge_current_sensor(sensor::Sensor *actual_charge_current) { actual_charge_current_ = actual_charge_current; }
    void set_max_current_limit_sensor(sensor::Sensor *max_current_limit) { max_current_limit_ = max_current_limit; }
    void set_firmware_revision_sensor(sensor::Sensor *firmware_revision) { firmware_revision_ = firmware_revision; }

    void update(bool running, const std::array<uint16_t, COUNT_STATUS_REGISTER> &status_register);

  protected:
    SimpleEvseComponent *const parent_;

    sensor::Sensor *set_charge_current_{nullptr};
    sensor::Sensor *actual_charge_current_{nullptr};
    sensor::Sensor *max_current_limit_{nullptr};
    sensor::Sensor *firmware_revision_{nullptr};  
};


}  // namespace simpleevse
}  // namespace esphome
