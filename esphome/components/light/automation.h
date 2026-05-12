#pragma once

#include "esphome/core/automation.h"
#include "light_state.h"
#include "addressable_light.h"

namespace esphome::light {

enum class LimitMode { CLAMP, DO_NOTHING };

template<bool HasTransitionLength, typename... Ts> class ToggleAction : public Action<Ts...> {
 public:
  explicit ToggleAction(LightState *state) : state_(state) {}

  template<typename V> void set_transition_length(V value) requires(HasTransitionLength) {
    this->transition_length_ = value;
  }

  void play(const Ts &...x) override {
    auto call = this->state_->toggle();
    if constexpr (HasTransitionLength) {
      call.set_transition_length(this->transition_length_.optional_value(x...));
    }
    call.perform();
  }

 protected:
  LightState *state_;
  struct NoTransition {};
  [[no_unique_address]] std::conditional_t<HasTransitionLength, TemplatableFn<uint32_t, Ts...>, NoTransition>
      transition_length_{};
};

// All configured fields are baked into a single stateless lambda whose
// constants live in flash. The action only stores one function pointer
// plus one parent pointer, regardless of how many fields the user set.
// Trigger args are forwarded to the apply function so user lambdas
// (e.g. `brightness: !lambda "return x;"`) keep working.
//
// Trigger args are normalized to `const std::remove_cvref_t<Ts> &...` so
// the codegen can emit a matching parameter list for both the apply lambda
// and any inner field lambdas without producing invalid C++ source text
// (e.g. `const T & &` if Ts already carries a reference, or `const const
// T &` if Ts already carries a const). This keeps trigger args no-copy
// regardless of whether the trigger supplies `T`, `T &`, or `const T &`.
template<typename... Ts> class LightControlAction : public Action<Ts...> {
 public:
  using ApplyFn = void (*)(LightState *, LightCall &, const std::remove_cvref_t<Ts> &...);
  LightControlAction(LightState *parent, ApplyFn apply) : parent_(parent), apply_(apply) {}

  void play(const Ts &...x) override {
    auto call = this->parent_->make_call();
    this->apply_(this->parent_, call, x...);
    call.perform();
  }

 protected:
  LightState *parent_;
  ApplyFn apply_;
};

template<bool HasTransitionLength, typename... Ts> class DimRelativeAction : public Action<Ts...> {
 public:
  explicit DimRelativeAction(LightState *parent) : parent_(parent) {}

  TEMPLATABLE_VALUE(float, relative_brightness)

  template<typename V> void set_transition_length(V value) requires(HasTransitionLength) {
    this->transition_length_ = value;
  }

  void play(const Ts &...x) override {
    auto call = this->parent_->make_call();
    float rel = this->relative_brightness_.value(x...);
    float cur;
    this->parent_->remote_values.as_brightness(&cur);
    if ((limit_mode_ == LimitMode::DO_NOTHING) && ((cur < min_brightness_) || (cur > max_brightness_))) {
      return;
    }
    float new_brightness = clamp(cur + rel, min_brightness_, max_brightness_);
    call.set_state(new_brightness != 0.0f);
    call.set_brightness(new_brightness);

    if constexpr (HasTransitionLength) {
      call.set_transition_length(this->transition_length_.optional_value(x...));
    }
    call.perform();
  }

  void set_min_max_brightness(float min, float max) {
    this->min_brightness_ = min;
    this->max_brightness_ = max;
  }

  void set_limit_mode(LimitMode limit_mode) { this->limit_mode_ = limit_mode; }

 protected:
  LightState *parent_;
  float min_brightness_{0.0};
  float max_brightness_{1.0};
  LimitMode limit_mode_{LimitMode::CLAMP};
  struct NoTransition {};
  [[no_unique_address]] std::conditional_t<HasTransitionLength, TemplatableFn<uint32_t, Ts...>, NoTransition>
      transition_length_{};
};

template<typename... Ts> class LightIsOnCondition : public Condition<Ts...> {
 public:
  explicit LightIsOnCondition(LightState *state) : state_(state) {}
  bool check(const Ts &...x) override { return this->state_->current_values.is_on(); }

 protected:
  LightState *state_;
};
template<typename... Ts> class LightIsOffCondition : public Condition<Ts...> {
 public:
  explicit LightIsOffCondition(LightState *state) : state_(state) {}
  bool check(const Ts &...x) override { return !this->state_->current_values.is_on(); }

 protected:
  LightState *state_;
};

class LightTurnOnTrigger : public Trigger<>, public LightRemoteValuesListener {
 public:
  explicit LightTurnOnTrigger(LightState *a_light) : light_(a_light) {
    a_light->add_remote_values_listener(this);
    this->last_on_ = a_light->current_values.is_on();
  }

