#include "gp8211_output.h"
#include "esphome/core/log.h"

namespace esphome {
namespace gp8211 {

static const char *const TAG = "gp8211.output";

static const uint8_t OUTPUT_REGISTER = 0x02;

void GP8211Output::dump_config() { ESP_LOGCONFIG(TAG, "GP8211 Output:"); }

void GP8211Output::write_state(float state) {
  ESP_LOGD(TAG, "Original brightness state received from Light component: %.5f", state);

  uint16_t value = static_cast<uint16_t>(state * 32767);
  ESP_LOGD(TAG, "Calculated DAC value: %u", value);

  i2c::ErrorCode err = this->parent_->write_register(OUTPUT_REGISTER, (uint8_t *) &value, 2);

  if (err != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Error writing to GP8211, code %d", err);
  }
}

}  // namespace gp8211
}  // namespace esphome
