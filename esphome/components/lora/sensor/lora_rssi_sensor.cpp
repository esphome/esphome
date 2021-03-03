#include "lora_rssi_sensor.h"
namespace esphome {
namespace lora {

static const char *TAG = "lora_rssi_sensor";
void LoraRSSISensor::dump_config() { LOG_SENSOR("", "RSSI Signal", this); }
void LoraRSSISensor::update() { this->publish_state(this->lora_->last_rssi); }
}  // namespace lora
}  // namespace esphome