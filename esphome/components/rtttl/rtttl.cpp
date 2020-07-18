#include "rtttl.h"
#include "esphome/core/log.h"

#include "../esp8266_pwm/esp8266_pwm.h"

namespace esphome {
namespace rtttl {

static const char* TAG = "rtttl";

static const uint8_t OCTAVE_OFFSET = 0;

// These values can also be found as constants in the Tone library (Tone.h)
static const uint16_t NOTES[] = {0,    262,  277,  294,  311,  330,  349,  370,  392,  415,  440,  466,  494,
                                 523,  554,  587,  622,  659,  698,  740,  784,  831,  880,  932,  988,  1047,
                                 1109, 1175, 1245, 1319, 1397, 1480, 1568, 1661, 1760, 1865, 1976, 2093, 2217,
                                 2349, 2489, 2637, 2794, 2960, 3136, 3322, 3520, 3729, 3951};

#define is_digit(n) (n >= '0' && n <= '9')

void Rtttl::play(std::string rtttl) {
  // Absolutely no error checking in here
  this->rtttl_ = rtttl;

  ESP_LOGD(TAG, "Playing song %s", rtttl_.c_str());

  p_ = rtttl_.cbegin();

  this->default_dur_ = 4;
  this->default_oct_ = 6;
  int bpm = 63;
  int num;

  // format: d=N,o=N,b=NNN:
  // find the start (skip name, etc)

  // ignore name
  while (*p_ != ':')
    p_++;

  // skip colon and spaces
  while (*p_ == ':' || *p_ == ' ')
    p_++;

  // get default duration
  if (*p_ == 'd') {
    p_++;
    p_++;  // skip "d="
    num = 0;
    while (is_digit(*p_)) {
      num = (num * 10) + (*p_++ - '0');
    }
    if (num > 0)
      default_dur_ = num;
    p_++;  // skip comma
  }

  // get default octave
  if (*p_ == 'o') {
    p_++;
    p_++;  // skip "o="
    num = *p_++ - '0';
    if (num >= 3 && num <= 7)
      default_oct_ = num;
    // skip comma and spaces
    while (*p_ == ',' || *p_ == ' ')
      p_++;
  }

  // get BPM
  if (*p_ == 'b') {
    p_++;
    p_++;  // skip "b="
    num = 0;
    while (is_digit(*p_)) {
      num = (num * 10) + (*p_++ - '0');
    }
    bpm = num;
    // skip colon and spaces
    while (*p_ == ':' || *p_ == ' ')
      p_++;
  }

  // BPM usually expresses the number of quarter notes per minute
  this->wholenote_ = (60 * 1000L / bpm) * 4;  // this is the time for whole note (in milliseconds)

  note_playing_ = false;
  next_tone_play_ = millis();
}

void Rtttl::loop() {
  if (next_tone_play_ == 0 || millis() < next_tone_play_)
    return;

  if (note_playing_) {
    // add small silence between notes, but this should be done only when repeating notes.
    next_tone_play_ = millis() + 10;
    output_->set_level(0);
    note_playing_ = false;
    return;
  }

  if (p_ == rtttl_.cend()) {
    output_->set_level(0.0);
    ESP_LOGD(TAG, "Play ended");
    next_tone_play_ = 0;
    return;
  }

  // first, get note duration, if available
  auto num = 0;
  while (is_digit(*p_)) {
    num = (num * 10) + (*p_++ - '0');
  }

  if (num)
    this->duration_ = wholenote_ / num;
  else
    this->duration_ = wholenote_ / default_dur_;  // we will need to check if we are a dotted note after

  // now get the note
  uint8_t note = 0;

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
      note = 0;
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
    duration_ += duration_ / 2;
    p_++;
  }

  // now, get scale
  uint8_t scale;
  if (isdigit(*p_)) {
    scale = *p_ - '0';
    p_++;
  } else {
    scale = default_oct_;
  }

  scale += OCTAVE_OFFSET;

  if (*p_ == ',')
    p_++;  // skip comma for next note (or we may be at the end)

  // Now play the note

  if (note) {
    auto freq = NOTES[(scale - 4) * 12 + note];

    ESP_LOGVV(TAG, "playing note: %d %d %d", duration_, note);
    output_->update_frequency(freq);
    output_->set_level(0.5);
    note_playing_ = true;
  } else {
    ESP_LOGVV(TAG, "waiting: %d", duration_);
    output_->set_level(0.0);
    note_playing_ = false;
  }
  next_tone_play_ += duration_;
}
}  // namespace rtttl
}  // namespace esphome
