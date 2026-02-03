#include "gp8403.h"

#include "esphome/core/log.h"

namespace esphome {
namespace gp8403 {

static const char *const TAG = "gp8403";

static const uint8_t RANGE_REGISTER = 0x01;
static const uint8_t OUTPUT_REGISTER = 0x02;

const LogString *model_to_string(GP8403Model model) {
  switch (model) {
    case GP8403Model::GP8403:
      return LOG_STR("GP8403");
    case GP8403Model::GP8413:
      return LOG_STR("GP8413");
  }
  return LOG_STR("Unknown");
};

void GP8403Component::setup() { this->write_register(RANGE_REGISTER, (uint8_t *) (&this->voltage_), 1); }

void GP8403Component::dump_config() {
  ESP_LOGCONFIG(TAG,
                "GP8403:\n"
                "  Voltage: %dV\n"
                "  Model: %s",
                this->voltage_ == GP8403_VOLTAGE_5V ? 5 : 10, LOG_STR_ARG(model_to_string(this->model_)));
  LOG_I2C_DEVICE(this);
}

void GP8403Component::write_state(float state, uint8_t channel) {
  uint16_t val = 0;
  switch (this->model_) {
    case GP8403Model::GP8403:
      val = ((uint16_t) (4095 * state)) << 4;
      break;
    case GP8403Model::GP8413:
      val = ((uint16_t) (32767 * state)) << 1;
      break;
    default:
      ESP_LOGE(TAG, "Unknown model %s", LOG_STR_ARG(model_to_string(this->model_)));
      return;
  }
  ESP_LOGV(TAG, "Calculated DAC value: %" PRIu16, val);
  i2c::ErrorCode err = this->write_register(OUTPUT_REGISTER + (2 * channel), (uint8_t *) &val, 2);
  if (err != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Error writing to %s, code %d", LOG_STR_ARG(model_to_string(this->model_)), err);
  }
}

}  // namespace gp8403
}  // namespace esphome
