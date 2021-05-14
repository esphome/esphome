#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "simplesevse.h"

namespace esphome {
namespace simpleevse {

class SimpleEvseBinarySensors : public UpdateListener {
  public:
    explicit SimpleEvseBinarySensors(SimpleEvseComponent *parent) : parent_(parent) { parent->add_observer(this); }

    /* setter methods for sensor configuration */
    void set_connected_sensor(binary_sensor::BinarySensor *connected) { connected_ = connected; };

    void update(bool running, const std::array<uint16_t, COUNT_STATUS_REGISTER> &status_register);

  protected:
    SimpleEvseComponent *const parent_;

    binary_sensor::BinarySensor *connected_{nullptr};
};

}  // namespace simpleevse
}  // namespace esphome
