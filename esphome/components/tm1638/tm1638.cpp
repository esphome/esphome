#include "tm1638.h"
#include "sevenseg.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace tm1638 {

static const char *const TAG = "display.tm1638";
static const uint8_t TM1638_REGISTER_FIXEDADDRESS = 0x44;
static const uint8_t TM1638_REGISTER_AUTOADDRESS = 0x40;
static const uint8_t TM1638_REGISTER_READBUTTONS = 0x42;
static const uint8_t TM1638_REGISTER_DISPLAYOFF = 0x80;
static const uint8_t TM1638_REGISTER_DISPLAYON = 0x88;
static const uint8_t TM1638_REGISTER_7SEG_0 = 0xC0;
static const uint8_t TM1638_REGISTER_LED_0 = 0xC1;
static const uint8_t TM1638_UNKNOWN_CHAR = 0b11111111;

static const uint8_t TM1638_SHIFT_DELAY = 4;  // clock pause between commands, default 4ms

void TM1638Component::setup() {
  ESP_LOGD(TAG, "Setting up TM1638...");

  this->clk_pin_->setup();  // OUTPUT
  this->dio_pin_->setup();  // OUTPUT
  this->stb_pin_->setup();  // OUTPUT

  this->clk_pin_->pin_mode(gpio::FLAG_OUTPUT);
  this->dio_pin_->pin_mode(gpio::FLAG_OUTPUT);
  this->stb_pin_->pin_mode(gpio::FLAG_OUTPUT);

  this->clk_pin_->digital_write(false);
  this->dio_pin_->digital_write(false);
  this->stb_pin_->digital_write(false);

  this->set_intensity(intensity_);

  this->reset_();  // all LEDs off

  for (uint8_t i = 0; i < 8; i++)  // zero fill print buffer
    this->buffer_[i] = 0;
}

void TM1638Component::dump_config() {
  ESP_LOGCONFIG(TAG, "TM1638:");
  ESP_LOGCONFIG(TAG, "  Intensity: %u", this->intensity_);
  LOG_PIN("  CLK Pin: ", this->clk_pin_);
  LOG_PIN("  DIO Pin: ", this->dio_pin_);
  LOG_PIN("  STB Pin: ", this->stb_pin_);
  LOG_UPDATE_INTERVAL(this);
}

void TM1638Component::loop() {
  if (this->listeners_.empty())
    return;

  uint8_t keys = this->get_keys();
  for (auto &listener : this->listeners_)
    listener->keys_update(keys);
}

uint8_t TM1638Component::get_keys() {
  uint8_t buttons = 0;

  this->stb_pin_->digital_write(false);

  this->shift_out_(TM1638_REGISTER_READBUTTONS);

  this->dio_pin_->pin_mode(gpio::FLAG_INPUT);

  delayMicroseconds(10);

  for (uint8_t i = 0; i < 4; i++) {  // read the 4 button registers
    uint8_t v = this->shift_in_();
    buttons |= v << i;  // shift bits to correct slots in the byte
  }

  this->dio_pin_->pin_mode(gpio::FLAG_OUTPUT);

  this->stb_pin_->digital_write(true);

  return buttons;
}

void TM1638Component::update() {  // this is called at the interval specified in the config.yaml
  if (this->writer_.has_value()) {
    (*this->writer_)(*this);
  }

  this->display();
}

float TM1638Component::get_setup_priority() const { return setup_priority::PROCESSOR; }

void TM1638Component::display() {
  for (uint8_t i = 0; i < 8; i++) {
    this->set_7seg_(i, buffer_[i]);
  }
}

void TM1638Component::reset_() {
  uint8_t num_commands = 16;  // 16 addresses, 8 for 7seg and 8 for LEDs
  uint8_t commands[num_commands];

  for (uint8_t i = 0; i < num_commands; i++) {
    commands[i] = 0;
  }

  this->send_command_sequence_(commands, num_commands, TM1638_REGISTER_7SEG_0);
}

/////////////// LEDs /////////////////

void TM1638Component::set_led(int led_pos, bool led_on_off) {
  this->send_command_(TM1638_REGISTER_FIXEDADDRESS);

  uint8_t commands[2];

  commands[0] = TM1638_REGISTER_LED_0 + (led_pos << 1);
  commands[1] = led_on_off;

  this->send_commands_(commands, 2);
}

void TM1638Component::set_7seg_(int seg_pos, uint8_t seg_bits) {
  this->send_command_(TM1638_REGISTER_FIXEDADDRESS);

  uint8_t commands[2] = {};

  commands[0] = TM1638_REGISTER_7SEG_0 + (seg_pos << 1);
  commands[1] = seg_bits;

  this->send_commands_(commands, 2);
}

void TM1638Component::set_intensity(uint8_t brightness_level) {
  this->intensity_ = brightness_level;

  this->send_command_(TM1638_REGISTER_FIXEDADDRESS);

  if (brightness_level > 0) {
    this->send_command_((uint8_t) (TM1638_REGISTER_DISPLAYON | intensity_));
  } else {
    this->send_command_(TM1638_REGISTER_DISPLAYOFF);
  }
}

