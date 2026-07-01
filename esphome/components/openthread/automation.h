#pragma once
#include "esphome/core/defines.h"
#ifdef USE_OPENTHREAD
#include "openthread.h"

#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"

namespace esphome::openthread {

/** Base class allowing to fetch OpenThread lock from parent component
 * while applying action
 *
 * - Nontemplate aspects belong here to avoid template bloat.
 * - Subclasses implement virtual action method that is called under lock.
 * - Seal leaf subclasses via @a final to support devirtualization.
 */
class OpenThreadComponentBaseAction : public Parented<OpenThreadComponent> {
 public:
  // Enforce ctor with parent argument (not without args)
  explicit OpenThreadComponentBaseAction(OpenThreadComponent *ot) : Parented<OpenThreadComponent>(ot) {}

 protected:
  /** Handler to implement in subclass for applying action parts that need lock */
  virtual void apply_locked(otInstance *instance) = 0;

  /** Fetch OT lock and then call @a apply_locked */
  void lock_and_apply_();

  /** Log a warning that this action has no effect on FTD devices */
  void warn_ftd_no_op_();

  /** Timeout (ms) for acquiring OT lock */
  static constexpr uint32_t LOCK_ACQUIRE_TIMEOUT_MS = 100;
};

/** Action to set single poll period parameter */
template<typename... Ts>
class OpenThreadComponentPollPeriodAction final : public Action<Ts...>, public OpenThreadComponentBaseAction {
  TEMPLATABLE_VALUE(uint32_t, poll_period)

 public:
  /* Passthrough ctor */
  using OpenThreadComponentBaseAction::OpenThreadComponentBaseAction;

 protected:
  void play(const Ts &...x) override {
#if CONFIG_OPENTHREAD_MTD
    this->parent_->set_poll_period(this->poll_period_.value(x...));

    this->lock_and_apply_();
#else
    this->warn_ftd_no_op_();
#endif
  }

  void apply_locked(otInstance *instance) override { this->parent_->apply_linkmode_(instance); }
};

}  // namespace esphome::openthread
#endif
