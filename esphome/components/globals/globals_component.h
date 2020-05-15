#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace globals {

template<typename T> class GlobalsComponent : public Component, public Nameable {
 public:
  using value_type = T;
  explicit GlobalsComponent() = default;
  explicit GlobalsComponent(std::array<typename std::remove_extent<T>::type, std::extent<T>::value> initial_value) {
    memcpy(this->value_, initial_value.data(), sizeof(T));
  }

  T &value() { return this->value_; }

  void setup() override {
    // TODO: check return bool?
    this->rtc_.load(&this->value_);
    memcpy(&this->prev_value_, &this->value_, sizeof(T));
  }

  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  void loop() override {
    int diff = memcmp(&this->value_, &this->prev_value_, sizeof(T));
    if (diff != 0) {
      this->rtc_.save(&this->value_);
      memcpy(&this->prev_value_, &this->value_, sizeof(T));
    }
  }

  uint32_t hash_base() override { return 1944399030U; }

  void set_preference(ESPPreferenceObject preference) { this->rtc_ = preference; }

 protected:
  T value_{};
  T prev_value_{};
  ESPPreferenceObject rtc_;
};

template<class C, typename... Ts> class GlobalVarSetAction : public Action<Ts...> {
 public:
  explicit GlobalVarSetAction(C *parent) : parent_(parent) {}

  using T = typename C::value_type;

  TEMPLATABLE_VALUE(T, value);

  void play(Ts... x) override { this->parent_->value() = this->value_.value(x...); }

 protected:
  C *parent_;
};

template<typename T> T &id(GlobalsComponent<T> *value) { return value->value(); }

}  // namespace globals
}  // namespace esphome
