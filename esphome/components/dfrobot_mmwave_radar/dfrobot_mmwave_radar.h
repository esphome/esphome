#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/uart/uart.h"
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
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
  virtual uint8_t execute(DfrobotMmwaveRadarComponent *component);
  virtual uint8_t on_message(std::string &message) = 0;

 protected:
  DfrobotMmwaveRadarComponent *component_{nullptr};
  std::string cmd_;
  bool cmd_sent_{false};
  int8_t retries_left_{2};
  uint32_t cmd_duration_ms_{1000};
  uint32_t timeout_ms_{1500};
};

static const uint8_t COMMAND_QUEUE_SIZE = 20;

class CircularCommandQueue {
 public:
  int8_t enqueue(std::unique_ptr<Command> cmd);
  std::unique_ptr<Command> dequeue();
  bool is_empty();
  bool is_full();
  uint8_t process(DfrobotMmwaveRadarComponent *component);

 protected:
  int front_{-1};
  int rear_{-1};
  std::unique_ptr<Command> commands_[COMMAND_QUEUE_SIZE];
};

#ifdef USE_SWITCH
class DfrobotMmwaveRadarSwitch : public switch_::Switch, public Component {
  enum SWITCH_TYPE {
    UNKNOWN,
    TURN_ON_SENSOR
  };
 public:
  void set_type(const char *type);
  void write_state(bool state) override;
  void set_component(DfrobotMmwaveRadarComponent *component) { component_ = component; }

 protected:
  DfrobotMmwaveRadarComponent *component_;
  enum SWITCH_TYPE type_{UNKNOWN};
};
#endif

class DfrobotMmwaveRadarComponent : public uart::UARTDevice, public Component {
 public:
  void dump_config() override;
  void setup() override;
  void loop() override;
  void set_active(bool active) {
    if (active != active_) {
#ifdef USE_SWITCH
      if (this->active_switch_ != nullptr)
        this->active_switch_->publish_state(active);
#endif
      active_ = active;
    }
  }
  bool is_active() { return active_; }

#ifdef USE_BINARY_SENSOR
  void set_detected_binary_sensor(binary_sensor::BinarySensor *detected_binary_sensor) {
    detected_binary_sensor_ = detected_binary_sensor;
  }
#endif

#ifdef USE_SWITCH
  void set_active_switch(switch_::Switch *active_switch) { active_switch_ = active_switch; }
#endif

  int8_t enqueue(std::unique_ptr<Command> cmd);

 protected:
#ifdef USE_BINARY_SENSOR
  binary_sensor::BinarySensor *detected_binary_sensor_{nullptr};
#endif
#ifdef USE_SWITCH
  switch_::Switch *active_switch_{nullptr};
#endif
  bool detected_{false};
  bool active_{false};
  char read_buffer_[MMWAVE_READ_BUFFER_LENGTH];
  size_t read_pos_{0};
  CircularCommandQueue cmd_queue_;
  uint32_t ts_last_cmd_sent_{0};

  uint8_t read_message_();
  uint8_t find_prompt_();
  uint8_t send_cmd_(const char *cmd, uint32_t duration);

  void set_detected_(bool detected);

  friend class Command;
  friend class ReadStateCommand;
};

class ReadStateCommand : public Command {
 public:
  uint8_t execute(DfrobotMmwaveRadarComponent *component) override;
  uint8_t on_message(std::string &message) override;

 protected:
  uint32_t timeout_ms_{500};
};

class PowerCommand : public Command {
 public:
  PowerCommand(bool power_on) : power_on_(power_on) {
    if (power_on) {
      cmd_ = "sensorStart";
    } else {
      cmd_ = "sensorStop";
    }
  };
  uint8_t on_message(std::string &message) override;

 protected:
  bool power_on_;
};

class DetRangeCfgCommand : public Command {
 public:
  DetRangeCfgCommand(float min1, float max1, float min2, float max2, float min3, float max3, float min4, float max4);
  uint8_t on_message(std::string &message) override;

 protected:
  float min1_, max1_, min2_, max2_, min3_, max3_, min4_, max4_;
  // TODO: Set min max values in component, so they can be published as sensor.
};

class OutputLatencyCommand : public Command {
 public:
  OutputLatencyCommand(float delay_after_detection, float delay_after_disappear);
  uint8_t on_message(std::string &message) override;

 protected:
  float delay_after_detection_;
  float delay_after_disappear_;
};

class SensorCfgStartCommand : public Command {
 public:
  SensorCfgStartCommand(bool startup_mode) : startup_mode_(startup_mode) {
    char tmp_cmd[20] = {0};
    sprintf(tmp_cmd, "sensorCfgStart %d", startup_mode);
    cmd_ = std::string(tmp_cmd);
  }
  uint8_t on_message(std::string &message) override;

 protected:
  bool startup_mode_;
};

class FactoryResetCommand : public Command {
 public:
  FactoryResetCommand() { cmd_ = "factoryReset 0x45670123 0xCDEF89AB 0x956128C6 0xDF54AC89"; };
  uint8_t on_message(std::string &message) override;
};

class ResetSystemCommand : public Command {
 public:
  ResetSystemCommand() { cmd_ = "resetSystem"; }
  uint8_t on_message(std::string &message) override;
};

class SaveCfgCommand : public Command {
 public:
  SaveCfgCommand() { cmd_ = "saveCfg 0x45670123 0xCDEF89AB 0x956128C6 0xDF54AC89"; }
  uint8_t on_message(std::string &message) override;

 protected:
  uint32_t cmd_duration_ms_{3000};
  uint32_t timeout_ms_{3500};
};

