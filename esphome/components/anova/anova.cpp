#include "anova.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome {
namespace anova {

static const char *const TAG = "anova";

using namespace esphome::climate;

void Anova::dump_config() { LOG_CLIMATE("", "Anova BLE Cooker", this); }

void Anova::setup() {
  this->codec_ = make_unique<AnovaCodec>();
  this->current_request_ = 0;
}

void Anova::loop() {}

void Anova::control(const ClimateCall &call) {
  if (call.get_mode().has_value()) {
    ClimateMode mode = *call.get_mode();
    AnovaPacket *pkt;
    switch (mode) {
      case climate::CLIMATE_MODE_OFF:
        pkt = this->codec_->get_stop_request();
        break;
      case climate::CLIMATE_MODE_HEAT:
        pkt = this->codec_->get_start_request();
        break;
      default:
        ESP_LOGW(TAG, "Unsupported mode: %d", mode);
        return;
    }
    auto status = esp_ble_gattc_write_char(this->parent_->gattc_if, this->parent_->conn_id, this->char_handle_,
                                           pkt->length, pkt->data, ESP_GATT_WRITE_TYPE_NO_RSP, ESP_GATT_AUTH_REQ_NONE);
    if (status)
      ESP_LOGW(TAG, "[%s] esp_ble_gattc_write_char failed, status=%d", this->parent_->address_str().c_str(), status);
  }
  if (call.get_target_temperature().has_value()) {
    auto pkt = this->codec_->get_set_target_temp_request(*call.get_target_temperature());
    auto status = esp_ble_gattc_write_char(this->parent_->gattc_if, this->parent_->conn_id, this->char_handle_,
                                           pkt->length, pkt->data, ESP_GATT_WRITE_TYPE_NO_RSP, ESP_GATT_AUTH_REQ_NONE);
    if (status)
      ESP_LOGW(TAG, "[%s] esp_ble_gattc_write_char failed, status=%d", this->parent_->address_str().c_str(), status);
  }
}

void Anova::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param) {
  switch (event) {
    case ESP_GATTC_DISCONNECT_EVT: {
      this->current_temperature = NAN;
      this->target_temperature = NAN;
      this->publish_state();
      break;
    }
    case ESP_GATTC_SEARCH_CMPL_EVT: {
      auto chr = this->parent_->get_characteristic(ANOVA_SERVICE_UUID, ANOVA_CHARACTERISTIC_UUID);
      if (chr == nullptr) {
        ESP_LOGW(TAG, "[%s] No control service found at device, not an Anova..?", this->get_name().c_str());
        ESP_LOGW(TAG, "[%s] Note, this component does not currently support Anova Nano.", this->get_name().c_str());
        break;
      }
      this->char_handle_ = chr->handle;

      auto status = esp_ble_gattc_register_for_notify(this->parent_->gattc_if, this->parent_->remote_bda, chr->handle);
      if (status) {
        ESP_LOGW(TAG, "[%s] esp_ble_gattc_register_for_notify failed, status=%d", this->get_name().c_str(), status);
      }
      break;
    }
    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
      this->node_state = espbt::ClientState::ESTABLISHED;
      this->current_request_ = 0;
      this->update();
      break;
    }
    case ESP_GATTC_NOTIFY_EVT: {
      if (param->notify.handle != this->char_handle_)
        break;
      this->codec_->decode(param->notify.value, param->notify.value_len);
      if (this->codec_->has_target_temp()) {
        this->target_temperature = this->codec_->target_temp_;
      }
      if (this->codec_->has_current_temp()) {
        this->current_temperature = this->codec_->current_temp_;
      }
      if (this->codec_->has_running()) {
        this->mode = this->codec_->running_ ? climate::CLIMATE_MODE_HEAT : climate::CLIMATE_MODE_OFF;
      }
      if (this->codec_->has_unit()) {
        this->fahrenheit_ = (this->codec_->unit_ == 'f');
        ESP_LOGD(TAG, "Anova units is %s", this->fahrenheit_ ? "fahrenheit" : "celcius");
        this->current_request_++;
      }
      this->publish_state();

      if (this->current_request_ > 1) {
        AnovaPacket *pkt = nullptr;
        switch (this->current_request_++) {
          case 2:
            pkt = this->codec_->get_read_target_temp_request();
            break;
          case 3:
            pkt = this->codec_->get_read_current_temp_request();
            break;
          default:
            this->current_request_ = 1;
            break;
        }
        if (pkt != nullptr) {
          auto status =
              esp_ble_gattc_write_char(this->parent_->gattc_if, this->parent_->conn_id, this->char_handle_, pkt->length,
                                       pkt->data, ESP_GATT_WRITE_TYPE_NO_RSP, ESP_GATT_AUTH_REQ_NONE);
          if (status)
            ESP_LOGW(TAG, "[%s] esp_ble_gattc_write_char failed, status=%d", this->parent_->address_str().c_str(),
                     status);
        }
      }
      break;
    }
    default:
      break;
  }
}

void Anova::set_unit_of_measurement(const char *unit) { this->fahrenheit_ = !strncmp(unit, "f", 1); }

void Anova::update() {
  if (this->node_state != espbt::ClientState::ESTABLISHED)
    return;

  if (this->current_request_ < 2) {
    auto pkt = this->codec_->get_read_device_status_request();
    if (this->current_request_ == 0)
      this->codec_->get_set_unit_request(this->fahrenheit_ ? 'f' : 'c');
    auto status = esp_ble_gattc_write_char(this->parent_->gattc_if, this->parent_->conn_id, this->char_handle_,
                                           pkt->length, pkt->data, ESP_GATT_WRITE_TYPE_NO_RSP, ESP_GATT_AUTH_REQ_NONE);
    if (status)
      ESP_LOGW(TAG, "[%s] esp_ble_gattc_write_char failed, status=%d", this->parent_->address_str().c_str(), status);
    this->current_request_++;
  }
}

}  // namespace anova
}  // namespace esphome

#endif
