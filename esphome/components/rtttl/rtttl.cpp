#include "rtttl.h"
#include "esphome/core/log.h"

namespace esphome {
namespace rtttl {

static const char* TAG = "rtttl";

// These values can also be found as constants in the Tone library (Tone.h)
static const uint16_t NOTES[] = {0,    262,  277,  294,  311,  330,  349,  370,  392,  415,  440,  466,  494,
                                 523,  554,  587,  622,  659,  698,  740,  784,  831,  880,  932,  988,  1047,
                                 1109, 1175, 1245, 1319, 1397, 1480, 1568, 1661, 1760, 1865, 1976, 2093, 2217,
                                 2349, 2489, 2637, 2794, 2960, 3136, 3322, 3520, 3729, 3951};

#define SKIP_CHAR_OR_SPACE(p, char) \
  while (*p == char || *p == ' ') \
    p++;
#define GET_INTEGER(p, var) \
  var = 0; \
  while (isdigit(*p)) { \
    var = (var * 10) + (*p++ - '0'); \
  }

void Rtttl::dump_config() { ESP_LOGCONFIG(TAG, "Rtttl"); }

void Rtttl::play(std::string rtttl) {
  // Absolutely no error checking in here
  this->rtttl_ = rtttl;

  ESP_LOGD(TAG, "Playing song %s", rtttl_.c_str());

  p_ = rtttl_.cbegin();

  this->default_duration_ = 4;
  this->default_octave_ = 6;
  int bpm = 63;
  int num;

  // format: d=N,o=N,b=NNN:
  // find the start (skip name, etc)

  // ignore name
  while (*p_ != ':')
    p_++;

  SKIP_CHAR_OR_SPACE(p_, ':')

  // get default duration
  if (*p_ == 'd') {
    p_++;
    p_++;  // skip "d="
    GET_INTEGER(p_, num)
    if (num > 0)
      default_duration_ = num;
    SKIP_CHAR_OR_SPACE(p_, ',')
  }

  // get default octave
  if (*p_ == 'o') {
    p_++;
    p_++;  // skip "o="
    GET_INTEGER(p_, num)
    if (num >= 3 && num <= 7)
      default_octave_ = num;
    SKIP_CHAR_OR_SPACE(p_, ',')
  }

  // get BPM
  if (*p_ == 'b') {
    p_++;
    p_++;  // skip "b="
    GET_INTEGER(p_, num)
    if (num != 0)
      bpm = num;
    SKIP_CHAR_OR_SPACE(p_, ':')
  }

  // BPM usually expresses the number of quarter notes per minute
  this->wholenote_ = (60 * 1000L / bpm) * 4;  // this is the time for whole note (in milliseconds)

  output_freq_ = 0;
  note_playing_ = false;
  next_tone_play_ = millis();
}

void Rtttl::loop() {
  if (next_tone_play_ == 0 || millis() < next_tone_play_)
    return;

  if (p_ == rtttl_.cend()) {
    output_->set_level(0.0);
    ESP_LOGD(TAG, "Playback finished");
    this->on_finished_playback_callback_.call();
    next_tone_play_ = 0;
    return;
  }

  // first, get note duration, if available
  uint8_t num;
  GET_INTEGER(p_, num)

  uint32_t duration;

  if (num)
    duration = wholenote_ / num;
  else
    duration = wholenote_ / default_duration_;  // we will need to check if we are a dotted note after

  // now get the note
  uint8_t note;

  switch (*p_) {
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
  p_++;

  // now, get optional '#' sharp
  if (*p_ == '#') {
    note++;
    p_++;
  }

  // now, get optional '.' dotted note
  if (*p_ == '.') {
    duration += duration / 2;
    p_++;
  }

  // now, get scale
  uint8_t scale;
  GET_INTEGER(p_, scale)
  if (!scale)
    scale = default_octave_;

  // skip comma for next note (or we may be at the end)
  SKIP_CHAR_OR_SPACE(p_, ',')

  // Now play the note
  if (note) {
    auto freq = NOTES[(scale - 4) * 12 + note];

    if (note_playing_ && freq == output_freq_) {
      // Add small silence gap between same note
      output_->set_level(0.0);
      delay(10);
    }
    output_freq_ = freq;

    ESP_LOGVV(TAG, "playing note: %d for %dms", note, duration);
    output_->update_frequency(freq);
    output_->set_level(0.5);
    note_playing_ = true;
  } else {
    ESP_LOGVV(TAG, "waiting: %dms", duration);
    output_->set_level(0.0);
    note_playing_ = false;
  }
  next_tone_play_ += duration;
}
}  // namespace rtttl
}  // namespace esphome
