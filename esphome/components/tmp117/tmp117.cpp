// Implementation based on:
//  - DHT 12 Component

#include "tmp117.h"
#include "esphome/core/log.h"

namespace esphome::tmp117 {

static const char *const TAG = "tmp117";

void TMP117Component::update() {
  int16_t data;
  if (!this->read_data_(&data)) {
    this->status_set_warning();
    return;
  }
  if ((uint16_t) data != 0x8000) {
    float temperature = data * 0.0078125f;

    this->publish_state(temperature);
    this->status_clear_warning();
  } else {
    ESP_LOGD(TAG, "Not ready");
  }
}
void TMP117Component::setup() {
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
  ESP_LOGCONFIG(TAG, "TMP117:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  }
  LOG_SENSOR("  ", "Temperature", this);
}

bool TMP117Component::read_data_(int16_t *data) {
  if (!this->read_byte_16(0, (uint16_t *) data)) {
    ESP_LOGW(TAG, "Updating failed");
    return false;
  }
  return true;
}

bool TMP117Component::read_config_(uint16_t *config) {
  if (!this->read_byte_16(1, (uint16_t *) config)) {
    ESP_LOGW(TAG, "Reading config failed");
    return false;
  }
  return true;
}

bool TMP117Component::write_config_(uint16_t config) {
  if (!this->write_byte_16(1, config)) {
    ESP_LOGE(TAG, "Writing config failed");
    return false;
  }
  return true;
}

}  // namespace esphome::tmp117
