#pragma once

#include "esphome/core/component.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/preferences.h"
#include "esphome/components/actuator/actuator.h"

#include "cover_traits.h"

namespace esphome::cover {

// Backward-compat aliases.
using CoverOperation = actuator::ActuatorOperation;
constexpr actuator::ActuatorOperation COVER_OPERATION_IDLE = actuator::ACTUATOR_OPERATION_IDLE;
constexpr actuator::ActuatorOperation COVER_OPERATION_OPENING = actuator::ACTUATOR_OPERATION_OPENING;
constexpr actuator::ActuatorOperation COVER_OPERATION_CLOSING = actuator::ACTUATOR_OPERATION_CLOSING;
static constexpr float COVER_OPEN = actuator::ACTUATOR_OPEN;
static constexpr float COVER_CLOSED = actuator::ACTUATOR_CLOSED;

#define LOG_COVER(prefix, type, obj) \
  if ((obj) != nullptr) { \
    ESP_LOGCONFIG(TAG, "%s%s '%s'", prefix, LOG_STR_LITERAL(type), (obj)->get_name().c_str()); \
    auto traits_ = (obj)->get_traits(); \
    if (traits_.get_is_assumed_state()) { \
      ESP_LOGCONFIG(TAG, "%s  Assumed State: YES", prefix); \
    } \
    LOG_ENTITY_DEVICE_CLASS(TAG, prefix, *(obj)); \
  }

class Cover;

// Inheritance: CoverCall -> ActuatorCallBase
class CoverCall : public actuator::ActuatorCallBase {
 public:
  CoverCall(Cover *parent);

  // Covariant wrappers — return CoverCall& for fluent chaining compatibility
  CoverCall &set_command(const char *command);
  CoverCall &set_command_open();
  CoverCall &set_command_close();
  CoverCall &set_command_stop();
  CoverCall &set_command_toggle();
  CoverCall &set_position(float position);
  CoverCall &set_tilt(float tilt);
  CoverCall &set_stop(bool stop);

  void perform();

  const optional<float> &get_position() const { return this->position_; }
  bool get_stop() const { return this->stop_; }
  const optional<float> &get_tilt() const { return this->tilt_; }
  const optional<bool> &get_toggle() const { return this->toggle_; }

 protected:
  optional<float> tilt_{};

 private:
  void validate() override;
};

/// Struct used to store the restored state of a cover
struct CoverRestoreState {
  float position;
  float tilt;

  /// Convert this struct to a cover call that can be performed.
  CoverCall to_call(Cover *cover);
  /// Apply these settings to the cover
  void apply(Cover *cover);
} __attribute__((packed));

const LogString *cover_operation_to_str(CoverOperation op);

/** Base class for all cover devices.
 *
 * Covers currently have three properties:
 *  - position - The current position of the cover from 0.0 (fully closed) to 1.0 (fully open).
 *    For covers with only binary OPEN/CLOSED position this will always be either 0.0 or 1.0
 *  - tilt - The tilt value of the cover from 0.0 (closed) to 1.0 (closed)
 *  - current_operation - The operation the cover is currently performing, this can
 *    be one of IDLE, OPENING and CLOSING.
 *
 * For users: All cover operations must be performed over the .make_call() interface.
 * To command a cover, use .make_call() to create a call object, set all properties
 * you wish to set, and activate the command with .perform().
 * For reading out the current values of the cover, use the public .position, .tilt etc
 * properties (these are read-only for users)
 *
 * For integrations: Integrations must implement two methods: control() and get_traits().
 * Control will be called with the arguments supplied by the user and should be used
 * to control all values of the cover. Also implement get_traits() to return what operations
 * the cover supports.
 */
// Inheritance: Cover -> ActuatorBase, IActuator
class Cover : public actuator::ActuatorBase, public actuator::IActuator {
 public:
  explicit Cover();

  /// The current tilt value of the cover from 0.0 to 1.0.
  float tilt{COVER_OPEN};

  /// Construct a new cover call used to control the cover.
  CoverCall make_call();

  using actuator::ActuatorBase::add_on_state_callback;

  /** Publish the current state of the cover.
   *
   * First set the .position, .tilt, etc values and then call this method
   * to publish the state of the cover.
   *
   * @param save Whether to save the updated values in RTC area.
   */
  void publish_state(bool save = true);

  virtual CoverTraits get_traits() = 0;

  // IActuator implementation
  float get_position() const override { return this->position; }
  void set_position(float p) override { this->position = p; }
  actuator::ActuatorOperation get_operation() const override { return this->current_operation; }
  void set_operation(actuator::ActuatorOperation op) override { this->current_operation = op; }
  void do_publish_state(bool save) override { this->publish_state(save); }
  optional<float> do_restore_state() override;
  const char *get_entity_name() const override { return this->get_name().c_str(); }

 protected:
  friend CoverCall;

  virtual void control(const CoverCall &call) = 0;

  optional<CoverRestoreState> restore_state_() { return ActuatorBase::restore_state_<CoverRestoreState>(); }
};

}  // namespace esphome::cover
