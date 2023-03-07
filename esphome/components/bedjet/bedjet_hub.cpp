#ifdef USE_ESP32

#include "bedjet_hub.h"
#include "bedjet_child.h"
#include "bedjet_const.h"

namespace esphome {
namespace bedjet {

static const LogString *bedjet_button_to_string(BedjetButton button) {
  switch (button) {
    case BTN_OFF:
      return LOG_STR("OFF");
    case BTN_COOL:
      return LOG_STR("COOL");
    case BTN_HEAT:
      return LOG_STR("HEAT");
    case BTN_EXTHT:
      return LOG_STR("EXT HT");
    case BTN_TURBO:
      return LOG_STR("TURBO");
    case BTN_DRY:
      return LOG_STR("DRY");
    case BTN_M1:
      return LOG_STR("M1");
    case BTN_M2:
      return LOG_STR("M2");
    case BTN_M3:
      return LOG_STR("M3");
    default:
      return LOG_STR("unknown");
  }
}

/* Public */

void BedJetHub::upgrade_firmware() {
  auto *pkt = this->codec_->get_button_request(MAGIC_UPDATE);
  auto status = this->write_bedjet_packet_(pkt);

  if (status) {
    ESP_LOGW(TAG, "[%s] MAGIC_UPDATE button failed, status=%d", this->get_name().c_str(), status);
  }
}

bool BedJetHub::button_heat() { return this->send_button(BTN_HEAT); }
bool BedJetHub::button_ext_heat() { return this->send_button(BTN_EXTHT); }
bool BedJetHub::button_turbo() { return this->send_button(BTN_TURBO); }
bool BedJetHub::button_cool() { return this->send_button(BTN_COOL); }
bool BedJetHub::button_dry() { return this->send_button(BTN_DRY); }
bool BedJetHub::button_off() { return this->send_button(BTN_OFF); }
bool BedJetHub::button_memory1() { return this->send_button(BTN_M1); }
bool BedJetHub::button_memory2() { return this->send_button(BTN_M2); }
bool BedJetHub::button_memory3() { return this->send_button(BTN_M3); }

bool BedJetHub::set_fan_index(uint8_t fan_speed_index) {
  if (fan_speed_index > 19) {
    ESP_LOGW(TAG, "Invalid fan speed index %d, expecting 0-19.", fan_speed_index);
    return false;
  }

  auto *pkt = this->codec_->get_set_fan_speed_request(fan_speed_index);
  auto status = this->write_bedjet_packet_(pkt);

  if (status) {
    ESP_LOGW(TAG, "[%s] writing fan speed failed, status=%d", this->get_name().c_str(), status);
  }
  return status == 0;
}

uint8_t BedJetHub::get_fan_index() {
  auto *status = this->codec_->get_status_packet();
  if (status != nullptr) {
    return status->fan_step;
  }
  return 0;
}

bool BedJetHub::set_target_temp(float temp_c) {
  auto *pkt = this->codec_->get_set_target_temp_request(temp_c);
  auto status = this->write_bedjet_packet_(pkt);

  if (status) {
    ESP_LOGW(TAG, "[%s] writing target temp failed, status=%d", this->get_name().c_str(), status);
  }
  return status == 0;
}

bool BedJetHub::set_time_remaining(uint8_t hours, uint8_t mins) {
  // FIXME: this may fail depending on current mode or other restrictions enforced by the unit.
  auto *pkt = this->codec_->get_set_runtime_remaining_request(hours, mins);
  auto status = this->write_bedjet_packet_(pkt);

  if (status) {
    ESP_LOGW(TAG, "[%s] writing remaining runtime failed, status=%d", this->get_name().c_str(), status);
  }
  return status == 0;
}

bool BedJetHub::send_button(BedjetButton button) {
  auto *pkt = this->codec_->get_button_request(button);
  auto status = this->write_bedjet_packet_(pkt);

  if (status) {
    ESP_LOGW(TAG, "[%s] writing button %s failed, status=%d", this->get_name().c_str(),
             LOG_STR_ARG(bedjet_button_to_string(button)), status);
  } else {
    ESP_LOGD(TAG, "[%s] writing button %s success", this->get_name().c_str(),
             LOG_STR_ARG(bedjet_button_to_string(button)));
  }
  return status == 0;
}

uint16_t BedJetHub::get_time_remaining() {
  auto *status = this->codec_->get_status_packet();
  if (status != nullptr) {
    return status->time_remaining_secs + status->time_remaining_mins * 60 + status->time_remaining_hrs * 3600;
  }
  return 0;
}

/* Bluetooth/GATT */

uint8_t BedJetHub::write_bedjet_packet_(BedjetPacket *pkt) {
  if (!this->is_connected()) {
    if (!this->parent_->enabled) {
      ESP_LOGI(TAG, "[%s] Cannot write packet: Not connected, enabled=false", this->get_name().c_str());
    } else {
      ESP_LOGW(TAG, "[%s] Cannot write packet: Not connected", this->get_name().c_str());
    }
    return -1;
  }
  auto status = esp_ble_gattc_write_char(this->parent_->get_gattc_if(), this->parent_->get_conn_id(),
                                         this->char_handle_cmd_, pkt->data_length + 1, (uint8_t *) &pkt->command,
                                         ESP_GATT_WRITE_TYPE_NO_RSP, ESP_GATT_AUTH_REQ_NONE);
  return status;
}

/** Configures the local ESP BLE client to register (`true`) or unregister (`false`) for status notifications. */
uint8_t BedJetHub::set_notify_(const bool enable) {
  uint8_t status;
  if (enable) {
    status = esp_ble_gattc_register_for_notify(this->parent_->get_gattc_if(), this->parent_->get_remote_bda(),
                                               this->char_handle_status_);
    if (status) {
      ESP_LOGW(TAG, "[%s] esp_ble_gattc_register_for_notify failed, status=%d", this->get_name().c_str(), status);
    }
  } else {
    status = esp_ble_gattc_unregister_for_notify(this->parent_->get_gattc_if(), this->parent_->get_remote_bda(),
                                                 this->char_handle_status_);
    if (status) {
      ESP_LOGW(TAG, "[%s] esp_ble_gattc_unregister_for_notify failed, status=%d", this->get_name().c_str(), status);
    }
  }
  ESP_LOGV(TAG, "[%s] set_notify: enable=%d; result=%d", this->get_name().c_str(), enable, status);
  return status;
}

bool BedJetHub::discover_characteristics_() {
  bool result = true;
  esphome::ble_client::BLECharacteristic *chr;

  if (!this->char_handle_cmd_) {
    chr = this->parent_->get_characteristic(BEDJET_SERVICE_UUID, BEDJET_COMMAND_UUID);
    if (chr == nullptr) {
      ESP_LOGW(TAG, "[%s] No control service found at device, not a BedJet..?", this->get_name().c_str());
      result = false;
    } else {
      this->char_handle_cmd_ = chr->handle;
    }
  }

  if (!this->char_handle_status_) {
    chr = this->parent_->get_characteristic(BEDJET_SERVICE_UUID, BEDJET_STATUS_UUID);
    if (chr == nullptr) {
      ESP_LOGW(TAG, "[%s] No status service found at device, not a BedJet..?", this->get_name().c_str());
      result = false;
    } else {
      this->char_handle_status_ = chr->handle;
    }
  }

  if (!this->config_descr_status_) {
    // We also need to obtain the config descriptor for this handle.
    // Otherwise once we set node_state=Established, the parent will flush all handles/descriptors, and we won't be
    // able to look it up.
    auto *descr = this->parent_->get_config_descriptor(this->char_handle_status_);
    if (descr == nullptr) {
      ESP_LOGW(TAG, "No config descriptor for status handle 0x%x. Will not be able to receive status notifications",
               this->char_handle_status_);
      result = false;
    } else if (descr->uuid.get_uuid().len != ESP_UUID_LEN_16 ||
               descr->uuid.get_uuid().uuid.uuid16 != ESP_GATT_UUID_CHAR_CLIENT_CONFIG) {
      ESP_LOGW(TAG, "Config descriptor 0x%x (uuid %s) is not a client config char uuid", this->char_handle_status_,
               descr->uuid.to_string().c_str());
      result = false;
    } else {
      this->config_descr_status_ = descr->handle;
    }
  }

  if (!this->char_handle_name_) {
    chr = this->parent_->get_characteristic(BEDJET_SERVICE_UUID, BEDJET_NAME_UUID);
    if (chr == nullptr) {
      ESP_LOGW(TAG, "[%s] No name service found at device, not a BedJet..?", this->get_name().c_str());
      result = false;
    } else {
      this->char_handle_name_ = chr->handle;
      auto status = esp_ble_gattc_read_char(this->parent_->get_gattc_if(), this->parent_->get_conn_id(),
                                            this->char_handle_name_, ESP_GATT_AUTH_REQ_NONE);
      if (status) {
        ESP_LOGI(TAG, "[%s] Unable to read name characteristic: %d", this->get_name().c_str(), status);
      }
    }
  }

  ESP_LOGI(TAG, "[%s] Discovered service characteristics: ", this->get_name().c_str());
  ESP_LOGI(TAG, "     - Command char: 0x%x", this->char_handle_cmd_);
  ESP_LOGI(TAG, "     - Status char: 0x%x", this->char_handle_status_);
  ESP_LOGI(TAG, "       - config descriptor: 0x%x", this->config_descr_status_);
  ESP_LOGI(TAG, "     - Name char: 0x%x", this->char_handle_name_);

  return result;
}

void BedJetHub::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                    esp_ble_gattc_cb_param_t *param) {
  switch (event) {
    case ESP_GATTC_DISCONNECT_EVT: {
      ESP_LOGV(TAG, "Disconnected: reason=%d", param->disconnect.reason);
      this->status_set_warning();
      this->dispatch_state_(false);
      break;
    }
    case ESP_GATTC_SEARCH_CMPL_EVT: {
      auto result = this->discover_characteristics_();

      if (result) {
        ESP_LOGD(TAG, "[%s] Services complete: obtained char handles.", this->get_name().c_str());
        this->node_state = espbt::ClientState::ESTABLISHED;
        this->set_notify_(true);

#ifdef USE_TIME
        if (this->time_id_.has_value()) {
          this->send_local_time();
        }
#endif

        this->dispatch_state_(true);
      } else {
        ESP_LOGW(TAG, "[%s] Failed discovering service characteristics.", this->get_name().c_str());
        this->parent()->set_enabled(false);
        this->status_set_warning();
        this->dispatch_state_(false);
      }
      break;
    }
    case ESP_GATTC_WRITE_DESCR_EVT: {
      if (param->write.status != ESP_GATT_OK) {
        if (param->write.status == ESP_GATT_INVALID_ATTR_LEN) {
          // This probably means that our hack for notify_en (8 bit vs 16 bit) didn't work right.
          // Should we try to fall back to BLEClient's way?
          ESP_LOGW(TAG, "[%s] Invalid attr length writing descr at handle 0x%04d, status=%d", this->get_name().c_str(),
                   param->write.handle, param->write.status);
        } else {
          ESP_LOGW(TAG, "[%s] Error writing descr at handle 0x%04d, status=%d", this->get_name().c_str(),
                   param->write.handle, param->write.status);
        }
        break;
      }
      ESP_LOGD(TAG, "[%s] Write to handle 0x%04x status=%d", this->get_name().c_str(), param->write.handle,
               param->write.status);
      break;
    }
    case ESP_GATTC_WRITE_CHAR_EVT: {
      if (param->write.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "Error writing char at handle 0x%04d, status=%d", param->write.handle, param->write.status);
        break;
      }
      if (param->write.handle == this->char_handle_cmd_) {
        if (this->force_refresh_) {
          // Command write was successful. Publish the pending state, hoping that notify will kick in.
          // FIXME: better to wait until we know the status has changed
          this->dispatch_status_();
        }
      }
      break;
    }
    case ESP_GATTC_READ_CHAR_EVT: {
      if (param->read.conn_id != this->parent_->get_conn_id())
        break;
      if (param->read.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "Error reading char at handle %d, status=%d", param->read.handle, param->read.status);
        break;
      }

      if (param->read.handle == this->char_handle_status_) {
        // This is the additional packet that doesn't fit in the notify packet.
        this->codec_->decode_extra(param->read.value, param->read.value_len);
        this->status_packet_ready_();
      } else if (param->read.handle == this->char_handle_name_) {
        // The data should represent the name.
        if (param->read.status == ESP_GATT_OK && param->read.value_len > 0) {
          std::string bedjet_name(reinterpret_cast<char const *>(param->read.value), param->read.value_len);
          ESP_LOGV(TAG, "[%s] Got BedJet name: '%s'", this->get_name().c_str(), bedjet_name.c_str());
          this->set_name_(bedjet_name);
        }
      }
      break;
    }
    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
      // This event means that ESP received the request to enable notifications on the client side. But we also have to
      // tell the server that we want it to send notifications. Normally BLEClient parent would handle this
      // automatically, but as soon as we set our status to Established, the parent is going to purge all the
      // service/char/descriptor handles, and then get_config_descriptor() won't work anymore. There's no way to disable
      // the BLEClient parent behavior, so our only option is to write the handle anyway, and hope a double-write
      // doesn't break anything.

