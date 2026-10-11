#include "esphome/core/defines.h"
#ifdef USE_ESP32
#include "ecocomfort2_hub.h"
#include "esphome/core/log.h"

namespace esphome::ecocomfort2 {

ESPHOME_LOG_TAG(TAG, "ecocomfort2");

static const espbt::ESPBTUUID SERVICE_UUID = espbt::ESPBTUUID::from_raw("f4b827c3-e660-4bc8-bdf6-3c8e9b845e0d");
static const espbt::ESPBTUUID C_OPER_UUID = espbt::ESPBTUUID::from_raw("b9d6f678-bc0d-4a73-90c8-60b0f07301f1");

void Ecocomfort2Hub::update() {
  if (!this->is_ready()) {
    ESP_LOGD(TAG, "[%s] Not ready, skipping update", this->parent_->address_str());
    return;
  }
  auto status = esp_ble_gattc_read_char(this->parent_->get_gattc_if(), this->parent_->get_conn_id(),
                                        this->char_handle_oper_, ESP_GATT_AUTH_REQ_NONE);
  if (status != ESP_GATT_OK) {
    ESP_LOGW(TAG, "[%s] C_OPER read request failed, status=%d", this->parent_->address_str(), status);
  }
}

void Ecocomfort2Hub::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Ecocomfort2 Hub:\n"
                "  Address: %s",
                this->parent_->address_str());
  LOG_UPDATE_INTERVAL(this);
}

void Ecocomfort2Hub::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                         esp_ble_gattc_cb_param_t *param) {
  switch (event) {
    case ESP_GATTC_DISCONNECT_EVT: {
      ESP_LOGD(TAG, "[%s] Disconnected: reason=%d", this->parent_->address_str(), param->disconnect.reason);
      this->cancel_timeout(AUTH_TIMEOUT_ID);
      this->cancel_timeout(READBACK_TIMEOUT_ID);
      this->status_set_warning();
      this->char_handle_oper_ = 0;
      break;
    }
    case ESP_GATTC_SEARCH_CMPL_EVT: {
      // Keep the handle, not the characteristic: ble_client frees its services once all nodes are established.
      auto *chr = this->parent_->get_characteristic(SERVICE_UUID, C_OPER_UUID);
      if (chr == nullptr) {
        ESP_LOGW(TAG, "[%s] No C_OPER characteristic found, disconnecting", this->parent_->address_str());
        this->parent_->disconnect();
        break;
      }
      this->char_handle_oper_ = chr->handle;
      this->node_state = espbt::ClientState::ESTABLISHED;
      this->status_set_warning();

      // A bonded device may already have re-encrypted the link before discovery finished.
      if (this->parent_->is_paired()) {
        this->on_paired_();
        break;
      }
      ESP_LOGI(TAG, "[%s] Requesting BLE encryption", this->parent_->address_str());
      if (this->parent_->pair() != ESP_OK) {
        ESP_LOGW(TAG, "[%s] BLE pair request failed, disconnecting", this->parent_->address_str());
        this->parent_->disconnect();
        break;
      }
      this->set_timeout(AUTH_TIMEOUT_ID, AUTH_TIMEOUT_MS, [this]() {
        ESP_LOGW(TAG, "[%s] BLE authentication timed out, disconnecting", this->parent_->address_str());
        this->parent_->disconnect();
      });
      break;
    }
    case ESP_GATTC_READ_CHAR_EVT: {
      if (param->read.conn_id != this->parent_->get_conn_id() || param->read.handle != this->char_handle_oper_) {
        break;
      }
      if (param->read.status == ESP_GATT_INSUF_ENCRYPTION || param->read.status == ESP_GATT_INSUF_AUTHENTICATION) {
        // ble_client reconnects and pairs again
        ESP_LOGW(TAG, "[%s] Encryption lost, disconnecting", this->parent_->address_str());
        this->parent_->disconnect();
        break;
      }
      if (param->read.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "[%s] C_OPER read failed, status=%d", this->parent_->address_str(), param->read.status);
        break;
      }
      if (param->read.value_len < 2) {
        ESP_LOGW(TAG, "[%s] C_OPER too short: %u bytes", this->parent_->address_str(), param->read.value_len);
        break;
      }
      this->mode_ = param->read.value[0];
      this->speed_ = param->read.value[1];
      ESP_LOGD(TAG, "[%s] C_OPER: mode=0x%02x, speed=0x%02x", this->parent_->address_str(), this->mode_, this->speed_);
#ifdef ECOCOMFORT2_CHILD_COUNT
      for (auto *child : this->children_) {
        child->on_status();
      }
#endif
      break;
    }
    default:
      break;
  }
}

void Ecocomfort2Hub::gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  if (event != ESP_GAP_BLE_AUTH_CMPL_EVT || !this->parent_->check_addr(param->ble_security.auth_cmpl.bd_addr) ||
      this->node_state != espbt::ClientState::ESTABLISHED) {
    return;
  }
  if (!param->ble_security.auth_cmpl.success) {
    this->cancel_timeout(AUTH_TIMEOUT_ID);
    ESP_LOGW(TAG, "[%s] BLE authentication failed, reason=%d, disconnecting", this->parent_->address_str(),
             param->ble_security.auth_cmpl.fail_reason);
    this->parent_->disconnect();
    return;
  }
  this->on_paired_();
}

void Ecocomfort2Hub::on_paired_() {
  this->cancel_timeout(AUTH_TIMEOUT_ID);
  this->status_clear_warning();
  ESP_LOGI(TAG, "[%s] Encryption confirmed, device ready", this->parent_->address_str());
  this->update();
}

void Ecocomfort2Hub::write_oper(uint8_t mode, uint8_t speed) {
  if (!this->is_ready()) {
    ESP_LOGW(TAG, "[%s] Not ready, cannot write C_OPER", this->parent_->address_str());
    return;
  }
  uint8_t data[2]{mode, speed};
  ESP_LOGD(TAG, "[%s] Writing C_OPER: mode=0x%02x, speed=0x%02x", this->parent_->address_str(), mode, speed);
  auto status =
      esp_ble_gattc_write_char(this->parent_->get_gattc_if(), this->parent_->get_conn_id(), this->char_handle_oper_,
                               sizeof(data), data, ESP_GATT_WRITE_TYPE_NO_RSP, ESP_GATT_AUTH_REQ_NONE);
  if (status != ESP_GATT_OK) {
    ESP_LOGW(TAG, "[%s] C_OPER write failed, status=%d", this->parent_->address_str(), status);
    return;
  }
  this->set_timeout(READBACK_TIMEOUT_ID, READBACK_DELAY_MS, [this]() { this->update(); });
}

#ifdef ECOCOMFORT2_CHILD_COUNT
void Ecocomfort2Hub::register_child(Ecocomfort2Client *obj) {
  this->children_.push_back(obj);
  obj->set_parent(this);
}
#endif

}  // namespace esphome::ecocomfort2

#endif  // USE_ESP32
