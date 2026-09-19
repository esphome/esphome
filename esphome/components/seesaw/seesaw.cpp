#include "seesaw.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome::seesaw {

static const char *const TAG = "seesaw";

const uint8_t SEESAW_HW_ID_SAMD09 = 0x55;
const uint8_t SEESAW_HW_ID_TINY806 = 0x84;
const uint8_t SEESAW_HW_ID_TINY807 = 0x85;
const uint8_t SEESAW_HW_ID_TINY816 = 0x86;
const uint8_t SEESAW_HW_ID_TINY817 = 0x87;
const uint8_t SEESAW_HW_ID_TINY1616 = 0x88;
const uint8_t SEESAW_HW_ID_TINY1617 = 0x89;

float Seesaw::get_setup_priority() const { return setup_priority::IO; }

static const char *cpuid_to_string(uint8_t id) {
  switch (id) {
    case SEESAW_HW_ID_SAMD09:
      return "SAMD09";
    case SEESAW_HW_ID_TINY806:
      return "ATtiny806";
    case SEESAW_HW_ID_TINY807:
      return "ATtiny807";
    case SEESAW_HW_ID_TINY816:
      return "ATtiny816";
    case SEESAW_HW_ID_TINY817:
      return "ATtiny817";
    case SEESAW_HW_ID_TINY1616:
      return "ATtiny1616";
    case SEESAW_HW_ID_TINY1617:
      return "ATtiny1617";
    default:
      return nullptr;
  }
}

void Seesaw::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Seesaw...");
  uint8_t c = 0;
  i2c::ErrorCode err = this->readbuf_(SEESAW_STATUS, SEESAW_STATUS_HW_ID, &c, 1);
  if (err != i2c::ERROR_OK) {
    this->mark_failed(LOG_STR("unable to communicate"));
    return;
  }
  this->cpuid_ = c;
  uint8_t buf[4];
  if (this->readbuf_(SEESAW_STATUS, SEESAW_STATUS_VERSION, buf, 4) == i2c::ERROR_OK) {
    this->version_ = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
  } else {
    this->version_ = 0;
  }
  if (this->readbuf_(SEESAW_STATUS, SEESAW_STATUS_OPTIONS, buf, 4) == i2c::ERROR_OK) {
    this->options_ = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
  } else {
    this->options_ = 0;
  }
}

void Seesaw::dump_config() {
  ESP_LOGCONFIG(TAG, "Seesaw module:");
  LOG_I2C_DEVICE(this);
  const char *cpu = cpuid_to_string(this->cpuid_);
  if (cpu != nullptr) {
    ESP_LOGCONFIG(TAG, "  CPU: %s", cpu);
  } else {
    ESP_LOGCONFIG(TAG, "  CPU: unknown (%02x)", this->cpuid_);
  }
  uint32_t v = this->version_;
  ESP_LOGCONFIG(TAG, "  Version: 20%02d-%02d-%02d %u", v & 0x3f, (v >> 7) & 0xf, (v >> 11) & 0x1f, v >> 16);
  ESP_LOGCONFIG(TAG, "  Options: %08x", this->options_);
}

void Seesaw::enable_encoder(uint8_t number) { this->write8_(SEESAW_ENCODER, SEESAW_ENCODER_INTENSET + number, 0x01); }

bool Seesaw::get_encoder_position(uint8_t number, int32_t *position) {
  if (position == nullptr)
    return false;

  uint8_t buf[4];
  if (this->readbuf_(SEESAW_ENCODER, SEESAW_ENCODER_POSITION + number, buf, 4, 1000) != i2c::ERROR_OK)
    return false;
  int32_t value = (buf[0] << 24) + (buf[1] << 16) + (buf[2] << 8) + buf[3];
  *position = -value;  // make clockwise positive
  return true;
}

int16_t Seesaw::get_touch_value(uint8_t channel) {
  uint8_t buf[2];
  if (this->readbuf_(SEESAW_TOUCH, SEESAW_TOUCH_CHANNEL_OFFSET + channel, buf, 2, 3000) != i2c::ERROR_OK)
    return -1;
  return ((uint16_t) buf[0] << 8) | buf[1];
}

float Seesaw::get_temperature() {
  uint8_t buf[4];
  if (this->readbuf_(SEESAW_STATUS, SEESAW_STATUS_TEMP, buf, 4, 1000) != i2c::ERROR_OK)
    return NAN;
  uint32_t value = (buf[0] << 24) + (buf[1] << 16) + (buf[2] << 8) + buf[3];
  if (value == 0xffffffff) {
    ESP_LOGW(TAG, "no temperature sensor");
    return NAN;
  }
  return float(value) / 0x10000;
}