      if (param->reg_for_notify.handle != this->char_handle_status_) {
        ESP_LOGW(TAG, "[%s] Register for notify on unexpected handle 0x%04x, expecting 0x%04x",
                 this->get_name().c_str(), param->reg_for_notify.handle, this->char_handle_status_);
        break;
      }

      this->write_notify_config_descriptor_(true);
      this->last_notify_ = 0;
      this->force_refresh_ = true;
      break;
    }
    case ESP_GATTC_UNREG_FOR_NOTIFY_EVT: {
      // This event is not handled by the parent BLEClient, so we need to do this either way.
      if (param->unreg_for_notify.handle != this->char_handle_status_) {
        ESP_LOGW(TAG, "[%s] Unregister for notify on unexpected handle 0x%04x, expecting 0x%04x",
                 this->get_name().c_str(), param->unreg_for_notify.handle, this->char_handle_status_);
        break;
      }

      this->write_notify_config_descriptor_(false);
      this->last_notify_ = 0;
      // Now we wait until the next update() poll to re-register notify...
      break;
    }
    case ESP_GATTC_NOTIFY_EVT: {
      if (this->processing_)
        break;

      if (param->notify.conn_id != this->parent_->get_conn_id()) {
        ESP_LOGW(TAG, "[%s] Received notify event for unexpected parent conn: expect %x, got %x",
                 this->get_name().c_str(), this->parent_->get_conn_id(), param->notify.conn_id);
        // FIXME: bug in BLEClient holding wrong conn_id.
      }

      if (param->notify.handle != this->char_handle_status_) {
        ESP_LOGW(TAG, "[%s] Unexpected notify handle, wanted %04X, got %04X", this->get_name().c_str(),
                 this->char_handle_status_, param->notify.handle);
        break;
      }

      // FIXME: notify events come in every ~200-300 ms, which is too fast to be helpful. So we
      //  throttle the updates to once every MIN_NOTIFY_THROTTLE (5 seconds).
      //  Another idea would be to keep notify off by default, and use update() as an opportunity to turn on
      //  notify to get enough data to update status, then turn off notify again.

      uint32_t now = millis();
      auto delta = now - this->last_notify_;

      if (!this->force_refresh_ && this->codec_->compare(param->notify.value, param->notify.value_len)) {
        // If the packet is meaningfully different, trigger children as well
        this->force_refresh_ = true;
        ESP_LOGV(TAG, "[%s] Incoming packet indicates a significant change.", this->get_name().c_str());
      }

      if (this->last_notify_ == 0 || delta > MIN_NOTIFY_THROTTLE || this->force_refresh_) {
        // Set reentrant flag to prevent processing multiple packets.
        this->processing_ = true;
        ESP_LOGVV(TAG, "[%s] Decoding packet: last=%d, delta=%d, force=%s", this->get_name().c_str(),
                  this->last_notify_, delta, this->force_refresh_ ? "y" : "n");
        bool needs_extra = this->codec_->decode_notify(param->notify.value, param->notify.value_len);

        if (needs_extra) {
          // This means the packet was partial, so read the status characteristic to get the second part.
          // Ideally this will complete quickly. We won't process additional notification events until it does.
          auto status = esp_ble_gattc_read_char(this->parent_->get_gattc_if(), this->parent_->get_conn_id(),
                                                this->char_handle_status_, ESP_GATT_AUTH_REQ_NONE);
          if (status) {
            ESP_LOGI(TAG, "[%s] Unable to read extended status packet", this->get_name().c_str());
          }
        } else {
          this->status_packet_ready_();
        }
      }
      break;
    }
    default:
      ESP_LOGVV(TAG, "[%s] gattc unhandled event: enum=%d", this->get_name().c_str(), event);
      break;
  }
}

