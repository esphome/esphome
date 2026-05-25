#pragma once

#include "esphome/core/entity_base.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"
#include "iactuator.h"

namespace esphome::actuator {

static constexpr float ACTUATOR_OPEN = 1.0f;
static constexpr float ACTUATOR_CLOSED = 0.0f;

enum ActuatorOperation : uint8_t {
  ACTUATOR_OPERATION_IDLE = 0,
  ACTUATOR_OPERATION_OPENING,
  ACTUATOR_OPERATION_CLOSING,
};

struct ActuatorRestoreState {
  float position;
} __attribute__((packed));

const LogString *actuator_operation_to_str(ActuatorOperation op);

class ActuatorBase;

// Inheritance: ActuatorCallBase — base call class for actuator commands
class ActuatorCallBase {
 public:
  explicit ActuatorCallBase(ActuatorBase *parent) : parent_(parent) {}

  ActuatorCallBase &set_command(const char *command);
  ActuatorCallBase &set_command_open();
  ActuatorCallBase &set_command_close();
  ActuatorCallBase &set_command_stop();
  ActuatorCallBase &set_command_toggle();
  ActuatorCallBase &set_position(float position);
  ActuatorCallBase &set_stop(bool stop);

  void perform();

  const optional<float> &get_position() const { return this->position_; }
  bool get_stop() const { return this->stop_; }
  const optional<bool> &get_toggle() const { return this->toggle_; }

 protected:
  virtual void validate();

  void call_control_();

  ActuatorBase *parent_;
  bool stop_{false};
  optional<float> position_{};
  optional<bool> toggle_{};
};

// Inheritance: ActuatorBase -> EntityBase
class ActuatorBase : public EntityBase {
 public:
  float position{ACTUATOR_CLOSED};
  ActuatorOperation current_operation{ACTUATOR_OPERATION_IDLE};

  bool is_fully_open() const;
  bool is_fully_closed() const;

  template<typename F> void add_on_state_callback(F &&f) { this->state_callback_.add(std::forward<F>(f)); }

 protected:
  friend ActuatorCallBase;

  virtual void control(const ActuatorCallBase &call) = 0;

  template<typename T> optional<T> restore_state_() {
    this->rtc_ = this->make_entity_preference<T>();
    T recovered{};
    if (!this->rtc_.load(&recovered))
      return {};
    return recovered;
  }

  LazyCallbackManager<void()> state_callback_{};
  ESPPreferenceObject rtc_;
};

}  // namespace esphome::actuator