class LedModeCommand : public Command {
 public:
  LedModeCommand(bool active) : active_(active) {
    if (active) {
      cmd_ = "setLedMode 1 0";
    } else {
      cmd_ = "setLedMode 1 1";
    }
  };
  uint8_t on_message(std::string &message) override;

 protected:
  bool active_;
};

class UartOutputCommand : public Command {
 public:
  UartOutputCommand(bool active) : active_(active) {
    if (active) {
      cmd_ = "setUartOutput 1 1";
    } else {
      cmd_ = "setUartOutput 1 0";
    }
  };
  uint8_t on_message(std::string &message) override;

 protected:
  bool active_;
};

class SensitivityCommand : public Command {
 public:
  SensitivityCommand(uint8_t sensitivity) : sensitivity_(sensitivity) {
    if (sensitivity > 9)
      sensitivity_ = sensitivity = 9;
    char tmp_cmd[20] = {0};
    sprintf(tmp_cmd, "setSensitivity %d", sensitivity);
    cmd_ = std::string(tmp_cmd);
  };
  uint8_t on_message(std::string &message) override;

 protected:
  uint8_t sensitivity_;
};

template<typename... Ts> class DfrobotMmwaveRadarPowerAction : public Action<Ts...> {
 public:
  DfrobotMmwaveRadarPowerAction(DfrobotMmwaveRadarComponent *parent) : parent_(parent) {}

  void set_power(bool power_state) { power_state_ = power_state; }

  void play(Ts... x) { parent_->enqueue(make_unique<PowerCommand>(power_state_)); }

 protected:
  DfrobotMmwaveRadarComponent *parent_;
  bool power_state_;
};

template<typename... Ts> class DfrobotMmwaveRadarResetAction : public Action<Ts...> {
 public:
  DfrobotMmwaveRadarResetAction(DfrobotMmwaveRadarComponent *parent) : parent_(parent) {}
  void play(Ts... x) { parent_->enqueue(make_unique<ResetSystemCommand>()); }

 protected:
  DfrobotMmwaveRadarComponent *parent_;
};

template<typename... Ts> class DfrobotMmwaveRadarSettingsAction : public Action<Ts...> {
 public:
  DfrobotMmwaveRadarSettingsAction(DfrobotMmwaveRadarComponent *parent) : parent_(parent) {}

  TEMPLATABLE_VALUE(int8_t, factory_reset)
  TEMPLATABLE_VALUE(int8_t, start_after_power_on)
  TEMPLATABLE_VALUE(int8_t, turn_on_led)
  TEMPLATABLE_VALUE(int8_t, presence_via_uart)
  TEMPLATABLE_VALUE(int8_t, sensitivity)
  TEMPLATABLE_VALUE(float, delay_after_detect)
  TEMPLATABLE_VALUE(float, delay_after_disappear)
  TEMPLATABLE_VALUE(float, det_min1)
  TEMPLATABLE_VALUE(float, det_max1)
  TEMPLATABLE_VALUE(float, det_min2)
  TEMPLATABLE_VALUE(float, det_max2)
  TEMPLATABLE_VALUE(float, det_min3)
  TEMPLATABLE_VALUE(float, det_max3)
  TEMPLATABLE_VALUE(float, det_min4)
  TEMPLATABLE_VALUE(float, det_max4)

  void play(Ts... x) {
    parent_->enqueue(make_unique<PowerCommand>(0));
    if (this->factory_reset_.has_value() && this->factory_reset_.value(x...) == true) {
      parent_->enqueue(make_unique<FactoryResetCommand>());
    }
    if (this->det_min1_.has_value() && this->det_max1_.has_value()) {
      if (this->det_min1_.value() >= 0 && this->det_max1_.value() >= 0) {
        parent_->enqueue(make_unique<DetRangeCfgCommand>(this->det_min1_.value_or(-1), this->det_max1_.value_or(-1),
                                                         this->det_min2_.value_or(-1), this->det_max2_.value_or(-1),
                                                         this->det_min3_.value_or(-1), this->det_max3_.value_or(-1),
                                                         this->det_min4_.value_or(-1), this->det_max4_.value_or(-1)));
      }
    }
    if (this->delay_after_detect_.has_value() && this->delay_after_disappear_.has_value()) {
      float detect = this->delay_after_detect_.value(x...);
      float disappear = this->delay_after_disappear_.value(x...);
      if (detect >= 0 && disappear >= 0) {
        parent_->enqueue(make_unique<OutputLatencyCommand>(detect, disappear));
      }
    }
    if (this->start_after_power_on_.has_value()) {
      int8_t val = this->start_after_power_on_.value(x...);
      if (val >= 0) {
        parent_->enqueue(make_unique<SensorCfgStartCommand>(val));
      }
    }
    if (this->turn_on_led_.has_value()) {
      int8_t val = this->turn_on_led_.value(x...);
      if (val >= 0) {
        parent_->enqueue(make_unique<LedModeCommand>(val));
      }
    }
    if (this->presence_via_uart_.has_value()) {
      int8_t val = this->presence_via_uart_.value(x...);
      if (val >= 0) {
        parent_->enqueue(make_unique<UartOutputCommand>(val));
      }
    }
    if (this->sensitivity_.has_value()) {
      int8_t val = this->sensitivity_.value(x...);
      if (val >= 0) {
        if (val > 9) {
          val = 9;
        }
        parent_->enqueue(make_unique<SensitivityCommand>(val));
      }
    }
    parent_->enqueue(make_unique<SaveCfgCommand>());
    parent_->enqueue(make_unique<PowerCommand>(1));
  }

 protected:
  DfrobotMmwaveRadarComponent *parent_;
};

}  // namespace dfrobot_mmwave_radar
}  // namespace esphome
