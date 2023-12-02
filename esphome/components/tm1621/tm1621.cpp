#include "tm1621.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace tm1621 {

static const char *const TAG = "tm1621";

const uint8_t TM1621_PULSE_WIDTH = 10;  // microseconds (Sonoff = 100)

const uint8_t TM1621_SYS_EN = 0x01;     // 0b00000001
const uint8_t TM1621_LCD_ON = 0x03;     // 0b00000011
const uint8_t TM1621_TIMER_DIS = 0x04;  // 0b00000100
const uint8_t TM1621_WDT_DIS = 0x05;    // 0b00000101
const uint8_t TM1621_TONE_OFF = 0x08;   // 0b00001000
const uint8_t TM1621_BIAS = 0x29;       // 0b00101001 = LCD 1/3 bias 4 commons option
const uint8_t TM1621_IRQ_DIS = 0x80;    // 0b100x0xxx

enum Tm1621Device { TM1621_USER, TM1621_POWR316D, TM1621_THR316D };

const uint8_t TM1621_COMMANDS[] = {TM1621_SYS_EN,  TM1621_LCD_ON,   TM1621_BIAS,   TM1621_TIMER_DIS,
                                   TM1621_WDT_DIS, TM1621_TONE_OFF, TM1621_IRQ_DIS};

const char TM1621_KCHAR[] PROGMEM = {"0|1|2|3|4|5|6|7|8|9|-| "};
//                                          0     1     2     3     4     5     6     7     8     9     -     off
const uint8_t TM1621_DIGIT_ROW[2][12] = {{0x5F, 0x50, 0x3D, 0x79, 0x72, 0x6B, 0x6F, 0x51, 0x7F, 0x7B, 0x20, 0x00},
                                         {0xF5, 0x05, 0xB6, 0x97, 0x47, 0xD3, 0xF3, 0x85, 0xF7, 0xD7, 0x02, 0x00}};

void TM1621Display::setup() {
  ESP_LOGCONFIG(TAG, "Setting up TM1621...");

  this->cs_pin_->setup();  // OUTPUT
  this->cs_pin_->digital_write(true);
  this->data_pin_->setup();  // OUTPUT
  this->data_pin_->digital_write(true);
  this->read_pin_->setup();  // OUTPUT
  this->read_pin_->digital_write(true);
  this->write_pin_->setup();  // OUTPUT
  this->write_pin_->digital_write(true);

  this->state_ = 100;

  this->cs_pin_->digital_write(false);
  delayMicroseconds(80);
  this->read_pin_->digital_write(false);
  delayMicroseconds(15);
  this->write_pin_->digital_write(false);
  delayMicroseconds(25);
  this->data_pin_->digital_write(false);
  delayMicroseconds(TM1621_PULSE_WIDTH);
  this->data_pin_->digital_write(true);

  for (uint8_t tm1621_command : TM1621_COMMANDS) {
    this->send_command_(tm1621_command);
  }

  this->send_address_(0x00);
  for (uint32_t segment = 0; segment < 16; segment++) {
    this->send_common_(0);
  }
  this->stop_();

  snprintf(this->row_[0], sizeof(this->row_[0]), "----");
  snprintf(this->row_[1], sizeof(this->row_[1]), "----");

  this->display();
}
void TM1621Display::dump_config() {
  ESP_LOGCONFIG(TAG, "TM1621:");
  LOG_PIN("  CS Pin: ", this->cs_pin_);
  LOG_PIN("  DATA Pin: ", this->data_pin_);
  LOG_PIN("  READ Pin: ", this->read_pin_);
  LOG_PIN("  WRITE Pin: ", this->write_pin_);
  LOG_UPDATE_INTERVAL(this);
}

void TM1621Display::update() {
  // memset(this->row, 0, sizeof(this->row));
  if (this->writer_.has_value())
    (*this->writer_)(*this);
  this->display();
}

float TM1621Display::get_setup_priority() const { return setup_priority::PROCESSOR; }
void TM1621Display::bit_delay_() { delayMicroseconds(100); }

void TM1621Display::stop_() {
  this->cs_pin_->digital_write(true);  // Stop command sequence
  delayMicroseconds(TM1621_PULSE_WIDTH / 2);
  this->data_pin_->digital_write(true);  // Reset data
}

