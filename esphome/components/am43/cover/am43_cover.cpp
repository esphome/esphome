#include "am43_cover.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome {
namespace am43 {

static const char *const TAG = "am43_cover";

using namespace esphome::cover;

void Am43Component::dump_config() {
  LOG_COVER("", "AM43 Cover", this);
  ESP_LOGCONFIG(TAG, "  Device Pin: %d", this->pin_);
  ESP_LOGCONFIG(TAG, "  Invert Position: %d", (int) this->invert_position_);
}

void Am43Component::setup() {
  this->position = COVER_OPEN;
  this->encoder_ = make_unique<Am43Encoder>();
  this->decoder_ = make_unique<Am43Decoder>();
  this->logged_in_ = false;
}

void Am43Component::loop() {
  if (this->node_state == espbt::ClientState::ESTABLISHED && !this->logged_in_) {
    auto *packet = this->encoder_->get_send_pin_request(this->pin_);
    auto status =
        esp_ble_gattc_write_char(this->parent_->get_gattc_if(), this->parent_->get_conn_id(), this->char_handle_,
                                 packet->length, packet->data, ESP_GATT_WRITE_TYPE_NO_RSP, ESP_GATT_AUTH_REQ_NONE);
    ESP_LOGI(TAG, "[%s] Logging into AM43", this->get_name().c_str());
    if (status) {
      ESP_LOGW(TAG, "[%s] Error writing set_pin to device, error = %d", this->get_name().c_str(), status);
    } else {
      this->logged_in_ = true;
    }
  }
}

CoverTraits Am43Component::get_traits() {
  auto traits = CoverTraits();
  traits.set_supports_stop(true);
  traits.set_supports_position(true);
  traits.set_supports_tilt(false);
  traits.set_is_assumed_state(false);
  return traits;
}

void Am43Component::control(const CoverCall &call) {
  if (this->node_state != espbt::ClientState::ESTABLISHED) {
    ESP_LOGW(TAG, "[%s] Cannot send cover control, not connected", this->get_name().c_str());
    return;
  }
  if (call.get_stop()) {
    auto *packet = this->encoder_->get_stop_request();
    auto status =
        esp_ble_gattc_write_char(this->parent_->get_gattc_if(), this->parent_->get_conn_id(), this->char_handle_,
                                 packet->length, packet->data, ESP_GATT_WRITE_TYPE_NO_RSP, ESP_GATT_AUTH_REQ_NONE);
    if (status) {
      ESP_LOGW(TAG, "[%s] Error writing stop command to device, error = %d", this->get_name().c_str(), status);
    }
  }
  if (call.get_position().has_value()) {
    auto pos = *call.get_position();

    if (this->invert_position_)
      pos = 1 - pos;
    auto *packet = this->encoder_->get_set_position_request(100 - (uint8_t) (pos * 100));
    auto status =
        esp_ble_gattc_write_char(this->parent_->get_gattc_if(), this->parent_->get_conn_id(), this->char_handle_,
                                 packet->length, packet->data, ESP_GATT_WRITE_TYPE_NO_RSP, ESP_GATT_AUTH_REQ_NONE);
    if (status) {
      ESP_LOGW(TAG, "[%s] Error writing set_position command to device, error = %d", this->get_name().c_str(), status);
    }
  }
}

void Am43Component::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                        esp_ble_gattc_cb_param_t *param) {
  switch (event) {
    case ESP_GATTC_DISCONNECT_EVT: {
      this->logged_in_ = false;
      break;
    }
    case ESP_GATTC_SEARCH_CMPL_EVT: {
      auto *chr = this->parent_->get_characteristic(AM43_SERVICE_UUID, AM43_CHARACTERISTIC_UUID);
      if (chr == nullptr) {
        if (this->parent_->get_characteristic(AM43_TUYA_SERVICE_UUID, AM43_TUYA_CHARACTERISTIC_UUID) != nullptr) {
          ESP_LOGE(TAG, "[%s] Detected a Tuya AM43 which is not supported, sorry.", this->get_name().c_str());
        } else {
          ESP_LOGE(TAG, "[%s] No control service found at device, not an AM43..?", this->get_name().c_str());
        }
        break;
      }
      this->char_handle_ = chr->handle;

      auto status = esp_ble_gattc_register_for_notify(this->parent_->get_gattc_if(), this->parent_->get_remote_bda(),
                                                      chr->handle);
      if (status) {
        ESP_LOGW(TAG, "[%s] esp_ble_gattc_register_for_notify failed, status=%d", this->get_name().c_str(), status);
      }
      break;
    }
    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
      this->node_state = espbt::ClientState::ESTABLISHED;
      break;
    }
    case ESP_GATTC_NOTIFY_EVT: {
      if (param->notify.handle != this->char_handle_)
        break;
      this->decoder_->decode(param->notify.value, param->notify.value_len);

      if (this->decoder_->has_position()) {
        this->position = ((float) this->decoder_->position_ / 100.0);
        if (!this->invert_position_)
          this->position = 1 - this->position;
        if (this->position > 0.97)
          this->position = 1.0;
        if (this->position < 0.02)
          this->position = 0.0;
        this->publish_state();
      }

      if (this->decoder_->has_pin_response()) {
        if (this->decoder_->pin_ok_) {
          ESP_LOGI(TAG, "[%s] AM43 pin accepted.", this->get_name().c_str());
          auto *packet = this->encoder_->get_position_request();
          auto status = esp_ble_gattc_write_char(this->parent_->get_gattc_if(), this->parent_->get_conn_id(),
                                                 this->char_handle_, packet->length, packet->data,
                                                 ESP_GATT_WRITE_TYPE_NO_RSP, ESP_GATT_AUTH_REQ_NONE);
          if (status) {
            ESP_LOGW(TAG, "[%s] Error writing set_position to device, error = %d", this->get_name().c_str(), status);
          }
        } else {
          ESP_LOGW(TAG, "[%s] AM43 pin rejected!", this->get_name().c_str());
        }
      }

      if (this->decoder_->has_set_position_response() && !this->decoder_->set_position_ok_) {
        ESP_LOGW(TAG, "[%s] Got nack after sending set_position. Bad pin?", this->get_name().c_str());
      }

      if (this->decoder_->has_set_state_response() && !this->decoder_->set_state_ok_) {
        ESP_LOGW(TAG, "[%s] Got nack after sending set_state. Bad pin?", this->get_name().c_str());
      }
      break;
    }
    default:
      break;
  }
}

}  // namespace am43
}  // namespace esphome

#endif
