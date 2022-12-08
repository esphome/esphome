#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/uart/uart.h"
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif

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
    unsigned long cmd_duration_ms_{1000};
    unsigned long timeout_ms_{1500};
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

#ifdef USE_BINARY_SENSOR
    void set_detected_binary_sensor(binary_sensor::BinarySensor *detected_binary_sensor) {
      detected_binary_sensor_ = detected_binary_sensor;
    }
#endif

    int8_t enqueue(Command * cmd);
 protected:
 #ifdef USE_BINARY_SENSOR
    binary_sensor::BinarySensor *detected_binary_sensor_{nullptr};
#endif
    bool detected_{0};
    char read_buffer_[MMWAVE_READ_BUFFER_LENGTH];
    size_t read_pos_{0};
    CircularCommandQueue cmdQueue_;
    unsigned long ts_last_cmd_sent_{0};

    uint8_t read_message();
    uint8_t find_prompt();
    uint8_t send_cmd(const char * cmd, unsigned long duration);

    void set_detected_(bool detected);

    friend class Command;
    friend class ReadStateCommand;
};

class ReadStateCommand : public Command {
 public:
   uint8_t execute(DfrobotMmwaveRadarComponent * component) override;
   uint8_t onMessage(std::string & message) override;
 protected:
   unsigned long timeout_ms_{500};
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
   unsigned long cmd_duration_ms_{3000};
   unsigned long timeout_ms_{3500};
};

template<typename... Ts> class DfrobotMmwaveRadarDetRangeCfgAction : public Action<Ts...> {
 public:
  DfrobotMmwaveRadarDetRangeCfgAction(DfrobotMmwaveRadarComponent *parent) : parent_(parent) {}

  void set_segments(
    float min1, float max1,
    float min2, float max2,
    float min3, float max3,
    float min4, float max4
  ) {
     this->min1_ = min1;
     this->max1_ = max1;
     this->min2_ = min2;
     this->max2_ = max2;
     this->min3_ = min3;
     this->max3_ = max3;
     this->min4_ = min4;
     this->max4_ = max4;
  }

  void play(Ts... x) {
    parent_->enqueue(new PowerCommand(0));
    parent_->enqueue(new DetRangeCfgCommand(
       min1_, max1_,
       min2_, max2_,
       min3_, max3_,
       min4_, max4_
    ));
    parent_->enqueue(new SaveCfgCommand());
    parent_->enqueue(new PowerCommand(1));
  }
 protected:
  DfrobotMmwaveRadarComponent *parent_;
  float min1_, max1_;
  float min2_, max2_;
  float min3_, max3_;
  float min4_, max4_;
};

template<typename... Ts> class DfrobotMmwaveRadarOutLatencyAction : public Action<Ts...> {
 public:
  DfrobotMmwaveRadarOutLatencyAction(DfrobotMmwaveRadarComponent *parent) : parent_(parent) {}

  void set_delay_after_detect(float delay_after_detect) { delay_after_detect_ = delay_after_detect; }
  void set_delay_after_disappear(float delay_after_disappear) { delay_after_disappear_ = delay_after_disappear; }

  void play(Ts... x) {
    parent_->enqueue(new PowerCommand(0));
    parent_->enqueue(new OutputLatencyCommand(
       delay_after_detect_, delay_after_disappear_
    ));
    parent_->enqueue(new SaveCfgCommand());
    parent_->enqueue(new PowerCommand(1));
  }
 protected:
  DfrobotMmwaveRadarComponent *parent_;
  float delay_after_detect_;
  float delay_after_disappear_;
};

template<typename... Ts> class DfrobotMmwaveRadarResetAction : public Action<Ts...> {
 public:
  DfrobotMmwaveRadarResetAction(DfrobotMmwaveRadarComponent *parent) : parent_(parent) {}
  void play(Ts... x) {
    parent_->enqueue(new ResetSystemCommand());
  }
 protected:
  DfrobotMmwaveRadarComponent *parent_;
};

template<typename... Ts> class DfrobotMmwaveRadarFactoryResetAction : public Action<Ts...> {
 public:
  DfrobotMmwaveRadarFactoryResetAction(DfrobotMmwaveRadarComponent *parent) : parent_(parent) {}
  void play(Ts... x) {
    parent_->enqueue(new PowerCommand(0));
    parent_->enqueue(new FactoryResetCommand());
  }
 protected:
  DfrobotMmwaveRadarComponent *parent_;
};

}  // namespace dfrobot_mmwave_radar
}  // namespace esphome