void Seesaw::set_pinmode(uint8_t pin, uint8_t mode) {
  uint32_t pins = 1 << pin;
  if (mode == gpio::FLAG_OUTPUT) {
    this->write32_(SEESAW_GPIO, SEESAW_GPIO_DIRSET_BULK, pins);
  } else if (mode & gpio::FLAG_INPUT) {
    this->write32_(SEESAW_GPIO, SEESAW_GPIO_DIRCLR_BULK, pins);
    if (mode & gpio::FLAG_PULLUP) {
      this->write32_(SEESAW_GPIO, SEESAW_GPIO_PULLENSET, pins);
      this->write32_(SEESAW_GPIO, SEESAW_GPIO_BULK_SET, pins);
    } else if (mode & gpio::FLAG_PULLDOWN) {
      this->write32_(SEESAW_GPIO, SEESAW_GPIO_PULLENSET, pins);
      this->write32_(SEESAW_GPIO, SEESAW_GPIO_BULK_CLR, pins);
    } else {
      this->write32_(SEESAW_GPIO, SEESAW_GPIO_PULLENCLR, pins);
    }
  }
}

void Seesaw::set_gpio_interrupt(uint32_t pin, bool enabled) {
  uint32_t pins = 1 << pin;
  if (enabled) {
    this->write32_(SEESAW_GPIO, SEESAW_GPIO_INTENSET, pins);
  } else {
    this->write32_(SEESAW_GPIO, SEESAW_GPIO_INTENCLR, pins);
  }
}

uint16_t Seesaw::analog_read(uint8_t pin) {
  uint8_t buf[2];
  i2c::ErrorCode err = this->readbuf_(SEESAW_ADC, SEESAW_ADC_CHANNEL_OFFSET + pin, buf, 2, 1000);
  if (err == i2c::ERROR_OK)
    return (buf[0] << 8) + buf[1];
  return 0xffff;
}

bool Seesaw::digital_read(uint8_t pin) {
  uint32_t pins = 1 << pin;
  uint8_t buf[4];
  i2c::ErrorCode err = this->readbuf_(SEESAW_GPIO, SEESAW_GPIO_BULK, buf, 4);
  if (err == i2c::ERROR_OK) {
    uint32_t ret = (buf[0] << 24) + (buf[1] << 16) + (buf[2] << 8) + buf[3];
    return ret & pins;
  }
  return false;
}

void Seesaw::digital_write(uint8_t pin, bool state) {
  uint32_t pins = 1 << pin;
  if (state) {
    this->write32_(SEESAW_GPIO, SEESAW_GPIO_BULK_SET, pins);
  } else {
    this->write32_(SEESAW_GPIO, SEESAW_GPIO_BULK_CLR, pins);
  }
}

void Seesaw::setup_neopixel(int pin, uint16_t num_leds) {
  this->write8_(SEESAW_NEOPIXEL, SEESAW_NEOPIXEL_SPEED, 1);
  this->write16_(SEESAW_NEOPIXEL, SEESAW_NEOPIXEL_BUF_LENGTH, num_leds * 3);
  this->write8_(SEESAW_NEOPIXEL, SEESAW_NEOPIXEL_PIN, pin);
}

void Seesaw::color_neopixel(uint16_t n, uint8_t r, uint8_t g, uint8_t b) {
  uint16_t offset = n * 3;
  uint8_t buf[7] = {SEESAW_NEOPIXEL, SEESAW_NEOPIXEL_BUF, (uint8_t) (offset >> 8), (uint8_t) (offset & 0xff), g, r, b};
  this->write(buf, 7);
}

void Seesaw::update_neopixel() {
  uint8_t buf[2] = {SEESAW_NEOPIXEL, SEESAW_NEOPIXEL_SHOW};
  this->write(buf, 2);
}

i2c::ErrorCode Seesaw::write8_(SeesawModule mod, uint8_t reg, uint8_t value) {
  uint8_t buf[3] = {mod, reg, value};
  return this->write(buf, 3);
}

i2c::ErrorCode Seesaw::write16_(SeesawModule mod, uint8_t reg, uint16_t value) {
  uint8_t buf[4] = {mod, reg, (uint8_t) (value >> 8), (uint8_t) value};
  return this->write(buf, 4);
}

i2c::ErrorCode Seesaw::write32_(SeesawModule mod, uint8_t reg, uint32_t value) {
  uint8_t buf[6] = {
      mod, reg, (uint8_t) (value >> 24), (uint8_t) (value >> 16), (uint8_t) (value >> 8), (uint8_t) value};
  return this->write(buf, 6);
}

i2c::ErrorCode Seesaw::readbuf_(SeesawModule mod, uint8_t reg, uint8_t *buf, uint8_t len, int wait_us) {
  uint8_t sendbuf[2] = {mod, reg};
  i2c::ErrorCode err = this->write(sendbuf, 2);
  if (err != i2c::ERROR_OK)
    return err;
  if (wait_us)
    delayMicroseconds(wait_us);
  return this->read(buf, len);
}

void SeesawGPIOPin::setup() { this->pin_mode(flags_); }

void SeesawGPIOPin::pin_mode(gpio::Flags flags) { this->parent_->set_pinmode(this->pin_, flags); }

bool SeesawGPIOPin::digital_read() { return this->parent_->digital_read(this->pin_) != this->inverted_; }

void SeesawGPIOPin::digital_write(bool value) { this->parent_->digital_write(this->pin_, value != this->inverted_); }

size_t SeesawGPIOPin::dump_summary(char *buffer, size_t len) const {
  return snprintf(buffer, len, "%u via Seesaw", this->pin_);
}

}  // namespace esphome::seesaw
