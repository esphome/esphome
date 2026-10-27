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

void Am43Component::setup() { this->position = COVER_OPEN; }

void Am43Component::loop() {
  if (this->node_state == espbt::ClientState::ESTABLISHED && !this->logged_in_) {
    ESP_LOGI(TAG, "[%s] Logging into AM43", LOG_STR_ARG(this->get_log_name()));
    this->logged_in_ = this->write_packet_(Am43Encoder::get_send_pin_request(this->pin_), LOG_STR("set_pin"));
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
  if (call.get_stop())
    this->write_packet_(Am43Encoder::get_stop_request(), LOG_STR("stop"));
  auto opt_pos = call.get_position();
  if (opt_pos.has_value()) {
    auto pos = *opt_pos;

    if (this->invert_position_)
      pos = 1 - pos;
    this->write_packet_(Am43Encoder::get_set_position_request(100 - (uint8_t) (pos * 100)), LOG_STR("set_position"));
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
      Am43Decoder decoder;
      decoder.decode(param->notify.value, param->notify.value_len);

      if (decoder.has_position()) {
        this->position = ((float) decoder.position_ / 100.0f);
        if (!this->invert_position_)
          this->position = 1 - this->position;
        if (this->position > 0.97f)
          this->position = 1.0f;
        if (this->position < 0.02f)
          this->position = 0.0f;
        this->publish_state();
      }

      if (decoder.has_pin_response()) {
        if (decoder.pin_ok_) {
          ESP_LOGI(TAG, "[%s] AM43 pin accepted.", LOG_STR_ARG(this->get_log_name()));
          this->write_packet_(Am43Encoder::get_position_request(), LOG_STR("get_position"));
        } else {
          ESP_LOGW(TAG, "[%s] AM43 pin rejected!", LOG_STR_ARG(this->get_log_name()));
        }
      }

      if (decoder.has_set_position_response() && !decoder.set_position_ok_) {
        ESP_LOGW(TAG, "[%s] Got nack after sending set_position. Bad pin?", LOG_STR_ARG(this->get_log_name()));
      }

      if (decoder.has_set_state_response() && !decoder.set_state_ok_) {
        ESP_LOGW(TAG, "[%s] Got nack after sending set_state. Bad pin?", LOG_STR_ARG(this->get_log_name()));
      }
      break;
    }
    default:
      break;
  }
}

bool Am43Component::write_packet_(const Am43Packet &packet, const LogString *what) {
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast) the IDF API takes a mutable buffer it only reads
  auto status = esp_ble_gattc_write_char(this->parent_->get_gattc_if(), this->parent_->get_conn_id(),
                                         this->char_handle_, packet.length, const_cast<uint8_t *>(packet.data),
                                         ESP_GATT_WRITE_TYPE_NO_RSP, ESP_GATT_AUTH_REQ_NONE);
  if (status) {
    ESP_LOGW(TAG, "[%s] Error writing %s to device, error = %d", LOG_STR_ARG(this->get_log_name()), LOG_STR_ARG(what),
             status);
    return false;
  }
  return true;
}

}  // namespace esphome::am43

#endif
