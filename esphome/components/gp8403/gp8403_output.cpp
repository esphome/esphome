#include "gp8403_output.h"

#include "esphome/core/log.h"

namespace esphome {
namespace gp8403 {

static const char *const TAG = "gp8403.output";

static const uint8_t REGISTER = 0x02;

void GP8403Output::dump_config() {
  ESP_LOGCONFIG(TAG, "GP8403 Output:");
  ESP_LOGCONFIG(TAG, "  Channel: %u", this->channel_);
  LOG_I2C_DEVICE(this);
}

void GP8403Output::write_state(float state) {
  if (this->write_byte_16(REGISTER + (2 * this->channel_), ((uint16_t)(state * 4095)) << 4) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Error writing to GP8403");
  }
}

}  // namespace gp8403
}  // namespace esphome
