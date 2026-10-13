#include "am43_cover.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome::am43 {

ESPHOME_LOG_TAG(TAG, "am43_cover");

using namespace esphome::cover;

void Am43Component::dump_config() {
  LOG_COVER("", "AM43 Cover", this);
  ESP_LOGCONFIG(TAG,
                "  Device Pin: %d\n"
                "  Invert Position: %d",
                this->pin_, (int) this->invert_position_);
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
    ESP_LOGI(TAG, "[%s] Logging into AM43", LOG_STR_ARG(this->get_log_name()));
    if (status) {
      ESP_LOGW(TAG, "[%s] Error writing set_pin to device, error = %d", LOG_STR_ARG(this->get_log_name()), status);
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
    ESP_LOGW(TAG, "[%s] Cannot send cover control, not connected", LOG_STR_ARG(this->get_log_name()));
    return;
  }
  if (call.get_stop()) {
    auto *packet = this->encoder_->get_stop_request();
    auto status =
        esp_ble_gattc_write_char(this->parent_->get_gattc_if(), this->parent_->get_conn_id(), this->char_handle_,
                                 packet->length, packet->data, ESP_GATT_WRITE_TYPE_NO_RSP, ESP_GATT_AUTH_REQ_NONE);
    if (status) {
      ESP_LOGW(TAG, "[%s] Error writing stop command to device, error = %d", LOG_STR_ARG(this->get_log_name()), status);
    }
  }
  auto opt_pos = call.get_position();
  if (opt_pos.has_value()) {
    auto pos = *opt_pos;

    if (this->invert_position_)
      pos = 1 - pos;
    auto *packet = this->encoder_->get_set_position_request(100 - (uint8_t) (pos * 100));
    auto status =
        esp_ble_gattc_write_char(this->parent_->get_gattc_if(), this->parent_->get_conn_id(), this->char_handle_,
                                 packet->length, packet->data, ESP_GATT_WRITE_TYPE_NO_RSP, ESP_GATT_AUTH_REQ_NONE);
    if (status) {
      ESP_LOGW(TAG, "[%s] Error writing set_position command to device, error = %d", LOG_STR_ARG(this->get_log_name()),
               status);
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
          ESP_LOGE(TAG, "[%s] Detected a Tuya AM43 which is not supported, sorry.", LOG_STR_ARG(this->get_log_name()));
        } else {
          ESP_LOGE(TAG, "[%s] No control service found at device, not an AM43..?", LOG_STR_ARG(this->get_log_name()));
        }
        break;
      }
      this->char_handle_ = chr->handle;

      auto status = esp_ble_gattc_register_for_notify(this->parent_->get_gattc_if(), this->parent_->get_remote_bda(),
                                                      chr->handle);
      if (status) {
        ESP_LOGW(TAG, "[%s] esp_ble_gattc_register_for_notify failed, status=%d", LOG_STR_ARG(this->get_log_name()),
                 status);
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
        this->position = ((float) this->decoder_->position_ / 100.0f);
        if (!this->invert_position_)
          this->position = 1 - this->position;
        if (this->position > 0.97f)
          this->position = 1.0f;
        if (this->position < 0.02f)
          this->position = 0.0f;
        this->publish_state();
      }

      if (this->decoder_->has_pin_response()) {
        if (this->decoder_->pin_ok_) {
          ESP_LOGI(TAG, "[%s] AM43 pin accepted.", LOG_STR_ARG(this->get_log_name()));
          auto *packet = this->encoder_->get_position_request();
          auto status = esp_ble_gattc_write_char(this->parent_->get_gattc_if(), this->parent_->get_conn_id(),
                                                 this->char_handle_, packet->length, packet->data,
                                                 ESP_GATT_WRITE_TYPE_NO_RSP, ESP_GATT_AUTH_REQ_NONE);
          if (status) {
            ESP_LOGW(TAG, "[%s] Error writing set_position to device, error = %d", LOG_STR_ARG(this->get_log_name()),
                     status);
          }
        } else {
          ESP_LOGW(TAG, "[%s] AM43 pin rejected!", LOG_STR_ARG(this->get_log_name()));
        }
      }

      if (this->decoder_->has_set_position_response() && !this->decoder_->set_position_ok_) {
        ESP_LOGW(TAG, "[%s] Got nack after sending set_position. Bad pin?", LOG_STR_ARG(this->get_log_name()));
      }

      if (this->decoder_->has_set_state_response() && !this->decoder_->set_state_ok_) {
        ESP_LOGW(TAG, "[%s] Got nack after sending set_state. Bad pin?", LOG_STR_ARG(this->get_log_name()));
      }
      break;
    }
    default:
      break;
  }
}

}  // namespace esphome::am43

#endif
