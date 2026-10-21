#include "seesaw.h"

#include <cinttypes>
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome::seesaw {

ESPHOME_LOG_TAG(TAG, "seesaw");

static constexpr uint8_t SEESAW_HW_ID_SAMD09 = 0x55;
static constexpr uint8_t SEESAW_HW_ID_TINY806 = 0x84;
static constexpr uint8_t SEESAW_HW_ID_TINY807 = 0x85;
static constexpr uint8_t SEESAW_HW_ID_TINY816 = 0x86;
static constexpr uint8_t SEESAW_HW_ID_TINY817 = 0x87;
static constexpr uint8_t SEESAW_HW_ID_TINY1616 = 0x88;
static constexpr uint8_t SEESAW_HW_ID_TINY1617 = 0x89;

float Seesaw::get_setup_priority() const { return setup_priority::IO; }

static const LogString *cpuid_to_string(uint8_t id) {
  switch (id) {
    case SEESAW_HW_ID_SAMD09:
      return LOG_STR("SAMD09");
    case SEESAW_HW_ID_TINY806:
      return LOG_STR("ATtiny806");
    case SEESAW_HW_ID_TINY807:
      return LOG_STR("ATtiny807");
    case SEESAW_HW_ID_TINY816:
      return LOG_STR("ATtiny816");
    case SEESAW_HW_ID_TINY817:
      return LOG_STR("ATtiny817");
    case SEESAW_HW_ID_TINY1616:
      return LOG_STR("ATtiny1616");
    case SEESAW_HW_ID_TINY1617:
      return LOG_STR("ATtiny1617");
    default:
      return nullptr;
  }
}

void Seesaw::setup() {
  uint8_t c = 0;
  i2c::ErrorCode err = this->readbuf_(SEESAW_STATUS, SEESAW_STATUS_HW_ID, &c, 1);
  if (err != i2c::ERROR_OK) {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }
  this->cpuid_ = c;
  uint8_t buf[4];
  if (this->readbuf_(SEESAW_STATUS, SEESAW_STATUS_VERSION, buf, 4) == i2c::ERROR_OK) {
    this->version_ = encode_uint32(buf[0], buf[1], buf[2], buf[3]);
  }
  if (this->readbuf_(SEESAW_STATUS, SEESAW_STATUS_OPTIONS, buf, 4) == i2c::ERROR_OK) {
    this->options_ = encode_uint32(buf[0], buf[1], buf[2], buf[3]);
  }
}

void Seesaw::dump_config() {
  ESP_LOGCONFIG(TAG, "Seesaw module:");
  LOG_I2C_DEVICE(this);
  const LogString *cpu = cpuid_to_string(this->cpuid_);
  if (cpu != nullptr) {
    ESP_LOGCONFIG(TAG, "  CPU: %s", LOG_STR_ARG(cpu));
  } else {
    ESP_LOGCONFIG(TAG, "  CPU: unknown (%02x)", this->cpuid_);
  }
  uint32_t v = this->version_;
  ESP_LOGCONFIG(TAG, "  Version: 20%02d-%02d-%02d %u", (uint8_t) (v & 0x3f), (uint8_t) ((v >> 7) & 0xf),
                (uint8_t) ((v >> 11) & 0x1f), (uint16_t) (v >> 16));
  ESP_LOGCONFIG(TAG, "  Options: %08" PRIx32, this->options_);
}

void Seesaw::enable_encoder(uint8_t number) { this->write8_(SEESAW_ENCODER, SEESAW_ENCODER_INTENSET + number, 0x01); }

bool Seesaw::get_encoder_position(uint8_t number, int32_t *position) {
  if (position == nullptr)
    return false;

  uint8_t buf[4];
  if (this->readbuf_(SEESAW_ENCODER, SEESAW_ENCODER_POSITION + number, buf, 4, 1000) != i2c::ERROR_OK)
    return false;
  *position = -encode_value<int32_t>(buf);  // make clockwise positive
  return true;
}

void Seesaw::set_encoder_position(uint8_t number, int32_t position) {
  // Stored negated, as get_encoder_position() reads it
  this->write32_(SEESAW_ENCODER, SEESAW_ENCODER_POSITION + number, static_cast<uint32_t>(-position));
}

int16_t Seesaw::get_touch_value(uint8_t channel) {
  uint8_t buf[2];
  if (this->readbuf_(SEESAW_TOUCH, SEESAW_TOUCH_CHANNEL_OFFSET + channel, buf, 2, 3000) != i2c::ERROR_OK)
    return -1;
  return encode_uint16(buf[0], buf[1]);
}

float Seesaw::get_temperature() {
  uint8_t buf[4];
  if (this->readbuf_(SEESAW_STATUS, SEESAW_STATUS_TEMP, buf, 4, 1000) != i2c::ERROR_OK)
    return NAN;
  const uint32_t value = encode_uint32(buf[0], buf[1], buf[2], buf[3]);
  if (value == 0xffffffff) {
    ESP_LOGW(TAG, "no temperature sensor");
    return NAN;
  }
  return float(value) / 0x10000;
}

void Seesaw::set_pinmode(uint8_t pin, gpio::Flags mode) {
  uint32_t pins = 1 << pin;
  if (mode & gpio::FLAG_OUTPUT) {
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

uint16_t Seesaw::analog_read(uint8_t pin) {
  uint8_t buf[2];
  i2c::ErrorCode err = this->readbuf_(SEESAW_ADC, SEESAW_ADC_CHANNEL_OFFSET + pin, buf, 2, 1000);
  if (err == i2c::ERROR_OK)
    return encode_uint16(buf[0], buf[1]);
  return 0xffff;
}

bool Seesaw::digital_read(uint8_t pin) {
  uint32_t pins = 1 << pin;
  uint8_t buf[4];
  // One bulk read covers every pin, so pins read in the same loop pass share it. A failed read keeps the last
  // levels, so an inverted input does not read as active on a bus error.
  const uint32_t now = App.get_loop_component_start_time();
  if (!this->gpio_read_valid_ || now != this->gpio_read_time_) {
    if (this->status_comm_result(this->readbuf_(SEESAW_GPIO, SEESAW_GPIO_BULK, buf, 4) == i2c::ERROR_OK)) {
      this->gpio_levels_ = encode_uint32(buf[0], buf[1], buf[2], buf[3]);
      this->gpio_read_time_ = now;
      this->gpio_read_valid_ = true;
    }
  }
  return this->gpio_levels_ & pins;
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

void SeesawGPIOPin::setup() { this->pin_mode(this->flags_); }

void SeesawGPIOPin::pin_mode(gpio::Flags flags) { this->parent_->set_pinmode(this->pin_, flags); }

bool SeesawGPIOPin::digital_read() { return this->parent_->digital_read(this->pin_) != this->inverted_; }

void SeesawGPIOPin::digital_write(bool value) { this->parent_->digital_write(this->pin_, value != this->inverted_); }

size_t SeesawGPIOPin::dump_summary(char *buffer, size_t len) const {
  return snprintf(buffer, len, "%u via Seesaw", this->pin_);
}

}  // namespace esphome::seesaw
