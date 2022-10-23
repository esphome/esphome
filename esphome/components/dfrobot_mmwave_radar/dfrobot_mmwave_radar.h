#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace dfrobot_mmwave_radar {

const uint8_t MMWAVE_READ_BUFFER_LENGTH = 255;

class DfrobotMmwaveRadarComponent : public uart::UARTDevice, public Component {
 public:
    void dump_config() override;
    void setup() override;
    void loop() override;

    void set_detection_range_min(float range_min) { range_min_ = range_min; }
    void set_detection_range_max(float range_max) { range_max_ = range_max; }
    void set_delay_after_detect(float delay_after_detect) { delay_after_detect_ = delay_after_detect; }
    void set_delay_after_disappear(float delay_after_disappear) { delay_after_disappear_ = delay_after_disappear; }
 protected:
    float range_min_;
    float range_max_;
    float delay_after_detect_;
    float delay_after_disappear_;

    int8_t sensor_state{-1};
    char read_buffer_[MMWAVE_READ_BUFFER_LENGTH];
    size_t read_pos_{0};

    uint8_t read_message();

    friend class ReadStateCommand;
};

// Use command queue and time stamps to avoid blocking.
// When component has run time, check if minimum time (1s) between
// commands has passed. After that run a command from the queue.
class Command {
 public:
    virtual ~Command() = default;
    virtual uint8_t execute() = 0;
};

class ReadStateCommand : public Command {
 public:
   ReadStateCommand(DfrobotMmwaveRadarComponent *component) : component_(component) {};
   uint8_t execute() override;
 protected:
   DfrobotMmwaveRadarComponent * component_{nullptr};
   int8_t parse_sensing_results();
};

class PowerCommand : public Command {
 public:
  uint8_t execute() override;
};

class DetRangeCfgCommand : public Command {
 public:
   uint8_t execute() override;
};

class OutputLatencyCommand : public Command {
 public:
   uint8_t execute() override;
};

class SensorCfgStartCommand : public Command {
 public:
   uint8_t execute() override;
};

}  // namespace dfrobot_mmwave_radar
}  // namespace esphome
