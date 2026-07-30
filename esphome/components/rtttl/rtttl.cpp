#include "rtttl.h"
#include <cmath>
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"

namespace esphome::rtttl {

static const char *const TAG = "rtttl";

static constexpr uint8_t SONG_NAME_LENGTH_LIMIT = 64;
static constexpr uint8_t SEMITONES_IN_OCTAVE = 12;

static constexpr uint8_t MIN_OCTAVE = 4;
static constexpr uint8_t MAX_OCTAVE = 7;

static constexpr uint8_t DEFAULT_BPM = 63;  // Default beats per minute

// These values can also be found as constants in the Tone library (Tone.h)
static constexpr uint16_t NOTES[] = {0,    262,  277,  294,  311,  330,  349,  370,  392,  415,  440,  466,  494,
                                     523,  554,  587,  622,  659,  698,  740,  784,  831,  880,  932,  988,  1047,
                                     1109, 1175, 1245, 1319, 1397, 1480, 1568, 1661, 1760, 1865, 1976, 2093, 2217,
                                     2349, 2489, 2637, 2794, 2960, 3136, 3322, 3520, 3729, 3951};
static constexpr uint8_t NOTES_COUNT = static_cast<uint8_t>(sizeof(NOTES) / sizeof(NOTES[0]));

static constexpr uint8_t REPEATING_NOTE_GAP_MS = 10;

#ifdef USE_SPEAKER
static constexpr uint16_t SAMPLE_BUFFER_SIZE = 2048;
static constexpr uint16_t SAMPLE_RATE = 16000;

inline double deg2rad(double degrees) {
  static constexpr double PI_ON_180 = M_PI / 180.0;
  return degrees * PI_ON_180;
}
#endif  // USE_SPEAKER

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
// RTTTL state strings indexed by State enum (0-4): STOPPED, INIT, STARTING, RUNNING, STOPPING, plus UNKNOWN fallback
PROGMEM_STRING_TABLE(RtttlStateStrings, "State::STOPPED", "State::INIT", "State::STARTING", "State::RUNNING",
                     "State::STOPPING", "UNKNOWN");

static const LogString *state_to_string(State state) {
  return RtttlStateStrings::get_log_str(static_cast<uint8_t>(state), RtttlStateStrings::LAST_INDEX);
}
#endif  // ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE

static uint8_t note_index_from_char(char note) {
  switch (note) {
    case 'c':
      return 1;
    // 'c#': 2
    case 'd':
      return 3;
    // 'd#': 4
    case 'e':
      return 5;
    case 'f':
      return 6;
    // 'f#': 7
    case 'g':
      return 8;
    // 'g#': 9
    case 'a':
      return 10;
    // 'a#': 11
    // Support both 'b' (English notation for B natural) and 'h' (German notation for B natural)
    case 'b':
    case 'h':
      return 12;
    case 'p':
    default:
      return 0;
  }
}

void Rtttl::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Rtttl:\n"
                "  Gain: %f",
                this->gain_);
}

