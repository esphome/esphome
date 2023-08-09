#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"

#ifdef USE_OUTPUT
#include "esphome/components/output/float_output.h"
#endif

#ifdef USE_SPEAKER
#include "esphome/components/speaker/speaker.h"
#endif

namespace esphome {
namespace rtttl {

#ifdef USE_SPEAKER
static const size_t SAMPLE_BUFFER_SIZE = 256;

struct SpeakerSample {
  int16_t left{0};
  int16_t right{0};
};
#endif

class Rtttl : public Component {
 public:
#ifdef USE_OUTPUT
  void set_output(output::FloatOutput *output) { output_ = output; }
#endif
#ifdef USE_SPEAKER
  void set_speaker(speaker::Speaker *speaker) { speaker_ = speaker; }
#endif
  void play(std::string rtttl);
  void stop() {
    note_duration_ = 0;
#ifdef USE_OUTPUT
    if (output_ != nullptr) {
      output_->set_level(0.0);
    }
#endif
#ifdef USE_SPEAKER
    if (this->speaker_ != nullptr) {
      if (this->speaker_->is_running()) {
        this->speaker_->stop();
      }
    }
#endif
  }
  void dump_config() override;

  bool is_playing() { return note_duration_ != 0; }
  void loop() override;

  void add_on_finished_playback_callback(std::function<void()> callback) {
    this->on_finished_playback_callback_.add(std::move(callback));
  }

 protected:
  inline uint8_t get_integer_() {
    uint8_t ret = 0;
    while (isdigit(rtttl_[position_])) {
      ret = (ret * 10) + (rtttl_[position_++] - '0');
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

#ifdef USE_OUTPUT
  output::FloatOutput *output_;
#endif

  void play_output_();

#ifdef USE_SPEAKER
  speaker::Speaker *speaker_;
  void play_speaker_();
  int sample_rate_{16000};
  int ttlSamplesPerWave_{0};
  int ttlSamplesSent_{0};
  int ttlSamples_{0};
  int ttGapFirst_{0};
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
