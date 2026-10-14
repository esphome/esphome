#include "esphome/core/defines.h"
#ifdef USE_ESP32
#include "ecocomfort2_hub.h"
#include "esphome/core/log.h"

#include <cstdio>

namespace esphome::ecocomfort2 {

ESPHOME_LOG_TAG(TAG, "ecocomfort2");

void Ecocomfort2Hub::setup() {
#ifdef USE_BINARY_SENSOR
  if (this->connected_binary_sensor_ != nullptr)
    this->connected_binary_sensor_->publish_initial_state(false);
#endif
#ifdef USE_TIME
  // The clock is written after each time-source sync
  if (this->time_ != nullptr)
    this->time_->add_on_time_sync_callback([this]() { this->sync_clock_(); });
#endif
}

void Ecocomfort2Hub::update() {
  if (!this->is_ready()) {
    ESP_LOGD(TAG, "[%s] Not ready, skipping update", this->parent_->address_str());
    return;
  }
  // The GATT client runs the requests one after the other
  this->read_char_(this->char_handle_state_);
  this->read_char_(this->char_handle_oper_);
}

void Ecocomfort2Hub::read_char_(uint16_t handle) {
  auto status = esp_ble_gattc_read_char(this->parent_->get_gattc_if(), this->parent_->get_conn_id(), handle,
                                        ESP_GATT_AUTH_REQ_NONE);
  if (status != ESP_GATT_OK) {
    ESP_LOGW(TAG, "[%s] Read request for handle 0x%x failed, status=%d", this->parent_->address_str(), handle, status);
  }
}

void Ecocomfort2Hub::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Ecocomfort2 Hub:\n"
                "  Address: %s",
                this->parent_->address_str());
  LOG_UPDATE_INTERVAL(this);
#ifdef USE_TIME
  ESP_LOGCONFIG(TAG, "  Clock sync: %s", YESNO(this->time_ != nullptr));
#endif
#ifdef USE_SENSOR
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
  LOG_SENSOR("  ", "VOC", this->voc_sensor_);
  LOG_SENSOR("  ", "Direction", this->direction_sensor_);
#endif
#ifdef USE_BINARY_SENSOR
  LOG_BINARY_SENSOR("  ", "Connected", this->connected_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Boost", this->boost_binary_sensor_);
#endif
#ifdef USE_TEXT_SENSOR
  LOG_TEXT_SENSOR("  ", "Firmware", this->firmware_text_sensor_);
#endif
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
      this->char_handle_state_ = 0;
      this->char_handle_info_ = 0;
      this->char_handle_clock_ = 0;
      this->set_connected_(false);
      this->publish_unknown_();
      break;
    }
    case ESP_GATTC_SEARCH_CMPL_EVT: {
      const auto service_uuid = espbt::ESPBTUUID::from_raw("f4b827c3-e660-4bc8-bdf6-3c8e9b845e0d");
      const auto oper_uuid = espbt::ESPBTUUID::from_raw("b9d6f678-bc0d-4a73-90c8-60b0f07301f1");
      const auto state_uuid = espbt::ESPBTUUID::from_raw("438d3433-7e5a-459a-a8e4-66343fad2bb0");
      const auto info_uuid = espbt::ESPBTUUID::from_raw("f5f56229-dd4f-480f-a829-9189269d8b37");
      const auto clock_uuid = espbt::ESPBTUUID::from_raw("82788997-49e4-4533-b949-7ed433678044");
      // Keep the handles, not the characteristics: ble_client frees its services once all nodes are established.
      auto *oper = this->parent_->get_characteristic(service_uuid, oper_uuid);
      auto *state = this->parent_->get_characteristic(service_uuid, state_uuid);
      if (oper == nullptr || state == nullptr) {
        ESP_LOGW(TAG, "[%s] C_OPER or C_STATE characteristic not found, disconnecting", this->parent_->address_str());
        this->parent_->disconnect();
        break;
      }
      this->char_handle_oper_ = oper->handle;
      this->char_handle_state_ = state->handle;
      // The version and clock characteristics only feed optional features, so a unit without them still works
      auto *info = this->parent_->get_characteristic(service_uuid, info_uuid);
      this->char_handle_info_ = info != nullptr ? info->handle : 0;
      auto *clock = this->parent_->get_characteristic(service_uuid, clock_uuid);
      this->char_handle_clock_ = clock != nullptr ? clock->handle : 0;
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
      if (param->read.conn_id != this->parent_->get_conn_id()) {
        break;
      }
      const uint16_t handle = param->read.handle;
      if (handle == this->char_handle_oper_) {
        if (this->read_ok_(param))
          this->on_oper_(param->read.value, param->read.value_len);
      } else if (handle == this->char_handle_state_) {
        if (this->read_ok_(param))
          this->on_state_(param->read.value, param->read.value_len);
      } else if (handle == this->char_handle_info_) {
        if (this->read_ok_(param))
          this->on_info_(param->read.value, param->read.value_len);
      } else {
        break;
      }
      break;
    }
    default:
      break;
  }
}

