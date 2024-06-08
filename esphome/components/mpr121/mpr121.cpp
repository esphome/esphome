#include "mpr121.h"

#include <cstdint>

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace mpr121 {

static const char *const TAG = "mpr121";

void MPR121Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up MPR121...");
  // soft reset device
  this->write_byte(MPR121_SOFTRESET, 0x63);
  delay(100);  // NOLINT
  if (!this->write_byte(MPR121_ECR, 0x0)) {
    this->error_code_ = COMMUNICATION_FAILED;
    this->mark_failed();
    return;
  }

  // set touch sensitivity for all 12 channels
  for (auto *channel : this->channels_) {
    channel->setup();
  }
  this->write_byte(MPR121_MHDR, 0x01);
  this->write_byte(MPR121_NHDR, 0x01);
  this->write_byte(MPR121_NCLR, 0x0E);
  this->write_byte(MPR121_FDLR, 0x00);

  this->write_byte(MPR121_MHDF, 0x01);
  this->write_byte(MPR121_NHDF, 0x05);
  this->write_byte(MPR121_NCLF, 0x01);
  this->write_byte(MPR121_FDLF, 0x00);

  this->write_byte(MPR121_NHDT, 0x00);
  this->write_byte(MPR121_NCLT, 0x00);
  this->write_byte(MPR121_FDLT, 0x00);

  this->write_byte(MPR121_DEBOUNCE, 0);
  // default, 16uA charge current
  this->write_byte(MPR121_CONFIG1, 0x10);
  // 0.5uS encoding, 1ms period
  this->write_byte(MPR121_CONFIG2, 0x20);

  // Write the Electrode Configuration Register
  // * Highest 2 bits is "Calibration Lock", which we set to a value corresponding to 5 bits.
  // * The 2 bits below is "Proximity Enable" and are left at 0.
  // * The 4 least significant bits control how many electrodes are enabled. Electrodes are enabled
  //   as a range, starting at 0 up to the highest channel index used.
  this->write_byte(MPR121_ECR, 0x80 | (this->max_touch_channel_ + 1));

  this->flush_gpio_();
}

void MPR121Component::set_touch_debounce(uint8_t debounce) {
  uint8_t mask = debounce << 4;
  this->debounce_ &= 0x0f;
  this->debounce_ |= mask;
}

void MPR121Component::set_release_debounce(uint8_t debounce) {
  uint8_t mask = debounce & 0x0f;
  this->debounce_ &= 0xf0;
  this->debounce_ |= mask;
};

void MPR121Component::dump_config() {
  ESP_LOGCONFIG(TAG, "MPR121:");
  LOG_I2C_DEVICE(this);
  switch (this->error_code_) {
    case COMMUNICATION_FAILED:
      ESP_LOGE(TAG, "Communication with MPR121 failed!");
      break;
    case WRONG_CHIP_STATE:
      ESP_LOGE(TAG, "MPR121 has wrong default value for CONFIG2?");
      break;
    case NONE:
    default:
      break;
  }
}
void MPR121Component::loop() {
  uint16_t val = 0;
  this->read_byte_16(MPR121_TOUCHSTATUS_L, &val);

  // Flip order
  uint8_t lsb = val >> 8;
  uint8_t msb = val;
  val = (uint16_t(msb) << 8) | lsb;

  for (auto *channel : this->channels_)
    channel->process(val);

  this->read_byte(MPR121_GPIODATA, &this->gpio_input_);
}

bool MPR121Component::digital_read(uint8_t ionum) { return (this->gpio_input_ & (1 << ionum)) != 0; }

void MPR121Component::digital_write(uint8_t ionum, bool value) {
  if (value) {
    this->gpio_output_ |= (1 << ionum);
  } else {
    this->gpio_output_ &= ~(1 << ionum);
  }
  this->flush_gpio_();
}

void MPR121Component::pin_mode(uint8_t ionum, gpio::Flags flags) {
  this->gpio_enable_ |= (1 << ionum);
  if (flags & gpio::FLAG_INPUT) {
    this->gpio_direction_ &= ~(1 << ionum);
  } else if (flags & gpio::FLAG_OUTPUT) {
    this->gpio_direction_ |= 1 << ionum;
  }
  this->flush_gpio_();
}

bool MPR121Component::flush_gpio_() {
  if (this->is_failed()) {
    return false;
  }

  // TODO: The CTL registers can configure internal pullup/pulldown resistors.
  this->write_byte(MPR121_GPIOCTL0, 0x00);
  this->write_byte(MPR121_GPIOCTL1, 0x00);
  this->write_byte(MPR121_GPIOEN, this->gpio_enable_);
  this->write_byte(MPR121_GPIODIR, this->gpio_direction_);

  if (!this->write_byte(MPR121_GPIODATA, this->gpio_output_)) {
    this->status_set_warning();
    return false;
  }

  this->status_clear_warning();
  return true;
}

void MPR121GPIOPin::setup() { this->pin_mode(this->flags_); }

void MPR121GPIOPin::pin_mode(gpio::Flags flags) {
  assert(this->pin_ >= 4);
  this->parent_->pin_mode(this->pin_ - 4, flags);
}

bool MPR121GPIOPin::digital_read() {
  assert(this->pin_ >= 4);
  return this->parent_->digital_read(this->pin_ - 4) != this->inverted_;
}

void MPR121GPIOPin::digital_write(bool value) {
  assert(this->pin_ >= 4);
  this->parent_->digital_write(this->pin_ - 4, value != this->inverted_);
}

std::string MPR121GPIOPin::dump_summary() const {
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "ELE%u on MPR121", this->pin_);
  return buffer;
}

}  // namespace mpr121
}  // namespace esphome
