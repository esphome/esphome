#include "ht16k33.h"
#include "esphome/core/log.h"
#include "esphome/core/time.h"

namespace esphome {
namespace ht16k33 {

static const char *const TAG = "ht16k33";

constexpr uint8_t HT16K33_REGISTER_RAM = 0x00;
constexpr uint8_t HT16K33_REGISTER_OSCILLATOR = 0x20;
constexpr uint8_t HT16K33_REGISTER_BLINK = 0x80;
constexpr uint8_t HT16K33_REGISTER_INTENSITY = 0xE0;

constexpr uint8_t HT16K33_ON = 0x01;
constexpr uint8_t HT16K33_OFF = 0x00;

constexpr uint8_t HT16K33_BLINK_2HZ = 0x01;
constexpr uint8_t HT16K33_BLINK_1HZ = 0x02;
constexpr uint8_t HT16K33_BLINK_HALFHZ = 0x03;

float HT16K33Component::get_setup_priority() const { return setup_priority::PROCESSOR; }

void HT16K33Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up HT16K33...");
  this->init_reset_();

  delay(10);
  auto err = this->write(nullptr, 0);
  if (err != i2c::ERROR_OK) {
    this->error_code_ = COMMUNICATION_FAILED;
    this->mark_failed();
    ESP_LOGE(TAG, "Error in communication with HT16K33 %i", err);
    return;
  }
  base_address_ = this->address_;

  // turn on oscillator
  this->command_all_(HT16K33_REGISTER_OSCILLATOR | HT16K33_ON);
  ESP_LOGD(TAG, "HT16K33 oscillator turned on");

  // internal RAM powers up with garbage/random values.
  // ensure internal RAM is cleared before turning on display
  // this ensures that no garbage pixels show up on the display
  // when it is turned on.
  this->stepsleft_ = 0;
  for (int chip_line = 0; chip_line < this->num_chip_lines_; chip_line++) {
    std::vector<uint16_t> vec(1);
    this->max_displaybuffer_.push_back(vec);
    // Initialize buffer with 0 for display so all non written pixels are blank
    this->max_displaybuffer_[chip_line].resize(get_width_internal(), 0);
  }
  this->display();
  ESP_LOGD(TAG, "display cleared");

  this->intensity(this->intensity_);
  ESP_LOGD(TAG, "intensity set to %u", this->intensity_);

