#ifdef USE_ESP32

#include "ecocomfort2_hub.h"
#include "ecocomfort2_child.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <esp_gap_ble_api.h>

namespace esphome {
namespace ecocomfort2 {

static const char *const TAG = "ecocomfort2";

void Ecocomfort2Hub::setup() {
  char pref_key[40];
  if (this->parent_ != nullptr) {
    snprintf(pref_key, sizeof(pref_key), "ecocomfort2_%s", this->parent_->address_str());
  } else {
    snprintf(pref_key, sizeof(pref_key), "ecocomfort2");
  }

  this->pref_ = global_preferences->make_preference<Ecocomfort2PersistedState>(fnv1_hash(pref_key));
  if (this->pref_.load(&this->persisted_)) {
    ESP_LOGD(TAG, "Restored state: preset=%d, speed=%d, auto=%d", this->persisted_.preset, this->persisted_.speed,
             this->persisted_.auto_mode);
  } else {
    ESP_LOGD(TAG, "No saved state, using defaults: preset=%d, speed=%d, auto=%d", this->persisted_.preset,
             this->persisted_.speed, this->persisted_.auto_mode);
  }

  if (this->persisted_.speed == 0) {
    this->persisted_.speed = SPEED_VEL1;
  }

  esp_ble_auth_req_t auth_req = ESP_LE_AUTH_REQ_SC_BOND;
  esp_ble_io_cap_t io_cap = ESP_IO_CAP_NONE;
  uint8_t key_size = 16;
  uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
  uint8_t rsp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;

  auto set_security_param = [this](esp_ble_sm_param_t param, void *value, uint8_t len, const char *name) {
    auto err = esp_ble_gap_set_security_param(param, value, len);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "[%s] Failed to set BLE security param %s, err=%d", this->log_id_(), name, err);
    }
  };

  set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, static_cast<uint8_t>(sizeof(auth_req)), "auth_req");
  set_security_param(ESP_BLE_SM_IOCAP_MODE, &io_cap, static_cast<uint8_t>(sizeof(io_cap)), "io_cap");
  set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, static_cast<uint8_t>(sizeof(key_size)), "key_size");
  set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key, static_cast<uint8_t>(sizeof(init_key)), "init_key");
  set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key, static_cast<uint8_t>(sizeof(rsp_key)), "rsp_key");
}

void Ecocomfort2Hub::loop() {
  if (!this->is_ready()) {
    return;
  }

  uint32_t now = millis();
  uint32_t retry_interval = this->last_clock_sync_ == 0 ? CLOCK_SYNC_RETRY_INTERVAL : CLOCK_SYNC_INTERVAL;
  if (this->last_clock_attempt_ != 0 && (now - this->last_clock_attempt_) <= retry_interval) {
    return;
  }

  this->last_clock_attempt_ = now;
  if (this->send_clock()) {
    this->last_clock_sync_ = now;
  }
}

void Ecocomfort2Hub::update() {
  if (!this->is_ready()) {
    ESP_LOGD(TAG, "[%s] Not ready (connected=%d, ready=%d), skipping update", this->log_id_(), this->is_connected(),
             this->ready_);
    return;
  }

  auto read_char = [this](uint16_t handle, const char *name) {
    if (handle == 0) {
      return;
    }

    auto status = esp_ble_gattc_read_char(this->parent_->get_gattc_if(), this->parent_->get_conn_id(), handle,
                                          ESP_GATT_AUTH_REQ_NONE);
    if (status != ESP_GATT_OK) {
      ESP_LOGW(TAG, "[%s] Read request failed for %s handle 0x%x, status=%d", this->log_id_(), name, handle, status);
    }
  };

  read_char(this->char_handle_state_, "C_STATE");
  read_char(this->char_handle_oper_, "C_OPER");
  read_char(this->char_handle_config_, "C_CONFIG");
  read_char(this->char_handle_advanced_, "C_ADVANCED");
  if (!this->firmware_read_) {
    read_char(this->char_handle_info_, "C_INFO");
  }
}

