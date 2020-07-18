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
  const char* p = rtttl.c_str();

  ESP_LOGD(TAG, "Playing song %s", p);

  this->default_dur_ = 4;
  this->default_oct_ = 6;
  int bpm = 63;
  int num;

  // format: d=N,o=N,b=NNN:
  // find the start (skip name, etc)

  while (*p != ':')
    p++;  // ignore name

  // skip colon and spaces
  while (*p == ':' || *p == ' ')
    p++;

  // get default duration
  if (*p == 'd') {
    p++;
    p++;  // skip "d="
    num = 0;
    while (is_digit(*p)) {
      num = (num * 10) + (*p++ - '0');
    }
    if (num > 0)
      default_dur_ = num;
    p++;  // skip comma
  }

  // get default octave
  if (*p == 'o') {
    p++;
    p++;  // skip "o="
    num = *p++ - '0';
    if (num >= 3 && num <= 7)
      default_oct_ = num;
    // skip comma and spaces
    while (*p == ',' || *p == ' ')
      p++;
  }

  // get BPM
  if (*p == 'b') {
    p++;
    p++;  // skip "b="
    num = 0;
    while (is_digit(*p)) {
      num = (num * 10) + (*p++ - '0');
    }
    bpm = num;
    // skip colon and spaces
    while (*p == ':' || *p == ' ')
      p++;
  }

  // BPM usually expresses the number of quarter notes per minute
  this->wholenote_ = (60 * 1000L / bpm) * 4;  // this is the time for whole note (in milliseconds)

  this->play_pointer_ = p;
  note_delay_ = millis();
}

void Rtttl::loop() {
  const char* p = this->play_pointer_;
  if (p == nullptr)
    return;

  if (millis() < note_delay_)
    return;

  if (!(*p)) {
    output_->set_level(0.0);
    this->play_pointer_ = nullptr;
    return;
  }

  // first, get note duration, if available
  auto num = 0;
  while (is_digit(*p)) {
    num = (num * 10) + (*p++ - '0');
  }

  if (num)
    this->duration_ = wholenote_ / num;
  else
    this->duration_ = wholenote_ / default_dur_;  // we will need to check if we are a dotted note after

  // now get the note
  uint8_t note = 0;

  switch (*p) {
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
  p++;

  // now, get optional '#' sharp
  if (*p == '#') {
    note++;
    p++;
  }

  // now, get optional '.' dotted note
  if (*p == '.') {
    duration_ += duration_ / 2;
    p++;
  }

  // now, get scale
  uint8_t scale;
  if (isdigit(*p)) {
    scale = *p - '0';
    p++;
  } else {
    scale = default_oct_;
  }

  scale += OCTAVE_OFFSET;

  if (*p == ',')
    p++;  // skip comma for next note (or we may be at the end)

  // Now play the note

  if (note) {
    auto freq = NOTES[(scale - 4) * 12 + note];

    ESP_LOGVV(TAG, "playing note: %d %d %d", duration_, note);
    output_->update_frequency(freq);
    output_->set_level(0.5);
  } else {
    ESP_LOGVV(TAG, "waiting: %d", duration_);
    output_->set_level(0.0);
  }
  this->play_pointer_ = p;
  note_delay_ += duration_;
}
}  // namespace rtttl
}  // namespace esphome