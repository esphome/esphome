#include "ble_room_tracker_sensor.h"

#ifdef USE_ESP32

namespace esphome {
namespace ble_room_tracker {

static const char *const TAG = "ble_room_tracker";

void BLERoomTracker::update_tracker(int rssi, int signal_power) {
  if (this->rssi_sensor_ != nullptr)
    this->rssi_sensor_->publish_state(rssi);

  if (this->signal_power_sensor_ != nullptr && signal_power != 0)
    this->signal_power_sensor_->publish_state(signal_power);

  if (signal_power == 0)
    signal_power = -71;

  float distance = pow(10, (signal_power - rssi) / (10.0f * 3.5f));

  if (this->distance_sensor_ != nullptr)
    this->distance_sensor_->publish_state(distance);

  this->mqtt_room_->send_tracker_update(this->id_, this->name_, distance);
}

void BLERoomTracker::dump_config() {
  ESP_LOGCONFIG(TAG, "BLE Room Tracker:");
  ESP_LOGCONFIG(TAG, "  Tracker ID: '%s'", this->id_.c_str());
  ESP_LOGCONFIG(TAG, "  Name: '%s'", this->name_.c_str());
  LOG_SENSOR("  ", "RSSI", this->rssi_sensor_);
  LOG_SENSOR("  ", "Signal power", this->signal_power_sensor_);
  LOG_SENSOR("  ", "Distance", this->distance_sensor_);
}
}  // namespace ble_room_tracker
}  // namespace esphome

#endif