void Ecocomfort2Hub::dump_config() {
  ESP_LOGCONFIG(TAG, "Ecocomfort2 Hub:");
  if (this->parent_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Address: %s", this->parent_->address_str());
  }
  LOG_UPDATE_INTERVAL(this);
  ESP_LOGCONFIG(TAG, "  Child components (%u):", static_cast<unsigned>(this->children_.size()));
  for (auto *child : this->children_) {
    ESP_LOGCONFIG(TAG, "    - %s", child->describe());
  }
}

bool Ecocomfort2Hub::discover_characteristics_() {
  bool result = true;
  esphome::ble_client::BLECharacteristic *chr;

  auto find_char = [&](const espbt::ESPBTUUID &uuid, uint16_t &handle, const char *name) {
    if (handle != 0) {
      return;
    }
    chr = this->parent_->get_characteristic(ECOCOMFORT2_SERVICE_UUID, uuid);
    if (chr == nullptr) {
      ESP_LOGW(TAG, "[%s] No %s characteristic found", this->log_id_(), name);
      result = false;
    } else {
      handle = chr->handle;
    }
  };

  find_char(ECOCOMFORT2_C_INFO_UUID, this->char_handle_info_, "C_INFO");
  find_char(ECOCOMFORT2_C_STATE_UUID, this->char_handle_state_, "C_STATE");
  find_char(ECOCOMFORT2_C_OPER_UUID, this->char_handle_oper_, "C_OPER");
  find_char(ECOCOMFORT2_C_CONFIG_UUID, this->char_handle_config_, "C_CONFIG");
  find_char(ECOCOMFORT2_C_CLOCK_UUID, this->char_handle_clock_, "C_CLOCK");
  find_char(ECOCOMFORT2_C_ADVANCED_UUID, this->char_handle_advanced_, "C_ADVANCED");

  ESP_LOGI(TAG, "[%s] Discovered characteristics: INFO=0x%x STATE=0x%x OPER=0x%x CONFIG=0x%x CLOCK=0x%x ADVANCED=0x%x",
           this->log_id_(), this->char_handle_info_, this->char_handle_state_, this->char_handle_oper_,
           this->char_handle_config_, this->char_handle_clock_, this->char_handle_advanced_);

  return result;
}

