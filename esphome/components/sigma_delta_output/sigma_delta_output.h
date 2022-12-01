#pragma once
#include "esphome/core/component.h"
#include "esphome/components/output/float_output.h"

namespace esphome {
namespace sigma_delta_output {
class SigmaDeltaOutput : public PollingComponent, public output::FloatOutput {
 public:
  void set_output(output::BinaryOutput *output) { this->output_ = output; }
  void write_state(float state) override { this->state_ = state; }
  void update() override {
    this->accum_ += this->state_;
    if (this->output_) {
      this->output_->set_state(this->accum_ > 0);
    }
    if (this->accum_ > 0) {
      this->accum_ -= 1.;
    }
  }

 protected:
  output::BinaryOutput *output_{nullptr};
  float accum_{0};
  float state_{0.};
};
}  // namespace sigma_delta_output
}  // namespace esphome