inline void BedJetHub::status_packet_ready_() {
  this->last_notify_ = millis();
  this->processing_ = false;

  if (this->force_refresh_) {
    // If we requested an immediate update, do that now.
    this->update();
    this->force_refresh_ = false;
  }
}

/** Reimplementation of BLEClient.gattc_event_handler() for ESP_GATTC_REG_FOR_NOTIFY_EVT.
 *
 * This is a copy of ble_client's automatic handling of `ESP_GATTC_REG_FOR_NOTIFY_EVT`, in order
 * to undo the same on unregister. It also allows us to maintain the config descriptor separately,
 * since the parent BLEClient is going to purge all descriptors once we set our connection status
 * to `Established`.
 */
uint8_t BedJetHub::write_notify_config_descriptor_(bool enable) {
  auto handle = this->config_descr_status_;
  if (handle == 0) {
    ESP_LOGW(TAG, "No descriptor found for notify of handle 0x%x", this->char_handle_status_);
    return -1;
  }

  // NOTE: BLEClient uses `uint8_t*` of length 1, but BLE spec requires 16 bits.
  uint16_t notify_en = enable ? 1 : 0;
  auto status = esp_ble_gattc_write_char_descr(this->parent_->get_gattc_if(), this->parent_->get_conn_id(), handle,
                                               sizeof(notify_en), (uint8_t *) &notify_en, ESP_GATT_WRITE_TYPE_RSP,
                                               ESP_GATT_AUTH_REQ_NONE);
  if (status) {
    ESP_LOGW(TAG, "esp_ble_gattc_write_char_descr error, status=%d", status);
    return status;
  }
  ESP_LOGD(TAG, "[%s] wrote notify=%s to status config 0x%04x, for conn %d", this->get_name().c_str(),
           enable ? "true" : "false", handle, this->parent_->get_conn_id());
  return ESP_GATT_OK;
}