void Ecocomfort2Hub::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                         esp_ble_gattc_cb_param_t *param) {
  switch (event) {
    case ESP_GATTC_DISCONNECT_EVT: {
      ESP_LOGD(TAG, "[%s] Disconnected: reason=%d", this->log_id_(), param->disconnect.reason);
      this->cancel_timeout("send_cmd");
      this->cancel_timeout("connect_encrypt");
      this->cancel_timeout("connect_auth_timeout");
      this->cancel_timeout("post_auth_sync");
      this->status_set_warning();
      this->node_state = espbt::ClientState::IDLE;
      this->char_handle_info_ = 0;
      this->char_handle_state_ = 0;
      this->char_handle_oper_ = 0;
      this->char_handle_config_ = 0;
      this->char_handle_clock_ = 0;
      this->char_handle_advanced_ = 0;
      this->firmware_read_ = false;
      this->ready_ = false;
      this->state_valid_ = false;
      this->oper_valid_ = false;
      this->config_valid_ = false;
      this->advanced_valid_ = false;
      this->last_clock_sync_ = 0;
      this->last_clock_attempt_ = 0;
      this->encryption_retries_ = 0;
      this->dispatch_connect_(false);
      break;
    }
    case ESP_GATTC_SEARCH_CMPL_EVT: {
      if (!this->discover_characteristics_()) {
        ESP_LOGW(TAG, "[%s] Failed to discover characteristics — disconnecting to retry", this->log_id_());
        this->parent_->disconnect();
        break;
      }

      ESP_LOGD(TAG, "[%s] Service discovery complete", this->log_id_());
      this->node_state = espbt::ClientState::ESTABLISHED;
      this->ready_ = false;
      this->state_valid_ = false;
      this->oper_valid_ = false;
      this->config_valid_ = false;
      this->advanced_valid_ = false;
      this->status_set_warning();
      this->dispatch_connect_(false);

      this->set_timeout("connect_encrypt", 500, [this]() { this->request_encryption_("initial connect"); });
      break;
    }
    case ESP_GATTC_READ_CHAR_EVT: {
      if (param->read.conn_id != this->parent_->get_conn_id()) {
        break;
      }
      if (param->read.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "[%s] Error reading char at handle 0x%x, status=%d", this->log_id_(), param->read.handle,
                 param->read.status);
        if ((param->read.status == ESP_GATT_INSUF_ENCRYPTION ||
             param->read.status == ESP_GATT_INSUF_AUTHENTICATION) &&
            this->ready_) {
          this->ready_ = false;
          this->status_set_warning();
          this->dispatch_connect_(false);
          if (++this->encryption_retries_ >= MAX_ENCRYPTION_RETRIES) {
            ESP_LOGW(TAG, "[%s] Encryption lost, max retries reached — disconnecting", this->log_id_());
            this->parent_->disconnect();
          } else {
            ESP_LOGW(TAG, "[%s] Encryption lost, retry %d/%d", this->log_id_(), this->encryption_retries_,
                     MAX_ENCRYPTION_RETRIES);
            this->set_timeout("connect_encrypt", 500, [this]() { this->request_encryption_("encryption lost"); });
          }
        }
        break;
      }

      if (param->read.handle == this->char_handle_state_) {
        if (this->parse_state_(param->read.value, param->read.value_len)) {
          this->state_valid_ = true;
          this->dispatch_status_();
        }
      } else if (param->read.handle == this->char_handle_oper_) {
        if (this->parse_oper_(param->read.value, param->read.value_len)) {
          this->oper_valid_ = true;
          this->dispatch_status_();
        }
      } else if (param->read.handle == this->char_handle_config_) {
        if (this->parse_config_(param->read.value, param->read.value_len)) {
          this->config_valid_ = true;
          this->dispatch_config_();
        }
      } else if (param->read.handle == this->char_handle_advanced_) {
        if (this->parse_advanced_(param->read.value, param->read.value_len)) {
          this->advanced_valid_ = true;
          this->dispatch_config_();
        }
      } else if (param->read.handle == this->char_handle_info_) {
        if (this->parse_info_(param->read.value, param->read.value_len)) {
          this->firmware_read_ = true;
          this->dispatch_status_();
        }
      }
      break;
    }
    case ESP_GATTC_WRITE_CHAR_EVT: {
      if (param->write.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "[%s] Error writing char at handle 0x%x, status=%d", this->log_id_(), param->write.handle,
                 param->write.status);
        if ((param->write.status == ESP_GATT_INSUF_ENCRYPTION ||
             param->write.status == ESP_GATT_INSUF_AUTHENTICATION) &&
            this->ready_) {
          this->ready_ = false;
          this->status_set_warning();
          this->dispatch_connect_(false);
          if (++this->encryption_retries_ >= MAX_ENCRYPTION_RETRIES) {
            ESP_LOGW(TAG, "[%s] Encryption lost, max retries reached — disconnecting", this->log_id_());
            this->parent_->disconnect();
          } else {
            ESP_LOGW(TAG, "[%s] Encryption lost, retry %d/%d", this->log_id_(), this->encryption_retries_,
                     MAX_ENCRYPTION_RETRIES);
            this->set_timeout("connect_encrypt", 500, [this]() { this->request_encryption_("encryption lost"); });
          }
        }
      }
      break;
    }
    default:
      break;
  }
}

