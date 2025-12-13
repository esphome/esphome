#include "pvvx_display.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32
namespace esphome {
namespace pvvx_mithermometer {

static const char *const TAG = "display.pvvx_mithermometer";

void PVVXDisplay::dump_config() {
  ESP_LOGCONFIG(TAG,
                "PVVX MiThermometer display:\n"
                "  MAC address           : %s\n"
                "  Service UUID          : %s\n"
                "  Characteristic UUID   : %s\n"
                "  Auto clear            : %s",
                this->parent_->address_str(), this->service_uuid_.to_string().c_str(),
                this->char_uuid_.to_string().c_str(), YESNO(this->auto_clear_enabled_));
#ifdef USE_TIME
  ESP_LOGCONFIG(TAG, "  Set time on connection: %s", YESNO(this->time_ != nullptr));
#endif
  ESP_LOGCONFIG(TAG, "  Disconnect delay      : %" PRIu32 "ms", this->disconnect_delay_ms_);
  LOG_UPDATE_INTERVAL(this);
}

void PVVXDisplay::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                      esp_ble_gattc_cb_param_t *param) {
  switch (event) {
    case ESP_GATTC_OPEN_EVT:
      if (param->open.status == ESP_GATT_OK) {
        ESP_LOGV(TAG, "[%s] Connected successfully!", this->parent_->address_str());
        this->delayed_disconnect_();
      }
      break;
    case ESP_GATTC_DISCONNECT_EVT:
      ESP_LOGV(TAG, "[%s] Disconnected", this->parent_->address_str());
      this->connection_established_ = false;
      this->cancel_timeout("disconnect");
      this->char_handle_ = 0;
      break;
    case ESP_GATTC_SEARCH_CMPL_EVT: {
      auto *chr = this->parent_->get_characteristic(this->service_uuid_, this->char_uuid_);
      if (chr == nullptr) {
        ESP_LOGW(TAG, "[%s] Characteristic not found.", this->parent_->address_str());
        break;
      }
      this->connection_established_ = true;
      this->char_handle_ = chr->handle;

      // Attempt to write immediately
      // For devices without security, this will work
      // For devices with security that are already paired, this will work
      // For devices that need pairing, the write will be retried after auth completes
      this->sync_time_and_display_();
      break;
    }
    default:
      break;
  }
}

void PVVXDisplay::gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  switch (event) {
    case ESP_GAP_BLE_AUTH_CMPL_EVT: {
      if (!this->parent_->check_addr(param->ble_security.auth_cmpl.bd_addr))
        return;

      if (param->ble_security.auth_cmpl.success) {
        ESP_LOGD(TAG, "[%s] Authentication successful, performing writes.", this->parent_->address_str());
        // Now that pairing is complete, perform the pending writes
        this->sync_time_and_display_();
      } else {
        ESP_LOGW(TAG, "[%s] Authentication failed.", this->parent_->address_str());
      }
      break;
    }
    default:
      break;
  }
}

void PVVXDisplay::update() {
  if (this->auto_clear_enabled_)
    this->clear();
  if (this->writer_.has_value())
    (*this->writer_)(*this);
  this->display();
}

void PVVXDisplay::display() {
  if (!this->parent_->enabled) {
    ESP_LOGD(TAG, "[%s] BLE client not enabled.  Init connection.", this->parent_->address_str());
    this->parent_->set_enabled(true);
    return;
  }
  if (!this->connection_established_) {
    ESP_LOGW(TAG, "[%s] Not connected to BLE client.  State update can not be written.", this->parent_->address_str());
    return;
  }
  if (!this->char_handle_) {
    ESP_LOGW(TAG, "[%s] No ble handle to BLE client.  State update can not be written.", this->parent_->address_str());
    return;
  }
  ESP_LOGD(TAG, "[%s] Send to display: bignum %d, smallnum: %d, cfg: 0x%02x, validity period: %u.",
           this->parent_->address_str(), this->bignum_, this->smallnum_, this->cfg_, this->validity_period_);
  uint8_t blk[8] = {};
  blk[0] = 0x22;
  blk[1] = this->bignum_ & 0xff;
  blk[2] = (this->bignum_ >> 8) & 0xff;
  blk[3] = this->smallnum_ & 0xff;
  blk[4] = (this->smallnum_ >> 8) & 0xff;
  blk[5] = this->validity_period_ & 0xff;
  blk[6] = (this->validity_period_ >> 8) & 0xff;
  blk[7] = this->cfg_;
  this->send_to_setup_char_(blk, sizeof(blk));
}

void PVVXDisplay::setcfgbit_(uint8_t bit, bool value) {
  uint8_t mask = 1 << bit;
  if (value) {
    this->cfg_ |= mask;
  } else {
    this->cfg_ &= (0xFF ^ mask);
  }
}

void PVVXDisplay::send_to_setup_char_(uint8_t *blk, size_t size) {
  if (!this->connection_established_) {
    ESP_LOGW(TAG, "[%s] Not connected to BLE client.", this->parent_->address_str());
    return;
  }
  auto status =
      esp_ble_gattc_write_char(this->parent_->get_gattc_if(), this->parent_->get_conn_id(), this->char_handle_, size,
                               blk, ESP_GATT_WRITE_TYPE_NO_RSP, ESP_GATT_AUTH_REQ_NONE);
  if (status) {
    ESP_LOGW(TAG, "[%s] esp_ble_gattc_write_char failed, status=%d", this->parent_->address_str(), status);
  } else {
    ESP_LOGV(TAG, "[%s] send %u bytes", this->parent_->address_str(), size);
    this->delayed_disconnect_();
  }
}

void PVVXDisplay::delayed_disconnect_() {
  if (this->disconnect_delay_ms_ == 0)
    return;
  this->cancel_timeout("disconnect");
  this->set_timeout("disconnect", this->disconnect_delay_ms_, [this]() { this->parent_->set_enabled(false); });
}

void PVVXDisplay::sync_time_and_display_() {
#ifdef USE_TIME
  this->sync_time_();
#endif
  this->display();
}

#ifdef USE_TIME
void PVVXDisplay::sync_time_() {
  if (this->time_ == nullptr)
    return;
  if (!this->connection_established_) {
    ESP_LOGW(TAG, "[%s] Not connected to BLE client.  Time can not be synced.", this->parent_->address_str());
    return;
  }
  if (!this->char_handle_) {
    ESP_LOGW(TAG, "[%s] No ble handle to BLE client.  Time can not be synced.", this->parent_->address_str());
    return;
  }
  auto time = this->time_->now();
  if (!time.is_valid()) {
    ESP_LOGW(TAG, "[%s] Time is not yet valid.  Time can not be synced.", this->parent_->address_str());
    return;
  }
  time.recalc_timestamp_utc(true);  // calculate timestamp of local time
  uint8_t blk[5] = {};
  ESP_LOGD(TAG, "[%s] Sync time with timestamp %" PRIu64 ".", this->parent_->address_str(), time.timestamp);
  blk[0] = 0x23;
  blk[1] = time.timestamp & 0xff;
  blk[2] = (time.timestamp >> 8) & 0xff;
  blk[3] = (time.timestamp >> 16) & 0xff;
  blk[4] = (time.timestamp >> 24) & 0xff;
  this->send_to_setup_char_(blk, sizeof(blk));
}
#endif

}  // namespace pvvx_mithermometer
}  // namespace esphome

#endif
