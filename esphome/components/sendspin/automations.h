#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ESP32

#include "esphome/core/automation.h"
#include "sendspin_hub.h"

namespace esphome {
namespace sendspin {

#ifdef USE_SENDSPIN_CONTROLLER
template<typename... Ts> class SendSwitchCommandAction : public Action<Ts...>, public Parented<SendspinHub> {
 public:
  void play(const Ts &...x) override { this->parent_->send_client_command(SendspinControllerCommand::SWITCH); }
};
#endif  // USE_SENDSPIN_CONTROLLER

#ifdef USE_SENDSPIN_PLAYER
template<typename... Ts> class EnableStaticDelayAdjustmentAction : public Action<Ts...>, public Parented<SendspinHub> {
 public:
  void play(const Ts &...x) override { this->parent_->set_static_delay_adjustable(true); }
};

template<typename... Ts> class DisableStaticDelayAdjustmentAction : public Action<Ts...>, public Parented<SendspinHub> {
 public:
  void play(const Ts &...x) override { this->parent_->set_static_delay_adjustable(false); }
};
#endif  // USE_SENDSPIN_PLAYER

#ifdef USE_SENDSPIN_METADATA
// Forward declaration
template<typename... Ts> class GetTrackProgressAction;

/// Continuation that strips the injected uint32_t and continues the outer action chain.
template<typename... Ts> class GetTrackProgressContinuation : public Action<uint32_t, Ts...> {
 public:
  explicit GetTrackProgressContinuation(GetTrackProgressAction<Ts...> *parent) : parent_(parent) {}

  void play(const uint32_t &x, const Ts &...args) override { this->parent_->play_next_(args...); }

 protected:
  GetTrackProgressAction<Ts...> *parent_;
};

/// Action that captures the interpolated track progress (ms) and provides it as ``x`` to child actions.
template<typename... Ts> class GetTrackProgressAction : public Action<Ts...>, public Parented<SendspinHub> {
 public:
  void add_then(const std::initializer_list<Action<uint32_t, Ts...> *> &actions) {
    this->then_.add_actions(actions);
    this->then_.add_action(new GetTrackProgressContinuation<Ts...>(this));
  }

  friend class GetTrackProgressContinuation<Ts...>;

  void play_complex(const Ts &...x) override {
    this->num_running_++;
    uint32_t progress = this->parent_->get_track_progress_ms();
    this->then_.play(progress, x...);
  }

  void play(const Ts &...x) override { /* ignore - see play_complex */
  }

  void stop() override { this->then_.stop(); }

 protected:
  ActionList<uint32_t, Ts...> then_;
};
#endif  // USE_SENDSPIN_METADATA

}  // namespace sendspin
}  // namespace esphome

#endif  // USE_ESP32
