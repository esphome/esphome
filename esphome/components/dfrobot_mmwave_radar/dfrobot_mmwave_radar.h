#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace dfrobot_mmwave_radar {

const uint8_t MMWAVE_READ_BUFFER_LENGTH = 255;

// forward declaration due to circular dependency
class DfrobotMmwaveRadarComponent;

// Use command queue and time stamps to avoid blocking.
// When component has run time, check if minimum time (1s) between
// commands has passed. After that run a command from the queue.
class Command {
 public:
    virtual ~Command() = default;
    virtual uint8_t execute(DfrobotMmwaveRadarComponent * component);
    virtual uint8_t onMessage(std::string & message) = 0;
 protected:
    std::string cmd_;
    bool cmd_sent_{false};
    int8_t retries_left_{2};
    uint16_t timeout_ms_{500};
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

    friend class Command;
    friend class ReadStateCommand;
};

class ReadStateCommand : public Command {
 public:
   uint8_t execute(DfrobotMmwaveRadarComponent * component) override;
   uint8_t onMessage(std::string & message) override;
};

class PowerCommand : public Command {
 public:
   PowerCommand(bool powerOn) :
      powerOn_(powerOn) {
         if(powerOn)
            cmd_ = "sensorStart";
         else
            cmd_ = "sensorStop";
      };
   uint8_t onMessage(std::string & message) override;
 protected:
   bool powerOn_;
};

class DetRangeCfgCommand : public Command {
 public:
   DetRangeCfgCommand(
                        float min1, float max1,
                        float min2, float max2,
                        float min3, float max3,
                        float min4, float max4
                     );
   uint8_t onMessage(std::string & message) override;
 protected:
   float min1_, max1_, min2_, max2_, min3_, max3_, min4_, max4_;
   // TODO: Set min max values in component, so they can be published as sensor.
};

class OutputLatencyCommand : public Command {
 public:
   OutputLatencyCommand(
                        float delay_after_detection,
                        float delay_after_disappear
                       );
   uint8_t onMessage(std::string & message) override;
 protected:
   float delay_after_detection_;
   float delay_after_disappear_;
};

class SensorCfgStartCommand : public Command {
 public:
   SensorCfgStartCommand(bool startupMode) : startupMode_(startupMode) {
         char tmp_cmd[20] = {0};
         sprintf(tmp_cmd, "sensorCfgStart %d", startupMode);
         cmd_ = std::string(tmp_cmd);
   }
   uint8_t onMessage(std::string & message) override;
 protected:
   bool startupMode_;
};

class FactoryResetCommand : public Command {
 public:
   FactoryResetCommand() {
         cmd_ = "factoryReset 0x45670123 0xCDEF89AB 0x956128C6 0xDF54AC89";
      };
   uint8_t onMessage(std::string & message) override;
};

class ResetSystemCommand : public Command {
 public:
   ResetSystemCommand() {
         cmd_ = "resetSystem";
      }
   uint8_t onMessage(std::string & message) override;
};

class SaveCfgCommand : public Command {
 public:
   SaveCfgCommand() {
         cmd_ = "saveCfg 0x45670123 0xCDEF89AB 0x956128C6 0xDF54AC89";
      }
   uint8_t onMessage(std::string & message) override;
 protected:
   uint16_t timeout_ms_{900};
};

}  // namespace dfrobot_mmwave_radar
}  // namespace esphome
