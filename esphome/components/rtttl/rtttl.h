#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/progmem.h"

#ifdef USE_OUTPUT
#include "esphome/components/output/float_output.h"
#endif  // USE_OUTPUT

#ifdef USE_SPEAKER
#include "esphome/components/speaker/speaker.h"
#endif  // USE_SPEAKER

namespace esphome::rtttl {

namespace testing {
class RtttlParserAccess;
}  // namespace testing

inline constexpr uint8_t DEFAULT_NOTE_DENOMINATOR = 4;  // Default note-denominator (quarter note)
inline constexpr uint8_t DEFAULT_OCTAVE =
    6;  // Default octave for a note (see: `MIN_OCTAVE`, `MAX_OCTAVE` in `rtttl.cpp`)

enum class State : uint8_t {
  STOPPED = 0,
  INIT,
  STARTING,
  RUNNING,
  STOPPING,
};

/// A song in storage that outlives playback, such as a literal from generated code; PROGMEM on ESP8266.
struct StaticSong {
  ProgmemStr rtttl;
};

class Rtttl final : public Component {
 public:
#ifdef USE_OUTPUT
  void set_output(output::FloatOutput *output) { this->output_ = output; }
#endif  // USE_OUTPUT

#ifdef USE_SPEAKER
  void set_speaker(speaker::Speaker *speaker) { this->speaker_ = speaker; }
#endif  // USE_SPEAKER

  void dump_config() override;
  void loop() override;
  /// Play a song; the string is copied so it may come from anywhere.
  void play(std::string rtttl);
  /// Play a song from static storage without copying it.
  void play(StaticSong song);
  void stop();

  float get_gain() { return this->gain_; }
  void set_gain(float gain) { this->gain_ = clamp(gain, 0.0f, 1.0f); }

  bool is_playing() { return this->state_ != State::STOPPED; }

#ifdef USE_RTTTL_FINISHED_PLAYBACK_CALLBACK
  template<typename F> void add_on_finished_playback_callback(F &&callback) {
    this->on_finished_playback_callback_.add(std::forward<F>(callback));
  }
#endif

 protected:
  inline uint16_t get_integer_() {
    uint16_t ret = 0;
    while (isdigit(this->song_at_(this->position_))) {
      ret = (ret * 10) + (this->song_at_(this->position_++) - '0');
    }
    return ret;
  }
  /// The byte of the song at pos, or NUL past its end; the read is flash safe for a PROGMEM song on ESP8266.
  char song_at_(size_t pos) const {
    if (pos >= this->song_len_)
      return '\0';
    return static_cast<char>(progmem_read_byte(reinterpret_cast<const uint8_t *>(this->song_) + pos));
  }
  /// Index of the first `c` in [from, end), or `end` when absent.
  size_t song_find_(char c, size_t from, size_t end) const;
  /// Index of `key` followed by '=' in [from, end), or `end` when absent.
  size_t song_find_control_(char key, size_t from, size_t end) const;
  /// Copies the first name_len bytes of the song, its name, into buf for logging.
  void song_name_(char *buf, size_t size, size_t name_len) const;
  /// Points the view at a song, clamped to what song_len_ can hold.
  void set_song_(const char *song, size_t len);
  /// Parses the header and starts playback of the song the view points at.
  void start_();
  /// Logs and returns true when a song is still playing, so a new one must wait.
  bool is_busy_() const;
  /**
   * @brief Finalizes the playback of the RTTTL string.
   *
   * This method is called internally when the end of the RTTTL string is reached
   * or when a parsing error occurs. It stops the output, sets the component state,
   * and triggers the on_finished_playback_callback_.
   */
  void finish_();
  void set_state_(State state);

  /// The song being played; points at owned_song_ or at static storage.
  const char *song_{nullptr};
  /// Backing store for play(std::string).
  std::string owned_song_;
  /// The time in milliseconds since microcontroller boot when the last note was started.
  uint32_t last_note_start_time_;
  /// The frequency of the current note in Hz.
  uint32_t output_freq_{0};
  /// The gain of the output.
  float gain_{0.6f};
  /// Length of the song in bytes; songs are a few hundred bytes, so 16 bits is ample.
  uint16_t song_len_{0};
  /// The current position in the song.
  uint16_t position_{0};
  /// The duration of the current note in milliseconds.
  uint16_t note_duration_{0};
  /// The duration of a whole note in milliseconds.
  uint16_t wholenote_duration_;
  /// The default duration of a note (e.g. 4 for a quarter note).
  uint8_t default_note_denominator_{DEFAULT_NOTE_DENOMINATOR};
  /// The default octave for a note.
  uint8_t default_octave_{DEFAULT_OCTAVE};
  /// The current state of the RTTTL player.
  State state_{State::STOPPED};

#ifdef USE_OUTPUT
  /// The output to write the sound to.
  output::FloatOutput *output_{nullptr};
#endif  // USE_OUTPUT

#ifdef USE_SPEAKER
  /// The speaker to write the sound to.
  speaker::Speaker *speaker_{nullptr};
  /// The number of samples for one full cycle of a note's waveform, in Q10 fixed-point format.
  uint32_t samples_per_wave_{0};
  /// The number of samples sent.
  uint32_t samples_sent_{0};
  /// The total number of samples to send.
  uint32_t samples_count_{0};
  /// The number of samples for the gap between notes.
  uint32_t samples_gap_{0};
#endif  // USE_SPEAKER

#ifdef USE_RTTTL_FINISHED_PLAYBACK_CALLBACK
  /// The callback to call when playback is finished.
  CallbackManager<void()> on_finished_playback_callback_;
#endif

  friend class testing::RtttlParserAccess;
};

}  // namespace esphome::rtttl
