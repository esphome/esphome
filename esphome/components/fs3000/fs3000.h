#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace fs3000 {

// FS3000 has two models, 1005 and 1015
//  1005 has a max speed detection of 7.23 m/s
//  1015 has a max speed detection of 15 m/s
enum FS3000Model {FIVE, FIFTEEN};

class FS3000Component : public PollingComponent, public i2c::I2CDevice, public sensor::Sensor {
    public:
        void update() override;

        void dump_config() override;
        float get_setup_priority() const override { return setup_priority::DATA; }

        void set_model(FS3000Model model);

    protected:
        FS3000Model model_{FIVE};

        uint16_t raw_data_points_[13];
        float mps_data_points_[13];

        float fit_raw_(uint16_t raw_value);
};

}  // namespace fs3000
}  // namespace esphome
