#include "m5angle8.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace m5angle8 {

static const char *const TAG = "m5angle8";

void M5Angle8LightOutput::setup() {
  ExternalRAMAllocator<uint8_t> allocator(ExternalRAMAllocator<uint8_t>::ALLOW_FAILURE);
  this->buf_ = allocator.allocate(M5ANGLE8_NUM_LEDS * M5ANGLE8_BYTES_PER_LED);
  if (this->buf_ == nullptr) {
    esph_log_e(TAG, "Failed to allocate buffer of size %u", M5ANGLE8_NUM_LEDS * M5ANGLE8_BYTES_PER_LED);
    this->mark_failed();
    return;
  };
  memset(this->buf_, 0xFF, M5ANGLE8_NUM_LEDS * M5ANGLE8_BYTES_PER_LED);

  this->effect_data_ = allocator.allocate(M5ANGLE8_NUM_LEDS);
  if (this->effect_data_ == nullptr) {
    ESP_LOGE(TAG, "Failed to allocate effect data of size %u", M5ANGLE8_NUM_LEDS);
    this->mark_failed();
    return;
  };
  memset(this->effect_data_, 0x00, M5ANGLE8_NUM_LEDS);
}

void M5Angle8LightOutput::write_state(light::LightState *state) {
  for (int i = 0; i < M5ANGLE8_NUM_LEDS; i++) {
    this->parent_->write_register(M5ANGLE8_REGISTER_RGB_24B + i * M5ANGLE8_BYTES_PER_LED,
                                  this->buf_ + i * M5ANGLE8_BYTES_PER_LED, M5ANGLE8_BYTES_PER_LED);
  }
}

void M5Angle8Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up M5ANGLE8...");
  i2c::ErrorCode err;

  err = this->read(nullptr, 0);
  if (err != i2c::NO_ERROR) {
    ESP_LOGE(TAG, "I2C error %02X...", err);
    this->mark_failed();
    return;
  };

  err = this->read_register(M5ANGLE8_REGISTER_FW_VERSION, &this->fw_version_, 1);
  if (err != i2c::NO_ERROR) {
    ESP_LOGE(TAG, "I2C error %02X...", err);
    this->mark_failed();
    return;
  };
}

void M5Angle8Component::dump_config() {
  ESP_LOGCONFIG(TAG, "M5ANGLE8:");
  LOG_I2C_DEVICE(this);
  ESP_LOGCONFIG(TAG, "  Firmware version: %d ", this->fw_version_);
}

float M5Angle8Component::read_knob_pos(uint8_t channel) {
  uint8_t knob_pos;
  i2c::ErrorCode err = this->read_register(M5ANGLE8_REGISTER_ANALOG_INPUT_8B + channel, (uint8_t *) &knob_pos, 1);
  if (err == i2c::NO_ERROR) {
    return 1.0f - (knob_pos / 255.0f);
  } else {
    return -1.0f;
  }
}

int M5Angle8Component::read_switch() {
  uint8_t out;
  i2c::ErrorCode err = this->read_register(M5ANGLE8_REGISTER_DIGITAL_INPUT, (uint8_t *) &out, 1);
  if (err == i2c::NO_ERROR) {
    return out ? 1 : 0;
  } else {
    return -1;
  }
}

float M5Angle8Component::get_setup_priority() const { return setup_priority::DATA; }

void M5Angle8SensorKnob::update() {
  if (this->parent_ != nullptr) {
    float knob_pos = this->parent_->read_knob_pos(this->knob_index_);
    if (knob_pos >= 0)
      this->publish_state(knob_pos);
  };
}

void M5Angle8SensorSwitch::update() {
  if (this->parent_ != nullptr) {
    int sw_pos = this->parent_->read_switch();
    if (sw_pos >= 0)
      this->publish_state(sw_pos);
  };
}

}  // namespace m5angle8
}  // namespace esphome