bool Ecocomfort2Hub::read_ok_(const esp_ble_gattc_cb_param_t *param) {
  if (param->read.status == ESP_GATT_INSUF_ENCRYPTION || param->read.status == ESP_GATT_INSUF_AUTHENTICATION) {
    // ble_client reconnects and pairs again
    ESP_LOGW(TAG, "[%s] Encryption lost, disconnecting", this->parent_->address_str());
    this->parent_->disconnect();
    return false;
  }
  if (param->read.status != ESP_GATT_OK) {
    ESP_LOGW(TAG, "[%s] Read of handle 0x%x failed, status=%d", this->parent_->address_str(), param->read.handle,
             param->read.status);
    return false;
  }
  return true;
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
  this->set_connected_(true);
#ifdef USE_TIME
  if (this->time_ != nullptr)
    this->sync_clock_();
#endif
#ifdef USE_TEXT_SENSOR
  if (this->firmware_text_sensor_ != nullptr && this->char_handle_info_ != 0)
    this->read_char_(this->char_handle_info_);
#endif
  this->update();
}

void Ecocomfort2Hub::set_connected_(bool connected) {
#ifdef USE_BINARY_SENSOR
  if (this->connected_binary_sensor_ != nullptr)
    this->connected_binary_sensor_->publish_state(connected);
#endif
}

// Readings from a unit that is out of reach would otherwise look current for hours
void Ecocomfort2Hub::publish_unknown_() {
#ifdef USE_SENSOR
  for (auto *sensor : {this->temperature_sensor_, this->humidity_sensor_, this->voc_sensor_, this->direction_sensor_}) {
    if (sensor != nullptr)
      sensor->publish_state(NAN);
  }
#endif
}

// C_OPER: mode byte, speed/flags byte
void Ecocomfort2Hub::on_oper_(const uint8_t *data, uint16_t len) {
  if (len < 2) {
    ESP_LOGW(TAG, "[%s] C_OPER too short: %u bytes", this->parent_->address_str(), len);
    return;
  }
  this->mode_ = data[0];
  this->speed_ = data[1];
  ESP_LOGD(TAG, "[%s] C_OPER: mode=0x%02x, speed=0x%02x", this->parent_->address_str(), this->mode_, this->speed_);
#ifdef ECOCOMFORT2_CHILD_COUNT
  for (auto *child : this->children_) {
    child->on_status();
  }
#endif
}

