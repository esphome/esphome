#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace dfrobot_mmwave_radar {

const uint8_t MMWAVE_READ_BUFFER_LENGTH = 255;

// Use command queue and time stamps to avoid blocking.
// When component has run time, check if minimum time (1s) between
// commands has passed. After that run a command from the queue.
class Command {
 public:
    virtual ~Command() = default;
    virtual uint8_t execute() = 0;
 protected:
    std::string cmd_;
    bool cmd_sent_{false};
};

static const uint8_t COMMAND_QUEUE_SIZE = 20;

class CircularCommandQueue {
 public:
   int8_t enqueue(Command * cmd);
   Command * dequeue();
   Command * peek();
   bool isEmpty();
   bool isFull();
 protected:
   int front_{-1};
   int rear_{-1};
   Command * commands_[COMMAND_QUEUE_SIZE];
};

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
    CircularCommandQueue cmdQueue_;
    unsigned long ts_last_cmd_sent_{0};

    uint8_t read_message();
    uint8_t find_prompt();
    uint8_t send_cmd(const char * cmd);

    friend class ReadStateCommand;
    friend class PowerCommand;
    friend class DetRangeCfgCommand;
    friend class OutputLatencyCommand;
    friend class SensorCfgStartCommand;
    friend class FactoryResetCommand;
    friend class ResetSystemCommand;
    friend class SaveCfgCommand;
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
   PowerCommand(DfrobotMmwaveRadarComponent *component, bool powerOn) :
      component_(component),
      powerOn_(powerOn) {
         if(powerOn)
            cmd_ = "sensorStart";
         else
            cmd_ = "sensorStop";
      };
   uint8_t execute() override;
 protected:
   DfrobotMmwaveRadarComponent * component_;
   bool powerOn_;
   int8_t retries_left_{2};
};

class DetRangeCfgCommand : public Command {
 public:
   DetRangeCfgCommand(DfrobotMmwaveRadarComponent *component,
                        float min1, float max1,
                        float min2, float max2,
                        float min3, float max3,
                        float min4, float max4
                     );
   uint8_t execute() override;
 protected:
   DfrobotMmwaveRadarComponent * component_;
   int8_t retries_left_{2};
   float min1_, max1_, min2_, max2_, min3_, max3_, min4_, max4_;
   // TODO: Set min max values in component, so they can be published as sensor.
};

class OutputLatencyCommand : public Command {
 public:
   OutputLatencyCommand(DfrobotMmwaveRadarComponent *component,
                        float delay_after_detection,
                        float delay_after_disappear
                       );
   uint8_t execute() override;
 protected:
   DfrobotMmwaveRadarComponent * component_;
   int8_t retries_left_{2};
   float delay_after_detection_;
   float delay_after_disappear_;
};

class SensorCfgStartCommand : public Command {
 public:
   SensorCfgStartCommand(DfrobotMmwaveRadarComponent *component,bool startupMode) :
                        component_(component), startupMode_(startupMode) {
         char tmp_cmd[20] = {0};
         sprintf(tmp_cmd, "sensorCfgStart %d", startupMode);
         cmd_ = std::string(tmp_cmd);
   }
   uint8_t execute() override;
 protected:
   DfrobotMmwaveRadarComponent * component_;
   int8_t retries_left_{2};
   bool startupMode_;
};

class FactoryResetCommand : public Command {
 public:
   FactoryResetCommand(DfrobotMmwaveRadarComponent *component) :
      component_(component) {
         cmd_ = "factoryReset 0x45670123 0xCDEF89AB 0x956128C6 0xDF54AC89";
      };
   uint8_t execute() override;
 protected:
   DfrobotMmwaveRadarComponent * component_;
   int8_t retries_left_{2};
};

class ResetSystemCommand : public Command {
 public:
   ResetSystemCommand(DfrobotMmwaveRadarComponent *component) :
      component_(component) {
         cmd_ = "resetSystem";
      }
   uint8_t execute() override;
 protected:
   DfrobotMmwaveRadarComponent * component_;
   int8_t retries_left_{2};
};

class SaveCfgCommand : public Command {
 public:
   SaveCfgCommand(DfrobotMmwaveRadarComponent *component) :
      component_(component) {
         cmd_ = "saveCfg 0x45670123 0xCDEF89AB 0x956128C6 0xDF54AC89";
      }
   uint8_t execute() override;
 protected:
   DfrobotMmwaveRadarComponent * component_;
   int8_t retries_left_{2};
};

}  // namespace dfrobot_mmwave_radar
}  // namespace esphome
