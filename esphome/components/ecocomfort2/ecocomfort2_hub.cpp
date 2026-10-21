#include "esphome/core/defines.h"
#ifdef USE_ESP32
#include "ecocomfort2_hub.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

#include <cmath>
#include <cstdio>
#include <cstring>

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
  // The vendor app can change the settings too
  this->read_settings_();
}

// Counts an answer. The unit answers reads in order, so only the last pending answer shows the current value
static bool last_pending_read(SettingsReads &reads) {
  reads.active_ms = App.get_loop_component_start_time();
  if (reads.pending > 0)
    reads.pending--;
  return reads.pending == 0;
}

bool Ecocomfort2Hub::write_char_(uint16_t handle, uint8_t *data, uint16_t len) {
  auto status = esp_ble_gattc_write_char(this->parent_->get_gattc_if(), this->parent_->get_conn_id(), handle, len, data,
                                         ESP_GATT_WRITE_TYPE_NO_RSP, ESP_GATT_AUTH_REQ_NONE);
  if (status != ESP_GATT_OK) {
    ESP_LOGW(TAG, "[%s] Write to handle 0x%x failed, status=%d", this->parent_->address_str(), handle, status);
    return false;
  }
  return true;
}

bool Ecocomfort2Hub::read_char_(uint16_t handle) {
  auto status = esp_ble_gattc_read_char(this->parent_->get_gattc_if(), this->parent_->get_conn_id(), handle,
                                        ESP_GATT_AUTH_REQ_NONE);
  if (status != ESP_GATT_OK) {
    ESP_LOGW(TAG, "[%s] Read request for handle 0x%x failed, status=%d", this->parent_->address_str(), handle, status);
    return false;
  }
  return true;
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
#ifdef USE_NUMBER
  LOG_NUMBER("  ", "Humidity threshold", this->numbers_[static_cast<uint8_t>(NumberSetting::HUMIDITY_THRESHOLD)]);
  LOG_NUMBER("  ", "Luminosity threshold", this->numbers_[static_cast<uint8_t>(NumberSetting::LUMINOSITY_THRESHOLD)]);
  LOG_NUMBER("  ", "VOC threshold", this->numbers_[static_cast<uint8_t>(NumberSetting::VOC_THRESHOLD)]);
  LOG_NUMBER("  ", "Temperature offset", this->numbers_[static_cast<uint8_t>(NumberSetting::TEMPERATURE_OFFSET)]);
  LOG_NUMBER("  ", "Humidity offset", this->numbers_[static_cast<uint8_t>(NumberSetting::HUMIDITY_OFFSET)]);
#endif
#ifdef USE_SWITCH
  LOG_SWITCH("  ", "Humidity advanced", this->switches_[static_cast<uint8_t>(SwitchSetting::HUMIDITY_ADVANCED)]);
  LOG_SWITCH("  ", "VOC advanced", this->switches_[static_cast<uint8_t>(SwitchSetting::VOC_ADVANCED)]);
#endif
#ifdef USE_SELECT
  LOG_SELECT("  ", "Season", this->selects_[static_cast<uint8_t>(SelectSetting::SEASON)]);
  LOG_SELECT("  ", "Free cooling", this->selects_[static_cast<uint8_t>(SelectSetting::FREE_COOLING)]);
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
      this->config_reads_ = {};
      this->advanced_reads_ = {};
      this->char_handle_config_ = 0;
      this->char_handle_advanced_ = 0;
      this->config_read_ = false;
      this->offsets_read_ = false;
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
      const auto config_uuid = espbt::ESPBTUUID::from_raw("d3dac48e-b4e1-4f3a-8715-326ddf1da89a");
      const auto advanced_uuid = espbt::ESPBTUUID::from_raw("f8b2284e-61dd-44e3-a782-a93c9503ab2d");
      auto *config = this->parent_->get_characteristic(service_uuid, config_uuid);
      this->char_handle_config_ = config != nullptr ? config->handle : 0;
      auto *advanced = this->parent_->get_characteristic(service_uuid, advanced_uuid);
      this->char_handle_advanced_ = advanced != nullptr ? advanced->handle : 0;
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
      } else if (handle == this->char_handle_config_) {
        if (last_pending_read(this->config_reads_) && this->read_ok_(param))
          this->on_config_(param->read.value, param->read.value_len);
      } else if (handle == this->char_handle_advanced_) {
        if (last_pending_read(this->advanced_reads_) && this->read_ok_(param))
          this->on_advanced_(param->read.value, param->read.value_len);
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
#ifdef USE_BINARY_SENSOR
  if (this->boost_binary_sensor_ != nullptr)
    this->boost_binary_sensor_->invalidate_state();
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

// C_STATE: bytes 0-1 the mode and speed the unit runs; byte 1 also has the boost flag (0x40) and the advanced
// control step (0x20). Byte 2 the air flow direction, bytes 3-4 temperature and 5-6 humidity in hundredths,
// 7-8 VOC in ppb
void Ecocomfort2Hub::on_state_(const uint8_t *data, uint16_t len) {
  if (len < 9) {
    ESP_LOGW(TAG, "[%s] C_STATE too short: %u bytes", this->parent_->address_str(), len);
    return;
  }
  const float temperature = encode_value<int16_t>(&data[3]) / 100.0f;
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
  uint8_t data[8]{
      static_cast<uint8_t>(now.year % 100), now.month, now.day_of_month, weekday, now.hour, now.minute, now.second, 0};
  ESP_LOGD(TAG, "[%s] Syncing clock to %04u-%02u-%02u %02u:%02u:%02u", this->parent_->address_str(), now.year,
           now.month, now.day_of_month, now.hour, now.minute, now.second);
  this->write_char_(this->char_handle_clock_, data, sizeof(data));
}
#endif

void Ecocomfort2Hub::write_oper(uint8_t mode, uint8_t speed) {
  if (!this->is_ready()) {
    ESP_LOGW(TAG, "[%s] Not ready, cannot write C_OPER", this->parent_->address_str());
    return;
  }
  uint8_t data[2]{mode, speed};
  ESP_LOGD(TAG, "[%s] Writing C_OPER: mode=0x%02x, speed=0x%02x", this->parent_->address_str(), mode, speed);
  if (!this->write_char_(this->char_handle_oper_, data, sizeof(data)))
    return;
  this->set_timeout(READBACK_TIMEOUT_ID, READBACK_DELAY_MS, [this]() { this->read_char_(this->char_handle_oper_); });
}

// In a C_CONFIGURATION write, 0x7F in bytes 0-5 keeps the current value, as the vendor app does
static constexpr uint8_t CONFIG_KEEP = 0x7F;
// Threshold bytes: level 0-3 in the low bits, the advanced control flag in bit 7
static constexpr uint8_t THRESHOLD_ADVANCED = 0x80;
static constexpr uint8_t THRESHOLD_LEVEL_MASK = 0x7F;
// Byte 4: season in the high nibble (0 winter, 1 summer), free cooling level 0-3 in the low nibble
static constexpr uint8_t SEASON_SUMMER = 0x10;
static constexpr uint8_t KEEP_SEASON = 0x70;
static constexpr uint8_t KEEP_FREE_COOLING = 0x0F;
// The thresholds the humidity and VOC advanced flags belong to
static constexpr uint8_t ADVANCED_THRESHOLD[] = {0, 2};

// The unit answers within a few seconds; after this long an answer was lost (the BLE event pool was full) and
// the reads start over, which can at worst accept one late answer as current until the next poll
static constexpr uint32_t SETTINGS_READ_STALL_MS = 30000;

bool Ecocomfort2Hub::request_read_(uint16_t handle, SettingsReads &reads) {
  if (!this->read_char_(handle))
    return false;
  reads.active_ms = App.get_loop_component_start_time();
  if (reads.owed) {
    reads.owed = false;
  } else {
    reads.pending++;
  }
  return true;
}

// A poll waits for the answers still pending, so a short update interval cannot keep the count above zero
void Ecocomfort2Hub::read_settings_() {
  const uint32_t now = App.get_loop_component_start_time();
  for (auto *reads : {&this->config_reads_, &this->advanced_reads_}) {
    if (reads->pending != 0 && now - reads->active_ms > SETTINGS_READ_STALL_MS) {
      ESP_LOGW(TAG, "[%s] Settings read not answered, retrying", this->parent_->address_str());
      *reads = {};
    }
  }
  if (this->needs_config_ && this->char_handle_config_ != 0 &&
      (this->config_reads_.pending == 0 || this->config_reads_.owed))
    this->request_read_(this->char_handle_config_, this->config_reads_);
  if (this->needs_offsets_ && this->char_handle_advanced_ != 0 &&
      (this->advanced_reads_.pending == 0 || this->advanced_reads_.owed))
    this->request_read_(this->char_handle_advanced_, this->advanced_reads_);
}

// C_CONFIGURATION: role, the humidity, luminosity and VOC thresholds, season and free cooling, then the
// fields for a unit paired to another one, which are left alone
void Ecocomfort2Hub::on_config_(const uint8_t *data, uint16_t len) {
  if (len < 5) {
    ESP_LOGW(TAG, "[%s] C_CONFIGURATION too short: %u bytes", this->parent_->address_str(), len);
    return;
  }
  // A write already updated the copy and the entity, so only a change made elsewhere or a refused write is new
  if (this->config_read_ && memcmp(this->thresholds_, data + 1, sizeof(this->thresholds_)) == 0 &&
      this->season_byte_ == data[4])
    return;
  memcpy(this->thresholds_, data + 1, sizeof(this->thresholds_));
  this->season_byte_ = data[4];
  this->config_read_ = true;
  ESP_LOGD(TAG, "[%s] C_CONFIGURATION: thresholds=0x%02x 0x%02x 0x%02x, season=0x%02x", this->parent_->address_str(),
           data[1], data[2], data[3], data[4]);
  this->publish_config_();
}

// C_ADVANCED_SETTINGS: temperature and humidity offsets in hundredths, signed big-endian
void Ecocomfort2Hub::on_advanced_(const uint8_t *data, uint16_t len) {
  if (len < 4) {
    ESP_LOGW(TAG, "[%s] C_ADVANCED_SETTINGS too short: %u bytes", this->parent_->address_str(), len);
    return;
  }
  const int16_t temperature = encode_value<int16_t>(data);
  const int16_t humidity = encode_value<int16_t>(data + 2);
  if (this->offsets_read_ && this->offsets_[0] == temperature && this->offsets_[1] == humidity)
    return;
  this->offsets_[0] = temperature;
  this->offsets_[1] = humidity;
  this->offsets_read_ = true;
  ESP_LOGD(TAG, "[%s] C_ADVANCED_SETTINGS: offsets %d %d", this->parent_->address_str(), temperature, humidity);
  this->publish_offsets_();
}

// Numbers and selects run their callbacks on every publish, so an unchanged value is skipped: an on_value
// automation that writes another setting must not trigger itself again
#ifdef USE_NUMBER
static void publish_if_changed(number::Number *number, float value) {
  if (number != nullptr && (!number->has_state() || number->state != value))
    number->publish_state(value);
}
#endif
#ifdef USE_SELECT
static void publish_if_changed(select::Select *select, size_t index) {
  if (select == nullptr)
    return;
  const auto current = select->active_index();
  if (!current.has_value() || *current != index)
    select->publish_state(index);
}
#endif

void Ecocomfort2Hub::publish_config_() {
#ifdef USE_NUMBER
  for (size_t i = 0; i < sizeof(this->thresholds_); i++)
    publish_if_changed(this->numbers_[i], this->thresholds_[i] & THRESHOLD_LEVEL_MASK);
#endif
#ifdef USE_SWITCH
  for (uint8_t i = 0; i < static_cast<uint8_t>(SwitchSetting::COUNT); i++) {
    const uint8_t threshold = this->thresholds_[ADVANCED_THRESHOLD[i]];
    // The flag only counts while the threshold is set
    if (this->switches_[i] != nullptr)
      this->switches_[i]->publish_state((threshold & THRESHOLD_ADVANCED) && (threshold & THRESHOLD_LEVEL_MASK));
  }
#endif
#ifdef USE_SELECT
  const bool summer = (this->season_byte_ & SEASON_SUMMER) != 0;
  auto *season = this->selects_[static_cast<uint8_t>(SelectSetting::SEASON)];
  publish_if_changed(season, static_cast<size_t>(summer));
  // The unit keeps the free cooling level through winter but only runs it in summer. The season's on_value
  // automations may have changed the season, so it is read again.
  auto *free_cooling = this->selects_[static_cast<uint8_t>(SelectSetting::FREE_COOLING)];
  const uint8_t level = (this->season_byte_ & SEASON_SUMMER) != 0 ? this->season_byte_ & KEEP_FREE_COOLING : 0;
  if (free_cooling != nullptr && free_cooling->has_index(level))
    publish_if_changed(free_cooling, static_cast<size_t>(level));
#endif
}

void Ecocomfort2Hub::publish_offsets_() {
#ifdef USE_NUMBER
  for (uint8_t i = 0; i < 2; i++) {
    publish_if_changed(this->numbers_[static_cast<uint8_t>(NumberSetting::TEMPERATURE_OFFSET) + i],
                       this->offsets_[i] / 100.0f);
  }
#endif
}

bool Ecocomfort2Hub::write_settings_(uint16_t handle, uint8_t *data, uint16_t len, SettingsReads &reads) {
  if (!this->is_ready() || handle == 0) {
    ESP_LOGW(TAG, "[%s] Not ready, cannot change the setting", this->parent_->address_str());
    return false;
  }
  if (!this->write_char_(handle, data, len))
    return false;
  // A read sent now returns what this write stored; without one, a reserved slot still drops the older answers
  if (!this->request_read_(handle, reads) && !reads.owed) {
    reads.active_ms = App.get_loop_component_start_time();
    reads.pending++;
    reads.owed = true;
  }
  return true;
}

// The readback takes seconds, so the copy and entities follow the write now; it corrects a refused write
void Ecocomfort2Hub::write_config_(uint8_t index, uint8_t value) {
  // Bytes 6-11 hold the address of the main unit for a secondary one; the unit ignores them while the role byte
  // is keep, and the vendor app sends them as 0x00 too
  uint8_t data[12]{CONFIG_KEEP, CONFIG_KEEP, CONFIG_KEEP, CONFIG_KEEP, CONFIG_KEEP, CONFIG_KEEP};
  data[index] = value;
  ESP_LOGD(TAG, "[%s] Writing C_CONFIGURATION byte %u: 0x%02x", this->parent_->address_str(), index, value);
  if (!this->write_settings_(this->char_handle_config_, data, sizeof(data), this->config_reads_))
    return;
  if (index == 4) {
    if ((value & 0xF0) != KEEP_SEASON)
      this->season_byte_ = (this->season_byte_ & 0x0F) | (value & 0xF0);
    if ((value & 0x0F) != KEEP_FREE_COOLING)
      this->season_byte_ = (this->season_byte_ & 0xF0) | (value & 0x0F);
  } else {
    this->thresholds_[index - 1] = value;
  }
  this->publish_config_();
}

void Ecocomfort2Hub::write_offsets_(int16_t temperature, int16_t humidity) {
  uint8_t data[4]{static_cast<uint8_t>(temperature >> 8), static_cast<uint8_t>(temperature),
                  static_cast<uint8_t>(humidity >> 8), static_cast<uint8_t>(humidity)};
  ESP_LOGD(TAG, "[%s] Writing C_ADVANCED_SETTINGS: offsets %d %d", this->parent_->address_str(), temperature, humidity);
  if (!this->write_settings_(this->char_handle_advanced_, data, sizeof(data), this->advanced_reads_))
    return;
  this->offsets_[0] = temperature;
  this->offsets_[1] = humidity;
  this->publish_offsets_();
}

#ifdef USE_NUMBER
void Ecocomfort2Hub::write_number(NumberSetting setting, float value) {
  if (setting == NumberSetting::TEMPERATURE_OFFSET || setting == NumberSetting::HUMIDITY_OFFSET) {
    // Both offsets go in one write, so the other one must be known
    if (!this->offsets_read_) {
      ESP_LOGW(TAG, "[%s] Offsets not read yet", this->parent_->address_str());
      return;
    }
    int16_t offsets[2]{this->offsets_[0], this->offsets_[1]};
    offsets[setting == NumberSetting::HUMIDITY_OFFSET] = static_cast<int16_t>(lroundf(value * 100.0f));
    this->write_offsets_(offsets[0], offsets[1]);
    return;
  }
  // A threshold keeps its advanced flag, which only exists while the threshold is set
  if (!this->config_read_) {
    ESP_LOGW(TAG, "[%s] Configuration not read yet", this->parent_->address_str());
    return;
  }
  const uint8_t index = static_cast<uint8_t>(setting);
  const uint8_t level = static_cast<uint8_t>(lroundf(value));
  const bool advanced = (this->thresholds_[index] & THRESHOLD_ADVANCED) != 0 && level > 0;
  this->write_config_(index + 1, level | (advanced ? THRESHOLD_ADVANCED : 0));
}
#endif

#ifdef USE_SWITCH
void Ecocomfort2Hub::write_switch(SwitchSetting setting, bool state) {
  if (!this->config_read_) {
    ESP_LOGW(TAG, "[%s] Configuration not read yet", this->parent_->address_str());
    return;
  }
  const uint8_t index = ADVANCED_THRESHOLD[static_cast<uint8_t>(setting)];
  const uint8_t level = this->thresholds_[index] & THRESHOLD_LEVEL_MASK;
  if (state && level == 0) {
    ESP_LOGW(TAG, "[%s] Set the threshold before turning on advanced control", this->parent_->address_str());
    return;
  }
  this->write_config_(index + 1, level | (state ? THRESHOLD_ADVANCED : 0));
}
#endif

#ifdef USE_SELECT
void Ecocomfort2Hub::write_select(SelectSetting setting, size_t index) {
  if (!this->config_read_) {
    ESP_LOGW(TAG, "[%s] Configuration not read yet", this->parent_->address_str());
    return;
  }
  if (setting == SelectSetting::SEASON) {
    this->write_config_(4, (index == 1 ? SEASON_SUMMER : 0) | KEEP_FREE_COOLING);
    return;
  }
  // The unit only runs free cooling in summer and keeps its level through winter, so even Off is refused then
  if ((this->season_byte_ & SEASON_SUMMER) == 0) {
    ESP_LOGW(TAG, "[%s] Free cooling is only available in summer", this->parent_->address_str());
    return;
  }
  this->write_config_(4, KEEP_SEASON | static_cast<uint8_t>(index));
}
#endif

#ifdef ECOCOMFORT2_CHILD_COUNT
void Ecocomfort2Hub::register_child(Ecocomfort2Client *obj) {
  this->children_.push_back(obj);
  obj->set_parent(this);
}
#endif

}  // namespace esphome::ecocomfort2

#endif  // USE_ESP32
