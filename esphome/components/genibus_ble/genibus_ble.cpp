#include "genibus_ble.h"

#ifdef USE_ESP32

#include "esphome/core/log.h"

#include "write_chunks.h"

namespace esphome::genibus_ble {

ESPHOME_LOG_TAG(TAG, "genibus_ble");

// The GENI service and its characteristic 859cffd1-036e-432a-aa28-1a0085b87ba9 (bytes in reverse order)
static const espbt::ESPBTUUID GENI_SERVICE_UUID = espbt::ESPBTUUID::from_uint16(0xFE5D);
static const espbt::ESPBTUUID GENI_CHARACTERISTIC_UUID = espbt::ESPBTUUID::from_raw(
    {0xA9, 0x7B, 0xB8, 0x85, 0x00, 0x1A, 0x28, 0xAA, 0x2A, 0x43, 0x6E, 0x03, 0xD1, 0xFF, 0x9C, 0x85});
// The payload of one write with the default ATT MTU of 23
static constexpr size_t CHUNK_SIZE = 20;
// How long the link settles after notifications are on before the first request
static constexpr uint32_t SETTLE_MS = 500;

void GenibusBLE::dump_config() {
  ESP_LOGCONFIG(TAG, "GENIbus BLE:\n  Address: %s", this->parent_->address_str());
  GenibusBus::dump_config();
}

void GenibusBLE::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                     esp_ble_gattc_cb_param_t *param) {
  switch (event) {
    case ESP_GATTC_CONNECT_EVT: {
      // The pump answers only on an encrypted link
      esp_err_t err = this->parent_->pair();
      if (err != ESP_OK) {
        ESP_LOGW(TAG, "[%s] Starting encryption failed: %s", this->parent_->address_str(), esp_err_to_name(err));
      }
      break;
    }
    case ESP_GATTC_SEARCH_CMPL_EVT: {
      auto *chr = this->parent_->get_characteristic(GENI_SERVICE_UUID, GENI_CHARACTERISTIC_UUID);
      if (chr == nullptr) {
        this->link_fault_(LOG_STR("No GENI characteristic"));
        break;
      }
      this->link_.set_handle(chr->handle);
      if (this->parent_->register_for_notify(chr->handle) != ESP_OK)
        this->link_fault_(LOG_STR("Notify registration failed"));
      break;
    }
    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
      if (!this->link_.is_ours(param->reg_for_notify.handle))
        break;
      if (param->reg_for_notify.status != ESP_GATT_OK) {
        this->link_fault_(LOG_STR("Notify registration failed"));
        break;
      }
      // BLEClientBase has just requested the CCCD write in this event; the BLE stack queues our first request
      // behind it. The CCCD lookup needs the GATT cache, which ble_client frees once every node is established.
      this->node_state = espbt::ClientState::ESTABLISHED;
      // The pump drops requests sent soon after notifications are on. Measured on an ALPHA3 from a Mac, 10
      // connections each: with waits of 0, 100 and 200 ms 4 to 5 lost a request, with 300 and 500 ms none.
      // Wait without blocking; a disconnect cancels the wait.
      if (this->link_.settle())
        this->set_timeout(SETTLE_TIMEOUT_ID, SETTLE_MS, [this]() { this->link_up_(); });
      break;
    }
    case ESP_GATTC_NOTIFY_EVT: {
      if (this->link_.accepts(param->notify.handle))
        this->receive_bytes({param->notify.value, param->notify.value_len});
      break;
    }
    // A passive disconnect sends DISCONNECT, then CLOSE; an active one may send only CLOSE
    case ESP_GATTC_DISCONNECT_EVT:
    case ESP_GATTC_CLOSE_EVT:
      this->link_down_();
      break;
    default:
      break;
  }
}

void GenibusBLE::gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  if (event != ESP_GAP_BLE_AUTH_CMPL_EVT || param->ble_security.auth_cmpl.success ||
      !this->parent_->check_addr(param->ble_security.auth_cmpl.bd_addr))
    return;
  // esp32_ble_client logs every failure with its reason; warn once per series of failures. The docs explain how to
  // pair. They must not tell users to hold the Connect button: holding it for 2 s changes the control mode of an
  // ALPHA3 (installation and operating instructions 99392378, section 7.10).
  if (this->link_.fault()) {
    ESP_LOGW(TAG, "[%s] Pairing failed", this->parent_->address_str());
    this->status_set_warning(LOG_STR("Pairing failed"));
  }
}

bool GenibusBLE::write_frame(std::span<const uint8_t> frame) {
  if (!this->link_.is_up())
    return false;
  return write_in_chunks(frame, CHUNK_SIZE, [this](std::span<const uint8_t> chunk) {
    // The API does not write through the pointer
    esp_err_t err = esp_ble_gattc_write_char(this->parent_->get_gattc_if(), this->parent_->get_conn_id(),
                                             this->link_.handle(), chunk.size(), const_cast<uint8_t *>(chunk.data()),
                                             ESP_GATT_WRITE_TYPE_NO_RSP, ESP_GATT_AUTH_REQ_NONE);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "[%s] Write failed: %s", this->parent_->address_str(), esp_err_to_name(err));
    }
    return err == ESP_OK;
  });
}

void GenibusBLE::link_up_() {
  if (!this->link_.is_settling())
    return;
  if (this->link_.up())
    this->status_clear_warning();
  ESP_LOGI(TAG, "[%s] Connected", this->parent_->address_str());
  this->connection_established_();
}

void GenibusBLE::link_down_() {
  this->cancel_timeout(SETTLE_TIMEOUT_ID);
  if (!this->link_.down())
    return;
  ESP_LOGI(TAG, "[%s] Disconnected", this->parent_->address_str());
  this->connection_lost_();
  // A timeout just before the link dropped is no fault; without a link no transaction would clear it
  if (!this->link_.has_fault())
    this->status_clear_warning();
}

void GenibusBLE::link_fault_(const LogString *reason) {
  this->cancel_timeout(SETTLE_TIMEOUT_ID);
  if (this->link_.fault()) {
    ESP_LOGW(TAG, "[%s] %s, disconnecting", this->parent_->address_str(), LOG_STR_ARG(reason));
    this->status_set_warning(reason);
  } else {
    ESP_LOGD(TAG, "[%s] %s, disconnecting", this->parent_->address_str(), LOG_STR_ARG(reason));
  }
  // Free the pump's only connection for the Grundfos GO app; ble_client reconnects when auto_connect is on
  this->parent_->disconnect();
}

}  // namespace esphome::genibus_ble

#endif  // USE_ESP32
