#ifdef USE_ESP32_FRAMEWORK_ARDUINO

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "display.h"
#include "max6921.h"

namespace esphome {
namespace max6921 {

static const char *const TAG = "max6921.display";

// segments of 7-segment character
static const uint8_t SEG_A = (1 << 0);
static const uint8_t SEG_B = (1 << 1);
static const uint8_t SEG_C = (1 << 2);
static const uint8_t SEG_D = (1 << 3);
static const uint8_t SEG_E = (1 << 4);
static const uint8_t SEG_F = (1 << 5);
static const uint8_t SEG_G = (1 << 6);
static const uint8_t SEG_DP = (1 << 7);
static const uint8_t SEG_UNSUPPORTED_CHAR = 0;

// ASCII table from 0x20..0x7E
const uint8_t ASCII_TO_SEG[FONT_SIZE] PROGMEM = {
    0,                                                      // ' ', (0x20)
    SEG_UNSUPPORTED_CHAR,                                   // '!', (0x21)
    SEG_B | SEG_F,                                          // '"', (0x22)
    SEG_UNSUPPORTED_CHAR,                                   // '#', (0x23)
    SEG_UNSUPPORTED_CHAR,                                   // '$', (0x24)
    SEG_UNSUPPORTED_CHAR,                                   // '%', (0x25)
    SEG_UNSUPPORTED_CHAR,                                   // '&', (0x26)
    SEG_F,                                                  // ''', (0x27)
    SEG_A | SEG_D | SEG_E | SEG_F,                          // '(', (0x28)
    SEG_A | SEG_B | SEG_C | SEG_D,                          // ')', (0x29)
    SEG_UNSUPPORTED_CHAR,                                   // '*', (0x2A)
    SEG_UNSUPPORTED_CHAR,                                   // '+', (0x2B)
    SEG_DP,                                                 // ',', (0x2C)
    SEG_G,                                                  // '-', (0x2D)
    SEG_DP,                                                 // '.', (0x2E)
    SEG_UNSUPPORTED_CHAR,                                   // '/', (0x2F)
    SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F,          // '0', (0x30)
    SEG_B | SEG_C,                                          // '1', (0x31)
    SEG_A | SEG_B | SEG_D | SEG_E | SEG_G,                  // '2', (0x32)
    SEG_A | SEG_B | SEG_C | SEG_D | SEG_G,                  // '3', (0x33)
    SEG_B | SEG_C | SEG_F | SEG_G,                          // '4', (0x34)
    SEG_A | SEG_C | SEG_D | SEG_F | SEG_G,                  // '5', (0x35)
    SEG_A | SEG_C | SEG_D | SEG_E | SEG_F | SEG_G,          // '6', (0x36)
    SEG_A | SEG_B | SEG_C,                                  // '7', (0x37)
    SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F | SEG_G,  // '8', (0x38)
    SEG_A | SEG_B | SEG_C | SEG_D | SEG_F | SEG_G,          // '9', (0x39)
    SEG_UNSUPPORTED_CHAR,                                   // ':', (0x3A)
    SEG_UNSUPPORTED_CHAR,                                   // ';', (0x3B)
    SEG_UNSUPPORTED_CHAR,                                   // '<', (0x3C)
    SEG_D | SEG_G,                                          // '=', (0x3D)
    SEG_UNSUPPORTED_CHAR,                                   // '>', (0x3E)
    SEG_A | SEG_B | SEG_E | SEG_G,                          // '?', (0x3F)
    SEG_A | SEG_B | SEG_D | SEG_E | SEG_F | SEG_G,          // '@', (0x40)
    SEG_A | SEG_B | SEG_C | SEG_E | SEG_F | SEG_G,          // 'A', (0x41)
    SEG_C | SEG_D | SEG_E | SEG_F | SEG_G,                  // 'B', (0x42)
    SEG_A | SEG_D | SEG_E | SEG_F,                          // 'C', (0x43)
    SEG_B | SEG_C | SEG_D | SEG_E | SEG_G,                  // 'D', (0x44)
    SEG_A | SEG_D | SEG_E | SEG_F | SEG_G,                  // 'E', (0x45)
    SEG_A | SEG_E | SEG_F | SEG_G,                          // 'F', (0x46)
    SEG_A | SEG_C | SEG_D | SEG_E | SEG_F,                  // 'G', (0x47)
    SEG_B | SEG_C | SEG_E | SEG_F | SEG_G,                  // 'H', (0x48)
    SEG_B | SEG_C,                                          // 'I', (0x49)
    SEG_B | SEG_C | SEG_D | SEG_E,                          // 'J', (0x4A)
    SEG_UNSUPPORTED_CHAR,                                   // 'K', (0x4B)
    SEG_D | SEG_E | SEG_F,                                  // 'L', (0x4C)
    SEG_UNSUPPORTED_CHAR,                                   // 'M', (0x4D)
    SEG_C | SEG_E | SEG_G,                                  // 'N', (0x4E)
    SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F,          // 'O', (0x4F)
    SEG_A | SEG_B | SEG_E | SEG_F | SEG_G,                  // 'P', (0x50)
    SEG_UNSUPPORTED_CHAR,                                   // 'Q', (0x51)
    SEG_E | SEG_G,                                          // 'R', (0x52)
    SEG_A | SEG_C | SEG_D | SEG_F | SEG_G,                  // 'S', (0x53)
    SEG_UNSUPPORTED_CHAR,                                   // 'T', (0x54)
    SEG_B | SEG_C | SEG_D | SEG_E | SEG_F,                  // 'U', (0x55)
    SEG_UNSUPPORTED_CHAR,                                   // 'V', (0x56)
    SEG_UNSUPPORTED_CHAR,                                   // 'W', (0x57)
    SEG_UNSUPPORTED_CHAR,                                   // 'X', (0x58)
    SEG_B | SEG_E | SEG_F | SEG_G,                          // 'Y', (0x59)
    SEG_UNSUPPORTED_CHAR,                                   // 'Z', (0x5A)
    SEG_A | SEG_D | SEG_E | SEG_F,                          // '[', (0x5B)
    SEG_UNSUPPORTED_CHAR,                                   // '\', (0x5C)
    SEG_A | SEG_B | SEG_C | SEG_D,                          // ']', (0x5D)
    SEG_UNSUPPORTED_CHAR,                                   // '^', (0x5E)
    SEG_D,                                                  // '_', (0x5F)
    SEG_F,                                                  // '`', (0x60)
    SEG_A | SEG_B | SEG_C | SEG_E | SEG_F | SEG_G,          // 'a', (0x61)
    SEG_C | SEG_D | SEG_E | SEG_F | SEG_G,                  // 'b', (0x62)
    SEG_D | SEG_E | SEG_G,                                  // 'c', (0x63)
    SEG_B | SEG_C | SEG_D | SEG_E | SEG_G,                  // 'd', (0x64)
    SEG_A | SEG_D | SEG_E | SEG_F | SEG_G,                  // 'e', (0x65)
    SEG_A | SEG_E | SEG_F | SEG_G,                          // 'f', (0x66)
    SEG_A | SEG_C | SEG_D | SEG_E | SEG_F,                  // 'g', (0x67)
    SEG_C | SEG_E | SEG_F | SEG_G,                          // 'h', (0x68)
    SEG_C,                                                  // 'i', (0x69)
    SEG_B | SEG_C | SEG_D | SEG_E,                          // 'j', (0x6A)
    SEG_UNSUPPORTED_CHAR,                                   // 'k', (0x6B)
    SEG_D | SEG_E | SEG_F,                                  // 'l', (0x6C)
    SEG_UNSUPPORTED_CHAR,                                   // 'm', (0x6D)
    SEG_C | SEG_E | SEG_G,                                  // 'n', (0x6E)
    SEG_C | SEG_D | SEG_E | SEG_G,                          // 'o', (0x6F)
    SEG_A | SEG_B | SEG_E | SEG_F | SEG_G,                  // 'p', (0x70)
    SEG_UNSUPPORTED_CHAR,                                   // 'q', (0x71)
    SEG_E | SEG_G,                                          // 'r', (0x72)
    SEG_A | SEG_C | SEG_D | SEG_F | SEG_G,                  // 's', (0x73)
    SEG_UNSUPPORTED_CHAR,                                   // 't', (0x74)
    SEG_C | SEG_D | SEG_E,                                  // 'u', (0x75)
    SEG_UNSUPPORTED_CHAR,                                   // 'v', (0x76)
    SEG_UNSUPPORTED_CHAR,                                   // 'w', (0x77)
    SEG_UNSUPPORTED_CHAR,                                   // 'x', (0x78)
    SEG_B | SEG_E | SEG_F | SEG_G,                          // 'y', (0x79)
    SEG_UNSUPPORTED_CHAR,                                   // 'z', (0x7A)
    SEG_B | SEG_C | SEG_G,                                  // '{', (0x7B)
    SEG_UNSUPPORTED_CHAR,                                   // '|', (0x7C)
    SEG_E | SEG_F | SEG_G,                                  // '}', (0x7D)
    SEG_UNSUPPORTED_CHAR,                                   // '~', (0x7E)
};

void Display::setup(std::vector<uint8_t> &seg_to_out_map, std::vector<uint8_t> &pos_to_out_map) {
  this->default_update_interval_ = this->max6921_->get_update_interval();
  this->seg_to_out_map_ = seg_to_out_map;
  this->pos_to_out_map_ = pos_to_out_map;
  this->num_digits_ = pos_to_out_map.size();
  ESP_LOGCONFIG(TAG, "Display digits: %u", this->num_digits_);

  // setup font...
  init_font__();

  // display output buffer...
  this->out_buf_size_ = this->num_digits_ * 3;
  this->out_buf_ = new uint8_t[this->out_buf_size_];  // NOLINT
  memset(this->out_buf_, 0, this->out_buf_size_);

  // find smallest segment DOUT number...
  this->seg_out_smallest_ = 19;
  for (uint8_t i = 0; i < this->seg_to_out_map_.size(); i++) {
    if (this->seg_to_out_map_[i] < this->seg_out_smallest_)
      this->seg_out_smallest_ = this->seg_to_out_map_[i];
  }
  ESP_LOGCONFIG(TAG, "Display smallest DOUT number: %u", this->seg_out_smallest_);

  // calculate refresh period for 60Hz
  this->refresh_period_us_ = 1000000 / 60 / this->num_digits_;
  ESP_LOGCONFIG(TAG, "Set display refresh period: %" PRIu32 "us for %u digits @ 60Hz", this->refresh_period_us_,
                this->num_digits_);

  /* Setup display refresh.
   * Using a timer is not an option, because the WiFi component uses timer as
   * well, which leads to unstable refresh cycles (flickering). Therefore a
   * thread on 2nd MCU core is used.
   */
  xTaskCreatePinnedToCore(&Display::display_refresh_task_,
                          "display_refresh_task",  // name
                          2048,                    // stack size
                          this,                    // pass component pointer as task parameter pv
                          1,                       // priority (one above IDLE task)
                          nullptr,                 // handle
                          1                        // core
  );

  // ESP_LOGCONFIG(TAG, "Display mode: %u", this->mode);
  // ESP_LOGCONFIG(TAG, "Display text effect: %u", this->disp_text_.effect);
}

void HOT Display::display_refresh_task_(void *pv) {
  Display *display = (Display *) pv;
  static uint count = display->num_digits_;
  static uint current_pos = 1;

  if (display->refresh_period_us_ == 0) {
    ESP_LOGE(TAG, "Invalid display refresh period -> using default 2ms");
    display->refresh_period_us_ = 2000;
  }

  while (true) {
    // one-time verbose output for all positions after any content change...
    if (display->disp_text_.content_changed) {
      count = 0;
      display->disp_text_.content_changed = false;
    }
    if (count < display->num_digits_) {
      count++;
      ESP_LOGVV(TAG, "%s(): SPI transfer for position %u: 0x%02x%02x%02x", __func__, current_pos,
                display->out_buf_[(current_pos - 1) * 3], display->out_buf_[(current_pos - 1) * 3 + 1],
                display->out_buf_[(current_pos - 1) * 3 + 2]);
    }

    // write MAX9621 data of current display position...
    display->max6921_->write_data(&display->out_buf_[(current_pos - 1) * 3], 3);

    // next display position...
    if (++current_pos > display->num_digits_)
      current_pos = 1;

    delayMicroseconds(display->refresh_period_us_);
  }
}

void Display::dump_config() {
  char seg_name[3];

  // display segment to DOUTx mapping...
  for (uint i = 0; i < this->seg_to_out_map_.size(); i++) {
    if (i < 7) {
      seg_name[0] = 'a' + i;
      seg_name[1] = 0;
    } else
      strncpy(seg_name, "dp", sizeof(seg_name));
    ESP_LOGCONFIG(TAG, "  Display segment %2s: OUT%u", seg_name, this->seg_to_out_map_[i]);
  }
  // display position to DOUTx mapping...
  for (uint i = 0; i < this->seg_to_out_map_.size(); i++) {
    ESP_LOGCONFIG(TAG, "  Display position %2u: OUT%u", i, this->pos_to_out_map_[i]);
  }
  ESP_LOGCONFIG(TAG, "  Brightness: %.1f", get_brightness());
}

/**
 * @brief Checks if the given character activates the point segment only.
 *
 * @param c character to check
 *
 * @return true, if character activates the point segment only, otherwise false
 */
bool Display::isPointSegOnly(char c) { return ((c == ',') || (c == '.')); }

void Display::init_font__(void) {
  uint8_t seg_data;

  this->ascii_out_data_ = new uint8_t[ARRAY_ELEM_COUNT(ASCII_TO_SEG)];  // NOLINT

  for (size_t ascii_idx = 0; ascii_idx < ARRAY_ELEM_COUNT(ASCII_TO_SEG); ascii_idx++) {
    this->ascii_out_data_[ascii_idx] = 0;
    seg_data = progmem_read_byte(&ASCII_TO_SEG[ascii_idx]);
    if (seg_data & SEG_A)
      this->ascii_out_data_[ascii_idx] |= (1 << (this->seg_to_out_map_[0] % 8));
    if (seg_data & SEG_B)
      this->ascii_out_data_[ascii_idx] |= (1 << (this->seg_to_out_map_[1] % 8));
    if (seg_data & SEG_C)
      this->ascii_out_data_[ascii_idx] |= (1 << (this->seg_to_out_map_[2] % 8));
    if (seg_data & SEG_D)
      this->ascii_out_data_[ascii_idx] |= (1 << (this->seg_to_out_map_[3] % 8));
    if (seg_data & SEG_E)
      this->ascii_out_data_[ascii_idx] |= (1 << (this->seg_to_out_map_[4] % 8));
    if (seg_data & SEG_F)
      this->ascii_out_data_[ascii_idx] |= (1 << (this->seg_to_out_map_[5] % 8));
    if (seg_data & SEG_G)
      this->ascii_out_data_[ascii_idx] |= (1 << (this->seg_to_out_map_[6] % 8));
    if (seg_data & SEG_DP)
      this->ascii_out_data_[ascii_idx] |= (1 << (this->seg_to_out_map_[7] % 8));
  }
}

/**
 * @brief Clears the whole display buffer or only the given position.
 *
 * @param pos display position 0..n (optional, default=whole display)
 */
void Display::clear(int pos) {
  if (pos < 0)
    memset(this->out_buf_, 0, this->out_buf_size_);  // clear whole display buffer
  else if (pos < this->num_digits_)
    memset(&this->out_buf_[pos * 3], 0, 3);  // clear display buffer at given position
  else
    ESP_LOGW(TAG, "Invalid display position %i (max=%u)", pos, this->num_digits_ - 1);
}

/**
 * @brief Sets the display update interval.
 *
 * @param interval_ms update interval in ms (optional, default=restore)
 */
void Display::set_update_interval(uint32_t interval_ms) {
  ESP_LOGD(TAG, "Change polling interval: %" PRIu32 "ms", interval_ms);
  this->max6921_->stop_poller();
  this->max6921_->set_update_interval(interval_ms);
  this->max6921_->start_poller();
}

/**
 * @brief Restores the configured display update interval.
 */
void Display::restore_update_interval(void) {
  if (this->max6921_->get_update_interval() != this->default_update_interval_) {
    ESP_LOGD(TAG, "Restore polling interval: %" PRIu32 "ms", this->default_update_interval_);
    this->max6921_->stop_poller();
    this->max6921_->set_update_interval(this->default_update_interval_);
    this->max6921_->start_poller();
  }
}

/**
 * @brief Updates the display.
 */
void Display::update(void) {
  // handle display brightness...
  if (this->brightness_cfg_changed_) {
    uint32_t inverted_duty =
        this->brightness_max_duty_ -
        this->brightness_max_duty_ * this->brightness_cfg_value_;  // calc duty for low-active BLANK pin
    ESP_LOGD(TAG, "Change display brightness to %.1f (off-time duty=%u/%u)", brightness_cfg_value_, inverted_duty,
             this->brightness_max_duty_);
    ledcWrite(this->brightness_pwm_channel_, inverted_duty);
    this->brightness_cfg_changed_ = false;
  }

  // handle text effects...
  if (this->disp_text_.effect != TEXT_EFFECT_NONE) {  // any effect enabled?
    switch (this->disp_text_.effect) {
      case TEXT_EFFECT_BLINK:
        update_out_buf_();
        this->disp_text_.blink();
        break;
      case TEXT_EFFECT_SCROLL_LEFT:
        update_out_buf_();
        this->disp_text_.scroll_left();
        break;
      default:
        set_mode(DISP_MODE_PRINT);
        break;
    }
    if (this->disp_text_.effect == TEXT_EFFECT_NONE) {  // effect finished?
      this->duration_ms = 0;
      restore_update_interval();
      set_mode(DISP_MODE_PRINT);  // switch back to "it" interface
    }
  }

  // handle duration...
  if (this->duration_ms > 0) {
    if ((millis() - this->duration_ms_start_) >= this->duration_ms) {
      ESP_LOGD(TAG, "Effect duration of %" PRIu32 "ms expired", this->duration_ms);
      this->disp_text_.set_text_effect(TEXT_EFFECT_NONE);
      set_mode(DISP_MODE_PRINT);
    }
  }
}

void Display::set_demo_mode(demo_mode_t mode, uint32_t interval, uint8_t cycle_num) {
  uint text_idx, font_idx;

  ESP_LOGD(TAG, "Set demo mode: mode=%i, update-interval=%" PRIu32 "ms, cycle_num=%u", mode, interval, cycle_num);

  switch (mode) {
    case DEMO_MODE_SCROLL_FONT:
      // generate scroll text based on font...
      for (text_idx = 0, font_idx = 0; font_idx < ARRAY_ELEM_COUNT(ASCII_TO_SEG); font_idx++) {
        if (this->ascii_out_data_[font_idx] > 0)               // displayable character?
          this->disp_text_.text[text_idx++] = ' ' + font_idx;  // add character to string
        if (text_idx >= sizeof(this->disp_text_.text) - 1) {   // max. text buffer lenght reached?
          ESP_LOGD(TAG, "Font too large for internal text buffer");
          break;
        }
      }
      this->disp_text_.text[text_idx] = 0;
      ESP_LOGV(TAG, "%s(): text: %s", __func__, this->disp_text_.text);
      // set text effect...
      this->disp_text_.set_text_effect(TEXT_EFFECT_SCROLL_LEFT, cycle_num);
      set_mode(DISP_MODE_OTHER);
      break;
    default:
      set_mode(DISP_MODE_PRINT);
      break;
  }
  if ((mode != DEMO_MODE_OFF) && (interval > 0))
    set_update_interval(interval);
  clear();
}

void Display::set_demo_mode(const std::string &mode, uint32_t interval, uint8_t cycle_num) {
  if (str_equals_case_insensitive(mode, "off")) {
    this->set_demo_mode(DEMO_MODE_OFF, interval, cycle_num);
  } else if (str_equals_case_insensitive(mode, "scroll_font")) {
    this->set_demo_mode(DEMO_MODE_SCROLL_FONT, interval, cycle_num);
  } else {
    ESP_LOGW(TAG, "Invalid demo mode: %s", mode.c_str());
  }
}

/**
 * @brief Triggered by action. Shows the given text on the display.
 *
 * @param text text to display
 * @param start_pos display position 0..n
 * @param align text alignment
 * @param duration text effect duration in ms
 * @param effect text effect
 * @param interval text effect update interval in ms
 * @param cycle_num text effect cycle count
 *
 * @return number of characters displayed
 */
int Display::set_text(const std::string &text, uint8_t start_pos, const std::string &align, uint32_t duration,
                      const std::string &effect, uint32_t interval, uint8_t cycle_num) {
  ESP_LOGD(TAG,
           "Set text (given):  text=%s, start-pos=%u, align=%s, duration=%" PRIu32 "ms, "
           "effect=%s, effect-update-interval=%" PRIu32 "ms, cycles=%u",
           text.c_str(), start_pos, align.c_str(), duration, effect.c_str(), interval, cycle_num);

  // store new text...
  this->disp_text_.set_text(start_pos, this->num_digits_ - 1, text);

  // set text align...
  this->disp_text_.set_text_align(align);

  // set text effect...
  this->disp_text_.set_text_effect(effect, cycle_num);
  if ((this->disp_text_.effect != TEXT_EFFECT_NONE) && (interval > 0)) {
    set_update_interval(interval);
  }

  // update display mode...
  set_mode(DISP_MODE_OTHER, duration);

  ESP_LOGD(TAG, "Set text (result): text=%s, start-pos=%u, vi-idx=%u, vi-len=%u", this->disp_text_.text,
           this->disp_text_.start_pos, this->disp_text_.visible_idx, this->disp_text_.visible_len);

  return update_out_buf_();
}

/**
 * @brief Triggered by it.print(). Shows the given text on the display.
 *
 * @param text text to display
 * @param start_pos display position 0..n (optional, default=0)
 *
 * @return number of characters displayed
 */
int Display::set_text(const char *text, uint8_t start_pos) {
  ESP_LOGVV(TAG, "%s(): str=%s, prev=%s", __func__, text, this->disp_text_.text);
  if (strncmp(text, this->disp_text_.text, sizeof(this->disp_text_.text)) == 0)  // text not changed?
    return strlen(text);                                                         // yes -> exit function
  ESP_LOGV(TAG, "%s(): Text changed: str=%s, prev=%s", __func__, text, this->disp_text_.text);

  // store new text...
  std::string text_str = text;
  this->disp_text_.set_text(start_pos, this->num_digits_ - 1, text_str);

  return update_out_buf_();
}

/**
 * @brief Updates the display buffer containing the MAX6921 OUT data according
 *        to current visible text.
 *
 * @return number of visible characters
 */
int Display::update_out_buf_(void) {
  uint visible_idx_offset = 0;

  for (uint pos = 0; pos < this->num_digits_; pos++) {
    char pos_char;
    uint32_t out_data;
    bool bGetNextChar, bClearPos = true;
    do {
      // determine character for current display position...
      if ((pos < this->disp_text_.start_pos) ||                                  // empty position before text or
          (pos >= (this->disp_text_.start_pos + this->disp_text_.visible_len)))  // empty position after text?
        pos_char = ' ';
      else
        pos_char = this->disp_text_.text[this->disp_text_.visible_idx + visible_idx_offset++];

      // special handling for point segment...
      bGetNextChar = false;
      if (isPointSegOnly(pos_char)) {                                     // is point segment only?
        if (this->disp_text_.visible_idx + visible_idx_offset - 1 > 0) {  // not the 1st text character?
          if (isPointSegOnly(this->disp_text_.text[this->disp_text_.visible_idx + visible_idx_offset -
                                                   2])) {  // previous text character wasn't a point?
            if (pos == 0) {                                // 1st (most left) display position?
              bGetNextChar = true;                         // yes -> ignore point, get next character
            } else {
              --pos;  // no -> add point to previous display position
              bClearPos = false;
            }
          }
        }
      }
    } while (bGetNextChar);
    if (bClearPos)
      clear(pos);

    // create segment data...
    if ((pos_char >= ' ') && ((pos_char - ' ') < ARRAY_ELEM_COUNT(ASCII_TO_SEG))) {  // supported char?
      out_data = this->ascii_out_data_[pos_char - ' '];                              // yes ->
    } else {
      ESP_LOGW(TAG, "Encountered unsupported character (0x%02x): %c", pos_char, (pos_char >= 0x20) ? pos_char : ' ');
      out_data = SEG_UNSUPPORTED_CHAR;
    }
    ESP_LOGVV(TAG, "%s(): segment data: 0x%06x", __func__, out_data);
#if 0
    // At the moment an unsupport character is equal to blank (' ').
    // To distinguish an unsupported character from blank we would need to
    // increase font data type from uint8_t to uint16_t!
    if (out_data == SEG_UNSUPPORTED_CHAR) {
      ESP_LOGW(TAG, "Encountered character '%c (0x%02x)' with no display representation!", *vi_text, *vi_text);
    }
#endif

    // shift data to the smallest segment OUT position...
    out_data <<= (this->seg_out_smallest_);
    ESP_LOGVV(TAG, "%s(): segment data shifted to first segment bit (OUT%u): 0x%06x", __func__, this->seg_out_smallest_,
              out_data);

    // add position data...
    out_data |= (1 << this->pos_to_out_map_[pos]);
    ESP_LOGVV(TAG, "%s(): OUT data with position: 0x%06x", __func__, out_data);

    // write to appropriate position of display buffer...
    this->out_buf_[pos * 3 + 0] |= (uint8_t) ((out_data >> 16) & 0xFF);
    this->out_buf_[pos * 3 + 1] |= (uint8_t) ((out_data >> 8) & 0xFF);
    this->out_buf_[pos * 3 + 2] |= (uint8_t) (out_data & 0xFF);
    ESP_LOGVV(TAG, "%s(): display buffer of position %u: 0x%02x%02x%02x", __func__, pos + 1,
              this->out_buf_[pos * 3 + 0], this->out_buf_[pos * 3 + 1], this->out_buf_[pos * 3 + 2]);

    ESP_LOGV(TAG, "%s(): pos=%u, char='%c' (0x%02x), vi-idx=%u, vi-idx-off=%u, vi-len=%u", __func__, pos, pos_char,
             pos_char, this->disp_text_.visible_idx, visible_idx_offset, this->disp_text_.visible_len);
  }

  this->disp_text_.content_changed = true;

  return this->num_digits_ - this->disp_text_.start_pos;
}

/**
 * @brief Constructor.
 */
DisplayText::DisplayText() {
  this->text[0] = 0;
  this->visible_idx = 0;
  this->visible_len = 0;
  this->content_changed = false;
  this->start_pos = 0;
  this->max_pos = 0;
  this->align = TEXT_ALIGN_CENTER;
  this->effect = TEXT_EFFECT_NONE;
}

/**
 * @brief Stores the given text.
 *
 * @param start_pos display start position of text (0..n)
 * @param max_pos display max. position (as index)
 * @param text text to store
 *
 * @return number of stored characters
 */
int DisplayText::set_text(uint start_pos, uint max_pos, const std::string &text) {
  // check start position...
  if (start_pos >= max_pos) {
    ESP_LOGW(TAG, "Invalid start position: %u");
    this->start_pos = 0;
  } else
    this->start_pos = start_pos;

  this->max_pos = max_pos;
  strncpy(this->text, text.c_str(), sizeof(this->text) - 1);
  this->text[sizeof(this->text) - 1] = 0;
  this->visible_idx = 0;
  this->visible_len = std::min(strlen(this->text), this->max_pos - this->start_pos + 1);
  return strlen(this->text);
}

/**
 * @brief Inits the text object according to selected align.
 */
void DisplayText::init_text_align_(void) {
  this->visible_idx = 0;
  this->visible_len = std::min(strlen(this->text), this->max_pos + 1);
  switch (this->align) {
    case TEXT_ALIGN_LEFT:
      this->start_pos = 0;
      break;
    case TEXT_ALIGN_CENTER:
      this->start_pos = ((this->max_pos + 1) - this->visible_len) / 2;
      break;
    case TEXT_ALIGN_RIGHT:
      this->start_pos = (this->max_pos + 1) - this->visible_len;
      break;
  }
}

/**
 * @brief Inits the text object according to selected effect.
 */
void DisplayText::init_text_effect_(void) {
  switch (this->effect) {
    case TEXT_EFFECT_SCROLL_LEFT:
      this->start_pos = this->max_pos;  // start at right side
      this->visible_idx = 0;
      this->visible_len = 1;
      break;
    case TEXT_EFFECT_BLINK:
    case TEXT_EFFECT_NONE:
    default:
      // Don't change display start position!
      this->visible_idx = 0;
      this->visible_len = std::min(strlen(this->text), this->max_pos - this->start_pos + 1);
      break;
  }
}

/**
 * @brief Sets the text align.
 *
 * @param align text align
 */
void DisplayText::set_text_align(text_align_t align) {
  if (align >= TEXT_ALIGN_LAST_ENUM) {
    ESP_LOGE(TAG, "Invalid display text align: %i", align);
    return;
  }
  this->align = align;
  init_text_align_();
  ESP_LOGD(TAG, "Set align: text=%s, align=%i, start-pos=%u, max-pos=%u, vi-idx=%u, vi-len=%u", this->text, this->align,
           this->start_pos, this->max_pos, this->visible_idx, this->visible_len);
}

/**
 * @brief Sets the text align.
 *
 * @param align text align (as string)
 */
void DisplayText::set_text_align(const std::string &align) {
  text_align_t text_align = TEXT_ALIGN_LAST_ENUM;

  if (!align.empty()) {
    if (str_equals_case_insensitive(align, "left"))
      text_align = TEXT_ALIGN_LEFT;
    else if (str_equals_case_insensitive(align, "center"))
      text_align = TEXT_ALIGN_CENTER;
    else if (str_equals_case_insensitive(align, "right"))
      text_align = TEXT_ALIGN_RIGHT;
    else
      ESP_LOGW(TAG, "Invalid text align: %s", align);
  } else
    ESP_LOGW(TAG, "No text align given");
  if (text_align >= TEXT_ALIGN_LAST_ENUM)
    return;
  set_text_align(text_align);
}

/**
 * @brief Sets the text effect.
 *
 * @param effect text effect
 * @param cycle_num number of effect cycles (optional, default=endless)
 */
void DisplayText::set_text_effect(text_effect_t effect, uint8_t cycle_num) {
  if (effect >= TEXT_EFFECT_LAST_ENUM) {
    ESP_LOGE(TAG, "Invalid display text effect: %i", effect);
    return;
  }
  if (effect != this->effect) {
    this->effect = effect;
    this->cycle_num = cycle_num;
    if (effect == TEXT_EFFECT_BLINK)
      this->effect_change_count_ = -1;
    init_text_effect_();
    ESP_LOGD(TAG, "Set effect: text=%s, effect=%i, cycles=%u, start-pos=%u, max-pos=%u, vi-idx=%u, vi-len=%u",
             this->text, this->effect, this->cycle_num, this->start_pos, this->max_pos, this->visible_idx,
             this->visible_len);
  }
}

/**
 * @brief Sets the text effect.
 *
 * @param effect text effect (as string)
 * @param cycle_num number of effect cycles (optional, default=endless)
 */
void DisplayText::set_text_effect(const std::string &effect, uint8_t cycle_num) {
  text_effect_t text_effect = TEXT_EFFECT_LAST_ENUM;

  if (!effect.empty()) {
    if (str_equals_case_insensitive(effect, "none"))
      text_effect = TEXT_EFFECT_NONE;
    else if (str_equals_case_insensitive(effect, "blink"))
      text_effect = TEXT_EFFECT_BLINK;
    else if (str_equals_case_insensitive(effect, "scroll_left"))
      text_effect = TEXT_EFFECT_SCROLL_LEFT;
    else
      ESP_LOGW(TAG, "Invalid text effect: %s", effect);
  } else
    ESP_LOGW(TAG, "No text effect given");
  if (text_effect >= TEXT_EFFECT_LAST_ENUM)
    return;
  set_text_effect(text_effect, cycle_num);
}

/**
 * @brief Updates the mode "blink". The display buffer must be updated before.
 */
void DisplayText::blink(void) {
  ESP_LOGV(TAG, "%s(): ENTRY: start-idx=%u, text-idx=%u, text-len=%u", __func__, this->start_pos, this->visible_idx,
           this->visible_len);

  // update effect mode...
  if (++this->effect_change_count_ >= 2) {  // one on/off phase complete?
    this->effect_change_count_ = 0;
    if (this->cycle_num > 0) {
      ESP_LOGD(TAG, "Blink cycle finished (%u left)", this->cycle_num - 1);
      if (--this->cycle_num == 0) {
        this->effect = TEXT_EFFECT_NONE;
        ESP_LOGD(TAG, "Blink finished");
        return;
      }
    }
  }

  // update visible text...
  if (this->visible_len > 0) {  // "on" phase?
    this->visible_len = 0;      // yes -> switch to "off" phase
  } else {
    init_text_effect_();  // no -> switch to "on" phase
  }

  ESP_LOGV(TAG, "%s(): EXIT:  start-idx=%u, text-idx=%u, text-len=%u", __func__, this->start_pos, this->visible_idx,
           this->visible_len);
}

/**
 * @brief Updates the mode "scroll left". The display buffer must be updated before.
 */
void DisplayText::scroll_left(void) {
  ESP_LOGV(TAG, "%s(): ENTRY: start-idx=%u, text-idx=%u, text-len=%u", __func__, this->start_pos, this->visible_idx,
           this->visible_len);

  // update effect mode...
  if (this->visible_len == 0) {
    init_text_effect_();
    if (this->cycle_num > 0) {
      ESP_LOGD(TAG, "Scroll cycle finished (%u left)", this->cycle_num - 1);
      if (--this->cycle_num == 0) {
        this->effect = TEXT_EFFECT_NONE;
        ESP_LOGD(TAG, "Scroll finished");
        return;
      }
    }
  }

  // update visible text...
  if (this->start_pos > 0) {  // left display side not reached (scroll in from right side)?
    --this->start_pos;        // decrement display start position
    if (this->visible_len < strlen(this->text))
      ++this->visible_len;  // increment visible text length
  } else {
    ++this->visible_idx;                                               // increment visible start index
    if ((this->visible_idx + this->visible_len) > strlen(this->text))  // visible part reached at end of text?
      --this->visible_len;  // decrement visible text length (scroll out to left side)
  }

  ESP_LOGV(TAG, "%s(): EXIT:  start-idx=%u, text-idx=%u, text-len=%u", __func__, this->start_pos, this->visible_idx,
           this->visible_len);
}

/**
 * @brief Configures the PWM for display brightness control.
 *
 * @param pwm_pin_no PWM pin number
 * @param channel PWM channel
 * @param resolution PWM resolution
 * @param freq PWM frequency
 *
 * @return frequency supported by hardware (0 = no support)
 */
uint32_t DisplayBrightness::config_brightness_pwm(uint8_t pwm_pin_no, uint8_t channel, uint8_t resolution,
                                                  uint32_t freq) {
  uint32_t freq_supported;

  if ((freq_supported = ledcSetup(channel, freq, resolution)) != 0) {
    ledcAttachPin(pwm_pin_no, channel);
    this->brightness_pwm_channel_ = channel;
    this->brightness_max_duty_ = pow(2, resolution);  // max. duty value for given resolution
    ESP_LOGD(TAG, "Prepare brightness PWM: pin=%u, channel=%u, resolution=%ubit, freq=%uHz", pwm_pin_no, channel,
             resolution, freq_supported);
  } else {
    ESP_LOGD(TAG, "Failed to configure brightness PWM");
  }

  return freq_supported;
}

/**
 * @brief Sets the display brightness.
 *
 * @param percent brightness in percent (0.0-1.0)
 */
void DisplayBrightness::set_brightness(float percent) {
  if ((percent >= 0.0) && (percent <= 1.0)) {
    this->brightness_cfg_value_ = percent;
    this->brightness_cfg_changed_ = true;
  } else
    ESP_LOGW(TAG, "Invalid brightness value: %f", percent);
}

/**
 * @brief Constructor.
 */
DisplayMode::DisplayMode() {
  this->mode = DISP_MODE_PRINT;
  this->duration_ms = 0;  // endless
}

/**
 * @brief Sets the display mode.
 *
 * @param mode display start position of text (0..n)
 * @param duration duration in ms (optional, default=endless)
 *                 not applicable for "it" mode
 */
void DisplayMode::set_mode(display_mode_t mode, uint32_t duration_ms) {
  if (mode >= DISP_MODE_LAST_ENUM) {
    ESP_LOGE(TAG, "Invalid display mode: %i", mode);
    return;
  }
  if (mode != this->mode) {
    this->mode = mode;
    this->duration_ms = duration_ms;
    if (duration_ms > 0)
      this->duration_ms_start_ = millis();
    ESP_LOGD(TAG, "Set display mode: mode=%i, duration=%" PRIu32 "ms, duration-start=%" PRIu32 "ms", this->mode,
             this->duration_ms, this->duration_ms_start_);
  }
}

}  // namespace max6921
}  // namespace esphome

#endif  // USE_ESP32_FRAMEWORK_ARDUINO
