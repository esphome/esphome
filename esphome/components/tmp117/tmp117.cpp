// Implementation based on:
//  - DHT 12 Component

#include "tmp117.h"
#include "esphome/core/log.h"

namespace esphome {
namespace tmp117 {

static const char *const TAG = "tmp117";

void TMP117Component::update() {
  int16_t data;
  if (!this->read_data_(&data)) {
    this->status_set_warning();
    return;
  }
  if ((uint16_t) data != 0x8000) {
    float temperature = data * 0.0078125f;

    ESP_LOGD(TAG, "Got temperature=%.2f°C", temperature);
    this->publish_state(temperature);
    this->status_clear_warning();
  } else {
    ESP_LOGD(TAG, "TMP117 not ready");
  }
}
void TMP117Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up TMP117...");

  if (!this->write_config_(this->config_)) {
    this->mark_failed();
    return;
  }

  int16_t data;
  if (!this->read_data_(&data)) {
    this->mark_failed();
    return;
  }
}
void TMP117Component::dump_config() {
  ESP_LOGD(TAG, "TMP117:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with TMP117 failed!");
  }
  LOG_SENSOR("  ", "Temperature", this);
}
float TMP117Component::get_setup_priority() const { return setup_priority::DATA; }
bool TMP117Component::read_data_(int16_t *data) {
  if (!this->read_byte_16(0, (uint16_t *) data)) {
    ESP_LOGW(TAG, "Updating TMP117 failed!");
    return false;
  }
  return true;
}

bool TMP117Component::read_config_(uint16_t *config) {
  if (!this->read_byte_16(1, (uint16_t *) config)) {
    ESP_LOGW(TAG, "Reading TMP117 config failed!");
    return false;
  }
  return true;
}

bool TMP117Component::write_config_(uint16_t config) {
  if (!this->write_byte_16(1, config)) {
    ESP_LOGE(TAG, "Writing TMP117 config failed!");
    return false;
  }
  return true;
}

}  // namespace tmp117
}  // namespace esphome
