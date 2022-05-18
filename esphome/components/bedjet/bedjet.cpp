#include "bedjet.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome {
namespace bedjet {

using namespace esphome::climate;

/// Converts a BedJet temp step into degrees Celsius.
float bedjet_temp_to_c(const uint8_t temp) {
  // BedJet temp is "C*2"; to get C, divide by 2.
  return temp / 2.0f;
}

/// Converts a BedJet fan step to a speed percentage, in the range of 5% to 100%.
uint8_t bedjet_fan_step_to_speed(const uint8_t fan) {
  //  0 =  5%
  // 19 = 100%
  return 5 * fan + 5;
}

static const std::string *bedjet_fan_step_to_fan_mode(const uint8_t fan_step) {
  if (fan_step >= 0 && fan_step <= 19)
    return &BEDJET_FAN_STEP_NAME_STRINGS[fan_step];
  return nullptr;
}

static uint8_t bedjet_fan_speed_to_step(const std::string &fan_step_percent) {
  for (int i = 0; i < sizeof(BEDJET_FAN_STEP_NAME_STRINGS); i++) {
    if (fan_step_percent == BEDJET_FAN_STEP_NAME_STRINGS[i]) {
      return i;
    }
  }
  return -1;
}

void Bedjet::upgrade_firmware() {
  auto *pkt = this->codec_->get_button_request(MAGIC_UPDATE);
  auto status = this->write_bedjet_packet_(pkt);

  if (status) {
    ESP_LOGW(TAG, "[%s] esp_ble_gattc_write_char failed, status=%d", this->parent_->address_str().c_str(), status);
  }
}

void Bedjet::dump_config() {
  LOG_CLIMATE("", "BedJet Climate", this);
  auto traits = this->get_traits();

  ESP_LOGCONFIG(TAG, "  Supported modes:");
  for (auto mode : traits.get_supported_modes()) {
    ESP_LOGCONFIG(TAG, "   - %s", LOG_STR_ARG(climate_mode_to_string(mode)));
  }

  ESP_LOGCONFIG(TAG, "  Supported fan modes:");
  for (const auto &mode : traits.get_supported_fan_modes()) {
    ESP_LOGCONFIG(TAG, "   - %s", LOG_STR_ARG(climate_fan_mode_to_string(mode)));
  }
  for (const auto &mode : traits.get_supported_custom_fan_modes()) {
    ESP_LOGCONFIG(TAG, "   - %s (c)", mode.c_str());
  }

  ESP_LOGCONFIG(TAG, "  Supported presets:");
  for (auto preset : traits.get_supported_presets()) {
    ESP_LOGCONFIG(TAG, "   - %s", LOG_STR_ARG(climate_preset_to_string(preset)));
  }
  for (const auto &preset : traits.get_supported_custom_presets()) {
    ESP_LOGCONFIG(TAG, "   - %s (c)", preset.c_str());
  }
}

void Bedjet::setup() {
  this->codec_ = make_unique<BedjetCodec>();

  // restore set points
  auto restore = this->restore_state_();
  if (restore.has_value()) {
    ESP_LOGI(TAG, "Restored previous saved state.");
    restore->apply(this);
  } else {
    // Initial status is unknown until we connect
    this->reset_state_();
  }

#ifdef USE_TIME
  this->setup_time_();
#endif
}

/** Resets states to defaults. */
void Bedjet::reset_state_() {
  this->mode = climate::CLIMATE_MODE_OFF;
  this->action = climate::CLIMATE_ACTION_IDLE;
  this->target_temperature = NAN;
  this->current_temperature = NAN;
  this->preset.reset();
  this->custom_preset.reset();
  this->publish_state();
}

void Bedjet::loop() {}

void Bedjet::control(const ClimateCall &call) {
  ESP_LOGD(TAG, "Received Bedjet::control");
  if (this->node_state != espbt::ClientState::ESTABLISHED) {
    ESP_LOGW(TAG, "Not connected, cannot handle control call yet.");
    return;
  }

  if (call.get_mode().has_value()) {
    ClimateMode mode = *call.get_mode();
    BedjetPacket *pkt;
    switch (mode) {
      case climate::CLIMATE_MODE_OFF:
        pkt = this->codec_->get_button_request(BTN_OFF);
        break;
      case climate::CLIMATE_MODE_HEAT:
        pkt = this->codec_->get_button_request(BTN_HEAT);
        break;
      case climate::CLIMATE_MODE_FAN_ONLY:
        pkt = this->codec_->get_button_request(BTN_COOL);
        break;
      case climate::CLIMATE_MODE_DRY:
        pkt = this->codec_->get_button_request(BTN_DRY);
        break;
      default:
        ESP_LOGW(TAG, "Unsupported mode: %d", mode);
        return;
    }

    auto status = this->write_bedjet_packet_(pkt);

    if (status) {
      ESP_LOGW(TAG, "[%s] esp_ble_gattc_write_char failed, status=%d", this->parent_->address_str().c_str(), status);
    } else {
      this->force_refresh_ = true;
      this->mode = mode;
      // We're using (custom) preset for Turbo, EXT HT, & M1-3 presets, so changing climate mode will clear those
      this->custom_preset.reset();
      this->preset.reset();
    }
  }

  if (call.get_target_temperature().has_value()) {
    auto target_temp = *call.get_target_temperature();
    auto *pkt = this->codec_->get_set_target_temp_request(target_temp);
    auto status = this->write_bedjet_packet_(pkt);

    if (status) {
      ESP_LOGW(TAG, "[%s] esp_ble_gattc_write_char failed, status=%d", this->parent_->address_str().c_str(), status);
    } else {
      this->target_temperature = target_temp;
    }
  }

  if (call.get_preset().has_value()) {
    ClimatePreset preset = *call.get_preset();
    BedjetPacket *pkt;

    if (preset == climate::CLIMATE_PRESET_BOOST) {
      pkt = this->codec_->get_button_request(BTN_TURBO);
    } else {
      ESP_LOGW(TAG, "Unsupported preset: %d", preset);
      return;
    }

    auto status = this->write_bedjet_packet_(pkt);
    if (status) {
      ESP_LOGW(TAG, "[%s] esp_ble_gattc_write_char failed, status=%d", this->parent_->address_str().c_str(), status);
    } else {
      // We use BOOST preset for TURBO mode, which is a short-lived/high-heat mode.
      this->mode = climate::CLIMATE_MODE_HEAT;
      this->preset = preset;
      this->custom_preset.reset();
      this->force_refresh_ = true;
    }
  } else if (call.get_custom_preset().has_value()) {
    std::string preset = *call.get_custom_preset();
    BedjetPacket *pkt;

    if (preset == "M1") {
      pkt = this->codec_->get_button_request(BTN_M1);
    } else if (preset == "M2") {
      pkt = this->codec_->get_button_request(BTN_M2);
    } else if (preset == "M3") {
      pkt = this->codec_->get_button_request(BTN_M3);
    } else if (preset == "EXT HT") {
      pkt = this->codec_->get_button_request(BTN_EXTHT);
    } else {
      ESP_LOGW(TAG, "Unsupported preset: %s", preset.c_str());
      return;
    }

    auto status = this->write_bedjet_packet_(pkt);
    if (status) {
      ESP_LOGW(TAG, "[%s] esp_ble_gattc_write_char failed, status=%d", this->parent_->address_str().c_str(), status);
    } else {
      this->force_refresh_ = true;
      this->custom_preset = preset;
      this->preset.reset();
    }
  }

  if (call.get_fan_mode().has_value()) {
    // Climate fan mode only supports low/med/high, but the BedJet supports 5-100% increments.
    // We can still support a ClimateCall that requests low/med/high, and just translate it to a step increment here.
    auto fan_mode = *call.get_fan_mode();
    BedjetPacket *pkt;
    if (fan_mode == climate::CLIMATE_FAN_LOW) {
      pkt = this->codec_->get_set_fan_speed_request(3 /* = 20% */);
    } else if (fan_mode == climate::CLIMATE_FAN_MEDIUM) {
      pkt = this->codec_->get_set_fan_speed_request(9 /* = 50% */);
    } else if (fan_mode == climate::CLIMATE_FAN_HIGH) {
      pkt = this->codec_->get_set_fan_speed_request(14 /* = 75% */);
    } else {
      ESP_LOGW(TAG, "[%s] Unsupported fan mode: %s", this->get_name().c_str(),
               LOG_STR_ARG(climate_fan_mode_to_string(fan_mode)));
      return;
    }

    auto status = this->write_bedjet_packet_(pkt);
    if (status) {
      ESP_LOGW(TAG, "[%s] esp_ble_gattc_write_char failed, status=%d", this->parent_->address_str().c_str(), status);
    } else {
      this->force_refresh_ = true;
    }
  } else if (call.get_custom_fan_mode().has_value()) {
    auto fan_mode = *call.get_custom_fan_mode();
    auto fan_step = bedjet_fan_speed_to_step(fan_mode);
    if (fan_step >= 0 && fan_step <= 19) {
      ESP_LOGV(TAG, "[%s] Converted fan mode %s to bedjet fan step %d", this->get_name().c_str(), fan_mode.c_str(),
               fan_step);
      // The index should represent the fan_step index.
      BedjetPacket *pkt = this->codec_->get_set_fan_speed_request(fan_step);
      auto status = this->write_bedjet_packet_(pkt);
      if (status) {
        ESP_LOGW(TAG, "[%s] esp_ble_gattc_write_char failed, status=%d", this->parent_->address_str().c_str(), status);
      } else {
        this->force_refresh_ = true;
      }
    }
  }
}

void Bedjet::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param) {
  switch (event) {
    case ESP_GATTC_DISCONNECT_EVT: {
      ESP_LOGV(TAG, "Disconnected: reason=%d", param->disconnect.reason);
      this->status_set_warning();
      break;
    }
    case ESP_GATTC_SEARCH_CMPL_EVT: {
      auto *chr = this->parent_->get_characteristic(BEDJET_SERVICE_UUID, BEDJET_COMMAND_UUID);
      if (chr == nullptr) {
        ESP_LOGW(TAG, "[%s] No control service found at device, not a BedJet..?", this->get_name().c_str());
        break;
      }
      this->char_handle_cmd_ = chr->handle;

      chr = this->parent_->get_characteristic(BEDJET_SERVICE_UUID, BEDJET_STATUS_UUID);
      if (chr == nullptr) {
        ESP_LOGW(TAG, "[%s] No status service found at device, not a BedJet..?", this->get_name().c_str());
        break;
      }

      this->char_handle_status_ = chr->handle;
      // We also need to obtain the config descriptor for this handle.
      // Otherwise once we set node_state=Established, the parent will flush all handles/descriptors, and we won't be
      // able to look it up.
      auto *descr = this->parent_->get_config_descriptor(this->char_handle_status_);
      if (descr == nullptr) {
        ESP_LOGW(TAG, "No config descriptor for status handle 0x%x. Will not be able to receive status notifications",
                 this->char_handle_status_);
      } else if (descr->uuid.get_uuid().len != ESP_UUID_LEN_16 ||
                 descr->uuid.get_uuid().uuid.uuid16 != ESP_GATT_UUID_CHAR_CLIENT_CONFIG) {
        ESP_LOGW(TAG, "Config descriptor 0x%x (uuid %s) is not a client config char uuid", this->char_handle_status_,
                 descr->uuid.to_string().c_str());
      } else {
        this->config_descr_status_ = descr->handle;
      }

      chr = this->parent_->get_characteristic(BEDJET_SERVICE_UUID, BEDJET_NAME_UUID);
      if (chr != nullptr) {
        this->char_handle_name_ = chr->handle;
        auto status = esp_ble_gattc_read_char(this->parent_->gattc_if, this->parent_->conn_id, this->char_handle_name_,
                                              ESP_GATT_AUTH_REQ_NONE);
        if (status) {
          ESP_LOGI(TAG, "[%s] Unable to read name characteristic: %d", this->get_name().c_str(), status);
        }
      }

      ESP_LOGD(TAG, "Services complete: obtained char handles.");
      this->node_state = espbt::ClientState::ESTABLISHED;

      this->set_notify_(true);

#ifdef USE_TIME
      if (this->time_id_.has_value()) {
        this->send_local_time_();
      }
#endif
      break;
    }
    case ESP_GATTC_WRITE_DESCR_EVT: {
      if (param->write.status != ESP_GATT_OK) {
        // ESP_GATT_INVALID_ATTR_LEN
        ESP_LOGW(TAG, "Error writing descr at handle 0x%04d, status=%d", param->write.handle, param->write.status);
        break;
      }
      // [16:44:44][V][bedjet:279]: [JOENJET] Register for notify event success: h=0x002a s=0
      // This might be the enable-notify descriptor? (or disable-notify)
      ESP_LOGV(TAG, "[%s] Write to handle 0x%04x status=%d", this->get_name().c_str(), param->write.handle,
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
          this->publish_state();
        }
      }
      break;
    }
    case ESP_GATTC_READ_CHAR_EVT: {
      if (param->read.conn_id != this->parent_->conn_id)
        break;
      if (param->read.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "Error reading char at handle %d, status=%d", param->read.handle, param->read.status);
        break;
      }
      if (param->read.handle == this->char_handle_status_) {
        // This is the additional packet that doesn't fit in the notify packet.
        this->codec_->decode_extra(param->read.value, param->read.value_len);
      } else if (param->read.handle == this->char_handle_name_) {
        // The data should represent the name.
        if (param->read.status == ESP_GATT_OK && param->read.value_len > 0) {
          std::string bedjet_name(reinterpret_cast<char const *>(param->read.value), param->read.value_len);
          // this->set_name(bedjet_name);
          ESP_LOGV(TAG, "[%s] Got BedJet name: '%s'", this->get_name().c_str(), bedjet_name.c_str());
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

      if (this->last_notify_ == 0 || delta > MIN_NOTIFY_THROTTLE || this->force_refresh_) {
        bool needs_extra = this->codec_->decode_notify(param->notify.value, param->notify.value_len);
        this->last_notify_ = now;

        if (needs_extra) {
          // this means the packet was partial, so read the status characteristic to get the second part.
          auto status = esp_ble_gattc_read_char(this->parent_->gattc_if, this->parent_->conn_id,
                                                this->char_handle_status_, ESP_GATT_AUTH_REQ_NONE);
          if (status) {
            ESP_LOGI(TAG, "[%s] Unable to read extended status packet", this->get_name().c_str());
          }
        }

        if (this->force_refresh_) {
          // If we requested an immediate update, do that now.
          this->update();
          this->force_refresh_ = false;
        }
      }
      break;
    }
    default:
      ESP_LOGVV(TAG, "[%s] gattc unhandled event: enum=%d", this->get_name().c_str(), event);
      break;
  }
}

