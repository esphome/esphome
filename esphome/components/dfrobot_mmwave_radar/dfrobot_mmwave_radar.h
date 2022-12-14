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

class LedModeCommand : public Command {
 public:
   LedModeCommand(bool active) :
      active_(active) {
         if(active)
            cmd_ = "setLedMode 1 0";
         else
            cmd_ = "setLedMode 1 1";
      };
   uint8_t onMessage(std::string & message) override;
 protected:
   bool active_;
};

template<typename... Ts> class DfrobotMmwaveRadarPowerAction : public Action<Ts...> {
 public:
  DfrobotMmwaveRadarPowerAction(DfrobotMmwaveRadarComponent *parent) : parent_(parent) {}

  void set_power(bool powerState) {
    powerState_ = powerState;
  }

  void play(Ts... x) {
    parent_->enqueue(new PowerCommand(powerState_));
  }
 protected:
  DfrobotMmwaveRadarComponent *parent_;
  bool powerState_;
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

template<typename... Ts> class DfrobotMmwaveRadarSettingsAction : public Action<Ts...> {
 public:
  DfrobotMmwaveRadarSettingsAction(DfrobotMmwaveRadarComponent *parent) : parent_(parent) {}

  void set_factory_reset(bool factory_reset) {
    factory_reset_ = factory_reset;
  }

  void set_segments(
    float min1, float max1,
    float min2, float max2,
    float min3, float max3,
    float min4, float max4
  ) {
     this->det_min1_ = min1;
     this->det_max1_ = max1;
     this->det_min2_ = min2;
     this->det_max2_ = max2;
     this->det_min3_ = min3;
     this->det_max3_ = max3;
     this->det_min4_ = min4;
     this->det_max4_ = max4;
  }

  void set_ouput_delays(
    float delay_after_detect,
    float delay_after_disappear
  ) {
    delay_after_detect_ = delay_after_detect;
    delay_after_disappear_ = delay_after_disappear;
  }

  void set_start_immediately(bool start_immediately) {
    start_immediately_ = start_immediately;
  }

  void set_led_active(bool led_active) { led_active_ = led_active; }

  void play(Ts... x) {
    parent_->enqueue(new PowerCommand(0));
    if(factory_reset_ == 1) {
      parent_->enqueue(new FactoryResetCommand());
    }
    if(det_min1_ >= 0 && det_max1_ >= 0) {
      parent_->enqueue(new DetRangeCfgCommand(
       det_min1_, det_max1_,
       det_min2_, det_max2_,
       det_min3_, det_max3_,
       det_min4_, det_max4_
      ));
    }
    if(delay_after_detect_ >= 0 &&
       delay_after_disappear_ >= 0) {
      parent_->enqueue(new OutputLatencyCommand(
        delay_after_detect_, delay_after_disappear_
      ));
    }
    if(start_immediately_ >= 0) {
      parent_->enqueue(new SensorCfgStartCommand(start_immediately_));
    }
    if(led_active_ >= 0) {
      parent_->enqueue(new LedModeCommand(led_active_));
    }
    parent_->enqueue(new SaveCfgCommand());
    parent_->enqueue(new PowerCommand(1));
  }
 protected:
  DfrobotMmwaveRadarComponent *parent_;
  int8_t factory_reset_{-1};
  float det_min1_{-1}, det_max1_{-1};
  float det_min2_{-1}, det_max2_{-1};
  float det_min3_{-1}, det_max3_{-1};
  float det_min4_{-1}, det_max4_{-1};
  float delay_after_detect_{-1};
  float delay_after_disappear_{-1};
  int8_t start_immediately_{-1};
  int8_t led_active_{-1};
};

}  // namespace dfrobot_mmwave_radar
}  // namespace esphome