void Rtttl::loop() {
  if (this->state_ == State::STOPPED) {
    this->disable_loop();
    return;
  }

#ifdef USE_OUTPUT
  if (this->output_ != nullptr && millis() - this->last_note_start_time_ < this->note_duration_) {
    return;
  }
#endif  // USE_OUTPUT

#ifdef USE_SPEAKER
  if (this->speaker_ != nullptr) {
    if (this->state_ == State::STOPPING) {
      if (this->speaker_->is_stopped()) {
        this->set_state_(State::STOPPED);
      } else {
        return;
      }
    } else if (this->state_ == State::INIT) {
      if (this->speaker_->is_stopped()) {
        audio::AudioStreamInfo audio_stream_info = audio::AudioStreamInfo(16, 1, SAMPLE_RATE);
        this->speaker_->set_audio_stream_info(audio_stream_info);
        this->speaker_->set_volume(this->gain_);
        this->speaker_->start();
        this->set_state_(State::STARTING);
      }
    } else if (this->state_ == State::STARTING) {
      if (this->speaker_->is_running()) {
        this->set_state_(State::RUNNING);
      }
    }
    if (!this->speaker_->is_running()) {
      return;
    }
    if (this->samples_sent_ != this->samples_count_) {
      int16_t sample[SAMPLE_BUFFER_SIZE];
      uint16_t sample_index = 0;
      double rem = 0.0;

      while (sample_index < SAMPLE_BUFFER_SIZE && this->samples_sent_ < this->samples_count_) {
        // Try and send out the remainder of the existing note, one per `loop()`
        if (this->samples_per_wave_ != 0 && this->samples_sent_ >= this->samples_gap_) {  // Play note
          rem = ((this->samples_sent_ << 10) % this->samples_per_wave_) * (360.0 / this->samples_per_wave_);
          sample[sample_index] = INT16_MAX * sin(deg2rad(rem));
        } else {
          sample[sample_index] = 0;
        }
        this->samples_sent_++;
        sample_index++;
      }
      if (sample_index > 0) {
        size_t bytes = sample_index * sizeof(int16_t);
        size_t sent_bytes = this->speaker_->play((uint8_t *) (&sample), bytes);
        size_t samples_sent = sent_bytes / sizeof(int16_t);
        if (samples_sent != sample_index) {
          this->samples_sent_ -= (sample_index - samples_sent);
        }
        return;
      }
    }
  }
#endif  // USE_SPEAKER

  // Align to note: most rtttl's out there does not add any space after the ',' separator but just in case
  while (this->position_ < this->rtttl_.length()) {
    char c = this->rtttl_[this->position_];
    if (c != ',' && c != ' ')
      break;
    this->position_++;
  }

  if (this->position_ >= this->rtttl_.length()) {
    this->finish_();
    return;
  }

  // First, get note duration, if available
  uint8_t note_denominator = this->get_integer_();

  if (note_denominator) {
    this->note_duration_ = this->wholenote_duration_ / note_denominator;
  } else {
    // We will need to check if we are a dotted note after
    this->note_duration_ = this->wholenote_duration_ / this->default_note_denominator_;
  }

  uint8_t note_index_in_octave = note_index_from_char(this->rtttl_[this->position_]);

  this->position_++;

  // Now, get optional '#' sharp
  if (this->rtttl_[this->position_] == '#') {
    note_index_in_octave++;
    this->position_++;
  }

  // Now, get scale
  uint8_t scale = this->get_integer_();
  if (scale == 0) {
    scale = this->default_octave_;
  }

  if (scale < MIN_OCTAVE || scale > MAX_OCTAVE) {
    ESP_LOGE(TAG, "Octave must be between %d and %d (it is %d)", MIN_OCTAVE, MAX_OCTAVE, scale);
    this->finish_();
    return;
  }

  // Now, get optional '.' dotted note
  if (this->rtttl_[this->position_] == '.') {
    this->note_duration_ += this->note_duration_ / 2;  // Duration +50%
    this->position_++;
  }

  bool need_note_gap = false;

  // Now play the note
  if (note_index_in_octave == 0) {
    this->output_freq_ = 0;
    ESP_LOGVV(TAG, "Waiting: %dms", this->note_duration_);
  } else {
    uint8_t note_index = (scale - MIN_OCTAVE) * SEMITONES_IN_OCTAVE + note_index_in_octave;
    if (note_index >= NOTES_COUNT) {
      ESP_LOGE(TAG, "Note out of range (note: %d, scale: %d, index: %d, max: %d)", note_index_in_octave, scale,
               note_index, NOTES_COUNT);
      this->finish_();
      return;
    }
    uint16_t freq = NOTES[note_index];
    need_note_gap = freq == this->output_freq_;

    // Add small silence gap between same note
    this->output_freq_ = freq;

    ESP_LOGVV(TAG, "Playing note: %d for %dms", note_index_in_octave, this->note_duration_);
  }

#ifdef USE_OUTPUT
  if (this->output_ != nullptr) {
    if (this->output_freq_ == 0) {
      this->output_->set_level(0.0);
    } else {
      if (need_note_gap && this->note_duration_ > REPEATING_NOTE_GAP_MS) {
        this->output_->set_level(0.0);
        delay(REPEATING_NOTE_GAP_MS);
        this->note_duration_ -= REPEATING_NOTE_GAP_MS;
      }
      this->output_->update_frequency(this->output_freq_);
      this->output_->set_level(this->gain_);
    }
  }
#endif  // USE_OUTPUT

#ifdef USE_SPEAKER
  if (this->speaker_ != nullptr) {
    this->samples_sent_ = 0;
    this->samples_gap_ = 0;
    this->samples_per_wave_ = 0;
    this->samples_count_ = (SAMPLE_RATE * this->note_duration_) / 1000;
    if (need_note_gap) {
      this->samples_gap_ = (SAMPLE_RATE * REPEATING_NOTE_GAP_MS) / 1000;
    }
    if (this->output_freq_ != 0) {
      // Make sure there is enough samples to add a full last sinus.
      uint32_t samples_wish = this->samples_count_;
      this->samples_per_wave_ = (SAMPLE_RATE << 10) / this->output_freq_;

      uint16_t division = ((this->samples_count_ << 10) / this->samples_per_wave_) + 1;

      this->samples_count_ = (division * this->samples_per_wave_) >> 10;
      ESP_LOGVV(TAG, "Calc play time: wish: %" PRIu32 " gets: %" PRIu32 " (div: %d spw: %" PRIu32 ")", samples_wish,
                this->samples_count_, division, this->samples_per_wave_);
    }
    // Convert from frequency in Hz to high and low samples in fixed point
  }
#endif  // USE_SPEAKER

  this->last_note_start_time_ = millis();
}

