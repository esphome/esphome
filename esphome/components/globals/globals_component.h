#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"

namespace esphome {
namespace globals {

template<typename T> class GlobalsComponent : public Component {
 public:
  using value_type = T;
  explicit GlobalsComponent() = default;
  explicit GlobalsComponent(T initial_value) : value_(initial_value) {}
  explicit GlobalsComponent(std::array<typename std::remove_extent<T>::type, std::extent<T>::value> initial_value) {
    memcpy(this->value_, initial_value.data(), sizeof(T));
  }

  T &value() { return this->value_; }

  void setup() override {
    if (this->restore_value_) {
      this->rtc_ = global_preferences.make_preference<T>(1944399030U ^ this->name_hash_);
      this->rtc_.load(&this->value_);
    }
    memcpy(&this->prev_value_, &this->value_, sizeof(T));
  }

  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  void loop() override {
    if (this->restore_value_) {
      int diff = memcmp(&this->value_, &this->prev_value_, sizeof(T));
      if (diff != 0) {
        this->rtc_.save(&this->value_);
        memcpy(&this->prev_value_, &this->value_, sizeof(T));
      }
    }
  }

  void set_restore_value(uint32_t name_hash) {
    this->restore_value_ = true;
    this->name_hash_ = name_hash;
  }

 protected:
  T value_{};
  T prev_value_{};
  bool restore_value_{false};
  uint32_t name_hash_{};
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

}  // namespace globals
}  // namespace esphome
