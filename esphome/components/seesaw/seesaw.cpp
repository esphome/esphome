#include "seesaw.h"

#include <cinttypes>
#include "esphome/core/log.h"

namespace esphome::seesaw {

ESPHOME_LOG_TAG(TAG, "seesaw");

static constexpr uint8_t SEESAW_HW_ID_SAMD09 = 0x55;
static constexpr uint8_t SEESAW_HW_ID_TINY806 = 0x84;
static constexpr uint8_t SEESAW_HW_ID_TINY807 = 0x85;
static constexpr uint8_t SEESAW_HW_ID_TINY816 = 0x86;
static constexpr uint8_t SEESAW_HW_ID_TINY817 = 0x87;
static constexpr uint8_t SEESAW_HW_ID_TINY1616 = 0x88;
static constexpr uint8_t SEESAW_HW_ID_TINY1617 = 0x89;

// The firmware takes at most 32 bytes per write; 4 go to the module, register and offset
static constexpr uint16_t NEOPIXEL_CHUNK = 28;

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
  // The loop only clears the GPIO cache, so it runs once a pin is set up
  this->disable_loop();
  if (!this->readbuf_(SEESAW_STATUS, SEESAW_STATUS_HW_ID, &this->cpuid_, 1)) {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }
  uint8_t buf[4]{};
  if (this->readbuf_(SEESAW_STATUS, SEESAW_STATUS_VERSION, buf, 4))
    this->version_ = encode_value<uint32_t>(buf);
  if (this->readbuf_(SEESAW_STATUS, SEESAW_STATUS_OPTIONS, buf, 4))
    this->options_ = encode_value<uint32_t>(buf);
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
  ESP_LOGCONFIG(TAG,
                "  Version: 20%02d-%02d-%02d %u\n"
                "  Options: %08" PRIx32,
                (uint8_t) (v & 0x3f), (uint8_t) ((v >> 7) & 0xf), (uint8_t) ((v >> 11) & 0x1f), (uint16_t) (v >> 16),
                this->options_);
}

void Seesaw::enable_encoder(uint8_t number) {
  this->write_byte_(SEESAW_ENCODER, SEESAW_ENCODER_INTENSET + number, 0x01);
}

bool Seesaw::get_encoder_position(uint8_t number, int32_t &position) {
  uint8_t buf[4]{};
  if (!this->readbuf_(SEESAW_ENCODER, SEESAW_ENCODER_POSITION + number, buf, 4))
    return false;
  position = -encode_value<int32_t>(buf);  // make clockwise positive
  return true;
}

bool Seesaw::get_encoder_delta(uint8_t number, int32_t &delta) {
  uint8_t buf[4]{};
  // Reading the delta clears it on the chip
  if (!this->readbuf_(SEESAW_ENCODER, SEESAW_ENCODER_DELTA + number, buf, 4))
    return false;
  delta = -encode_value<int32_t>(buf);  // make clockwise positive
  return true;
}

bool Seesaw::get_touch_value(uint8_t channel, uint16_t &value) {
  uint8_t buf[2]{};
  if (!this->readbuf_(SEESAW_TOUCH, SEESAW_TOUCH_CHANNEL_OFFSET + channel, buf, 2, 3000))
    return false;
  value = encode_uint16(buf[0], buf[1]);
  return true;
}

bool Seesaw::get_temperature(float &value) {
  uint8_t buf[4]{};
  if (!this->readbuf_(SEESAW_STATUS, SEESAW_STATUS_TEMP, buf, 4, 1000))
    return false;
  // Signed 16.16 fixed point
  const auto raw = static_cast<int32_t>(encode_value<uint32_t>(buf));
  if (raw == -1) {
    ESP_LOGW(TAG, "No temperature sensor");
    return false;
  }
  value = float(raw) / 0x10000;
  return true;
}

