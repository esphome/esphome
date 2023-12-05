#pragma once

#include "esphome/components/number/number.h"
#include "../mr24hpc1.h"

namespace esphome {
namespace mr24hpc1 {

class MotionThresholdNumber : public number::Number, public Parented<mr24hpc1Component> {
    public:
        MotionThresholdNumber() = default;

    protected:
        void control(float value) override;
};

}  // namespace mr24hpc1
}  // namespace esphome
