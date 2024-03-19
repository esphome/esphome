// Datasheet: https://www.analog.com/media/en/technical-documentation/data-sheets/MAX6921-MAX6931.pdf

#include "max6921.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace max6921 {

#define ARRAY_ELEM_COUNT(array)  (sizeof(array)/sizeof(array[0]))

static const char *const TAG = "max6921";

// display segment to DOUTx mapping...
static const uint8_t DISP_SEG_TO_OUT[] = {
// a  b  c  d  e  f  g  dp
   0, 2, 5, 6, 4, 1, 3, 7
};

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



void MAX6921Component::init_display_(void) {
  this->display_.out_buf_size_ = this->display_.num_digits * 3;
  this->display_.out_buf_ = new uint8_t[this->display_.out_buf_size_];  // NOLINT
  memset(this->display_.out_buf_, 0, this->display_.out_buf_size_);

  this->display_.current_text = new char[this->display_.num_digits + 1];  // NOLINT
  this->display_.current_text[0] = 0;

  this->display_.current_pos = 1;

  // find smallest segment DOUT number...
  this->display_.seg_out_smallest = 19;
  for (uint8_t i=0; i<ARRAY_ELEM_COUNT(DISP_SEG_TO_OUT); i++) {
    if (DISP_SEG_TO_OUT[i] < this->display_.seg_out_smallest)
      this->display_.seg_out_smallest = DISP_SEG_TO_OUT[i];
  }
}


void MAX6921Component::init_font_(void) {
  uint8_t seg_data;

  this->ascii_out_data_ = new uint8_t[ARRAY_ELEM_COUNT(ASCII_TO_SEG)];  // NOLINT

  for (size_t ascii_idx=0; ascii_idx<ARRAY_ELEM_COUNT(ASCII_TO_SEG); ascii_idx++) {
    this->ascii_out_data_[ascii_idx] = 0;
    seg_data = progmem_read_byte(&ASCII_TO_SEG[ascii_idx]);
    if (seg_data & SEG_A)
      this->ascii_out_data_[ascii_idx] |= (1 << (DISP_SEG_TO_OUT[0] % 8));
    if (seg_data & SEG_B)
      this->ascii_out_data_[ascii_idx] |= (1 << (DISP_SEG_TO_OUT[1] % 8));
    if (seg_data & SEG_C)
      this->ascii_out_data_[ascii_idx] |= (1 << (DISP_SEG_TO_OUT[2] % 8));
    if (seg_data & SEG_D)
      this->ascii_out_data_[ascii_idx] |= (1 << (DISP_SEG_TO_OUT[3] % 8));
    if (seg_data & SEG_E)
      this->ascii_out_data_[ascii_idx] |= (1 << (DISP_SEG_TO_OUT[4] % 8));
    if (seg_data & SEG_F)
      this->ascii_out_data_[ascii_idx] |= (1 << (DISP_SEG_TO_OUT[5] % 8));
    if (seg_data & SEG_G)
      this->ascii_out_data_[ascii_idx] |= (1 << (DISP_SEG_TO_OUT[6] % 8));
    if (seg_data & SEG_DP)
      this->ascii_out_data_[ascii_idx] |= (1 << (DISP_SEG_TO_OUT[7] % 8));
  }
}


float MAX6921Component::get_setup_priority() const { return setup_priority::BUS; }

void MAX6921Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up MAX6921...");
  this->spi_setup();
  this->load_pin_->setup();
  this->load_pin_->pin_mode(gpio::FLAG_OUTPUT);
  this->blank_pin_->setup();
  this->blank_pin_->pin_mode(gpio::FLAG_OUTPUT);
  this->disable_load();       // disable output latch
  this->enable_blank();       // disable display
  this->init_display_();
  this->init_font_();
  this->disable_blank();      // enable display (max. intensity)
}

void MAX6921Component::dump_config() {
  ESP_LOGCONFIG(TAG, "MAX6921:");
  LOG_PIN("  LOAD Pin: ", this->load_pin_);
  LOG_PIN("  BLANK Pin: ", this->blank_pin_);
}

/*
 * Clocks data into MAX6921 via SPI (MSB first).
 * Data must contain 3 bytes with following format:
 *   bit  | 23 | 22 | 21 | 20 | 19 | 18 | ... | 1 | 0
 *   ------------------------------------------------
 *   DOUT | x  | x  | x  | x  | 19 | 18 | ... | 1 | 0
 */