  this->blink_rate(this->blink_rate_);
  ESP_LOGD(TAG, "blink rate set to off");
}

void HT16K33Component::dump_config() {
  ESP_LOGCONFIG(TAG, "HT16K33:");
  LOG_I2C_DEVICE(this);
  ESP_LOGCONFIG(TAG, "  Number of Chips: %u", this->num_chips_);
  ESP_LOGCONFIG(TAG, "  Number of Chips Lines: %u", this->num_chip_lines_);
  ESP_LOGCONFIG(TAG, "  Chips Lines Style : %u", this->chip_lines_style_);
  ESP_LOGCONFIG(TAG, "  Intensity: %u", this->intensity_);
  ESP_LOGCONFIG(TAG, "  Blink rate: %u", this->blink_rate_);
  ESP_LOGCONFIG(TAG, "  Scroll Enabled: %u", this->scroll_);
  ESP_LOGCONFIG(TAG, "  Scroll Mode: %u", this->scroll_mode_);
  ESP_LOGCONFIG(TAG, "  Scroll Speed: %u", this->scroll_speed_);
  ESP_LOGCONFIG(TAG, "  Scroll Dwell: %u", this->scroll_dwell_);
  ESP_LOGCONFIG(TAG, "  Scroll Delay: %u", this->scroll_delay_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_UPDATE_INTERVAL(this);
}

void HT16K33Component::loop() {
  uint32_t const now = millis();

  // check if the buffer has shrunk past the current position since last update
  if ((this->max_displaybuffer_[0].size() >= this->old_buffer_size_ + 3) ||
      (this->max_displaybuffer_[0].size() <= this->old_buffer_size_ - 3)) {
    this->stepsleft_ = 0;
    this->display();
    this->old_buffer_size_ = this->max_displaybuffer_[0].size();
  }

  // Reset the counter back to 0 when full string has been displayed.
  if (this->stepsleft_ > this->max_displaybuffer_[0].size())
    this->stepsleft_ = 0;

  // Return if there is no need to scroll or scroll is off
  if (!this->scroll_ || (this->max_displaybuffer_[0].size() <= (size_t) get_width_internal())) {
    this->display();
    return;
  }

  if ((this->stepsleft_ == 0) && (now - this->last_scroll_ < this->scroll_delay_)) {
    this->display();
    return;
  }

  // Dwell time at end of string in case of stop at end
  if (this->scroll_mode_ == ScrollMode::STOP) {
    if (this->stepsleft_ >= this->max_displaybuffer_[0].size() - (size_t) get_width_internal() + 1) {
      if (now - this->last_scroll_ >= this->scroll_dwell_) {
        this->stepsleft_ = 0;
        this->last_scroll_ = now;
        this->display();
      }
      return;
    }
  }

  // Actual call to scroll left action
  if (now - this->last_scroll_ >= this->scroll_speed_) {
    this->last_scroll_ = now;
    this->scroll_left();
    this->display();
  }
}

void HT16K33Component::display() {
  uint16_t pixels[8];
  // Run this loop for every CHIP (GRID OF 64 leds)
  // Run this routine for the rows of every chip 8x row 0 top to 7 bottom
  // Fill the pixel parameter with display data
  // Send the data to the chip
  for (uint8_t chip = 0; chip < this->num_chips_ / this->num_chip_lines_; chip++) {
    for (uint8_t chip_line = 0; chip_line < this->num_chip_lines_; chip_line++) {
      for (uint8_t j = 0; j < 8; j++) {
        bool const reverse =
            chip_line % 2 != 0 && this->chip_lines_style_ == ChipLinesStyle::SNAKE ? !this->reverse_ : this->reverse_;
        if (reverse) {
          pixels[j] =
              this->max_displaybuffer_[chip_line][(this->num_chips_ / this->num_chip_lines_ - chip - 1) * 8 + j];
        } else {
          pixels[j] = this->max_displaybuffer_[chip_line][chip * 8 + j];
        }
      }
      if (chip_line % 2 != 0 && this->chip_lines_style_ == ChipLinesStyle::SNAKE)
        this->orientation_ = orientation_180_();
      this->send64pixels(chip_line * this->num_chips_ / this->num_chip_lines_ + chip, pixels);
      if (chip_line % 2 != 0 && this->chip_lines_style_ == ChipLinesStyle::SNAKE)
        this->orientation_ = orientation_180_();
    }
  }
}

uint8_t HT16K33Component::orientation_180_() {
  switch (this->orientation_) {
    case 0:
      return 2;
    case 1:
      return 3;
    case 2:
      return 0;
    case 3:
      return 1;
    default:
      return 0;
  }
}

int HT16K33Component::get_height_internal() { return 8 * this->num_chip_lines_; }

int HT16K33Component::get_width_internal() { return this->num_chips_ / this->num_chip_lines_ * 8; }

void HT16K33Component::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (x + 1 > (int) this->max_displaybuffer_[0].size()) {  // Extend the display buffer in case required
    for (int chip_line = 0; chip_line < this->num_chip_lines_; chip_line++) {
      this->max_displaybuffer_[chip_line].resize(x + 1, color_to_pixel_(this->bckgrnd_));
    }
  }

  if ((y >= this->get_height_internal()) || (y < 0) || (x < 0))  // If pixel is outside display then dont draw
    return;

  uint8_t const pos = x;     // X is starting at 0 top left
  uint8_t const subpos = y;  // Y is starting at 0 top left

  int const cl_mask = 1 << subpos % 8;
  int const ch_mask = 1 << (subpos % 8 + 8);
  uint16_t *pixel_ptr = &this->max_displaybuffer_[subpos / 8][pos];
  if (color == CH) {
    // Turn off cl LED.
    *pixel_ptr &= ~cl_mask;
    // Turn on ch LED.
    *pixel_ptr |= ch_mask;
  } else if (color == CLH) {
    // Turn on ch and cl LED.
    *pixel_ptr |= cl_mask;
    *pixel_ptr |= ch_mask;
  } else if (color == Color::BLACK) {
    // Turn off ch and cl LED.
    *pixel_ptr &= ~cl_mask;
    *pixel_ptr &= ~ch_mask;
  } else {
    // CH or unrecognized color, treat it as CL
    // Turn on cl LED.
    *pixel_ptr |= cl_mask;
    // Turn off ch LED.
    *pixel_ptr &= ~ch_mask;
  }
}

void HT16K33Component::command_(uint8_t value) {
  auto err = this->write_register(value, nullptr, 0);
  if (err != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Error in communication with HT16K33 %i", err);
  }
}

void HT16K33Component::command_all_(uint8_t value) {
  for (uint8_t chip_add = base_address_; chip_add < base_address_ + this->num_chips_; chip_add++) {
    this->set_i2c_address(chip_add);
    this->command_(value);
  }
  this->set_i2c_address(base_address_);
}

void HT16K33Component::update() {
  this->update_ = true;
  this->clear();
  if (this->writer_.has_value())  // insert Labda function if available
    (*this->writer_)(*this);
}

bool HT16K33Component::is_on() { return this->is_on_; }

void HT16K33Component::turn_on() {
  this->command_all_(HT16K33_REGISTER_BLINK | HT16K33_ON);
  this->is_on_ = true;
}

void HT16K33Component::turn_off() {
  this->command_all_(HT16K33_REGISTER_BLINK | HT16K33_OFF);
  this->is_on_ = false;
}

void HT16K33Component::scroll(bool on_off, ScrollMode mode, uint16_t speed, uint16_t delay, uint16_t dwell) {
  this->set_scroll(on_off);
  this->set_scroll_mode(mode);
  this->set_scroll_speed(speed);
  this->set_scroll_dwell(dwell);
  this->set_scroll_delay(delay);
}

void HT16K33Component::scroll(bool on_off, ScrollMode mode) {
  this->set_scroll(on_off);
  this->set_scroll_mode(mode);
}

void HT16K33Component::intensity(uint8_t intensity) {
  this->intensity_ = intensity;
  this->command_all_(HT16K33_REGISTER_INTENSITY | this->intensity_);
}

void HT16K33Component::blink_rate(uint8_t b) {
  this->blink_rate_ = b;
  if (this->blink_rate_ > 3)
    this->blink_rate_ = 0;  // turn off if not sure
  uint8_t const buffer = HT16K33_ON | (this->blink_rate_ << 1);
  this->command_all_(HT16K33_REGISTER_BLINK | buffer);
}

void HT16K33Component::scroll(bool on_off) { this->set_scroll(on_off); }

void HT16K33Component::scroll_left() {
  for (int chip_line = 0; chip_line < this->num_chip_lines_; chip_line++) {
    if (this->update_) {
      this->max_displaybuffer_[chip_line].push_back(color_to_pixel_(this->bckgrnd_));
      for (uint16_t i = 0; i < this->stepsleft_; i++) {
        this->max_displaybuffer_[chip_line].push_back(this->max_displaybuffer_[chip_line].front());
        this->max_displaybuffer_[chip_line].erase(this->max_displaybuffer_[chip_line].begin());
      }
    } else {
      this->max_displaybuffer_[chip_line].push_back(this->max_displaybuffer_[chip_line].front());
      this->max_displaybuffer_[chip_line].erase(this->max_displaybuffer_[chip_line].begin());
    }
  }
  this->update_ = false;
  this->stepsleft_++;
}

// send one character (data) to position (chip)
void HT16K33Component::send64pixels(uint8_t chip, const uint16_t pixels[8]) {
  for (uint8_t col = 0; col < 8; col++) {  // RUN THIS LOOP 8 times until column is 7
    uint16_t b = 0;                        // rotate pixels 90 degrees -- set byte to 0
    if (this->orientation_ == 0) {
      for (uint8_t i = 0; i < 8; i++) {
        // run this loop 8 times for all the pixels[8] received
        if (this->flip_x_) {
          b |= ((pixels[i] >> col) & 1) << i;  // change the column bits into row bits
          b |= ((pixels[i] >> (col + 8)) & 1) << (i + 8);
        } else {
          b |= ((pixels[i] >> col) & 1) << (7 - i);  // change the column bits into row bits
          b |= ((pixels[i] >> (col + 8)) & 1) << (15 - i);
        }
      }
    } else if (this->orientation_ == 1) {
      b = pixels[col];
    } else if (this->orientation_ == 2) {
      for (uint8_t i = 0; i < 8; i++) {
        if (this->flip_x_) {
          b |= ((pixels[i] >> (7 - col)) & 1) << (7 - i);
          b |= ((pixels[i] >> (15 - col)) & 1) << (15 - i);
        } else {
          b |= ((pixels[i] >> (7 - col)) & 1) << i;
          b |= ((pixels[i] >> (15 - col)) & 1) << (i + 8);
        }
      }
    } else {
      for (uint8_t i = 0; i < 8; i++) {
        b |= ((pixels[7 - col] >> i) & 1) << (7 - i);
        b |= ((pixels[7 - col] >> (i + 8)) & 1) << (15 - i);
      }
    }
    b = reverse_bits(b);
    // send this byte to display at selected chip
    this->set_i2c_address(base_address_ | chip << 1);
    this->write_byte_16(HT16K33_REGISTER_RAM | col << 1, b);
  }
}

uint16_t HT16K33Component::color_to_pixel_(Color color) {
  if (color == CL) {
    return 0x00FF;
  } else if (color == CH) {
    return 0xFF00;
  } else if (color == Color::BLACK) {
    return 0x0000;
  } else {
    return 0xFFFF;
  }
}

void HT16K33Component::clear() { fill(this->bckgrnd_); }

void HT16K33Component::fill(Color color) {
  for (int chip_line = 0; chip_line < this->num_chip_lines_; chip_line++) {
    this->max_displaybuffer_[chip_line].clear();
    this->max_displaybuffer_[chip_line].resize(get_width_internal(), color_to_pixel_(color));
  }
}

void HT16K33Component::init_reset_() {
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();
    this->reset_pin_->digital_write(true);
    delay(1);
    // Trigger Reset
    this->reset_pin_->digital_write(false);
    delay(10);
    // Wake up
    this->reset_pin_->digital_write(true);
  }
}

}  // namespace ht16k33
}  // namespace esphome
