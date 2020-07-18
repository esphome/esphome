#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"
#include "esphome/components/output/float_output.h"

namespace esphome {
namespace rtttl {

extern uint32_t global_rtttl_id;

class Rtttl : public Component {
 public:
  void set_output(output::FloatOutput *output) { output_ = output; }
  void play(std::string rtttl);

  void setup() override {}
  //   void dump_config() override;

  void loop() override;

 protected:
  std::string rtttl_;
  std::string::const_iterator p_{};
  uint32_t duration_;
  uint32_t wholenote_;
  uint32_t default_dur_;
  uint32_t default_oct_;

  uint32_t next_tone_play_{0};

  bool note_playing_;

  output::FloatOutput *output_;
};

template<typename... Ts> class RtttlPlayAction : public Action<Ts...> {
 public:
  RtttlPlayAction(Rtttl *rtttl) : rtttl_(rtttl) {}
  TEMPLATABLE_VALUE(std::string, value)

  void play(Ts... x) override { this->rtttl_->play(this->value_.value(x...)); }

 protected:
  Rtttl *rtttl_;
};

}  // namespace rtttl
}  // namespace esphome
