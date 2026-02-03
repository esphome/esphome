#pragma once

#include "esphome/components/select/select.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "esphome/core/string_ref.h"
#include "esphome/core/template_lambda.h"

namespace esphome::template_ {
static const char *const TAG = "template.select";
struct Empty {};
class BaseTemplateSelect : public select::Select, public PollingComponent {};

void dump_config_helper(BaseTemplateSelect *this_, bool optimistic, bool has_lambda, size_t initial_option_index,
                        bool restore_value);

/// Base template select class - used when no set_action is configured

template<bool HAS_LAMBDA, bool OPTIMISTIC, bool RESTORE_VALUE, size_t INITIAL_OPTION_INDEX>
class TemplateSelect : public BaseTemplateSelect {
 public:
  template<typename F> void set_lambda(F &&f) {
    if constexpr (HAS_LAMBDA) {
      this->f_.set(std::forward<F>(f));
    }
  }

  void setup() override {
    if constexpr (!HAS_LAMBDA) {
      size_t index = INITIAL_OPTION_INDEX;
      if constexpr (RESTORE_VALUE) {
        this->pref_ = this->template make_entity_preference<size_t>();
        if (this->pref_.load(&index) && this->has_index(index)) {
          esph_log_d(TAG, "State from restore: %s", this->option_at(index));
        } else {
          index = INITIAL_OPTION_INDEX;
          esph_log_d(TAG, "State from initial (no valid stored index): %s", this->option_at(INITIAL_OPTION_INDEX));
        }
      } else {
        esph_log_d(TAG, "State from initial: %s", this->option_at(INITIAL_OPTION_INDEX));
      }
      this->publish_state(index);
    }
  }

  void update() override {
    if constexpr (HAS_LAMBDA) {
      auto val = this->f_();
      if (val.has_value()) {
        if (!this->has_option(*val)) {
          esph_log_e(TAG, "Lambda returned an invalid option: %s", (*val).c_str());
          return;
        }
        this->publish_state(*val);
      }
    }
  }
  void dump_config() override {
    dump_config_helper(this, OPTIMISTIC, HAS_LAMBDA, INITIAL_OPTION_INDEX, RESTORE_VALUE);
  };
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

 protected:
  void control(size_t index) override {
    if constexpr (OPTIMISTIC)
      this->publish_state(index);
    if constexpr (RESTORE_VALUE)
      this->pref_.save(&index);
  }
  [[no_unique_address]] std::conditional_t<HAS_LAMBDA, TemplateLambda<std::string>, Empty> f_{};
  [[no_unique_address]] std::conditional_t<RESTORE_VALUE, ESPPreferenceObject, Empty> pref_{};
};

/// Template select with set_action trigger - only instantiated when set_action is configured
template<bool HAS_LAMBDA, bool OPTIMISTIC, bool RESTORE_VALUE, size_t INITIAL_OPTION_INDEX>
class TemplateSelectWithSetAction final
    : public TemplateSelect<HAS_LAMBDA, OPTIMISTIC, RESTORE_VALUE, INITIAL_OPTION_INDEX> {
 public:
  Trigger<StringRef> *get_set_trigger() { return &this->set_trigger_; }

 protected:
  void control(size_t index) override {
    this->set_trigger_.trigger(StringRef(this->option_at(index)));
    TemplateSelect<HAS_LAMBDA, OPTIMISTIC, RESTORE_VALUE, INITIAL_OPTION_INDEX>::control(index);
  }
  Trigger<StringRef> set_trigger_;
};

}  // namespace esphome::template_