void MAX6921Component::write_data(uint8_t *ptr, size_t length) {
  uint8_t data[3];
  static bool once = false;

  assert(length == 3);
  this->disable_load();                   // set LOAD to low
  memcpy(data, ptr, sizeof(data));        // make copy of data, because transfer buffer will be overwritten with SPI answer
  if (!once)
    ESP_LOGD(TAG, "SPI(%u): 0x%02x%02x%02x", length, data[0], data[1], data[2]);
  this->transfer_array(data, sizeof(data));
  this->enable_load();                    // set LOAD to high to update output latch
  // delayMicroseconds(10);
  once = true;
}

void MAX6921Component::update() {
  /*
  if (this->intensity_changed_) {
    this->send_to_all_(MAX6921_REGISTER_INTENSITY, this->intensity_);
    this->intensity_changed_ = false;
  }
  for (uint8_t i = 0; i < this->num_chips_ * 8; i++)
    this->buffer_[i] = 0;
  */
  if (this->writer_.has_value())
    (*this->writer_)(*this);
  this->display();
}

void MAX6921Component::display() {
  static uint count = 0;

  if (count < this->display_.num_digits) {
    count++;
    ESP_LOGD(TAG, "%s(): SPI transfer for position %u: 0x%02x%02x%02x", __func__,
             this->display_.current_pos,
             this->display_.out_buf_[(this->display_.current_pos-1)*3],
             this->display_.out_buf_[(this->display_.current_pos-1)*3+1],
             this->display_.out_buf_[(this->display_.current_pos-1)*3+2]);
  }
  // write MAX9621 data of current position...
  this->write_data(&this->display_.out_buf_[(this->display_.current_pos-1)*3], 3);

  // next display position...
  if (++this->display_.current_pos > this->display_.num_digits)
    this->display_.current_pos = 1;
}

uint8_t MAX6921Component::print(uint8_t start_pos, const char *str) {
  uint8_t pos = start_pos;

//  ESP_LOGD(TAG, "%s(): str=%s, prev=%s", __func__, str, this->display_.current_text);
  if (strcmp(str, this->display_.current_text) == 0)                            // display text not changed?
    return strlen(str);                                                         // yes -> abort
  strncpy(this->display_.current_text, str, this->display_.num_digits);
  this->display_.current_text[this->display_.num_digits] = 0;

  for (; *str != '\0'; str++) {
    uint32_t out_data;
    if (pos >= this->display_.num_digits) {
      ESP_LOGE(TAG, "MAX6921 string too long or invalid position for the display!");
      break;
    }

    // create segment data...
    ESP_LOGD(TAG, "%s(): pos: %u, char: '%c' (0x%02x)", __func__, pos+1, *str, *str);
    if ((*str >= ' ') &&
        ((*str - ' ') < ARRAY_ELEM_COUNT(ASCII_TO_SEG)))                        // supported char?
      out_data = this->ascii_out_data_[*str - ' '];                             // yes ->
    else
      out_data = SEG_UNSUPPORTED_CHAR;
    ESP_LOGD(TAG, "%s(): segment data: 0x%06x", __func__, out_data);
    if (out_data == SEG_UNSUPPORTED_CHAR) {
      ESP_LOGW(TAG, "Encountered character '%c' with no display representation!", *str);
    }
    if ((out_data == (uint32_t)SEG_DP) && (pos > start_pos))                    // is point/comma?
      pos--;                                                                    // yes -> modify display buffer of previous position
    else
      memset(&this->display_.out_buf_[pos*3], 0, 3);                            // no -> clear display buffer of current position (for later OR operation)

    // shift data to the smallest segment OUT position...
    out_data <<= (this->display_.seg_out_smallest);
    ESP_LOGD(TAG, "%s(): segment data shifted to first segment bit (OUT%u): 0x%06x",
             __func__, this->display_.seg_out_smallest, out_data);

    // add position data...
    out_data |= (1 << DISP_POS_TO_OUT[pos]);
    ESP_LOGD(TAG, "%s(): OUT data with position: 0x%06x", __func__, out_data);

    // write to appropriate position of display buffer...
    this->display_.out_buf_[pos*3+0] |= (uint8_t)((out_data >> 16) & 0xFF);
    this->display_.out_buf_[pos*3+1] |= (uint8_t)((out_data >> 8) & 0xFF);
    this->display_.out_buf_[pos*3+2] |= (uint8_t)(out_data & 0xFF);
    ESP_LOGD(TAG, "%s(): display buffer of position %u: 0x%02x%02x%02x",
             __func__, pos+1, this->display_.out_buf_[pos*3+0],
             this->display_.out_buf_[pos*3+1], this->display_.out_buf_[pos*3+2]);

    pos++;
  }

  return pos - start_pos;
}

uint8_t MAX6921Component::print(const char *str) { return this->print(0, str); }

void MAX6921Component::set_writer(max6921_writer_t &&writer) { this->writer_ = writer; }

}  // namespace max6921
}  // namespace esphome