// C_STATE: bytes 0-1 the mode and speed the unit runs, with the boost and night flags in byte 1,
// byte 2 the air flow direction, bytes 3-4 temperature and 5-6 humidity in hundredths, 7-8 VOC in ppb
void Ecocomfort2Hub::on_state_(const uint8_t *data, uint16_t len) {
  if (len < 9) {
    ESP_LOGW(TAG, "[%s] C_STATE too short: %u bytes", this->parent_->address_str(), len);
    return;
  }
  const float temperature = static_cast<int16_t>(encode_uint16(data[3], data[4])) / 100.0f;
  const float humidity = encode_uint16(data[5], data[6]) / 100.0f;
  const uint16_t voc = encode_uint16(data[7], data[8]);
  ESP_LOGD(TAG, "[%s] C_STATE: mode=0x%02x, speed=0x%02x, %.2f °C, %.2f %%, %u ppb, direction=%u",
           this->parent_->address_str(), data[0], data[1], temperature, humidity, voc, data[2]);
#ifdef USE_BINARY_SENSOR
  if (this->boost_binary_sensor_ != nullptr)
    this->boost_binary_sensor_->publish_state((data[1] & FLAG_BOOST) != 0);
#endif
#ifdef USE_SENSOR
  if (this->temperature_sensor_ != nullptr)
    this->temperature_sensor_->publish_state(temperature);
  if (this->humidity_sensor_ != nullptr)
    this->humidity_sensor_->publish_state(humidity);
  if (this->voc_sensor_ != nullptr)
    this->voc_sensor_->publish_state(voc);
  if (this->direction_sensor_ != nullptr)
    this->direction_sensor_->publish_state(data[2]);
#endif
}

// C_INFO: firmware version packed as 4-bit major, 6-bit minor and 6-bit patch
void Ecocomfort2Hub::on_info_(const uint8_t *data, uint16_t len) {
  if (len < 2) {
    ESP_LOGW(TAG, "[%s] C_INFO too short: %u bytes", this->parent_->address_str(), len);
    return;
  }
  const uint16_t field = encode_uint16(data[0] & 0x0F, data[1]);
  char version[12];
  snprintf(version, sizeof(version), "%u.%u.%u", data[0] >> 4, field >> 6, field & 0x3F);
  ESP_LOGD(TAG, "[%s] Firmware %s", this->parent_->address_str(), version);
#ifdef USE_TEXT_SENSOR
  if (this->firmware_text_sensor_ != nullptr)
    this->firmware_text_sensor_->publish_state(version);
#endif
}

#ifdef USE_TIME
// C_CLOCK: two-digit year, month, day, weekday with Monday as 0, hour, minute, second, automatic DST flag
void Ecocomfort2Hub::sync_clock_() {
  if (!this->is_ready() || this->char_handle_clock_ == 0)
    return;
  const ESPTime now = this->time_->now();
  if (!now.is_valid())
    return;
  // ESPTime counts Sunday=1 .. Saturday=7, the unit Monday=0 .. Sunday=6
  const uint8_t weekday = (now.day_of_week + 5) % 7;
  // The time source already applies DST and every sync rewrites the clock, so the unit must not shift it again
  uint8_t data[8]{static_cast<uint8_t>(now.year % 100),
                  now.month,
                  now.day_of_month,
                  weekday,
                  now.hour,
                  now.minute,
                  now.second,
                  0};
  ESP_LOGD(TAG, "[%s] Syncing clock to %04u-%02u-%02u %02u:%02u:%02u", this->parent_->address_str(), now.year,
           now.month, now.day_of_month, now.hour, now.minute, now.second);
  auto status =
      esp_ble_gattc_write_char(this->parent_->get_gattc_if(), this->parent_->get_conn_id(), this->char_handle_clock_,
                               sizeof(data), data, ESP_GATT_WRITE_TYPE_NO_RSP, ESP_GATT_AUTH_REQ_NONE);
  if (status != ESP_GATT_OK) {
    ESP_LOGW(TAG, "[%s] C_CLOCK write failed, status=%d", this->parent_->address_str(), status);
  }
}
#endif

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
  this->set_timeout(READBACK_TIMEOUT_ID, READBACK_DELAY_MS, [this]() { this->read_char_(this->char_handle_oper_); });
}

#ifdef ECOCOMFORT2_CHILD_COUNT
void Ecocomfort2Hub::register_child(Ecocomfort2Client *obj) {
  this->children_.push_back(obj);
  obj->set_parent(this);
}
#endif

}  // namespace esphome::ecocomfort2

#endif  // USE_ESP32
