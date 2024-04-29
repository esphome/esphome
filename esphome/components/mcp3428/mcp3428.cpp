#include "mcp3428.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace mcp3428 {

static const char *const TAG = "mcp3426/7/8";

void MCP3428Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up MCP3426/7/8...");
  uint8_t anwser[3];
  if (this->read(anwser, 3) != i2c::ErrorCode::NO_ERROR) {
    this->mark_failed();
    return;
  }

  ESP_LOGCONFIG(TAG, "Configuring MCP3426/7/8...");

  /* config byte structure: (bit|description)
   * 7 | ready bit, 0 means new data in the result
   * 6 | Channel selection bit 1
   * 5 | Channel selection bit 0
   * 4 | conversion mode bit (1 continuous mode, 0 singe shot)
   * 3 | Resolution bit 1
   * 2 | Resolution bit 0
   * 1 | Gain selection bit 1
   * 0 | Gain selection bit 0
   */
  uint8_t config = 0;
  // set ready bit and conversion mode bit
  //                  0b0xx0xxxx
  if (this->continuous_mode_) {
    // initial state should be no new measurement in continuous mode
    config = config | 0b10010000;
  } else {
    // don't initiate measurement in single shot mode
    config = config & 0b01101111;
  }
  // leave channel at 1, gain at 1x, and resolution at 12 bit

  if (this->write(&config, 1) != i2c::ErrorCode::NO_ERROR) {
    this->mark_failed();
    return;
  }
  this->prev_config_ = config;
}

void MCP3428Component::dump_config() {
  ESP_LOGCONFIG(TAG, "Setting up MCP3426/7/8...");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with MCP3426/7/8 failed!");
  }
}

float MCP3428Component::request_measurement(MCP3428Multiplexer multiplexer, MCP3428Gain gain,
                                            MCP3428Resolution resolution) {
  uint8_t config = 0;
  // set ready bit to 1, will starts measurement in single shot mode and mark measurement as not yet ready in continuous
  // mode
  config |= 1 << 7;
  // set channel
  config |= multiplexer << 5;
  // set conversion mode
  if (this->continuous_mode_) {
    config |= 1 << 4;
  }
  // set resolution
  config |= resolution << 2;
  // set gain
  config |= gain;

  // If continuous mode and config (besides ready bit) are the same there is no need to upload new config, reading the
  // result is enough
  if (!((this->prev_config_ & 0b00010000) > 0 and (this->prev_config_ & 0b01111111) == (config & 0b01111111))) {
    if (this->write(&config, 1) != i2c::ErrorCode::NO_ERROR) {
      this->status_set_warning();
      return NAN;
    }
    this->prev_config_ = config;
  }

  // MCP is now configured, read output until ready flag is 0 for a valid measurement
  uint32_t start = millis();
  uint8_t anwser[3];
  while (true) {
    if (this->read(anwser, 3) != i2c::ErrorCode::NO_ERROR) {
      this->status_set_warning();
      return NAN;
    }
    if ((anwser[2] & 0b10000000) == 0) {
      // ready flag is 0, valid measurement received
      break;
    }
    if (millis() - start > 100) {
      ESP_LOGW(TAG, "Reading MCP3428 measurement timed out");
      this->status_set_warning();
      return NAN;
    }
    yield();
  }

  // got valid measurement prepare tick size
  float tick_voltage = 2.048f / 32768;  // ref voltage 2.048V/non-sign bits, default 15 bits
  switch (resolution) {
    case MCP3428Resolution::MCP3428_12_BITS:
      tick_voltage *= 16;
      break;
    case MCP3428Resolution::MCP3428_14_BITS:
      tick_voltage *= 4;
      break;
    default:  // nothing to do for 16 bit
      break;
  }
  switch (gain) {
    case MCP3428Gain::MCP3428_GAIN_2:
      tick_voltage /= 2;
      break;
    case MCP3428Gain::MCP3428_GAIN_4:
      tick_voltage /= 4;
      break;
    case MCP3428Gain::MCP3428_GAIN_8:
      tick_voltage /= 8;
      break;
    default:
      break;
  }
  // convert code (first 2 bytes of cleaned up anwser) into voltage ticks
  int16_t ticks = anwser[0] << 8 | anwser[1];

  this->status_clear_warning();
  return tick_voltage * ticks;
}

}  // namespace mcp3428
}  // namespace esphome
