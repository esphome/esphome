// Datasheet: https://www.analog.com/media/en/technical-documentation/data-sheets/MAX6921-MAX6931.pdf

#include "max6921.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/hal.h"
#include <cinttypes>

namespace esphome {
namespace max6921 {

#define ARRAY_ELEM_COUNT(array)  (sizeof(array)/sizeof(array[0]))

static const char *const TAG = "max6921";

// display intensity (brightness)...
static const uint MAX_DISPLAY_INTENSITY = 16;

// display character position (left to right) to DOUTx mapping...
static const uint8_t DISP_POS_TO_OUT[] = {
// left 1   2   3   4   5   6   7   8  right
        15, 14, 13, 16, 12, 17, 11, 18
};

// segments of 7-segment character
static const uint8_t SEG_A  = (1<<0);
static const uint8_t SEG_B  = (1<<1);
static const uint8_t SEG_C  = (1<<2);
static const uint8_t SEG_D  = (1<<3);
static const uint8_t SEG_E  = (1<<4);
static const uint8_t SEG_F  = (1<<5);
static const uint8_t SEG_G  = (1<<6);
static const uint8_t SEG_DP = (1<<7);
static const uint8_t SEG_UNSUPPORTED_CHAR = 0;

// ASCII table from 0x20..0x7E
const uint8_t ASCII_TO_SEG[95] PROGMEM = {
  0,                                          // ' ', (0x20)
  SEG_B|SEG_C|SEG_DP,                         // '!', (0x21)
  SEG_B|SEG_F,                                // '"', (0x22)
  SEG_UNSUPPORTED_CHAR,                       // '#', (0x23)
  SEG_UNSUPPORTED_CHAR,                       // '$', (0x24)
  SEG_UNSUPPORTED_CHAR,                       // '%', (0x25)
  SEG_UNSUPPORTED_CHAR,                       // '&', (0x26)
  SEG_F,                                      // ''', (0x27)
  SEG_A|SEG_D|SEG_E|SEG_F,                    // '(', (0x28)
  SEG_A|SEG_B|SEG_C|SEG_D,                    // ')', (0x29)
  SEG_UNSUPPORTED_CHAR,                       // '*', (0x2A)
  SEG_UNSUPPORTED_CHAR,                       // '+', (0x2B)
  SEG_DP,                                     // ',', (0x2C)
  SEG_G,                                      // '-', (0x2D)
  SEG_DP,                                     // '.', (0x2E)
  SEG_UNSUPPORTED_CHAR,                       // '/', (0x2F)
  SEG_A|SEG_B|SEG_C|SEG_D|SEG_E|SEG_F,        // '0', (0x30)
  SEG_B|SEG_C,                                // '1', (0x31)
  SEG_A|SEG_B|SEG_D|SEG_E|SEG_G,              // '2', (0x32)
  SEG_A|SEG_B|SEG_C|SEG_D|SEG_G,              // '3', (0x33)
  SEG_B|SEG_C|SEG_F|SEG_G,                    // '4', (0x34)
  SEG_A|SEG_C|SEG_D|SEG_F|SEG_G,              // '5', (0x35)
  SEG_A|SEG_C|SEG_D|SEG_E|SEG_F|SEG_G,        // '6', (0x36)
  SEG_A|SEG_B|SEG_C,                          // '7', (0x37)
  SEG_A|SEG_B|SEG_C|SEG_D|SEG_E|SEG_F|SEG_G,  // '8', (0x38)
  SEG_A|SEG_B|SEG_C|SEG_D|SEG_F|SEG_G,        // '9', (0x39)
  SEG_UNSUPPORTED_CHAR,                       // ':', (0x3A)
  SEG_UNSUPPORTED_CHAR,                       // ';', (0x3B)
  SEG_UNSUPPORTED_CHAR,                       // '<', (0x3C)
  SEG_D|SEG_G,                                // '=', (0x3D)
  SEG_UNSUPPORTED_CHAR,                       // '>', (0x3E)
  SEG_A|SEG_B|SEG_E|SEG_G,                    // '?', (0x3F)
  SEG_A|SEG_B|SEG_D|SEG_E|SEG_F|SEG_G,        // '@', (0x40)
  SEG_A|SEG_B|SEG_C|SEG_E|SEG_F|SEG_G,        // 'A', (0x41)
  SEG_C|SEG_D|SEG_E|SEG_F|SEG_G,              // 'B', (0x42)
  SEG_A|SEG_D|SEG_E|SEG_F,                    // 'C', (0x43)
  SEG_B|SEG_C|SEG_D|SEG_E|SEG_G,              // 'D', (0x44)
  SEG_A|SEG_D|SEG_E|SEG_F|SEG_G,              // 'E', (0x45)
  SEG_A|SEG_E|SEG_F|SEG_G,                    // 'F', (0x46)
  SEG_A|SEG_C|SEG_D|SEG_E|SEG_F,              // 'G', (0x47)
  SEG_B|SEG_C|SEG_E|SEG_F|SEG_G,              // 'H', (0x48)
  SEG_B|SEG_C,                                // 'I', (0x49)
  SEG_B|SEG_C|SEG_D|SEG_E,                    // 'J', (0x4A)
  SEG_UNSUPPORTED_CHAR,                       // 'K', (0x4B)
  SEG_D|SEG_E|SEG_F,                          // 'L', (0x4C)
  SEG_UNSUPPORTED_CHAR,                       // 'M', (0x4D)
  SEG_C|SEG_E|SEG_G,                          // 'N', (0x4E)
  SEG_A|SEG_B|SEG_C|SEG_D|SEG_E|SEG_F,        // 'O', (0x4F)
  SEG_A|SEG_B|SEG_E|SEG_F|SEG_G,              // 'P', (0x50)
  SEG_UNSUPPORTED_CHAR,                       // 'Q', (0x51)
  SEG_E|SEG_G,                                // 'R', (0x52)
  SEG_A|SEG_C|SEG_D|SEG_F|SEG_G,              // 'S', (0x53)
  SEG_UNSUPPORTED_CHAR,                       // 'T', (0x54)
  SEG_B|SEG_C|SEG_D|SEG_E|SEG_F,              // 'U', (0x55)
  SEG_UNSUPPORTED_CHAR,                       // 'V', (0x56)
  SEG_UNSUPPORTED_CHAR,                       // 'W', (0x57)
  SEG_UNSUPPORTED_CHAR,                       // 'X', (0x58)
  SEG_B|SEG_E|SEG_F|SEG_G,                    // 'Y', (0x59)
  SEG_UNSUPPORTED_CHAR,                       // 'Z', (0x5A)
  SEG_A|SEG_D|SEG_E|SEG_F,                    // '[', (0x5B)
  SEG_UNSUPPORTED_CHAR,                       // '\', (0x5C)
  SEG_A|SEG_B|SEG_C|SEG_D,                    // ']', (0x5D)
  SEG_UNSUPPORTED_CHAR,                       // '^', (0x5E)
  SEG_D,                                      // '_', (0x5F)
  SEG_F,                                      // '`', (0x60)
  SEG_A|SEG_B|SEG_C|SEG_E|SEG_F|SEG_G,        // 'a', (0x61)
  SEG_C|SEG_D|SEG_E|SEG_F|SEG_G,              // 'b', (0x62)
  SEG_D|SEG_E|SEG_G,                          // 'c', (0x63)
  SEG_B|SEG_C|SEG_D|SEG_E|SEG_G,              // 'd', (0x64)
  SEG_A|SEG_D|SEG_E|SEG_F|SEG_G,              // 'e', (0x65)
  SEG_A|SEG_E|SEG_F|SEG_G,                    // 'f', (0x66)
  SEG_A|SEG_C|SEG_D|SEG_E|SEG_F,              // 'g', (0x67)
  SEG_C|SEG_E|SEG_F|SEG_G,                    // 'h', (0x68)
  SEG_C,                                      // 'i', (0x69)
  SEG_B|SEG_C|SEG_D|SEG_E,                    // 'j', (0x6A)
  SEG_UNSUPPORTED_CHAR,                       // 'k', (0x6B)
  SEG_D|SEG_E|SEG_F,                          // 'l', (0x6C)
  SEG_UNSUPPORTED_CHAR,                       // 'm', (0x6D)
  SEG_C|SEG_E|SEG_G,                          // 'n', (0x6E)
  SEG_C|SEG_D|SEG_E|SEG_G,                    // 'o', (0x6F)
  SEG_A|SEG_B|SEG_E|SEG_F|SEG_G,              // 'p', (0x70)
  SEG_UNSUPPORTED_CHAR,                       // 'q', (0x71)
  SEG_E|SEG_G,                                // 'r', (0x72)
  SEG_A|SEG_C|SEG_D|SEG_F|SEG_G,              // 's', (0x73)
  SEG_UNSUPPORTED_CHAR,                       // 't', (0x74)
  SEG_C|SEG_D|SEG_E,                          // 'u', (0x75)
  SEG_UNSUPPORTED_CHAR,                       // 'v', (0x76)
  SEG_UNSUPPORTED_CHAR,                       // 'w', (0x77)
  SEG_UNSUPPORTED_CHAR,                       // 'x', (0x78)
  SEG_B|SEG_E|SEG_F|SEG_G,                    // 'y', (0x79)
  SEG_UNSUPPORTED_CHAR,                       // 'z', (0x7A)
  SEG_B|SEG_C|SEG_G,                          // '{', (0x7B)
  SEG_UNSUPPORTED_CHAR,                       // '|', (0x7C)
  SEG_E|SEG_F|SEG_G,                          // '}', (0x7D)
  SEG_UNSUPPORTED_CHAR,                       // '~', (0x7E)
};

// MAX6921Component *global_max6921;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)


void MAX6921Component::init_display_(void) {
  uint32_t freq;
  const uint32_t PWM_FREQ_WANTED = 5000;
  const uint8_t PWM_RESOLUTION = 8;

  // setup PWM for blank pin (intensity)...
  this->display_.intensity.pwm_channel = 0;
  freq = ledcSetup(this->display_.intensity.pwm_channel, PWM_FREQ_WANTED, PWM_RESOLUTION);
  if (freq != 0) {
    ledcAttachPin(this->display_.intensity.pwm_pin->get_pin(), this->display_.intensity.pwm_channel);
    this->display_.intensity.max_duty = pow(2,PWM_RESOLUTION);                      // max. duty value for given resolution
    this->display_.intensity.duty_quotient = this->display_.intensity.max_duty / MAX_DISPLAY_INTENSITY; // pre-calc fixed duty quotient (256 / 16)
    ESP_LOGD(TAG, "Prepare intensity PWM: pin=%u, channel=%u, freq=%uHz, resolution=%ubit, duty quotient=%u",
             this->display_.intensity.pwm_pin->get_pin(),
             this->display_.intensity.pwm_channel, freq, PWM_RESOLUTION,
             this->display_.intensity.duty_quotient);
  } else {
    ESP_LOGE(TAG, "Failed to configure PWM -> set to max. intensity");
    pinMode(this->display_.intensity.pwm_pin->get_pin(), OUTPUT);
    this->disable_blank();      // enable display (max. intensity)
  }

  // display output buffer...
  this->display_.out_buf_size_ = this->display_.num_digits * 3;
  this->display_.out_buf_ = new uint8_t[this->display_.out_buf_size_];  // NOLINT
  memset(this->display_.out_buf_, 0, this->display_.out_buf_size_);

  // display text buffer...
  this->display_.current_text_buf_size = this->display_.num_digits * 2 + 1;     // twice number of digits, because of possible points + string terminator
  this->display_.current_text = new char[this->display_.current_text_buf_size]; // NOLINT
  this->display_.current_text[0] = 0;
  this->display_.text_changed = false;

  // display position...
  this->display_.current_pos = 1;

  // find smallest segment DOUT number...
  this->display_.seg_out_smallest = 19;
  for (uint8_t i=0; i<this->display_.seg_to_out_map.size(); i++) {
    if (this->display_.seg_to_out_map[i] < this->display_.seg_out_smallest)
      this->display_.seg_out_smallest = this->display_.seg_to_out_map[i];
  }

  // calculate refresh period for 60Hz
  this->display_.refresh_period_us = 1000000 / 60 / this->display_.num_digits;
  ESP_LOGD(TAG, "Set display refresh period: %" PRIu32 "us for %u digits @ 60Hz",
           this->display_.refresh_period_us, this->display_.num_digits);
}

void MAX6921Component::init_font_(void) {
  uint8_t seg_data;

  this->ascii_out_data_ = new uint8_t[ARRAY_ELEM_COUNT(ASCII_TO_SEG)];  // NOLINT

  for (size_t ascii_idx=0; ascii_idx<ARRAY_ELEM_COUNT(ASCII_TO_SEG); ascii_idx++) {
    this->ascii_out_data_[ascii_idx] = 0;
    seg_data = progmem_read_byte(&ASCII_TO_SEG[ascii_idx]);
    if (seg_data & SEG_A)
      this->ascii_out_data_[ascii_idx] |= (1 << (this->display_.seg_to_out_map[0] % 8));
    if (seg_data & SEG_B)
      this->ascii_out_data_[ascii_idx] |= (1 << (this->display_.seg_to_out_map[1] % 8));
    if (seg_data & SEG_C)
      this->ascii_out_data_[ascii_idx] |= (1 << (this->display_.seg_to_out_map[2] % 8));
    if (seg_data & SEG_D)
      this->ascii_out_data_[ascii_idx] |= (1 << (this->display_.seg_to_out_map[3] % 8));
    if (seg_data & SEG_E)
      this->ascii_out_data_[ascii_idx] |= (1 << (this->display_.seg_to_out_map[4] % 8));
    if (seg_data & SEG_F)
      this->ascii_out_data_[ascii_idx] |= (1 << (this->display_.seg_to_out_map[5] % 8));
    if (seg_data & SEG_G)
      this->ascii_out_data_[ascii_idx] |= (1 << (this->display_.seg_to_out_map[6] % 8));
    if (seg_data & SEG_DP)
      this->ascii_out_data_[ascii_idx] |= (1 << (this->display_.seg_to_out_map[7] % 8));
  }
}

/**
 * @brief Clears the whole display buffer or only the given position.
 *
 * @param pos display position 0..n (optional, default=whole display)
 */
void MAX6921Component::clear_display(int pos) {
  if (pos < 0)
    memset(this->display_.out_buf_, 0, this->display_.out_buf_size_);           // clear whole display buffer
  else if (pos < this->display_.num_digits)
    memset(&this->display_.out_buf_[pos*3], 0, 3);                              // clear display buffer at given position
  else
    ESP_LOGW(TAG, "Invalid display position %i (max=%u)", pos, this->display_.num_digits - 1);
}

/**
 * @brief Shows the given text on the display at given position.
 *
 * @param start_pos display position 0..n
 * @param str text to display
 *
 * @return number of characters displayed
 */
int MAX6921Component::set_display(uint8_t start_pos, const char *str) {
  uint8_t pos = start_pos;

  for (; *str != '\0'; str++) {
    uint32_t out_data;
    if (pos >= this->display_.num_digits) {
      ESP_LOGE(TAG, "MAX6921 string too long or invalid position for the display!");
      break;
    }

    // create segment data...
    ESP_LOGV(TAG, "%s(): pos: %u, char: '%c' (0x%02x)", __func__, pos+1, *str, *str);
    if ((*str >= ' ') &&
        ((*str - ' ') < ARRAY_ELEM_COUNT(ASCII_TO_SEG)))                        // supported char?
      out_data = this->ascii_out_data_[*str - ' '];                             // yes ->
    else
      out_data = SEG_UNSUPPORTED_CHAR;
    ESP_LOGV(TAG, "%s(): segment data: 0x%06x", __func__, out_data);
    #if 0
    // At the moment an unsupport character is equal to blank (' ').
    // To distinguish an unsupported character from blank we would need to
    // increase font data type from uint8_t to uint16_t!
    if (out_data == SEG_UNSUPPORTED_CHAR) {
      ESP_LOGW(TAG, "Encountered character '%c (0x%02x)' with no display representation!", *str, *str);
    }
    #endif
    if (((*str == ',') || (*str == '.')) && (pos > start_pos))                  // is point/comma?
      pos--;                                                                    // yes -> modify display buffer of previous position
    else
      this->clear_display(pos);                                                 // no -> clear display buffer of current position (for later OR operation)

    // shift data to the smallest segment OUT position...
    out_data <<= (this->display_.seg_out_smallest);
    ESP_LOGV(TAG, "%s(): segment data shifted to first segment bit (OUT%u): 0x%06x",
                  __func__, this->display_.seg_out_smallest, out_data);

    // add position data...
    out_data |= (1 << DISP_POS_TO_OUT[pos]);
    ESP_LOGV(TAG, "%s(): OUT data with position: 0x%06x", __func__, out_data);

    // write to appropriate position of display buffer...
    this->display_.out_buf_[pos*3+0] |= (uint8_t)((out_data >> 16) & 0xFF);
    this->display_.out_buf_[pos*3+1] |= (uint8_t)((out_data >> 8) & 0xFF);
    this->display_.out_buf_[pos*3+2] |= (uint8_t)(out_data & 0xFF);
    ESP_LOGV(TAG, "%s(): display buffer of position %u: 0x%02x%02x%02x",
                  __func__, pos+1, this->display_.out_buf_[pos*3+0],
                  this->display_.out_buf_[pos*3+1], this->display_.out_buf_[pos*3+2]);

    pos++;
  }
  this->display_.text_changed = true;

  return pos - start_pos;
}

void MAX6921Component::update_display_(void) {
  // handle display intensity...
  if (this->display_.intensity.config_changed) {
    // calc duty for low-active BLANK pin...
    uint32_t inverted_duty = this->display_.intensity.max_duty - \
                             this->display_.intensity.duty_quotient * \
                             this->display_.intensity.config_value;
    ESP_LOGD(TAG, "Change display intensity to %u (off-time duty=%u/%u)",
             this->display_.intensity.config_value, inverted_duty,
             this->display_.intensity.max_duty);
    ledcWrite(this->display_.intensity.pwm_channel, inverted_duty);
    this->display_.intensity.config_changed = false;
  }

  // handle demo modes...
  switch (this->get_demo_mode()) {
    case DEMO_MODE_SCROLL_FONT:
      this->update_demo_mode_scroll_font_();
      break;
    default:
      break;
  }
}

void MAX6921Component::update_demo_mode_scroll_font_(void) {
  static char *text = new char[this->display_.num_digits + 2 + 1];              // comma and point do not use an extra display position
  static uint8_t start_pos = this->display_.num_digits - 1;                     // start at right side
  static uint start_font_idx = 0;
  uint font_idx, text_idx = 0, char_with_point_num = 0;

  // build text...
  font_idx = start_font_idx;
  ESP_LOGV(TAG, "%s(): ENTRY: start-pos=%u, start-font-idx=%u",
           __func__, start_pos, start_font_idx);
  do {
    if (text_idx < start_pos) {                                                 // before start position?
      text[text_idx++] = ' ';                                                   // add blank to string
    } else {
      if (this->ascii_out_data_[font_idx] > 0) {                                // displayable character?
        text[text_idx] = ' ' + font_idx;                                        // add character to string
        if ((text[text_idx] == '.') || (text[text_idx] == ',')) {               // point-only character?
          if (text_idx > 0) {                                                   // yes -> point after a character?
            ++text_idx;
            ++char_with_point_num;
          }
        } else
          ++text_idx;
      }
      font_idx = (font_idx + 1) % ARRAY_ELEM_COUNT(ASCII_TO_SEG);               // next font character
    }
    ESP_LOGV(TAG, "%s(): LOOP: pos=%u, font-idx=%u, char='%c'",
             __func__, (text_idx>0)?text_idx-1:0, font_idx, (text_idx>0)?text[text_idx-1]:' ');
  } while ((text_idx - char_with_point_num) < this->display_.num_digits);
  text[text_idx] = 0;
  // determine next start font index...
  if (start_pos == 0) {
    start_font_idx = text[1] - ' ';
  }
  // update display start position...
  if (start_pos > 0)
    --start_pos;
  ESP_LOGV(TAG, "%s(): EXIT: start-pos=%u, start-font-idx=%u, text={%s}",
           __func__, start_pos, start_font_idx, text);

  this->set_display(0, text);

}

float MAX6921Component::get_setup_priority() const { return setup_priority::HARDWARE; }

void MAX6921Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up MAX6921...");
  // global_max6921 = this;

