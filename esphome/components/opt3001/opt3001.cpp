#include "opt3001.h"
#include "esphome/core/log.h"

namespace esphome {
namespace opt3001 {

static const char *const TAG = "opt3001.sensor";

static const uint8_t OPT3001_REG_RESULT = 0x00;
static const uint8_t OPT3001_REG_CONFIGURATION = 0x01;
// See datasheet for full description of each bit.
static const uint16_t OPT3001_CONFIGURATION_RANGE_FULL = 0b1100000000000000;
static const uint16_t OPT3001_CONFIGURATION_CONVERSION_TIME_800 = 0b100000000000;
static const uint16_t OPT3001_CONFIGURATION_CONVERSION_MODE_MASK = 0b11000000000;
static const uint16_t OPT3001_CONFIGURATION_CONVERSION_MODE_SINGLE_SHOT = 0b01000000000;
static const uint16_t OPT3001_CONFIGURATION_CONVERSION_MODE_SHUTDOWN = 0b00000000000;
// tl;dr: Configure an automatic-ranged, 800ms single shot reading,
// with INT processing disabled
static const uint16_t OPT3001_CONFIGURATION_FULL_RANGE_ONE_SHOT = OPT3001_CONFIGURATION_RANGE_FULL |
                                                                  OPT3001_CONFIGURATION_CONVERSION_TIME_800 |
                                                                  OPT3001_CONFIGURATION_CONVERSION_MODE_SINGLE_SHOT;
static const uint16_t OPT3001_CONVERSION_TIME_800 = 825;  // give it 25 extra ms; it seems to not be ready quite often

/*
opt3001 properties:

- e (exponent) = high 4 bits of result register
- m (mantissa) = low 12 bits of result register
- formula: (0.01 * 2^e) * m lx

*/

void OPT3001Sensor::read_result_(const std::function<void(float)> &f) {
  // ensure the single shot flag is clear, indicating it's done
  uint16_t raw_value;
  if (this->read(reinterpret_cast<uint8_t *>(&raw_value), 2) != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Reading configuration register failed");
    f(NAN);
    return;
  }
  raw_value = i2c::i2ctohs(raw_value);

  if ((raw_value & OPT3001_CONFIGURATION_CONVERSION_MODE_MASK) != OPT3001_CONFIGURATION_CONVERSION_MODE_SHUTDOWN) {
    // not ready; wait 10ms and try again
    ESP_LOGW(TAG, "Data not ready; waiting 10ms");
    this->set_timeout("opt3001_wait", 10, [this, f]() { read_result_(f); });
    return;
  }

  if (this->read_register(OPT3001_REG_RESULT, reinterpret_cast<uint8_t *>(&raw_value), 2) != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Reading result register failed");
    f(NAN);
    return;
  }
  raw_value = i2c::i2ctohs(raw_value);

  uint8_t exponent = raw_value >> 12;
  uint16_t mantissa = raw_value & 0b111111111111;

  double lx = 0.01 * pow(2.0, double(exponent)) * double(mantissa);
  f(float(lx));
}

void OPT3001Sensor::read_lx_(const std::function<void(float)> &f) {
  // turn on (after one-shot sensor automatically powers down)
  uint16_t start_measurement = i2c::htoi2cs(OPT3001_CONFIGURATION_FULL_RANGE_ONE_SHOT);
  if (this->write_register(OPT3001_REG_CONFIGURATION, reinterpret_cast<uint8_t *>(&start_measurement), 2) !=
      i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Triggering one shot measurement failed");
    f(NAN);
    return;
  }

  this->set_timeout("read", OPT3001_CONVERSION_TIME_800, [this, f]() {
    if (this->write(&OPT3001_REG_CONFIGURATION, 1) != i2c::ERROR_OK) {
      ESP_LOGW(TAG, "Starting configuration register read failed");
      f(NAN);
      return;
    }

    this->read_result_(f);
  });
}

void OPT3001Sensor::dump_config() {
  LOG_SENSOR("", "OPT3001", this);
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  }

  LOG_UPDATE_INTERVAL(this);
}

void OPT3001Sensor::update() {
  // Set a flag and skip just in case the sensor isn't responding,
  // and we just keep waiting for it in read_result_.
  // This way we don't end up with potentially boundless "threads"
  // using up memory and eventually crashing the device
  if (this->updating_) {
    return;
  }
  this->updating_ = true;

  this->read_lx_([this](float val) {
    this->updating_ = false;

    if (std::isnan(val)) {
      this->status_set_warning();
      this->publish_state(NAN);
      return;
    }
    ESP_LOGD(TAG, "'%s': Illuminance=%.1flx", this->get_name().c_str(), val);
    this->status_clear_warning();
    this->publish_state(val);
  });
}

float OPT3001Sensor::get_setup_priority() const { return setup_priority::DATA; }

}  // namespace opt3001
}  // namespace esphome