void Ecocomfort2Hub::gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  switch (event) {
    case ESP_GAP_BLE_AUTH_CMPL_EVT: {
      if (std::memcmp(param->ble_security.auth_cmpl.bd_addr, this->parent_->get_remote_bda(), sizeof(esp_bd_addr_t)) !=
          0) {
        return;
      }
      if (!this->is_connected()) {
        return;
      }
      if (!param->ble_security.auth_cmpl.success) {
        this->cancel_timeout("connect_auth_timeout");
        this->ready_ = false;
        this->status_set_warning();
        ESP_LOGW(TAG, "[%s] BLE authentication failed, reason=%d — disconnecting to retry", this->log_id_(),
                 param->ble_security.auth_cmpl.fail_reason);
        this->parent_->disconnect();
        return;
      }

      this->on_encryption_ready_();
      break;
    }
    default:
      break;
  }
}

bool Ecocomfort2Hub::parse_state_(const uint8_t *data, uint16_t len) {
  if (len < 9) {
    ESP_LOGW(TAG, "C_STATE too short: %d bytes", len);
    return false;
  }

  this->direction_ = data[2];

  int16_t temp_raw = (static_cast<int16_t>(data[3]) << 8) | data[4];
  this->temperature_ = temp_raw / 100.0f;

  uint16_t hum_raw = (static_cast<uint16_t>(data[5]) << 8) | data[6];
  this->humidity_ = hum_raw / 100.0f;

  uint16_t voc_raw = (static_cast<uint16_t>(data[7]) << 8) | data[8];
  this->voc_ = voc_raw;

  ESP_LOGD(TAG, "[%s] State: temp=%.1f C, humidity=%.1f%%, VOC=%d ppb, dir=%d", this->log_id_(), this->temperature_,
           this->humidity_, this->voc_, this->direction_);
  return true;
}

bool Ecocomfort2Hub::parse_oper_(const uint8_t *data, uint16_t len) {
  if (len < 2) {
    ESP_LOGW(TAG, "C_SETTING_OPER too short: %d bytes", len);
    return false;
  }

  this->actual_mode_ = data[0];
  uint8_t speed_byte = data[1];

  this->boost_active_ = (speed_byte & FLAG_BOOST) != 0;
  this->night_active_ = (speed_byte & FLAG_NIGHT) != 0;
  this->auto_active_ = (speed_byte & FLAG_AUTO) != 0;

  uint8_t base_speed = speed_byte & 0x0F;

  if (this->boost_active_) {
    this->actual_speed_ = SPEED_VEL3;
  } else if (this->night_active_) {
    this->actual_speed_ = SPEED_SLEEP;
  } else {
    this->actual_speed_ = base_speed;
  }

  ESP_LOGD(TAG, "[%s] Oper: mode=%d, speed=%d, boost=%d, night=%d, auto=%d", this->log_id_(), this->actual_mode_,
           this->actual_speed_, this->boost_active_, this->night_active_, this->auto_active_);
  return true;
}

bool Ecocomfort2Hub::parse_config_(const uint8_t *data, uint16_t len) {
  if (len < 5) {
    ESP_LOGW(TAG, "C_CONFIGURATION too short: %d bytes", len);
    return false;
  }

  this->role_ = data[0];
  this->humidity_threshold_ = data[1] & 0x7F;
  this->humidity_advanced_ = (data[1] & 0x80) != 0;
  this->luminosity_threshold_ = data[2] & 0x7F;
  this->voc_threshold_ = data[3] & 0x7F;
  this->voc_advanced_ = (data[3] & 0x80) != 0;
  this->season_summer_ = (data[4] & 0x08) != 0;
  this->free_cooling_level_ = data[4] & 0x03;

  ESP_LOGD(TAG, "[%s] Config: role=%d, hum_thr=%d(%s), lum_thr=%d, voc_thr=%d(%s), season=%s, fc=%d", this->log_id_(),
           this->role_, this->humidity_threshold_, this->humidity_advanced_ ? "adv" : "std",
           this->luminosity_threshold_, this->voc_threshold_, this->voc_advanced_ ? "adv" : "std",
           this->season_summer_ ? "summer" : "winter", this->free_cooling_level_);
  return true;
}