/* Time Component */

#ifdef USE_TIME
void BedJetHub::send_local_time() {
  if (this->time_id_.has_value()) {
    auto *time_id = *this->time_id_;
    time::ESPTime now = time_id->now();
    if (now.is_valid()) {
      this->set_clock(now.hour, now.minute);
      ESP_LOGD(TAG, "Using time component to set BedJet clock: %d:%02d", now.hour, now.minute);
    }
  } else {
    ESP_LOGI(TAG, "`time_id` is not configured: will not sync BedJet clock.");
  }
}

void BedJetHub::setup_time_() {
  if (this->time_id_.has_value()) {
    this->send_local_time();
    auto *time_id = *this->time_id_;
    time_id->add_on_time_sync_callback([this] { this->send_local_time(); });
  } else {
    ESP_LOGI(TAG, "`time_id` is not configured: will not sync BedJet clock.");
  }
}
#endif

void BedJetHub::set_clock(uint8_t hour, uint8_t minute) {
  if (!this->is_connected()) {
    ESP_LOGV(TAG, "[%s] Not connected, cannot send time.", this->get_name().c_str());
    return;
  }

  BedjetPacket *pkt = this->codec_->get_set_time_request(hour, minute);
  auto status = this->write_bedjet_packet_(pkt);
  if (status) {
    ESP_LOGW(TAG, "Failed setting BedJet clock: %d", status);
  } else {
    ESP_LOGD(TAG, "[%s] BedJet clock set to: %d:%02d", this->get_name().c_str(), hour, minute);
  }
}

