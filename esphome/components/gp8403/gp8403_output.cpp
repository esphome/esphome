#include "gp8403_output.h"

#include "esphome/core/log.h"

namespace esphome {
namespace gp8403 {

static const char *const TAG = "gp8403.output";

static const uint8_t REGISTER = 0x02;

void GP8403Output::setup() { ESP_LOGCONFIG(TAG, "Setting up GP8403 Output..."); }

void GP8403Output::dump_config() {
  ESP_LOGCONFIG(TAG, "GP8403 Output:");
  LOG_I2C_DEVICE(this);
}

void GP8403Output::write_state(float state) {
  std::vector<uint8_t> data;

  uint16_t to_write = ((uint16_t) (state * 4095)) << 4;

  switch (this->channel_) {
    case 0:
      data.push_back(REGISTER);
      data.push_back(to_write & 0xFF);
      data.push_back((to_write >> 8) & 0xFF);
      break;
    case 1:
      data.push_back(REGISTER + 2);
      data.push_back(to_write & 0xFF);
      data.push_back((to_write >> 8) & 0xFF);
      break;
    case 2:
      data.push_back(REGISTER);
      data.push_back(to_write & 0xFF);
      data.push_back((to_write >> 8) & 0xFF);
      data.push_back(to_write & 0xFF);
      data.push_back((to_write >> 8) & 0xFF);
      break;
  }
  if (this->write(data.data(), data.size()) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Error writing to GP8403");
  }
}

}  // namespace gp8403
}  // namespace esphome