void Rtttl::play(std::string rtttl) {
  if (this->state_ != State::STOPPED && this->state_ != State::STOPPING) {
    size_t pos = this->rtttl_.find(':');
    size_t len = (pos != std::string::npos) ? pos : this->rtttl_.length();
    ESP_LOGW(TAG, "Already playing: %.*s", (int) len, this->rtttl_.c_str());
    return;
  }

  this->rtttl_ = std::move(rtttl);

  this->default_note_denominator_ = DEFAULT_NOTE_DENOMINATOR;
  this->default_octave_ = DEFAULT_OCTAVE;
  this->note_duration_ = 0;

  uint16_t bpm = DEFAULT_BPM;
  uint16_t num;  // Used for: default note-denominator, default octave, BPM

  // Get name
  this->position_ = this->rtttl_.find(':');

  if (this->position_ == std::string::npos) {
    ESP_LOGE(TAG, "Unable to determine name; missing ':'");
    return;
  }
  if (this->position_ >= SONG_NAME_LENGTH_LIMIT) {
    ESP_LOGE(TAG, "Name is too long: length=%u, limit=%u", static_cast<unsigned>(this->position_),
             static_cast<unsigned>(SONG_NAME_LENGTH_LIMIT));
    return;
  }
  ESP_LOGD(TAG, "Playing song %.*s", (int) this->position_, this->rtttl_.c_str());

  size_t name_end_position = this->position_;
  size_t control_end = this->rtttl_.find(':', name_end_position + 1);
  if (control_end == std::string::npos) {
    ESP_LOGE(TAG, "Missing second ':'");
    return;
  }

  // Get default duration
  size_t pos = this->rtttl_.find("d=", name_end_position);
  if (pos == std::string::npos || pos >= control_end) {
    ESP_LOGW(TAG, "Missing 'd='; use default duration %d", this->default_note_denominator_);
  } else {
    this->position_ = pos + 2;
    num = this->get_integer_();
    if (num == 1 || num == 2 || num == 4 || num == 8 || num == 16 || num == 32) {
      this->default_note_denominator_ = num;
    } else {
      ESP_LOGE(TAG, "Invalid default duration: %d", num);
      return;
    }
  }

  // Get default octave
  pos = this->rtttl_.find("o=", name_end_position);
  if (pos == std::string::npos || pos >= control_end) {
    ESP_LOGW(TAG, "Missing 'o='; use default octave %d", this->default_octave_);
  } else {
    this->position_ = pos + 2;
    num = this->get_integer_();
    if (num >= MIN_OCTAVE && num <= MAX_OCTAVE) {
      this->default_octave_ = num;
    } else {
      ESP_LOGE(TAG, "Invalid default octave: %d", num);
      return;
    }
  }

  // Get BPM
  pos = this->rtttl_.find("b=", name_end_position);
  if (pos == std::string::npos || pos >= control_end) {
    ESP_LOGW(TAG, "Missing 'b='; use default BPM %d", bpm);
  } else {
    this->position_ = pos + 2;
    num = this->get_integer_();
    if (num >= 4) {  // Below 4 is not realistic and would cause a integer overflow
      bpm = num;
    } else {
      ESP_LOGE(TAG, "Invalid BPM: %d", num);
      return;
    }
  }

  this->position_ = control_end + 1;

  // BPM usually expresses the number of quarter notes per minute
  this->wholenote_duration_ = 60 * 1000L * 4 / bpm;  // This is the time for whole note (in milliseconds)

  this->output_freq_ = 0;
  this->last_note_start_time_ = millis();
  this->note_duration_ = 1;

#ifdef USE_OUTPUT
  if (this->output_ != nullptr) {
    this->set_state_(State::RUNNING);
  }
#endif  // USE_OUTPUT

#ifdef USE_SPEAKER
  if (this->speaker_ != nullptr) {
    this->set_state_(State::INIT);
    this->samples_sent_ = 0;
    this->samples_count_ = 0;
  }
#endif  // USE_SPEAKER
}