/** Reimplementation of BLEClient.gattc_event_handler() for ESP_GATTC_REG_FOR_NOTIFY_EVT.
 *
 * This is a copy of ble_client's automatic handling of `ESP_GATTC_REG_FOR_NOTIFY_EVT`, in order
 * to undo the same on unregister. It also allows us to maintain the config descriptor separately,
 * since the parent BLEClient is going to purge all descriptors once we set our connection status
 * to `Established`.
 */
uint8_t Bedjet::write_notify_config_descriptor_(bool enable) {
  auto handle = this->config_descr_status_;
  if (handle == 0) {
    ESP_LOGW(TAG, "No descriptor found for notify of handle 0x%x", this->char_handle_status_);
    return -1;
  }

  // NOTE: BLEClient uses `uint8_t*` of length 1, but BLE spec requires 16 bits.
  uint8_t notify_en[] = {0, 0};
  notify_en[0] = enable;
  auto status =
      esp_ble_gattc_write_char_descr(this->parent_->gattc_if, this->parent_->conn_id, handle, sizeof(notify_en),
                                     &notify_en[0], ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
  if (status) {
    ESP_LOGW(TAG, "esp_ble_gattc_write_char_descr error, status=%d", status);
    return status;
  }
  ESP_LOGD(TAG, "[%s] wrote notify=%s to status config 0x%04x", this->get_name().c_str(), enable ? "true" : "false",
           handle);
  return ESP_GATT_OK;
}

#ifdef USE_TIME
/** Attempts to sync the local time (via `time_id`) to the BedJet device. */
void Bedjet::send_local_time_() {
  if (this->node_state != espbt::ClientState::ESTABLISHED) {
    ESP_LOGV(TAG, "[%s] Not connected, cannot send time.", this->get_name().c_str());
    return;
  }
  auto *time_id = *this->time_id_;
  time::ESPTime now = time_id->now();
  if (now.is_valid()) {
    uint8_t hour = now.hour;
    uint8_t minute = now.minute;
    BedjetPacket *pkt = this->codec_->get_set_time_request(hour, minute);
    auto status = this->write_bedjet_packet_(pkt);
    if (status) {
      ESP_LOGW(TAG, "Failed setting BedJet clock: %d", status);
    } else {
      ESP_LOGD(TAG, "[%s] BedJet clock set to: %d:%02d", this->get_name().c_str(), hour, minute);
    }
  }
}

/** Initializes time sync callbacks to support syncing current time to the BedJet. */
void Bedjet::setup_time_() {
  if (this->time_id_.has_value()) {
    this->send_local_time_();
    auto *time_id = *this->time_id_;
    time_id->add_on_time_sync_callback([this] { this->send_local_time_(); });
    time::ESPTime now = time_id->now();
    ESP_LOGD(TAG, "Using time component to set BedJet clock: %d:%02d", now.hour, now.minute);
  } else {
    ESP_LOGI(TAG, "`time_id` is not configured: will not sync BedJet clock.");
  }
}
#endif

/** Writes one BedjetPacket to the BLE client on the BEDJET_COMMAND_UUID. */
uint8_t Bedjet::write_bedjet_packet_(BedjetPacket *pkt) {
  if (this->node_state != espbt::ClientState::ESTABLISHED) {
    if (!this->parent_->enabled) {
      ESP_LOGI(TAG, "[%s] Cannot write packet: Not connected, enabled=false", this->get_name().c_str());
    } else {
      ESP_LOGW(TAG, "[%s] Cannot write packet: Not connected", this->get_name().c_str());
    }
    return -1;
  }
  auto status = esp_ble_gattc_write_char(this->parent_->gattc_if, this->parent_->conn_id, this->char_handle_cmd_,
                                         pkt->data_length + 1, (uint8_t *) &pkt->command, ESP_GATT_WRITE_TYPE_NO_RSP,
                                         ESP_GATT_AUTH_REQ_NONE);
  return status;
}

/** Configures the local ESP BLE client to register (`true`) or unregister (`false`) for status notifications. */
uint8_t Bedjet::set_notify_(const bool enable) {
  uint8_t status;
  if (enable) {
    status = esp_ble_gattc_register_for_notify(this->parent_->gattc_if, this->parent_->remote_bda,
                                               this->char_handle_status_);
    if (status) {
      ESP_LOGW(TAG, "[%s] esp_ble_gattc_register_for_notify failed, status=%d", this->get_name().c_str(), status);
    }
  } else {
    status = esp_ble_gattc_unregister_for_notify(this->parent_->gattc_if, this->parent_->remote_bda,
                                                 this->char_handle_status_);
    if (status) {
      ESP_LOGW(TAG, "[%s] esp_ble_gattc_unregister_for_notify failed, status=%d", this->get_name().c_str(), status);
    }
  }
  ESP_LOGV(TAG, "[%s] set_notify: enable=%d; result=%d", this->get_name().c_str(), enable, status);
  return status;
}

/** Attempts to update the climate device from the last received BedjetStatusPacket.
 *
 * @return `true` if the status has been applied; `false` if there is nothing to apply.
 */
bool Bedjet::update_status_() {
  if (!this->codec_->has_status())
    return false;

  BedjetStatusPacket status = *this->codec_->get_status_packet();

  auto converted_temp = bedjet_temp_to_c(status.target_temp_step);
  if (converted_temp > 0)
    this->target_temperature = converted_temp;
  converted_temp = bedjet_temp_to_c(status.ambient_temp_step);
  if (converted_temp > 0)
    this->current_temperature = converted_temp;

  const auto *fan_mode_name = bedjet_fan_step_to_fan_mode(status.fan_step);
  if (fan_mode_name != nullptr) {
    this->custom_fan_mode = *fan_mode_name;
  }

  // TODO: Get biorhythm data to determine which preset (M1-3) is running, if any.
  switch (status.mode) {
    case MODE_WAIT:  // Biorhythm "wait" step: device is idle
    case MODE_STANDBY:
      this->mode = climate::CLIMATE_MODE_OFF;
      this->action = climate::CLIMATE_ACTION_IDLE;
      this->fan_mode = climate::CLIMATE_FAN_OFF;
      this->custom_preset.reset();
      this->preset.reset();
      break;

    case MODE_HEAT:
    case MODE_EXTHT:
      this->mode = climate::CLIMATE_MODE_HEAT;
      this->action = climate::CLIMATE_ACTION_HEATING;
      this->custom_preset.reset();
      this->preset.reset();
      break;

    case MODE_COOL:
      this->mode = climate::CLIMATE_MODE_FAN_ONLY;
      this->action = climate::CLIMATE_ACTION_COOLING;
      this->custom_preset.reset();
      this->preset.reset();
      break;

    case MODE_DRY:
      this->mode = climate::CLIMATE_MODE_DRY;
      this->action = climate::CLIMATE_ACTION_DRYING;
      this->custom_preset.reset();
      this->preset.reset();
      break;

    case MODE_TURBO:
      this->preset = climate::CLIMATE_PRESET_BOOST;
      this->custom_preset.reset();
      this->mode = climate::CLIMATE_MODE_HEAT;
      this->action = climate::CLIMATE_ACTION_HEATING;
      break;

    default:
      ESP_LOGW(TAG, "[%s] Unexpected mode: 0x%02X", this->get_name().c_str(), status.mode);
      break;
  }

  if (this->is_valid_()) {
    this->publish_state();
    this->codec_->clear_status();
    this->status_clear_warning();
  }

  return true;
}

void Bedjet::update() {
  ESP_LOGV(TAG, "[%s] update()", this->get_name().c_str());

  if (this->node_state != espbt::ClientState::ESTABLISHED) {
    if (!this->parent()->enabled) {
      ESP_LOGD(TAG, "[%s] Not connected, because enabled=false", this->get_name().c_str());
    } else {
      // Possibly still trying to connect.
      ESP_LOGD(TAG, "[%s] Not connected, enabled=true", this->get_name().c_str());
    }

    return;
  }

  auto result = this->update_status_();
  if (!result) {
    uint32_t now = millis();
    uint32_t diff = now - this->last_notify_;

    if (this->last_notify_ == 0) {
      // This means we're connected and haven't received a notification, so it likely means that the BedJet is off.
      // However, it could also mean that it's running, but failing to send notifications.
      // We can try to unregister for notifications now, and then re-register, hoping to clear it up...
      // But how do we know for sure which state we're in, and how do we actually clear out the buggy state?

      ESP_LOGI(TAG, "[%s] Still waiting for first GATT notify event.", this->get_name().c_str());
      this->set_notify_(false);
    } else if (diff > NOTIFY_WARN_THRESHOLD) {
      ESP_LOGW(TAG, "[%s] Last GATT notify was %d seconds ago.", this->get_name().c_str(), diff / 1000);
    }

    if (this->timeout_ > 0 && diff > this->timeout_ && this->parent()->enabled) {
      ESP_LOGW(TAG, "[%s] Timed out after %d sec. Retrying...", this->get_name().c_str(), this->timeout_);
      this->parent()->set_enabled(false);
      this->parent()->set_enabled(true);
    }
  }
}

}  // namespace bedjet
}  // namespace esphome

#endif
