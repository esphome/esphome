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

enum State : uint8_t {
  STATE_STOPPED = 0,
  STATE_INIT,
  STATE_STARTING,
  STATE_RUNNING,
  STATE_STOPPING,
};

#ifdef USE_SPEAKER
static const size_t SAMPLE_BUFFER_SIZE = 2048;

struct SpeakerSample {
  int8_t left{0};
  int8_t right{0};
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
  float get_gain() { return gain_; }
  void set_gain(float gain) { this->gain_ = clamp(gain, 0.0f, 1.0f); }
  void play(std::string rtttl);
  void stop();
  void dump_config() override;

  bool is_playing() { return this->state_ != State::STATE_STOPPED; }
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
  /**
   * @brief Finalizes the playback of the RTTTL string.
   *
   * This method is called internally when the end of the RTTTL string is reached
   * or when a parsing error occurs. It stops the output, sets the component state,
   * and triggers the on_finished_playback_callback_.
   */
  void finish_();
  void set_state_(State state);

  /// The RTTTL string to play.
  std::string rtttl_{""};
  /// The current position in the RTTTL string.
  size_t position_{0};
  /// The duration of a whole note in milliseconds.
  uint16_t wholenote_;
  /// The default duration of a note (e.g. 4 for a quarter note).
  uint16_t default_duration_;
  /// The default octave for a note.
  uint16_t default_octave_;
  /// The time the last note was started.
  uint32_t last_note_;
  /// The duration of the current note in milliseconds.
  uint16_t note_duration_;

  /// The frequency of the current note in Hz.
  uint32_t output_freq_;
  /// The gain of the output.
  float gain_{0.6f};
  /// The current state of the RTTTL player.
  State state_{State::STATE_STOPPED};

#ifdef USE_OUTPUT
  /// The output to write the sound to.
  output::FloatOutput *output_;
#endif

#ifdef USE_SPEAKER
  /// The speaker to write the sound to.
  speaker::Speaker *speaker_{nullptr};
  /// The sample rate of the speaker.
  int sample_rate_{16000};
  /// The number of samples for one full cycle of a note's waveform, in Q10 fixed-point format.
  int samples_per_wave_{0};
  /// The number of samples sent.
  int samples_sent_{0};
  /// The total number of samples to send.
  int samples_count_{0};
  /// The number of samples for the gap between notes.
  int samples_gap_{0};

#endif

  /// The callback to call when playback is finished.
  CallbackManager<void()> on_finished_playback_callback_;
};

template<typename... Ts> class PlayAction : public Action<Ts...> {
 public:
  PlayAction(Rtttl *rtttl) : rtttl_(rtttl) {}
  TEMPLATABLE_VALUE(std::string, value)

  void play(const Ts &...x) override { this->rtttl_->play(this->value_.value(x...)); }

 protected:
  Rtttl *rtttl_;
};

template<typename... Ts> class StopAction : public Action<Ts...>, public Parented<Rtttl> {
 public:
  void play(const Ts &...x) override { this->parent_->stop(); }
};

template<typename... Ts> class IsPlayingCondition : public Condition<Ts...>, public Parented<Rtttl> {
 public:
  bool check(const Ts &...x) override { return this->parent_->is_playing(); }
};

class FinishedPlaybackTrigger : public Trigger<> {
 public:
  explicit FinishedPlaybackTrigger(Rtttl *parent) {
    parent->add_on_finished_playback_callback([this]() { this->trigger(); });
  }
};

}  // namespace rtttl
}  // namespace esphome