bool Seesaw::analog_read(uint8_t pin, uint16_t &value) {
  // The SAMD09 firmware numbers the ADC inputs: pins 2 to 5 are channels 0 to 3; the other chips take the pin
  uint8_t channel = pin;
  if (this->cpuid_ == SEESAW_HW_ID_SAMD09) {
    if (pin < 2 || pin > 5) {
      ESP_LOGW(TAG, "Pin %u is not an ADC input", pin);
      return false;
    }
    channel = pin - 2;
  }
  uint8_t buf[2]{};
  if (!this->readbuf_(SEESAW_ADC, SEESAW_ADC_CHANNEL_OFFSET + channel, buf, 2, 500))
    return false;
  value = encode_uint16(buf[0], buf[1]);
  return true;
}

void Seesaw::pin_mode(uint8_t pin, gpio::Flags flags) {
  this->enable_loop();
  const uint32_t pins = 1UL << pin;
  if (flags & gpio::FLAG_OUTPUT) {
    this->write_value_(SEESAW_GPIO, SEESAW_GPIO_DIRSET_BULK, pins);
  } else if (flags & gpio::FLAG_INPUT) {
    this->write_value_(SEESAW_GPIO, SEESAW_GPIO_DIRCLR_BULK, pins);
    if (flags & (gpio::FLAG_PULLUP | gpio::FLAG_PULLDOWN)) {
      this->write_value_(SEESAW_GPIO, SEESAW_GPIO_PULLENSET, pins);
      this->write_value_(SEESAW_GPIO, (flags & gpio::FLAG_PULLUP) ? SEESAW_GPIO_BULK_SET : SEESAW_GPIO_BULK_CLR, pins);
    } else {
      this->write_value_(SEESAW_GPIO, SEESAW_GPIO_PULLENCLR, pins);
    }
  }
}

bool Seesaw::digital_read(uint8_t pin) {
  // One bulk read covers every pin until loop() clears it. A failed read keeps the last levels, so an inverted
  // input does not read as active on a bus error.
  if (!this->gpio_read_valid_) {
    uint8_t buf[4]{};
    if (this->status_comm_result(this->readbuf_(SEESAW_GPIO, SEESAW_GPIO_BULK, buf, 4))) {
      this->gpio_levels_ = encode_value<uint32_t>(buf);
      this->gpio_read_valid_ = true;
    }
  }
  return this->gpio_levels_ & (1UL << pin);
}

void Seesaw::digital_write(uint8_t pin, bool state) {
  this->write_value_(SEESAW_GPIO, state ? SEESAW_GPIO_BULK_SET : SEESAW_GPIO_BULK_CLR, uint32_t(1UL << pin));
}

void Seesaw::setup_neopixel(uint8_t pin, uint16_t num_leds) {
  this->write_byte_(SEESAW_NEOPIXEL, SEESAW_NEOPIXEL_SPEED, 1);
  this->write_value_<uint16_t>(SEESAW_NEOPIXEL, SEESAW_NEOPIXEL_BUF_LENGTH, num_leds * 3);
  this->write_byte_(SEESAW_NEOPIXEL, SEESAW_NEOPIXEL_PIN, pin);
}

bool Seesaw::show_neopixel(const uint8_t *data, uint16_t len) {
  uint8_t buf[4 + NEOPIXEL_CHUNK] = {SEESAW_NEOPIXEL, SEESAW_NEOPIXEL_BUF};
  for (uint16_t offset = 0; offset < len; offset += NEOPIXEL_CHUNK) {
    const uint16_t chunk = std::min<uint16_t>(NEOPIXEL_CHUNK, len - offset);
    buf[2] = offset >> 8;
    buf[3] = offset & 0xff;
    memcpy(buf + 4, data + offset, chunk);
    if (this->write(buf, 4 + chunk) != i2c::ERROR_OK)
      return false;
  }
  return this->write_(SEESAW_NEOPIXEL, SEESAW_NEOPIXEL_SHOW) == i2c::ERROR_OK;
}

bool Seesaw::readbuf_(SeesawModule mod, uint8_t reg, uint8_t *buf, uint8_t len, uint32_t wait_us) {
  if (this->write_(mod, reg) != i2c::ERROR_OK)
    return false;
  delayMicroseconds(wait_us);
  return this->read(buf, len) == i2c::ERROR_OK;
}

}  // namespace esphome::seesaw

ESPHOME_EXPANDER_PIN_SUMMARY(esphome::seesaw::Seesaw, "%u via Seesaw")