/* Internal */

void BedJetHub::loop() {}
void BedJetHub::update() { this->dispatch_status_(); }

void BedJetHub::dump_config() {
  ESP_LOGCONFIG(TAG, "BedJet Hub '%s'", this->get_name().c_str());
  ESP_LOGCONFIG(TAG, "  ble_client.app_id: %d", this->parent()->app_id);
  ESP_LOGCONFIG(TAG, "  ble_client.conn_id: %d", this->parent()->get_conn_id());
  LOG_UPDATE_INTERVAL(this)
  ESP_LOGCONFIG(TAG, "  Child components (%d):", this->children_.size());
  for (auto *child : this->children_) {
    ESP_LOGCONFIG(TAG, "    - %s", child->describe().c_str());
  }
}

void BedJetHub::dispatch_state_(bool is_ready) {
  for (auto *child : this->children_) {
    child->on_bedjet_state(is_ready);
  }
}

void BedJetHub::dispatch_status_() {
  auto *status = this->codec_->get_status_packet();

  if (!this->is_connected()) {
    ESP_LOGD(TAG, "[%s] Not connected, will not send status.", this->get_name().c_str());
  } else if (status != nullptr) {
    ESP_LOGD(TAG, "[%s] Notifying %d children of latest status @%p.", this->get_name().c_str(), this->children_.size(),
             status);
    for (auto *child : this->children_) {
      child->on_status(status);
    }
  } else {
    uint32_t now = millis();
    uint32_t diff = now - this->last_notify_;

    if (this->last_notify_ == 0) {
      // This means we're connected and haven't received a notification, so it likely means that the BedJet is off.
      // However, it could also mean that it's running, but failing to send notifications.
      // We can try to unregister for notifications now, and then re-register, hoping to clear it up...
      // But how do we know for sure which state we're in, and how do we actually clear out the buggy state?

      ESP_LOGI(TAG, "[%s] Still waiting for first GATT notify event.", this->get_name().c_str());
    } else if (diff > NOTIFY_WARN_THRESHOLD) {
      ESP_LOGW(TAG, "[%s] Last GATT notify was %d seconds ago.", this->get_name().c_str(), diff / 1000);
    }

    if (this->timeout_ > 0 && diff > this->timeout_ && this->parent()->enabled) {
      ESP_LOGW(TAG, "[%s] Timed out after %d sec. Retrying...", this->get_name().c_str(), this->timeout_);
      // set_enabled(false) will only close the connection if state != IDLE.
      this->parent()->set_state(espbt::ClientState::CONNECTING);
      this->parent()->set_enabled(false);
      this->parent()->set_enabled(true);
    }
  }
}

void BedJetHub::register_child(BedJetClient *obj) {
  this->children_.push_back(obj);
  obj->set_parent(this);
}

}  // namespace bedjet
}  // namespace esphome

#endif