bool Ecocomfort2Hub::parse_advanced_(const uint8_t *data, uint16_t len) {
  if (len < 4) {
    ESP_LOGW(TAG, "C_ADVANCED too short: %d bytes", len);
    return false;
  }

  this->temp_offset_raw_ = (static_cast<int16_t>(data[0]) << 8) | data[1];
  this->humidity_offset_raw_ = (static_cast<int16_t>(data[2]) << 8) | data[3];

  ESP_LOGD(TAG, "[%s] Advanced: temp_offset=%.2f C, hum_offset=%.2f%%", this->log_id_(),
           this->temp_offset_raw_ / 100.0f, this->humidity_offset_raw_ / 100.0f);
  return true;
}

bool Ecocomfort2Hub::parse_info_(const uint8_t *data, uint16_t len) {
  if (len < 2) {
    ESP_LOGW(TAG, "C_INFO too short: %d bytes", len);
    return false;
  }

  uint8_t major = (data[0] >> 4) & 0x0F;
  uint16_t field = (static_cast<uint16_t>(data[0] & 0x0F) << 8) | data[1];
  uint8_t minor = field >> 6;
  uint8_t patch = field & 0x3F;

  snprintf(this->firmware_version_, sizeof(this->firmware_version_), "%u.%u.%u", major, minor, patch);

  ESP_LOGI(TAG, "[%s] Firmware: %s", this->log_id_(), this->firmware_version_);
  return true;
}

void Ecocomfort2Hub::send_operation_command(uint8_t preset, uint8_t speed, bool auto_mode) {
  this->pending_preset_ = preset;
  this->pending_speed_ = speed;
  this->pending_auto_mode_ = auto_mode;

  if (preset != OPER_OFF) {
    this->persisted_.preset = preset;
    this->persisted_.auto_mode = auto_mode;
    if (speed != 0) {
      this->persisted_.speed = speed;
    }
    this->pref_.save(&this->persisted_);
  }

  this->set_timeout("send_cmd", 100, [this]() { this->do_send_operation_command_(); });
}

void Ecocomfort2Hub::do_send_operation_command_() {
  if (!this->is_ready()) {
    ESP_LOGW(TAG, "[%s] Not ready, cannot send operation command", this->log_id_());
    return;
  }

  uint8_t data[2];
  data[0] = this->pending_preset_;
  data[1] = this->pending_auto_mode_ ? FLAG_AUTO : this->pending_speed_;

  ESP_LOGD(TAG, "[%s] Sending operation: preset=0x%02x, speed=0x%02x, auto=%d", this->log_id_(), data[0], data[1],
           this->pending_auto_mode_);
  this->write_characteristic_(this->char_handle_oper_, data, 2);
}

void Ecocomfort2Hub::write_season(bool is_summer) {
  if (!this->is_ready()) {
    ESP_LOGW(TAG, "[%s] Not ready, cannot write season", this->log_id_());
    return;
  }
  if (!this->config_valid_) {
    ESP_LOGW(TAG, "[%s] Configuration not loaded yet, cannot write season", this->log_id_());
    return;
  }

  this->season_summer_ = is_summer;

  uint8_t data[12];
  memset(data, CONFIG_PRESERVE, sizeof(data));
  data[4] = (is_summer ? 0x08 : 0x00) | (this->free_cooling_level_ & 0x03);

  ESP_LOGD(TAG, "[%s] Writing season: %s (byte4=0x%02x)", this->log_id_(), is_summer ? "summer" : "winter", data[4]);
  this->write_characteristic_(this->char_handle_config_, data, 12);
}

