#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace pcf8583 {


//different address, depending on state of A0 Pin


class PCF8583Component : public PollingComponent, public i2c::I2CDevice {

  public:

    void set_counter(sensor::Sensor *counter)  { counter_ = counter; }
    void set_to_counter_mode();
    void update_and_reset();


    // Internal Methods
    void setup() override;
    void dump_config() override;
    float get_setup_priority() const override;
    void update() override;


  protected:

    void reset_counter_();
    uint32_t read_counter_();
    uint8_t bcd2byte_(uint8_t value);
    uint8_t byte2bcd_(uint8_t value);
    sensor::Sensor *counter_;
};



}       //namespace pcf8583
}       //namespace esphome