/////////////// DISPLAY PRINT /////////////////

uint8_t TM1638Component::print(uint8_t start_pos, const char *str) {
  uint8_t pos = start_pos;

  bool last_was_dot = false;

  for (; *str != '\0'; str++) {
    uint8_t data = TM1638_UNKNOWN_CHAR;

    if (*str >= ' ' && *str <= '~') {
      data = progmem_read_byte(&TM1638Translation::SEVEN_SEG[*str - 32]);  // subract 32 to account for ASCII offset
    } else if (data == TM1638_UNKNOWN_CHAR) {
      ESP_LOGW(TAG, "Encountered character '%c' with no TM1638 representation while translating string!", *str);
    }

    if (*str == '.')  // handle dots
    {
      if (pos != start_pos &&
          !last_was_dot)  // if we are not at the first position, backup by one unless last char was a dot
      {
        pos--;
      }
      this->buffer_[pos] |= 0b10000000;  // turn on the dot on the previous position
      last_was_dot = true;               // set a bit in case the next chracter is also a dot
    } else                               // if not a dot, then just write the character to display
    {
      if (pos >= 8) {
        ESP_LOGI(TAG, "TM1638 String is too long for the display!");
        break;
      }
      this->buffer_[pos] = data;
      last_was_dot = false;  // clear dot tracking bit
    }

    pos++;
  }
  return pos - start_pos;
}

/////////////// PRINT /////////////////

uint8_t TM1638Component::print(const char *str) { return this->print(0, str); }

uint8_t TM1638Component::printf(uint8_t pos, const char *format, ...) {
  va_list arg;
  va_start(arg, format);
  char buffer[64];
  int ret = vsnprintf(buffer, sizeof(buffer), format, arg);
  va_end(arg);
  if (ret > 0)
    return this->print(pos, buffer);
  return 0;
}
uint8_t TM1638Component::printf(const char *format, ...) {
  va_list arg;
  va_start(arg, format);
  char buffer[64];
  int ret = vsnprintf(buffer, sizeof(buffer), format, arg);
  va_end(arg);
  if (ret > 0)
    return this->print(buffer);
  return 0;
}

uint8_t TM1638Component::strftime(uint8_t pos, const char *format, ESPTime time) {
  char buffer[64];
  size_t ret = time.strftime(buffer, sizeof(buffer), format);
  if (ret > 0)
    return this->print(pos, buffer);
  return 0;
}
uint8_t TM1638Component::strftime(const char *format, ESPTime time) { return this->strftime(0, format, time); }

//////////////// SPI   ////////////////

void TM1638Component::send_command_(uint8_t value) {
  this->stb_pin_->pin_mode(gpio::FLAG_OUTPUT);
  this->stb_pin_->digital_write(false);
  this->shift_out_(value);
  this->stb_pin_->digital_write(true);
}

void TM1638Component::send_commands_(uint8_t const commands[], uint8_t num_commands) {
  this->stb_pin_->digital_write(false);

  for (uint8_t i = 0; i < num_commands; i++) {
    uint8_t command = commands[i];
    this->shift_out_(command);
  }
  this->stb_pin_->digital_write(true);
}

void TM1638Component::send_command_leave_open_(uint8_t value) {
  this->stb_pin_->digital_write(false);
  this->shift_out_(value);
}

void TM1638Component::send_command_sequence_(uint8_t commands[], uint8_t num_commands, uint8_t starting_address) {
  this->send_command_(TM1638_REGISTER_AUTOADDRESS);
  this->send_command_leave_open_(starting_address);

  for (uint8_t i = 0; i < num_commands; i++) {
    this->shift_out_(commands[i]);
  }

  this->stb_pin_->digital_write(true);
}

uint8_t TM1638Component::shift_in_() {
  uint8_t value = 0;

  for (int i = 0; i < 8; ++i) {
    value |= dio_pin_->digital_read() << i;
    delayMicroseconds(TM1638_SHIFT_DELAY);
    this->clk_pin_->digital_write(true);
    delayMicroseconds(TM1638_SHIFT_DELAY);
    this->clk_pin_->digital_write(false);
    delayMicroseconds(TM1638_SHIFT_DELAY);
  }
  return value;
}

void TM1638Component::shift_out_(uint8_t val) {
  for (int i = 0; i < 8; i++) {
    this->dio_pin_->digital_write((val & (1 << i)));
    delayMicroseconds(TM1638_SHIFT_DELAY);

    this->clk_pin_->digital_write(true);
    delayMicroseconds(TM1638_SHIFT_DELAY);

    this->clk_pin_->digital_write(false);
    delayMicroseconds(TM1638_SHIFT_DELAY);
  }
}

}  // namespace tm1638
}  // namespace esphome