  void on_light_remote_values_update() override {
    // using the remote value because of transitions we need to trigger as early as possible
    auto is_on = this->light_->remote_values.is_on();
    // only trigger when going from off to on
    auto should_trigger = is_on && !this->last_on_;
    // Set new state immediately so that trigger() doesn't devolve
    // into infinite loop
    this->last_on_ = is_on;
    if (should_trigger) {
      this->trigger();
    }
  }

 protected:
  LightState *light_;
  bool last_on_;
};

class LightTurnOffTrigger : public Trigger<>, public LightTargetStateReachedListener {
 public:
  explicit LightTurnOffTrigger(LightState *a_light) : light_(a_light) {
    a_light->add_target_state_reached_listener(this);
  }

  void on_light_target_state_reached() override {
    auto is_on = this->light_->current_values.is_on();
    // only trigger when going from on to off
    if (!is_on) {
      this->trigger();
    }
  }

 protected:
  LightState *light_;
};

class LightStateTrigger : public Trigger<>, public LightRemoteValuesListener {
 public:
  explicit LightStateTrigger(LightState *a_light) { a_light->add_remote_values_listener(this); }

  void on_light_remote_values_update() override { this->trigger(); }
};

// This is slightly ugly, but we can't log in headers, and can't make this a static method on AddressableSet
// due to the template. It's just a temporary warning anyway.
void addressableset_warn_about_scale(const char *field);

template<typename... Ts> class AddressableSet : public Action<Ts...> {
 public:
  explicit AddressableSet(LightState *parent) : parent_(parent) {}

  TEMPLATABLE_VALUE(int32_t, range_from)
  TEMPLATABLE_VALUE(int32_t, range_to)
  TEMPLATABLE_VALUE(float, color_brightness)
  TEMPLATABLE_VALUE(float, red)
  TEMPLATABLE_VALUE(float, green)
  TEMPLATABLE_VALUE(float, blue)
  TEMPLATABLE_VALUE(float, white)

  void play(const Ts &...x) override {
    auto *out = (AddressableLight *) this->parent_->get_output();
    int32_t range_from = interpret_index(this->range_from_.value_or(x..., 0), out->size());
    if (range_from < 0 || range_from >= out->size())
      range_from = 0;

    int32_t range_to = interpret_index(this->range_to_.value_or(x..., out->size() - 1) + 1, out->size());
    if (range_to < 0 || range_to >= out->size())
      range_to = out->size();

    uint8_t color_brightness =
        to_uint8_scale(this->color_brightness_.value_or(x..., this->parent_->remote_values.get_color_brightness()));
    auto range = out->range(range_from, range_to);
    if (this->red_.has_value())
      range.set_red(esp_scale8(to_uint8_compat(this->red_.value(x...), "red"), color_brightness));
    if (this->green_.has_value())
      range.set_green(esp_scale8(to_uint8_compat(this->green_.value(x...), "green"), color_brightness));
    if (this->blue_.has_value())
      range.set_blue(esp_scale8(to_uint8_compat(this->blue_.value(x...), "blue"), color_brightness));
    if (this->white_.has_value())
      range.set_white(to_uint8_compat(this->white_.value(x...), "white"));
    out->schedule_show();
  }

 protected:
  LightState *parent_;

  // Historically, this action required uint8_t (0-255) for RGBW values from lambdas. Keep compatibility.
  static inline uint8_t to_uint8_compat(float value, const char *field) {
    if (value > 1.0f) {
      addressableset_warn_about_scale(field);
      return static_cast<uint8_t>(value);
    }
    return to_uint8_scale(value);
  }
};

}  // namespace esphome::light
