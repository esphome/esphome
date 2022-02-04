#include "rtttl.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace rtttl {

static const char *const TAG = "rtttl";

static const uint32_t DOUBLE_NOTE_GAP_MS = 10;

// These values can also be found as constants in the Tone library (Tone.h)
static const uint16_t NOTES[] = {0,    262,  277,  294,  311,  330,  349,  370,  392,  415,  440,  466,  494,
                                 523,  554,  587,  622,  659,  698,  740,  784,  831,  880,  932,  988,  1047,
                                 1109, 1175, 1245, 1319, 1397, 1480, 1568, 1661, 1760, 1865, 1976, 2093, 2217,
                                 2349, 2489, 2637, 2794, 2960, 3136, 3322, 3520, 3729, 3951};

void Rtttl::dump_config() { ESP_LOGCONFIG(TAG, "Rtttl"); }

void Rtttl::play(std::string rtttl) {
  rtttl_ = std::move(rtttl);

  default_duration_ = 4;
  default_octave_ = 6;
  int bpm = 63;
  uint8_t num;

  // Get name
  position_ = rtttl_.find(':');

  // it's somewhat documented to be up to 10 characters but let's be a bit flexible here
  if (position_ == std::string::npos || position_ > 15) {
    ESP_LOGE(TAG, "Missing ':' when looking for name.");
    return;
  }

  auto name = this->rtttl_.substr(0, position_);
  ESP_LOGD(TAG, "Playing song %s", name.c_str());

  // get default duration
  position_ = this->rtttl_.find("d=", position_);
  if (position_ == std::string::npos) {
    ESP_LOGE(TAG, "Missing 'd='");
    return;
  }
  position_ += 2;
  num = this->get_integer_();
  if (num > 0)
    default_duration_ = num;

  // get default octave
  position_ = rtttl_.find("o=", position_);
  if (position_ == std::string::npos) {
    ESP_LOGE(TAG, "Missing 'o=");
    return;
  }
  position_ += 2;
  num = get_integer_();
  if (num >= 3 && num <= 7)
    default_octave_ = num;

  // get BPM
  position_ = rtttl_.find("b=", position_);
  if (position_ == std::string::npos) {
    ESP_LOGE(TAG, "Missing b=");
    return;
  }
  position_ += 2;
  num = get_integer_();
  if (num != 0)
    bpm = num;

  position_ = rtttl_.find(':', position_);
  if (position_ == std::string::npos) {
    ESP_LOGE(TAG, "Missing second ':'");
    return;
  }
  position_++;

  // BPM usually expresses the number of quarter notes per minute
  wholenote_ = 60 * 1000L * 4 / bpm;  // this is the time for whole note (in milliseconds)

  output_freq_ = 0;
  last_note_ = millis();
  note_duration_ = 1;
}

void Rtttl::loop() {
  if (note_duration_ == 0 || millis() - last_note_ < note_duration_)
    return;

  if (!rtttl_[position_]) {
    output_->set_level(0.0);
    ESP_LOGD(TAG, "Playback finished");
    this->on_finished_playback_callback_.call();
    note_duration_ = 0;
    return;
  }

  // align to note: most rtttl's out there does not add and space after the ',' separator but just in case...
  while (rtttl_[position_] == ',' || rtttl_[position_] == ' ')
    position_++;

  // first, get note duration, if available
  uint8_t num = this->get_integer_();

  if (num) {
    note_duration_ = wholenote_ / num;
  } else {
    note_duration_ = wholenote_ / default_duration_;  // we will need to check if we are a dotted note after
  }

  uint8_t note;

  switch (rtttl_[position_]) {
    case 'c':
      note = 1;
      break;
    case 'd':
      note = 3;
      break;
    case 'e':
      note = 5;
      break;
    case 'f':
      note = 6;
      break;
    case 'g':
      note = 8;
      break;
    case 'a':
      note = 10;
      break;
    case 'b':
      note = 12;
      break;
    case 'p':
    default:
      note = 0;
  }
  position_++;

  // now, get optional '#' sharp
  if (rtttl_[position_] == '#') {
    note++;
    position_++;
  }

  // now, get optional '.' dotted note
  if (rtttl_[position_] == '.') {
    note_duration_ += note_duration_ / 2;
    position_++;
  }

  // now, get scale
  uint8_t scale = get_integer_();
  if (scale == 0)
    scale = default_octave_;

  // Now play the note
  if (note) {
    auto note_index = (scale - 4) * 12 + note;
    if (note_index < 0 || note_index >= (int) sizeof(NOTES)) {
      ESP_LOGE(TAG, "Note out of valid range");
      return;
    }
    auto freq = NOTES[note_index];

    if (freq == output_freq_) {
      // Add small silence gap between same note
      output_->set_level(0.0);
      delay(DOUBLE_NOTE_GAP_MS);
      note_duration_ -= DOUBLE_NOTE_GAP_MS;
    }
    output_freq_ = freq;

    ESP_LOGVV(TAG, "playing note: %d for %dms", note, note_duration_);
    output_->update_frequency(freq);
    output_->set_level(0.5);
  } else {
    ESP_LOGVV(TAG, "waiting: %dms", note_duration_);
    output_->set_level(0.0);
  }

  last_note_ = millis();
}
}  // namespace rtttl
}  // namespace esphome
