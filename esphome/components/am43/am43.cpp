#include "am43.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

#ifdef USE_ESP32

namespace esphome {
namespace am43 {

static const char *const TAG = "am43";

void Am43::dump_config() {
  ESP_LOGCONFIG(TAG, "AM43");
  LOG_SENSOR(" ", "Battery", this->battery_);
  LOG_SENSOR(" ", "Illuminance", this->illuminance_);
}

void Am43::setup() {
  this->encoder_ = make_unique<Am43Encoder>();
  this->decoder_ = make_unique<Am43Decoder>();
  this->logged_in_ = false;
  this->last_battery_update_ = 0;
  this->current_sensor_ = 0;
}

void Am43::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param) {
  switch (event) {
    case ESP_GATTC_OPEN_EVT: {
      this->logged_in_ = false;
      break;
    }
    case ESP_GATTC_DISCONNECT_EVT: {
      this->logged_in_ = false;
      this->node_state = espbt::ClientState::IDLE;
      if (this->battery_ != nullptr)
        this->battery_->publish_state(NAN);
      if (this->illuminance_ != nullptr)
        this->illuminance_->publish_state(NAN);
      break;
    }
    case ESP_GATTC_SEARCH_CMPL_EVT: {
      auto chr = this->parent_->get_characteristic(AM43_SERVICE_UUID, AM43_CHARACTERISTIC_UUID);
      if (chr == nullptr) {
        if (this->parent_->get_characteristic(AM43_TUYA_SERVICE_UUID, AM43_TUYA_CHARACTERISTIC_UUID) != nullptr) {
          ESP_LOGE(TAG, "[%s] Detected a Tuya AM43 which is not supported, sorry.",
                   this->parent_->address_str().c_str());
        } else {
          ESP_LOGE(TAG, "[%s] No control service found at device, not an AM43..?",
                   this->parent_->address_str().c_str());
        }
        break;
      }
      this->char_handle_ = chr->handle;
      break;
    }
    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
      this->node_state = espbt::ClientState::ESTABLISHED;
      this->update();
      break;
    }
    case ESP_GATTC_NOTIFY_EVT: {
      if (param->notify.handle != this->char_handle_)
        break;
      this->decoder_->decode(param->notify.value, param->notify.value_len);

      if (this->battery_ != nullptr && this->decoder_->has_battery_level() &&
          millis() - this->last_battery_update_ > 10000) {
        this->battery_->publish_state(this->decoder_->battery_level_);
        this->last_battery_update_ = millis();
      }

      if (this->illuminance_ != nullptr && this->decoder_->has_light_level()) {
        this->illuminance_->publish_state(this->decoder_->light_level_);
      }

      if (this->current_sensor_ > 0) {
        if (this->illuminance_ != nullptr) {
          auto packet = this->encoder_->get_light_level_request();
          auto status = esp_ble_gattc_write_char(this->parent_->gattc_if, this->parent_->conn_id, this->char_handle_,
                                                 packet->length, packet->data, ESP_GATT_WRITE_TYPE_NO_RSP,
                                                 ESP_GATT_AUTH_REQ_NONE);
          if (status)
            ESP_LOGW(TAG, "[%s] esp_ble_gattc_write_char failed, status=%d", this->parent_->address_str().c_str(),
                     status);
        }
        this->current_sensor_ = 0;
      }
      break;
    }
    default:
      break;
  }
}

void Am43::update() {
  if (this->node_state != espbt::ClientState::ESTABLISHED) {
    ESP_LOGW(TAG, "[%s] Cannot poll, not connected", this->parent_->address_str().c_str());
    return;
  }
  if (this->current_sensor_ == 0) {
    if (this->battery_ != nullptr) {
      auto packet = this->encoder_->get_battery_level_request();
      auto status =
          esp_ble_gattc_write_char(this->parent_->gattc_if, this->parent_->conn_id, this->char_handle_, packet->length,
                                   packet->data, ESP_GATT_WRITE_TYPE_NO_RSP, ESP_GATT_AUTH_REQ_NONE);
      if (status)
        ESP_LOGW(TAG, "[%s] esp_ble_gattc_write_char failed, status=%d", this->parent_->address_str().c_str(), status);
    }
    this->current_sensor_++;
  }
}

}  // namespace am43
}  // namespace esphome

#endif
