#include "thermopro_tp970.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/components/esp32_ble/ble_uuid.h"
#include <cinttypes>

#ifdef USE_ESP32
namespace esphome::thermopro_tp970 {

ESPHOME_LOG_TAG(TAG, "thermopro_tp970");

void ThermoProTP970::setup() {
  this->invalidate_();
  this->set_interval(1000, [this]() {
    if (this->streaming_ && uint32_t(millis() - this->last_notification_) >= this->timeout_ms_) {
      ESP_LOGW(TAG, "Temperature notifications timed out");
      this->invalidate_();
    }
  });
}

void ThermoProTP970::dump_config() {
  ESP_LOGCONFIG(TAG, "ThermoPro TP970 Plus:");
  ESP_LOGCONFIG(TAG, "  Timeout: %" PRIu32 " ms", this->timeout_ms_);
#ifdef USE_SENSOR
  LOG_SENSOR("  ", "Internal Temperature", this->internal_temperature_sensor_);
  LOG_SENSOR("  ", "Ambient Temperature", this->ambient_temperature_sensor_);
  LOG_SENSOR("  ", "Battery Level", this->battery_level_sensor_);
  LOG_SENSOR("  ", "Notification Count", this->notification_count_sensor_);
#endif
#ifdef USE_BINARY_SENSOR
  LOG_BINARY_SENSOR("  ", "Temperature Stream", this->temperature_stream_binary_sensor_);
#endif
}

void ThermoProTP970::invalidate_() {
  this->streaming_ = false;
  this->status_set_warning();
#ifdef USE_SENSOR
  if (this->internal_temperature_sensor_ != nullptr)
    this->internal_temperature_sensor_->publish_state(NAN);
  if (this->ambient_temperature_sensor_ != nullptr)
    this->ambient_temperature_sensor_->publish_state(NAN);
  if (this->battery_level_sensor_ != nullptr)
    this->battery_level_sensor_->publish_state(NAN);
#endif
#ifdef USE_BINARY_SENSOR
  if (this->temperature_stream_binary_sensor_ != nullptr)
    this->temperature_stream_binary_sensor_->publish_state(false);
#endif
}

void ThermoProTP970::publish_reading_(const TemperatureReading &reading) {
  this->streaming_ = true;
  this->last_notification_ = millis();
  this->notification_count_++;
  this->status_clear_warning();
#ifdef USE_SENSOR
  if (this->internal_temperature_sensor_ != nullptr)
    this->internal_temperature_sensor_->publish_state(reading.internal_temperature);
  if (this->ambient_temperature_sensor_ != nullptr)
    this->ambient_temperature_sensor_->publish_state(reading.ambient_temperature);
  if (this->battery_level_sensor_ != nullptr)
    this->battery_level_sensor_->publish_state(reading.battery_level);
  if (this->notification_count_sensor_ != nullptr)
    this->notification_count_sensor_->publish_state(this->notification_count_);
#endif
#ifdef USE_BINARY_SENSOR
  if (this->temperature_stream_binary_sensor_ != nullptr)
    this->temperature_stream_binary_sensor_->publish_state(true);
#endif
}

void ThermoProTP970::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                         esp_ble_gattc_cb_param_t *param) {
  switch (event) {
    case ESP_GATTC_DISCONNECT_EVT:
    case ESP_GATTC_CLOSE_EVT:
      this->handle_ = 0;
      this->invalidate_();
      break;
    case ESP_GATTC_SEARCH_CMPL_EVT: {
      this->handle_ = 0;
      auto service = esp32_ble::ESPBTUUID::from_raw("93bb7cab-eb14-c5a9-2354-a24f4df9330f");
      auto characteristic = esp32_ble::ESPBTUUID::from_uint16(0xFF01);
      auto *chr = this->parent()->get_characteristic(service, characteristic);
      if (chr == nullptr) {
        ESP_LOGW(TAG, "Temperature notification characteristic not found");
        this->invalidate_();
        break;
      }
      this->handle_ = chr->handle;
      const auto status = this->parent()->register_for_notify(this->handle_);
      if (status != ESP_OK) {
        ESP_LOGW(TAG, "Cannot register temperature notifications: %d", status);
        this->invalidate_();
      }
      break;
    }
    case ESP_GATTC_REG_FOR_NOTIFY_EVT:
      if (param->reg_for_notify.handle != this->handle_)
        break;
      // Keep the GATT cache alive until asynchronous notification registration finishes.
      this->node_state = esp32_ble_tracker::ClientState::ESTABLISHED;
      if (param->reg_for_notify.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "Temperature notification registration failed: %d", param->reg_for_notify.status);
        this->invalidate_();
      }
      break;
    case ESP_GATTC_NOTIFY_EVT: {
      if (this->handle_ == 0 || param->notify.handle != this->handle_ ||
          param->notify.conn_id != this->parent()->get_conn_id())
        break;
      TemperatureReading reading;
      if (decode_notification(param->notify.value, param->notify.value_len, reading))
        this->publish_reading_(reading);
      break;
    }
    default:
      break;
  }
}

}  // namespace esphome::thermopro_tp970
#endif