void Ecocomfort2Hub::write_free_cooling(uint8_t level) {
  if (!this->is_ready()) {
    ESP_LOGW(TAG, "[%s] Not ready, cannot write free cooling", this->log_id_());
    return;
  }
  if (!this->config_valid_) {
    ESP_LOGW(TAG, "[%s] Configuration not loaded yet, cannot write free cooling", this->log_id_());
    return;
  }

  this->free_cooling_level_ = level;

  uint8_t data[12];
  memset(data, CONFIG_PRESERVE, sizeof(data));
  data[4] = (this->season_summer_ ? 0x08 : 0x00) | (level & 0x03);

  ESP_LOGD(TAG, "[%s] Writing free cooling: %d (byte4=0x%02x)", this->log_id_(), level, data[4]);
  this->write_characteristic_(this->char_handle_config_, data, 12);
}

void Ecocomfort2Hub::write_thresholds(uint8_t humidity, uint8_t luminosity, uint8_t voc) {
  if (!this->is_ready()) {
    ESP_LOGW(TAG, "[%s] Not ready, cannot write thresholds", this->log_id_());
    return;
  }
  if (!this->config_valid_) {
    ESP_LOGW(TAG, "[%s] Configuration not loaded yet, cannot write thresholds", this->log_id_());
    return;
  }

  this->humidity_threshold_ = humidity;
  this->luminosity_threshold_ = luminosity;
  this->voc_threshold_ = voc;

  uint8_t data[12];
  memset(data, CONFIG_PRESERVE, sizeof(data));
  data[1] = humidity + (this->humidity_advanced_ ? 128 : 0);
  data[2] = luminosity;
  data[3] = voc + (this->voc_advanced_ ? 128 : 0);

  ESP_LOGD(TAG, "[%s] Writing thresholds: hum=%d, lum=%d, voc=%d", this->log_id_(), humidity, luminosity, voc);
  this->write_characteristic_(this->char_handle_config_, data, 12);
}

void Ecocomfort2Hub::write_offsets(int16_t temp_offset_raw, int16_t humidity_offset_raw) {
  if (!this->is_ready()) {
    ESP_LOGW(TAG, "[%s] Not ready, cannot write offsets", this->log_id_());
    return;
  }
  if (!this->advanced_valid_) {
    ESP_LOGW(TAG, "[%s] Advanced settings not loaded yet, cannot write offsets", this->log_id_());
    return;
  }

  this->temp_offset_raw_ = temp_offset_raw;
  this->humidity_offset_raw_ = humidity_offset_raw;

  uint8_t data[4];
  data[0] = (temp_offset_raw >> 8) & 0xFF;
  data[1] = temp_offset_raw & 0xFF;
  data[2] = (humidity_offset_raw >> 8) & 0xFF;
  data[3] = humidity_offset_raw & 0xFF;

  ESP_LOGD(TAG, "[%s] Writing offsets: temp=%.2f, hum=%.2f", this->log_id_(), temp_offset_raw / 100.0f,
           humidity_offset_raw / 100.0f);
  this->write_characteristic_(this->char_handle_advanced_, data, 4);
}

void Ecocomfort2Hub::pair() {
  if (!this->is_connected()) {
    return;
  }
  if (this->ready_) {
    ESP_LOGI(TAG, "[%s] BLE link is already ready", this->log_id_());
    return;
  }

  this->request_encryption_("manual pair");
}