  this->spi_setup();
  this->load_pin_->setup();
  this->load_pin_->pin_mode(gpio::FLAG_OUTPUT);
  this->disable_load();                                                         // disable output latch
  this->init_display_();
  this->init_font_();


  /* Setup display refresh.
   * Using a timer is not an option, because the WiFi component uses timer as
   * well, which leads to unstable refresh cycles (flickering). Therefore a
   * thread on 2nd MCU core is used.
   */
  xTaskCreatePinnedToCore(&MAX6921Component::display_refresh_task,
                          "display_refresh_task", // name
                          2048,                   // stack size
                          this,                   // pass component pointer as task parameter pv
                          1,                      // priority (one above IDLE task)
                          nullptr,                // handle
                          1                       // core
  );

  this->setup_finished = true;
}

void MAX6921Component::dump_config() {
  char seg_name[3];
  ESP_LOGCONFIG(TAG, "MAX6921:");
  LOG_PIN("  LOAD Pin: ", this->load_pin_);
  ESP_LOGCONFIG(TAG, "  BLANK Pin: GPIO%u", this->display_.intensity.pwm_pin->get_pin());
  // display segment to DOUTx mapping...
  for (uint i=0; i<this->display_.seg_to_out_map.size(); i++) {
    if (i < 7) {
      seg_name[0] = 'a' + i;
      seg_name[1] = 0;
    } else
      strncpy(seg_name, "dp", sizeof(seg_name));
    ESP_LOGCONFIG(TAG, "  Display segment %s: OUT%u", seg_name, this->display_.seg_to_out_map[i]);
  }
  ESP_LOGCONFIG(TAG, "  Number of digits: %u", this->display_.num_digits);
  ESP_LOGCONFIG(TAG, "  Intensity: %u", this->display_.intensity.config_value);
  ESP_LOGCONFIG(TAG, "  Demo mode: %u", this->display_.demo_mode);
}

void MAX6921Component::set_intensity(uint8_t intensity) {
  if (!this->setup_finished) {
    ESP_LOGD(TAG, "Set intensity: setup not finished -> discard intensity value");
    return;
  }
  if (intensity > MAX_DISPLAY_INTENSITY) {
    ESP_LOGV(TAG, "Invalid intensity: %u (0..%u)", intensity, MAX_DISPLAY_INTENSITY);
    intensity = MAX_DISPLAY_INTENSITY;
  }
  if ((intensity == 0) || (intensity != this->display_.intensity.config_value)) {
    this->display_.intensity.config_value = intensity;
    ESP_LOGD(TAG, "Set intensity: %u", this->display_.intensity.config_value);
    this->display_.intensity.config_changed = true;
  }
}

/**
 * @brief Clocks data into MAX6921 via SPI (MSB first).
 *        Data must contain 3 bytes with following format:
 *          bit  | 23 | 22 | 21 | 20 | 19 | 18 | ... | 1 | 0
 *          ------------------------------------------------
 *          DOUT | x  | x  | x  | x  | 19 | 18 | ... | 1 | 0
 */
void HOT MAX6921Component::write_data(uint8_t *ptr, size_t length) {
  uint8_t data[3];
  static bool first_call_logged = false;

  assert(length == 3);
  this->disable_load();                   // set LOAD to low
  memcpy(data, ptr, sizeof(data));        // make copy of data, because transfer buffer will be overwritten with SPI answer
  if (!first_call_logged)
    ESP_LOGVV(TAG, "SPI(%u): 0x%02x%02x%02x", length, data[0], data[1], data[2]);
    first_call_logged = true;
  this->transfer_array(data, sizeof(data));
  this->enable_load();                    // set LOAD to high to update output latch
}

void MAX6921Component::update() {
  this->update_display_();

  if (this->writer_.has_value())
    (*this->writer_)(*this);
}

void HOT MAX6921Component::display_refresh_task(void *pv) {
  MAX6921Component *max6921_comp = (MAX6921Component*)pv;
  static uint count = max6921_comp->display_.num_digits;

  if (max6921_comp->display_.refresh_period_us == 0) {
    ESP_LOGE(TAG, "Invalid display refresh period -> using default 2ms");
    max6921_comp->display_.refresh_period_us = 2000;
  }

  while (true) {
    // one-time debug output after any text change for all digits...
    if (max6921_comp->display_.text_changed) {
      count = 0;
      max6921_comp->display_.text_changed = false;
    }
    if (count < max6921_comp->display_.num_digits) {
      count++;
      ESP_LOGVV(TAG, "%s(): SPI transfer for position %u: 0x%02x%02x%02x", __func__,
                max6921_comp->display_.current_pos,
                max6921_comp->display_.out_buf_[(max6921_comp->display_.current_pos-1)*3],
                max6921_comp->display_.out_buf_[(max6921_comp->display_.current_pos-1)*3+1],
                max6921_comp->display_.out_buf_[(max6921_comp->display_.current_pos-1)*3+2]);
    }

    // write MAX9621 data of current position...
    max6921_comp->write_data(&max6921_comp->display_.out_buf_[(max6921_comp->display_.current_pos-1)*3], 3);

    // next display position...
    if (++max6921_comp->display_.current_pos > max6921_comp->display_.num_digits)
      max6921_comp->display_.current_pos = 1;

    delayMicroseconds(max6921_comp->display_.refresh_period_us);
  }
}


/*
 * Evaluates lambda function
 *   start_pos: 0..n = left..right display position
 *   str      : display text
 */
uint8_t MAX6921Component::print(uint8_t start_pos, const char *str) {
  if ((this->get_demo_mode() != DEMO_MODE_OFF) ||                               // demo mode enabled or
      (strcmp(str, this->display_.current_text) == 0))                          // display text not changed?
    return strlen(str);                                                         // yes -> abort
  ESP_LOGV(TAG, "%s(): text changed: str=%s, prev=%s", __func__, str, this->display_.current_text);
  strncpy(this->display_.current_text, str, this->display_.current_text_buf_size-1);
  this->display_.current_text[this->display_.current_text_buf_size-1] = 0;

  return this->set_display(start_pos, str);
}

uint8_t MAX6921Component::print(const char *str) { return this->print(0, str); }

uint8_t MAX6921Component::strftime(uint8_t pos, const char *format, ESPTime time) {
  char buffer[64];
  size_t ret = time.strftime(buffer, sizeof(buffer), format);
  if (ret > 0)
    return this->print(pos, buffer);
  return 0;
}
uint8_t MAX6921Component::strftime(const char *format, ESPTime time) { return this->strftime(0, format, time); }

void MAX6921Component::set_writer(max6921_writer_t &&writer) { this->writer_ = writer; }

}  // namespace max6921
}  // namespace esphome
