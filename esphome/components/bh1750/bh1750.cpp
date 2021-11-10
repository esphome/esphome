#include "bh1750.h"
#include "esphome/core/log.h"

namespace esphome {
namespace bh1750 {

static const char *const TAG = "bh1750.sensor";

static const uint8_t BH1750_COMMAND_POWER_ON = 0b00000001;
static const uint8_t BH1750_COMMAND_MT_REG_HI = 0b01000000;  // last 3 bits
static const uint8_t BH1750_COMMAND_MT_REG_LO = 0b01100000;  // last 5 bits

void BH1750Sensor::setup() {
  ESP_LOGCONFIG(TAG, "Setting up BH1750 '%s'...", this->name_.c_str());
  if (!this->write_bytes(BH1750_COMMAND_POWER_ON, nullptr, 0)) {
    this->mark_failed();
    return;
  }

  uint8_t mtreg_hi = (this->measurement_duration_ >> 5) & 0b111;
  uint8_t mtreg_lo = (this->measurement_duration_ >> 0) & 0b11111;
  this->write_bytes(BH1750_COMMAND_MT_REG_HI | mtreg_hi, nullptr, 0);
  this->write_bytes(BH1750_COMMAND_MT_REG_LO | mtreg_lo, nullptr, 0);
}

void BH1750Sensor::dump_config() {
  LOG_SENSOR("", "BH1750", this);
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with BH1750 failed!");
  }

  const char *resolution_s;
  switch (this->resolution_) {
    case BH1750_RESOLUTION_0P5_LX:
      resolution_s = "0.5";
      break;
    case BH1750_RESOLUTION_1P0_LX:
      resolution_s = "1";
      break;
    case BH1750_RESOLUTION_4P0_LX:
      resolution_s = "4";
      break;
    default:
      resolution_s = "Unknown";
      break;
  }
  ESP_LOGCONFIG(TAG, "  Resolution: %s", resolution_s);
  LOG_UPDATE_INTERVAL(this);
}

void BH1750Sensor::update() {
  if (!this->write_bytes(this->resolution_, nullptr, 0))
    return;

  uint32_t wait = 0;
  // use max conversion times
  switch (this->resolution_) {
    case BH1750_RESOLUTION_0P5_LX:
    case BH1750_RESOLUTION_1P0_LX:
      wait = 180;
      break;
    case BH1750_RESOLUTION_4P0_LX:
      wait = 24;
      break;
  }

  this->set_timeout("illuminance", wait, [this]() { this->read_data_(); });
}

float BH1750Sensor::get_setup_priority() const { return setup_priority::DATA; }
void BH1750Sensor::read_data_() {
  uint16_t raw_value;
  if (this->read(reinterpret_cast<uint8_t *>(&raw_value), 2) != i2c::ERROR_OK) {
    this->status_set_warning();
    return;
  }
  raw_value = i2c::i2ctohs(raw_value);

  float lx = float(raw_value) / 1.2f;
  lx *= 69.0f / this->measurement_duration_;
  if (this->resolution_ == BH1750_RESOLUTION_0P5_LX) {
    lx /= 2.0f;
  }
  ESP_LOGD(TAG, "'%s': Got illuminance=%.1flx", this->get_name().c_str(), lx);
  this->publish_state(lx);
  this->status_clear_warning();
}

void BH1750Sensor::set_resolution(BH1750Resolution resolution) { this->resolution_ = resolution; }

}  // namespace bh1750
}  // namespace esphome