void Rtttl::stop() {
#ifdef USE_OUTPUT
  if (this->output_ != nullptr) {
    this->output_->set_level(0.0);
    this->set_state_(State::STOPPED);
  }
#endif  // USE_OUTPUT

#ifdef USE_SPEAKER
  if (this->speaker_ != nullptr) {
    if (this->speaker_->is_running()) {
      this->speaker_->stop();
    }
    this->set_state_(State::STOPPING);
  }
#endif  // USE_SPEAKER

  this->position_ = this->rtttl_.length();
  this->note_duration_ = 0;
}

void Rtttl::finish_() {
  ESP_LOGV(TAG, "Rtttl::finish_()");

#ifdef USE_OUTPUT
  if (this->output_ != nullptr) {
    this->output_->set_level(0.0);
    this->set_state_(State::STOPPED);
  }
#endif  // USE_OUTPUT

#ifdef USE_SPEAKER
  if (this->speaker_ != nullptr) {
    int16_t sample[2] = {0, 0};
    this->speaker_->play((uint8_t *) (&sample), sizeof(sample));
    this->speaker_->finish();
    this->set_state_(State::STOPPING);
  }
#endif  // USE_SPEAKER

  // Ensure no more notes are played in case finish_() is called for an error.
  this->position_ = this->rtttl_.length();
  this->note_duration_ = 0;
}

void Rtttl::set_state_(State state) {
  State old_state = this->state_;
  this->state_ = state;
  ESP_LOGV(TAG, "State changed from %s to %s", LOG_STR_ARG(state_to_string(old_state)),
           LOG_STR_ARG(state_to_string(state)));

  // Clear loop_done when transitioning from `State::STOPPED` to any other state
  if (state == State::STOPPED) {
    this->disable_loop();
#ifdef USE_RTTTL_FINISHED_PLAYBACK_CALLBACK
    this->on_finished_playback_callback_.call();
#endif
    ESP_LOGD(TAG, "Playback finished");
  } else if (old_state == State::STOPPED) {
    this->enable_loop();
  }
}

}  // namespace esphome::rtttl
