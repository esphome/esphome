#pragma once

#include <array>

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace dfrobot_mmwave_radar {

class DfrobotMmwaveRadarComponent : public uart::UARTDevice, public Component {
 public:
    void dump_config() override;

    void set_detection_range_min(float range_min) { range_min_ = range_min; }
    void set_detection_range_max(float range_max) { range_max_ = range_max; }
    void set_delay_after_detect(float delay_after_detect) { delay_after_detect_ = delay_after_detect; }
    void set_delay_after_disappear(float delay_after_disappear) { delay_after_disappear_ = delay_after_disappear; }
 protected:
    float range_min_;
    float range_max_;
    float delay_after_detect_;
    float delay_after_disappear_;
};

}  // namespace dfrobot_mmwave_radar
}  // namespace esphome
