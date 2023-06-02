#pragma once
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/components/output/float_output.h"
#include "esphome/core/log.h"

namespace esphome {
namespace sigma_delta_output {

static const char *const TAG = "output.sigma_delta";

class SigmaDeltaOutput : public PollingComponent, public output::FloatOutput {
 public:
  Trigger<> *get_turn_on_trigger() {
    if (!this->turn_on_trigger_)
      this->turn_on_trigger_ = make_unique<Trigger<>>();
    return this->turn_on_trigger_.get();
  }
  Trigger<> *get_turn_off_trigger() {
    if (!this->turn_off_trigger_)
      this->turn_off_trigger_ = make_unique<Trigger<>>();
    return this->turn_off_trigger_.get();
  }

  Trigger<bool> *get_state_change_trigger() {
    if (!this->state_change_trigger_)
      this->state_change_trigger_ = make_unique<Trigger<bool>>();
    return this->state_change_trigger_.get();
  }

  void set_pin(GPIOPin *pin) { this->pin_ = pin; };
  void write_state(float state) override { this->state_ = state; }
  void setup() override {
    if (this->pin_)
      this->pin_->setup();
  }
  void dump_config() override {
    ESP_LOGCONFIG(TAG, "Sigma Delta Output:");
    LOG_PIN("  Pin: ", this->pin_);
    if (this->state_change_trigger_) {
      ESP_LOGCONFIG(TAG, "  State change automation configured");
    }
    if (this->turn_on_trigger_) {
      ESP_LOGCONFIG(TAG, "  Turn on automation configured");
    }
    if (this->turn_off_trigger_) {
      ESP_LOGCONFIG(TAG, "  Turn off automation configured");
    }
    LOG_UPDATE_INTERVAL(this);
    LOG_FLOAT_OUTPUT(this);
  }

  void update() override {
    this->accum_ += this->state_;
    const bool next_value = this->accum_ > 0;

    if (next_value) {
      this->accum_ -= 1.;
    }

    if (next_value != this->value_) {
      this->value_ = next_value;
      if (this->pin_) {
        this->pin_->digital_write(next_value);
      }

      if (this->state_change_trigger_) {
        this->state_change_trigger_->trigger(next_value);
      }

      if (next_value && this->turn_on_trigger_) {
        this->turn_on_trigger_->trigger();
      } else if (!next_value && this->turn_off_trigger_) {
        this->turn_off_trigger_->trigger();
      }
    }
  }

 protected:
  GPIOPin *pin_{nullptr};

  std::unique_ptr<Trigger<>> turn_on_trigger_{nullptr};
  std::unique_ptr<Trigger<>> turn_off_trigger_{nullptr};
  std::unique_ptr<Trigger<bool>> state_change_trigger_{nullptr};

  float accum_{0};
  float state_{0.};
  bool value_{false};
};
}  // namespace sigma_delta_output
}  // namespace esphome
