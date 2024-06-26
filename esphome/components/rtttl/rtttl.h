#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"

#ifdef USE_OUTPUT
#include "esphome/components/output/float_output.h"
#endif

#ifdef USE_SPEAKER
#include "esphome/components/speaker/speaker.h"
#endif

namespace esphome {
namespace rtttl {

#ifdef USE_SPEAKER
static const size_t SAMPLE_BUFFER_SIZE = 512;

struct SpeakerSample {
  int16_t left{0};
  int16_t right{0};
};
#endif

class Rtttl : public Component {
 public:
#ifdef USE_OUTPUT
  void set_output(output::FloatOutput *output) { this->output_ = output; }
#endif
#ifdef USE_SPEAKER
  void set_speaker(speaker::Speaker *speaker) { this->speaker_ = speaker; }
#endif
  void set_gain(float gain) {
    if (gain < 0.1f)
      gain = 0.1f;
    if (gain > 1.0f)
      gain = 1.0f;
    this->gain_ = gain;
  }
  void play(std::string rtttl);
  void stop();
  void dump_config() override;

  bool is_playing() { return this->note_duration_ != 0; }
  void loop() override;

  void add_on_finished_playback_callback(std::function<void()> callback) {
    this->on_finished_playback_callback_.add(std::move(callback));
  }

 protected:
  inline uint8_t get_integer_() {
    uint8_t ret = 0;
    while (isdigit(this->rtttl_[this->position_])) {
      ret = (ret * 10) + (this->rtttl_[this->position_++] - '0');
    }
    return ret;
  }

  std::string rtttl_{""};
  size_t position_{0};
  uint16_t wholenote_;
  uint16_t default_duration_;
  uint16_t default_octave_;
  uint32_t last_note_;
  uint16_t note_duration_;

  uint32_t output_freq_;
  float gain_{0.6f};

#ifdef USE_OUTPUT
  output::FloatOutput *output_;
#endif

  void play_output_();

#ifdef USE_SPEAKER
  speaker::Speaker *speaker_{nullptr};
  int sample_rate_{16000};
  int samples_per_wave_{0};
  int samples_sent_{0};
  int samples_count_{0};
  int samples_gap_{0};

#endif

  CallbackManager<void()> on_finished_playback_callback_;
};

template<typename... Ts> class PlayAction : public Action<Ts...> {
 public:
  PlayAction(Rtttl *rtttl) : rtttl_(rtttl) {}
  TEMPLATABLE_VALUE(std::string, value)

  void play(Ts... x) override { this->rtttl_->play(this->value_.value(x...)); }

 protected:
  Rtttl *rtttl_;
};

template<typename... Ts> class StopAction : public Action<Ts...>, public Parented<Rtttl> {
 public:
  void play(Ts... x) override { this->parent_->stop(); }
};

template<typename... Ts> class IsPlayingCondition : public Condition<Ts...>, public Parented<Rtttl> {
 public:
  bool check(Ts... x) override { return this->parent_->is_playing(); }
};

class FinishedPlaybackTrigger : public Trigger<> {
 public:
  explicit FinishedPlaybackTrigger(Rtttl *parent) {
    parent->add_on_finished_playback_callback([this]() { this->trigger(); });
  }
};

}  // namespace rtttl
}  // namespace esphome