bool Ecocomfort2Hub::send_clock() {
  if (!this->is_ready()) {
    return false;
  }

#ifdef USE_TIME
  if (this->time_id_ == nullptr) {
    ESP_LOGD(TAG, "[%s] No time source configured, skipping clock sync", this->log_id_());
    return false;
  }

  ESPTime now = this->time_id_->now();
  if (!now.is_valid()) {
    ESP_LOGW(TAG, "[%s] Time not valid yet, skipping clock sync", this->log_id_());
    return false;
  }

  uint8_t data[8];
  data[0] = now.hour;
  data[1] = now.minute;
  data[2] = now.second;

  uint8_t dow = now.day_of_week;
  if (dow == 1) {
    dow = 6;
  } else {
    dow = dow - 2;
  }
  data[3] = dow;
  data[4] = now.day_of_month;
  data[5] = now.month;
  data[6] = (now.year >> 8) & 0xFF;
  data[7] = now.year & 0xFF;

  ESP_LOGD(TAG, "[%s] Syncing clock: %02d:%02d:%02d", this->log_id_(), now.hour, now.minute, now.second);
  return this->write_characteristic_(this->char_handle_clock_, data, 8);
#endif

  return false;
}

bool Ecocomfort2Hub::write_characteristic_(uint16_t handle, const uint8_t *data, uint16_t len) {
  if (handle == 0) {
    ESP_LOGW(TAG, "[%s] Cannot write: handle not discovered", this->log_id_());
    return false;
  }

  auto status =
      esp_ble_gattc_write_char(this->parent_->get_gattc_if(), this->parent_->get_conn_id(), handle, len,
                               const_cast<uint8_t *>(data), ESP_GATT_WRITE_TYPE_NO_RSP, ESP_GATT_AUTH_REQ_NONE);
  if (status != ESP_GATT_OK) {
    ESP_LOGW(TAG, "[%s] Write failed for handle 0x%x, status=%d", this->log_id_(), handle, status);
    return false;
  }

  return true;
}

void Ecocomfort2Hub::request_encryption_(const char *reason) {
  if (!this->is_connected() || this->ready_) {
    return;
  }

  this->cancel_timeout("connect_auth_timeout");
  this->cancel_timeout("connect_encrypt");
  ESP_LOGI(TAG, "[%s] Requesting BLE encryption (%s)", this->log_id_(), reason);
  auto err = this->parent_->pair();
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "[%s] BLE pair request failed, err=%d — disconnecting to retry", this->log_id_(), err);
    this->parent_->disconnect();
    return;
  }

  this->set_timeout("connect_auth_timeout", ENCRYPTION_TIMEOUT_MS, [this]() {
    if (!this->is_connected() || this->ready_) {
      return;
    }
    this->status_set_warning();
    ESP_LOGW(TAG, "[%s] BLE authentication timed out — disconnecting to retry", this->log_id_());
    this->parent_->disconnect();
  });
}

const char *Ecocomfort2Hub::log_id_() const {
  if (this->parent_ != nullptr) {
    return this->parent_->address_str();
  }
  return "ecocomfort2";
}

void Ecocomfort2Hub::on_encryption_ready_() {
  if (!this->is_connected() || this->ready_) {
    return;
  }

  this->cancel_timeout("connect_encrypt");
  this->cancel_timeout("connect_auth_timeout");
  this->ready_ = true;
  this->encryption_retries_ = 0;
  this->status_clear_warning();
  ESP_LOGI(TAG, "[%s] Encryption confirmed, device ready", this->log_id_());
  this->dispatch_connect_(true);
  this->set_timeout("post_auth_sync", 0, [this]() {
    if (!this->is_ready()) {
      return;
    }
    this->last_clock_attempt_ = millis();
    if (this->send_clock()) {
      this->last_clock_sync_ = this->last_clock_attempt_;
    }
    this->update();
  });
}

void Ecocomfort2Hub::dispatch_status_() {
  for (auto *child : this->children_) {
    child->on_status();
  }
}

void Ecocomfort2Hub::dispatch_config_() {
  for (auto *child : this->children_) {
    child->on_config();
  }
}

void Ecocomfort2Hub::dispatch_connect_(bool connected) {
  for (auto *child : this->children_) {
    child->on_connect(connected);
  }
}

void Ecocomfort2Hub::register_child(Ecocomfort2Client *obj) {
  this->children_.push_back(obj);
  obj->set_parent(this);
}

}  // namespace ecocomfort2
}  // namespace esphome

#endif