void TM1621Display::display() {
  // Tm1621.row[x] = "text", "----", "    " or a number with one decimal like "0.4", "237.5", "123456.7"
  // "123456.7" will be shown as "9999" being a four digit overflow

  // AddLog(LOG_LEVEL_DEBUG, PSTR("TM1: Row1 '%s', Row2 '%s'"), Tm1621.row[0], Tm1621.row[1]);

  uint8_t buffer[8] = {0};  // TM1621 16-segment 4-bit common buffer
  char row[4];
  for (uint32_t j = 0; j < 2; j++) {
    // 0.4V => "  04", 0.0A => "  ", 1234.5V => "1234"
    uint32_t len = strlen(this->row_[j]);
    char *dp = nullptr;     // Expect number larger than "123"
    int row_idx = len - 3;  // "1234.5"
    if (len <= 5) {         // "----", "    ", "0.4", "237.5"
      dp = strchr(this->row_[j], '.');
      row_idx = len - 1;
    } else if (len > 6) {  // "12345.6"
      snprintf(this->row_[j], sizeof(this->row_[j]), "9999");
      row_idx = 3;
    }
    row[3] = (row_idx >= 0) ? this->row_[j][row_idx--] : ' ';
    if ((row_idx >= 0) && dp) {
      row_idx--;
    }
    row[2] = (row_idx >= 0) ? this->row_[j][row_idx--] : ' ';
    row[1] = (row_idx >= 0) ? this->row_[j][row_idx--] : ' ';
    row[0] = (row_idx >= 0) ? this->row_[j][row_idx--] : ' ';

    //    AddLog(LOG_LEVEL_DEBUG, PSTR("TM1: Dump%d %4_H"), j +1, row);

    char command[10];
    char needle[2] = {0};
    for (uint32_t i = 0; i < 4; i++) {
      needle[0] = row[i];
      int index = this->get_command_code_(command, sizeof(command), (const char *) needle, TM1621_KCHAR);
      if (-1 == index) {
        index = 11;
      }
      uint32_t bidx = (0 == j) ? i : 7 - i;
      buffer[bidx] = TM1621_DIGIT_ROW[j][index];
    }
    if (dp) {
      if (0 == j) {
        buffer[2] |= 0x80;  // Row 1 decimal point
      } else {
        buffer[5] |= 0x08;  // Row 2 decimal point
      }
    }
  }

  if (this->fahrenheit_) {
    buffer[1] |= 0x80;
  }
  if (this->celsius_) {
    buffer[3] |= 0x80;
  }
  if (this->kwh_) {
    buffer[4] |= 0x08;
  }
  if (this->humidity_) {
    buffer[6] |= 0x08;
  }
  if (this->voltage_) {
    buffer[7] |= 0x08;
  }

  //  AddLog(LOG_LEVEL_DEBUG, PSTR("TM1: Dump3 %8_H"), buffer);

  this->send_address_(0x10);  // Sonoff only uses the upper 16 Segments
  for (uint8_t i : buffer) {
    this->send_common_(i);
  }
  this->stop_();
}

bool TM1621Display::send_command_(uint16_t command) {
  uint16_t full_command = (0x0400 | command) << 5;  // 0b100cccccccc00000
  this->cs_pin_->digital_write(false);              // Start command sequence
  delayMicroseconds(TM1621_PULSE_WIDTH / 2);
  for (uint32_t i = 0; i < 12; i++) {
    this->write_pin_->digital_write(false);  // Start write sequence
    if (full_command & 0x8000) {
      this->data_pin_->digital_write(true);  // Set data
    } else {
      this->data_pin_->digital_write(false);  // Set data
    }
    delayMicroseconds(TM1621_PULSE_WIDTH);
    this->write_pin_->digital_write(true);  // Read data
    delayMicroseconds(TM1621_PULSE_WIDTH);
    full_command <<= 1;
  }
  this->stop_();
  return true;
}

bool TM1621Display::send_common_(uint8_t common) {
  for (uint32_t i = 0; i < 8; i++) {
    this->write_pin_->digital_write(false);  // Start write sequence
    if (common & 1) {
      this->data_pin_->digital_write(true);  // Set data
    } else {
      this->data_pin_->digital_write(false);  // Set data
    }
    delayMicroseconds(TM1621_PULSE_WIDTH);
    this->write_pin_->digital_write(true);  // Read data
    delayMicroseconds(TM1621_PULSE_WIDTH);
    common >>= 1;
  }
  return true;
}

bool TM1621Display::send_address_(uint16_t address) {
  uint16_t full_address = (address | 0x0140) << 7;  // 0b101aaaaaa0000000
  this->cs_pin_->digital_write(false);              // Start command sequence
  delayMicroseconds(TM1621_PULSE_WIDTH / 2);
  for (uint32_t i = 0; i < 9; i++) {
    this->write_pin_->digital_write(false);  // Start write sequence
    if (full_address & 0x8000) {
      this->data_pin_->digital_write(true);  // Set data
    } else {
      this->data_pin_->digital_write(false);  // Set data
    }
    delayMicroseconds(TM1621_PULSE_WIDTH);
    this->write_pin_->digital_write(true);  // Read data
    delayMicroseconds(TM1621_PULSE_WIDTH);
    full_address <<= 1;
  }
  return true;
}

uint8_t TM1621Display::print(uint8_t start_pos, const char *str) {
  // ESP_LOGD(TAG, "Print at %d: %s", start_pos, str);
  return snprintf(this->row_[start_pos], sizeof(this->row_[start_pos]), "%s", str);
}
uint8_t TM1621Display::print(const char *str) { return this->print(0, str); }
uint8_t TM1621Display::printf(uint8_t pos, const char *format, ...) {
  va_list arg;
  va_start(arg, format);
  char buffer[64];
  int ret = vsnprintf(buffer, sizeof(buffer), format, arg);
  va_end(arg);
  if (ret > 0)
    return this->print(pos, buffer);
  return 0;
}
uint8_t TM1621Display::printf(const char *format, ...) {
  va_list arg;
  va_start(arg, format);
  char buffer[64];
  int ret = vsnprintf(buffer, sizeof(buffer), format, arg);
  va_end(arg);
  if (ret > 0)
    return this->print(buffer);
  return 0;
}

int TM1621Display::get_command_code_(char *destination, size_t destination_size, const char *needle,
                                     const char *haystack) {
  // Returns -1 of not found
  // Returns index and command if found
  int result = -1;
  const char *read = haystack;
  char *write = destination;

  while (true) {
    result++;
    size_t size = destination_size - 1;
    write = destination;
    char ch = '.';
    while ((ch != '\0') && (ch != '|')) {
      ch = *(read++);
      if (size && (ch != '|')) {
        *write++ = ch;
        size--;
      }
    }
    *write = '\0';
    if (!strcasecmp(needle, destination)) {
      break;
    }
    if (0 == ch) {
      result = -1;
      break;
    }
  }
  return result;
}
}  // namespace tm1621
}  // namespace esphome
