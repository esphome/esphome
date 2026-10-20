#include "a2dp_source_speaker.h"

#ifdef USE_ESP32

#include <algorithm>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <new>
#include <string>

#include "esphome/components/esp32_ble/ble.h"
#ifdef USE_ESP32_BLE_TRACKER
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#endif
#include "esphome/core/log.h"

#include <esp_a2dp_legacy_api.h>
#include <esp_bt_main.h>
#include <esp_timer.h>

namespace esphome::a2dp_source {

static const char *const TAG = "a2dp_source";
static std::atomic<A2DPSourceSpeaker *> active_speaker{nullptr};
static std::atomic<A2DPSourceSpeaker *> desired_speaker{nullptr};
static A2DPSourceSpeaker *owner_speaker = nullptr;
static A2DPSourceSpeaker *registered_speakers[8]{};
static bool profile_initialized = false;
static bool profile_initialization_failed = false;
static std::atomic<bool> classic_profile_ready{false};
static uint32_t handoff_started_ms = 0;
static constexpr size_t PCM_BUFFER_BYTES = 32768;
static constexpr size_t MAX_DISCOVERED = 6;

struct DiscoveredDevice {
  uint8_t address[6]{};
  char name[33]{};
  int8_t rssi{-128};
};

static DiscoveredDevice discovered[MAX_DISCOVERED]{};
static size_t discovered_count = 0;
static std::atomic<bool> classic_discovery_running{false};
static portMUX_TYPE discovery_mux = portMUX_INITIALIZER_UNLOCKED;

static std::string format_address(const uint8_t *address) {
  char result[18];
  std::snprintf(result, sizeof(result), "%02X:%02X:%02X:%02X:%02X:%02X", address[0], address[1], address[2], address[3],
                address[4], address[5]);
  return result;
}

static std::string quote_json_string(const char *value, size_t length) {
  std::string result = "\"";
  for (size_t i = 0; i < length; i++) {
    const unsigned char ch = static_cast<unsigned char>(value[i]);
    if (ch == '"' || ch == '\\') {
      result += '\\';
      result += static_cast<char>(ch);
    } else if (ch < 0x20) {
      result += '?';
    } else {
      result += static_cast<char>(ch);
    }
  }
  result += '"';
  return result;
}

static bool parse_address(const char *address, uint8_t *result) {
  if (address == nullptr)
    return false;
  unsigned int octets[6];
  int consumed = 0;
  if (std::sscanf(address, "%2x:%2x:%2x:%2x:%2x:%2x%n", &octets[0], &octets[1], &octets[2], &octets[3], &octets[4],
                  &octets[5], &consumed) != 6 ||
      address[consumed] != '\0')
    return false;
  for (size_t i = 0; i < 6; i++)
    result[i] = static_cast<uint8_t>(octets[i]);
  return std::any_of(result, result + 6, [](uint8_t octet) { return octet != 0; });
}

bool A2DPSourceSpeaker::set_address(const char *address, bool persist) {
  if (this->connected_.load() || this->requested_.load() || this->pairing_.load() || this->remove_bond_pending_.load())
    return false;
  uint8_t parsed[6];
  if (!parse_address(address, parsed))
    return false;
  std::memcpy(this->address_, parsed, sizeof(parsed));
  return !persist || this->save_address_();
}

bool A2DPSourceSpeaker::has_address_() const {
  return std::any_of(std::begin(this->address_), std::end(this->address_), [](uint8_t octet) { return octet != 0; });
}

std::string A2DPSourceSpeaker::address_string() const {
  return this->has_address_() ? format_address(this->address_) : std::string{};
}

bool A2DPSourceSpeaker::save_address_() {
  if (!this->slot_)
    return false;
  uint8_t saved[8] = {0xA2,
                      0xD0,
                      this->address_[0],
                      this->address_[1],
                      this->address_[2],
                      this->address_[3],
                      this->address_[4],
                      this->address_[5]};
  bool ok = this->address_pref_.save(&saved);
  if (ok)
    global_preferences->sync();
  return ok;
}

bool A2DPSourceSpeaker::is_bonded_(const uint8_t *address) const {
  int count = esp_bt_gap_get_bond_device_num();
  if (count <= 0)
    return false;
  auto bonds = std::unique_ptr<esp_bd_addr_t[]>(new (std::nothrow) esp_bd_addr_t[count]{});
  return bonds != nullptr && esp_bt_gap_get_bond_device_list(&count, bonds.get()) == ESP_OK &&
         std::any_of(bonds.get(), bonds.get() + count,
                     [address](const esp_bd_addr_t &bond) { return std::memcmp(bond, address, 6) == 0; });
}

bool A2DPSourceSpeaker::bonded() const {
  return this->has_address_() && esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_ENABLED &&
         this->is_bonded_(this->address_);
}

int A2DPSourceSpeaker::bond_count() const { return esp_bt_gap_get_bond_device_num(); }

std::string A2DPSourceSpeaker::bond_list(uint16_t page) const {
  int count = esp_bt_gap_get_bond_device_num();
  if (count < 0)
    return "Bluetooth unavailable";
  if (count == 0)
    return "none";
  auto bonds = std::unique_ptr<esp_bd_addr_t[]>(new (std::nothrow) esp_bd_addr_t[count]{});
  if (bonds == nullptr)
    return "out of memory";
  if (esp_bt_gap_get_bond_device_list(&count, bonds.get()) != ESP_OK)
    return "read failed";
  const size_t first = static_cast<size_t>(page) * 10;
  if (first >= static_cast<size_t>(count))
    return "none";
  std::string result;
  for (size_t i = first; i < static_cast<size_t>(count) && i < first + 10; i++) {
    if (i != first)
      result += ", ";
    result += format_address(bonds[i]);
  }
  return result;
}

bool A2DPSourceSpeaker::clear_address() {
  if (this->requested_.load() || this->connected_.load() || this->connect_pending_.load() || this->pairing_.load() ||
      this->remove_bond_pending_.load() || (owner_speaker != nullptr && owner_speaker->forget_pending_.load()))
    return false;
  std::memset(this->address_, 0, sizeof(this->address_));
  this->pairing_status_ = PairingStatus::IDLE;
  return this->save_address_();
}

bool A2DPSourceSpeaker::unpair() {
  if (!this->has_address_() || this->requested_.load() || this->connected_.load() || this->connect_pending_.load() ||
      this->pairing_.load() || this->remove_bond_pending_.load() ||
      (owner_speaker != nullptr && owner_speaker->forget_pending_.load()) ||
      esp_bluedroid_get_status() != ESP_BLUEDROID_STATUS_ENABLED)
    return false;
  if (!this->is_bonded_(this->address_))
    return this->clear_address();
  this->remove_bond_result_.store(0);
  this->remove_bond_pending_.store(true);
  this->remove_bond_deadline_ms_ = millis() + 10000;
  if (esp_bt_gap_remove_bond_device(this->address_) != ESP_OK) {
    this->remove_bond_pending_.store(false);
    return false;
  }
  return true;
}

bool A2DPSourceSpeaker::select_bond(const char *address) {
  uint8_t parsed[6];
  if (!parse_address(address, parsed) || !this->slot_ || this->requested_.load() || this->connected_.load() ||
      this->connect_pending_.load() || this->pairing_.load() || this->remove_bond_pending_.load() ||
      (owner_speaker != nullptr && owner_speaker->forget_pending_.load()) || !this->is_bonded_(parsed))
    return false;
  for (auto *other : registered_speakers) {
    if (other != nullptr && other != this && other->has_address_() &&
        std::memcmp(other->address_, parsed, sizeof(parsed)) == 0) {
      ESP_LOGW(TAG, "Speaker is already assigned to another slot");
      return false;
    }
  }
  if (std::memcmp(this->address_, parsed, sizeof(parsed)) == 0)
    return true;
  uint8_t previous[6];
  std::memcpy(previous, this->address_, sizeof(previous));
  if (!this->set_address(address, true)) {
    std::memcpy(this->address_, previous, sizeof(previous));
    return false;
  }
  this->pairing_status_ = PairingStatus::PAIRED;
  ESP_LOGI(TAG, "Selected bonded speaker %s for slot %u", format_address(parsed).c_str(), this->slot_);
  return true;
}

const char *A2DPSourceSpeaker::forget_status() const {
  switch (this->forget_status_.load()) {
    case ForgetStatus::PENDING:
      return "pending";
    case ForgetStatus::REMOVED:
      return "removed";
    case ForgetStatus::FAILED:
      return "failed";
    default:
      return "idle";
  }
}

bool A2DPSourceSpeaker::forget_bond(const char *address) {
  uint8_t parsed[6];
  if (this != owner_speaker || this->forget_pending_.load())
    return false;
  if (!parse_address(address, parsed) || esp_bluedroid_get_status() != ESP_BLUEDROID_STATUS_ENABLED) {
    this->forget_status_ = ForgetStatus::FAILED;
    return false;
  }
  bool assigned = false;
  for (auto *speaker : registered_speakers) {
    if (speaker == nullptr)
      continue;
    if (speaker->remove_bond_pending_.load() || speaker->pairing_.load() || speaker->requested_.load() ||
        speaker->connected_.load() || speaker->connect_pending_.load()) {
      this->forget_status_ = ForgetStatus::FAILED;
      return false;
    }
    if (speaker->has_address_() && std::memcmp(speaker->address_, parsed, 6) == 0) {
      assigned = true;
    }
  }
  if (!this->is_bonded_(parsed)) {
    if (!assigned) {
      this->forget_status_ = ForgetStatus::FAILED;
      return false;
    }
    bool cleared = true;
    for (auto *speaker : registered_speakers) {
      if (speaker != nullptr && std::memcmp(speaker->address_, parsed, 6) == 0)
        cleared = speaker->clear_address() && cleared;
    }
    this->forget_status_ = cleared ? ForgetStatus::REMOVED : ForgetStatus::FAILED;
    return cleared;
  }
  std::memcpy(this->forget_address_, parsed, sizeof(parsed));
  this->forget_result_.store(0);
  this->forget_deadline_ms_ = millis() + 10000;
  this->forget_status_ = ForgetStatus::PENDING;
  this->forget_pending_.store(true);
  if (esp_bt_gap_remove_bond_device(parsed) != ESP_OK) {
    this->forget_pending_.store(false);
    this->forget_status_ = ForgetStatus::FAILED;
    return false;
  }
  return true;
}

bool A2DPSourceSpeaker::pair(const char *address, const char *pin, bool require_discovered) {
  if (this->requested_.load() || this->connected_.load() || this->pairing_.load() ||
      this->remove_bond_pending_.load() || (owner_speaker != nullptr && owner_speaker->forget_pending_.load()) ||
      classic_discovery_running.load() || !classic_profile_ready.load() || address == nullptr || pin == nullptr)
    return false;
  size_t pin_length = std::strlen(pin);
  if (pin_length > 16 || std::any_of(pin, pin + pin_length, [](char c) { return c < '0' || c > '9'; }))
    return false;
  std::memcpy(this->previous_address_, this->address_, 6);
  if (!this->set_address(address))
    return false;
  for (auto *other : registered_speakers) {
    if (other != nullptr && other != this && other->has_address_() &&
        std::memcmp(other->address_, this->address_, 6) == 0) {
      std::memcpy(this->address_, this->previous_address_, 6);
      ESP_LOGW(TAG, "Speaker is already assigned to another slot");
      return false;
    }
  }
  auto *current = active_speaker.load();
  if (current && current != this &&
      (current->requested_.load() || current->pairing_.load() || current->connected_.load() ||
       current->connect_pending_)) {
    std::memcpy(this->address_, this->previous_address_, 6);
    return false;
  }
  if (this->is_bonded_(this->address_)) {
    this->pairing_status_ = this->save_address_() ? PairingStatus::PAIRED : PairingStatus::FAILED;
    return this->pairing_status_.load() == PairingStatus::PAIRED;
  }
  bool discovered_address = false;
  portENTER_CRITICAL(&discovery_mux);
  for (size_t i = 0; i < discovered_count; i++) {
    if (std::memcmp(discovered[i].address, this->address_, 6) == 0) {
      discovered_address = true;
      break;
    }
  }
  portEXIT_CRITICAL(&discovery_mux);
  if (require_discovered && !discovered_address) {
    std::memcpy(this->address_, this->previous_address_, 6);
    ESP_LOGW(TAG, "Pairing requires an address from the latest Classic scan");
    return false;
  }
  std::memset(this->pin_, 0, sizeof(this->pin_));
  std::memcpy(this->pin_, pin, pin_length);
  this->pair_authenticated_.store(false);
  this->confirmation_pending_.store(false);
  this->pair_deadline_ms_ = millis() + 30000;
  this->next_connect_attempt_ms_ = 0;
  this->scanner_wait_started_ms_ = 0;
  this->scanner_stop_starting_requested_ = false;
  this->connect_attempts_.store(0);
  this->connection_session_active_.store(false);
  this->pairing_status_ = PairingStatus::CONNECTING;
  this->pairing_.store(true);
  desired_speaker.store(this);
  return true;
}

bool A2DPSourceSpeaker::confirm_pairing(bool approve) {
  if (!this->pairing_.load() || !this->confirmation_pending_.exchange(false))
    return false;
  esp_err_t err = esp_bt_gap_ssp_confirm_reply(this->confirmation_address_, approve);
  if (err != ESP_OK)
    return false;
  this->pairing_status_ = approve ? PairingStatus::WAITING_FOR_AUTH : PairingStatus::FAILED;
  if (!approve)
    this->pairing_.store(false);
  return true;
}

std::string A2DPSourceSpeaker::confirmation_prompt() const {
  if (!this->confirmation_pending_.load())
    return "none";
  char number[7];
  std::snprintf(number, sizeof(number), "%06" PRIu32, this->confirmation_number_);
  return format_address(this->confirmation_address_) + " code " + number;
}

const char *A2DPSourceSpeaker::pairing_status() const {
  switch (this->pairing_status_.load()) {
    case PairingStatus::CONNECTING:
      return "connecting";
    case PairingStatus::WAITING_FOR_AUTH:
      return "authenticating";
    case PairingStatus::WAITING_CONFIRM:
      return "confirm code in HA";
    case PairingStatus::PAIRED:
      return "paired";
    case PairingStatus::FAILED:
      return "failed";
    default:
      return "idle";
  }
}

bool A2DPSourceSpeaker::start_discovery() {
  auto *current = active_speaker.load();
  if (classic_discovery_running.load() || !classic_profile_ready.load() || desired_speaker.load() ||
      (current && (current->requested_.load() || current->connected_.load() || current->pairing_.load() ||
                   current->connect_pending_)))
    return false;
  portENTER_CRITICAL(&discovery_mux);
  discovered_count = 0;
  portEXIT_CRITICAL(&discovery_mux);
  esp_err_t err = esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, 6, MAX_DISCOVERED);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Bluetooth Classic discovery failed: %s", esp_err_to_name(err));
    return false;
  }
  classic_discovery_running.store(true);
  return true;
}

void A2DPSourceSpeaker::cancel_discovery() {
  if (classic_discovery_running.load())
    esp_bt_gap_cancel_discovery();
}

bool A2DPSourceSpeaker::discovery_running() const { return classic_discovery_running.load(); }

size_t A2DPSourceSpeaker::discovery_count() const {
  portENTER_CRITICAL(&discovery_mux);
  const size_t count = discovered_count;
  portEXIT_CRITICAL(&discovery_mux);
  return count;
}

std::string A2DPSourceSpeaker::discovery_device(size_t index) const {
  DiscoveredDevice device{};
  portENTER_CRITICAL(&discovery_mux);
  const bool found = index < discovered_count;
  if (found)
    device = discovered[index];
  portEXIT_CRITICAL(&discovery_mux);
  if (!found)
    return "null";

  std::string result = "{\"address\":\"" + format_address(device.address) + "\",\"name\":";
  result += device.name[0] ? quote_json_string(device.name, strnlen(device.name, sizeof(device.name))) : "null";
  result += ",\"rssi_dbm\":";
  result += device.rssi == -128 ? "null" : std::to_string(device.rssi);
  result += ",\"bonded_to_gateway\":";
  result += this->is_bonded_(device.address) ? "true" : "false";
  result += '}';
  return result;
}

std::string A2DPSourceSpeaker::discovery_results(uint8_t page) const {
  DiscoveredDevice snapshot[MAX_DISCOVERED]{};
  size_t count;
  portENTER_CRITICAL(&discovery_mux);
  count = discovered_count;
  std::memcpy(snapshot, discovered, count * sizeof(DiscoveredDevice));
  portEXIT_CRITICAL(&discovery_mux);
  const size_t first = static_cast<size_t>(page) * 3;
  if (count <= first)
    return classic_discovery_running.load() ? "scanning" : "none";
  std::string result;
  for (size_t i = first; i < count && i < first + 3; i++) {
    if (i != first)
      result += "; ";
    result += format_address(snapshot[i].address);
    if (snapshot[i].name[0]) {
      result += " ";
      result.append(snapshot[i].name, strnlen(snapshot[i].name, 24));
    }
    result += snapshot[i].rssi == -128 ? " (RSSI unavailable)"
                                       : " (" + std::to_string(snapshot[i].rssi) + " dBm)";
    result += this->is_bonded_(snapshot[i].address) ? " [paired]" : " [not paired]";
  }
  return result;
}

void A2DPSourceSpeaker::reset_connection_state_() {
  this->connected_.store(false);
  this->streaming_.store(false);
  this->connect_pending_.store(false);
  this->connection_failed_.store(false);
  this->connection_failed_latched_.store(false);
  this->connect_attempts_.store(0);
  this->connection_session_active_.store(false);
  this->disconnect_pending_.store(false);
  this->acl_connected_.store(false);
  this->media_check_pending_.store(false);
  this->media_start_pending_.store(false);
  this->avrcp_connected_.store(false);
  this->avrcp_ready_.store(false);
  this->avrcp_actual_volume_.store(-1);
  this->avrcp_response_volume_.store(-1);
  this->avrcp_command_pending_ = false;
  this->avrcp_connect_deadline_ms_ = 0;
  this->avrcp_last_requested_ = 255;
  this->next_connect_attempt_ms_ = 0;
  this->remove_bond_pending_.store(false);
  this->remove_bond_result_.store(0);
  if (this->forget_pending_.exchange(false))
    this->forget_status_ = ForgetStatus::FAILED;
  this->forget_result_.store(0);
  if (this->pairing_.exchange(false)) {
    this->confirmation_pending_.store(false);
    this->pairing_status_ = PairingStatus::FAILED;
    std::memset(this->pin_, 0, sizeof(this->pin_));
    std::memcpy(this->address_, this->previous_address_, 6);
  }
}

void A2DPSourceSpeaker::abort_connection_(const char *reason) {
  ESP_LOGW(TAG, "A2DP slot %u: %s; stopping this playback attempt", this->slot_, reason);
  this->connection_failed_latched_.store(true);
  this->requested_.store(false);
  this->connect_pending_.store(false);
  if (this->pairing_.exchange(false))
    this->pairing_status_ = PairingStatus::FAILED;
  this->finish_pending_.store(false);
  this->avrcp_ready_.store(false);
  this->preempted_.store(true);
  this->state_ = speaker::STATE_STOPPED;
  this->next_connect_attempt_ms_ = millis() + 60000;
  esp_a2d_source_disconnect(this->address_);
}

void A2DPSourceSpeaker::retry_or_abort_connection_(const char *reason) {
  if (this->connection_failed_latched_.load()) {
    this->connect_pending_.store(false);
    return;
  }
  if (!this->connection_session_active_.load() && !this->pairing_.load()) {
    this->connect_pending_.store(false);
    return;
  }
  this->connection_failure_count_.fetch_add(1);
  if ((this->pairing_.load() && !this->connection_session_active_.load()) || this->connect_attempts_.load() >= 4) {
    this->abort_connection_(reason);
    return;
  }
  ESP_LOGW(TAG, "A2DP slot %u: %s; retry %u/4 after 12 s", this->slot_, reason,
           static_cast<unsigned>(this->connect_attempts_.load() + 1));
  this->connect_pending_.store(false);
  this->next_connect_attempt_ms_ = millis() + 12000;
  esp_a2d_source_disconnect(this->address_);
}

bool A2DPSourceSpeaker::waiting_for_retry() const {
  const int32_t remaining = static_cast<int32_t>(this->next_connect_attempt_ms_.load() - millis());
  return (this->connection_session_active_.load() || this->pairing_.load()) && !this->connected_.load() &&
         !this->connect_pending_.load() && remaining > 2000 && remaining <= 12000;
}

void A2DPSourceSpeaker::reset_connection_sequence() {
  if (this->pairing_.load())
    return;
  this->connection_session_active_.store(false);
  this->requested_.store(false);
  this->connect_attempts_.store(0);
  this->next_connect_attempt_ms_.store(0);
  this->scanner_wait_started_ms_ = 0;
  this->scanner_stop_starting_requested_ = false;
  this->connection_failed_.store(false);
  this->connection_failed_latched_.store(false);
  this->preempted_.store(false);
}

void A2DPSourceSpeaker::setup() {
  this->audio_stream_info_ = audio::AudioStreamInfo(16, 2, 44100);
  this->volume_ = 0.02f;
  this->avrcp_target_volume_.store(3);
  if (this->slot_) {
    this->address_pref_ = global_preferences->make_preference<uint8_t[8]>(0xA2D00000UL + this->slot_, true);
    uint8_t saved[8]{};
    if (this->address_pref_.load(&saved) && saved[0] == 0xA2 && saved[1] == 0xD0) {
      std::memcpy(this->address_, saved + 2, sizeof(this->address_));
      this->restored_from_flash_ = true;
    }
    registered_speakers[this->slot_ - 1] = this;
  }
  if (owner_speaker == nullptr) {
    owner_speaker = this;
    active_speaker.store(this);
  }
}

void A2DPSourceSpeaker::loop() {
  if (!esp32_ble::global_ble->is_active() || esp_bluedroid_get_status() != ESP_BLUEDROID_STATUS_ENABLED) {
    this->reset_connection_state_();
    this->profile_ready_.store(false);
    if (this == owner_speaker) {
      profile_initialized = false;
      profile_initialization_failed = false;
      classic_profile_ready.store(false);
      classic_discovery_running.store(false);
    }
    return;
  }

  if (this == owner_speaker && profile_initialization_failed)
    return;

  if (this == owner_speaker && !profile_initialized) {
    esp_err_t err = esp_bt_gap_register_callback(A2DPSourceSpeaker::gap_event_);
    if (err == ESP_OK) {
      uint8_t iocap = ESP_BT_IO_CAP_IO;
      err = esp_bt_gap_set_security_param(ESP_BT_SP_IOCAP_MODE, &iocap, sizeof(iocap));
    }
    if (err == ESP_OK) {
      esp_bt_pin_code_t pin_code{};
      err = esp_bt_gap_set_pin(ESP_BT_PIN_TYPE_VARIABLE, 0, pin_code);
    }
    if (err == ESP_OK) {
      const esp_err_t page_err = esp_bt_gap_set_page_timeout(0x1000);  // 2.56 s per attempt.
      if (page_err != ESP_OK)
        ESP_LOGW(TAG, "Could not shorten Classic page timeout: %s", esp_err_to_name(page_err));
    }
    if (err == ESP_OK)
      err = esp_avrc_ct_init();
    if (err == ESP_OK)
      err = esp_avrc_ct_register_callback(A2DPSourceSpeaker::avrcp_event_);
    if (err == ESP_OK)
      err = esp_a2d_source_init();
    if (err == ESP_OK)
      err = esp_a2d_register_callback(A2DPSourceSpeaker::a2dp_event_);
    if (err == ESP_OK)
      err = esp_a2d_source_register_data_callback(A2DPSourceSpeaker::audio_data_);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "A2DP init failed: %s", esp_err_to_name(err));
      profile_initialization_failed = true;
      this->mark_failed();
      return;
    }
    profile_initialized = true;
    ESP_LOGI(TAG, "A2DP source initialized");
  }

  if (this == owner_speaker && !classic_profile_ready.load()) {
    esp_a2d_profile_status_t status{};
    if (esp_a2d_get_profile_status(&status) == ESP_OK && status.a2d_src_inited)
      classic_profile_ready.store(true);
  }
  this->profile_ready_.store(classic_profile_ready.load());
  if (!classic_profile_ready.load())
    return;

  if (this == owner_speaker && this->forget_pending_.load()) {
    const uint8_t result = this->forget_result_.exchange(0);
    if (result != 0) {
      this->forget_pending_.store(false);
      bool cleared = result == 1;
      if (cleared) {
        for (auto *speaker : registered_speakers) {
          if (speaker != nullptr && std::memcmp(speaker->address_, this->forget_address_, 6) == 0)
            cleared = speaker->clear_address() && cleared;
        }
      }
      this->forget_status_ = cleared ? ForgetStatus::REMOVED : ForgetStatus::FAILED;
      ESP_LOGI(TAG, "Forgetting Bluetooth speaker %s: %s", format_address(this->forget_address_).c_str(),
               this->forget_status());
    } else if (static_cast<int32_t>(millis() - this->forget_deadline_ms_) >= 0) {
      this->forget_pending_.store(false);
      this->forget_status_ = ForgetStatus::FAILED;
      ESP_LOGW(TAG, "Forgetting Bluetooth speaker timed out");
    }
  }

  if (this->remove_bond_pending_.load()) {
    const uint8_t result = this->remove_bond_result_.exchange(0);
    if (result != 0) {
      this->remove_bond_pending_.store(false);
      if (result == 1 && this->clear_address())
        ESP_LOGI(TAG, "Removed pairing for slot %u", this->slot_);
      else
        ESP_LOGW(TAG, "Could not remove pairing for slot %u", this->slot_);
    } else if (static_cast<int32_t>(millis() - this->remove_bond_deadline_ms_) >= 0) {
      this->remove_bond_pending_.store(false);
      ESP_LOGW(TAG, "Removing pairing timed out for slot %u", this->slot_);
    }
  }

  if (this == owner_speaker) {
    auto *current = active_speaker.load();
    auto *desired = desired_speaker.load();
    if (desired && current == desired) {
      desired_speaker.store(nullptr);
      handoff_started_ms = 0;
    } else if (desired) {
      if (classic_discovery_running.load())
        esp_bt_gap_cancel_discovery();
      // The underlying Classic ACL can outlive both A2DP and AVRCP. Wait for
      // its GAP disconnect event before opening a link to another receiver.
      if (current &&
          (current->connected_.load() || current->avrcp_connected_.load() || current->acl_connected_.load() ||
           current->connect_pending_.load() || current->disconnect_pending_.load())) {
        if (handoff_started_ms == 0)
          handoff_started_ms = millis();
        if (static_cast<int32_t>(millis() - handoff_started_ms) >= 10000) {
          ESP_LOGW(TAG, "A2DP/AVRCP handoff timed out; keeping the next output disconnected");
          desired->requested_.store(false);
          desired->preempted_.store(true);
          desired->state_ = speaker::STATE_STOPPED;
          desired_speaker.store(nullptr);
          handoff_started_ms = 0;
        } else if (current->connected_.load() && !current->disconnect_pending_) {
          if (current->requested_.load())
            current->preempted_.store(true);
          current->requested_.store(false);
          current->state_ = speaker::STATE_STOPPED;
          if (current->streaming_.load())
            esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_SUSPEND);
          if (esp_a2d_source_disconnect(current->address_) == ESP_OK)
            current->disconnect_pending_ = true;
        }
      } else if (!current || !current->connect_pending_) {
        if (current && current->requested_.load())
          current->preempted_.store(true);
        if (handoff_started_ms != 0) {
          // Let the shared Wi-Fi/Bluetooth radio settle after the old A2DP
          // and AVRCP links close before paging the next receiver.
          const uint32_t earliest_connect_ms = millis() + 5000;
          if (static_cast<int32_t>(desired->next_connect_attempt_ms_ - earliest_connect_ms) < 0)
            desired->next_connect_attempt_ms_ = earliest_connect_ms;
          ESP_LOGI(TAG, "A2DP handoff: waiting 5 s before connecting slot %u", desired->slot_);
        }
        active_speaker.store(desired);
        desired_speaker.store(nullptr);
        handoff_started_ms = 0;
      }
    } else {
      handoff_started_ms = 0;
    }
  }

  if (active_speaker.load() != this)
    return;

  // The PCM ring buffer accepts audio while the previous receiver disconnects
  // and this receiver connects. Keep the upstream resampler running during
  // that handoff; streaming_ separately reports when Bluetooth audio starts.
  if (this->requested_.load())
    this->state_ = speaker::STATE_RUNNING;

  const uint32_t now = millis();
  if (this->connection_failed_.exchange(false) && (this->connection_session_active_.load() || this->pairing_.load()))
    this->retry_or_abort_connection_("Classic connection failed");
  if (this->requested_.load() && this->connected_.load()) {
    if (this->avrcp_connect_deadline_ms_ == 0)
      this->avrcp_connect_deadline_ms_ = now + 15000;
    if (this->avrcp_connected_.load()) {
      int response = this->avrcp_response_volume_.exchange(-1);
      if (response >= 0 && this->avrcp_command_pending_) {
        this->avrcp_actual_volume_.store(response);
        this->avrcp_ready_.store(true);
        this->avrcp_command_pending_ = false;
        this->avrcp_attempts_ = 0;
        ESP_LOGI(TAG, "AVRCP slot %u accepted receiver volume %d/127", this->slot_, response);
      }
      if (this->avrcp_command_pending_ && static_cast<int32_t>(now - this->avrcp_command_deadline_ms_) >= 0) {
        this->avrcp_command_pending_ = false;
        if (this->avrcp_attempts_ >= 3)
          this->avrcp_failed_.store(true);
      }
      const uint8_t target = this->avrcp_target_volume_.load();
      if (!this->avrcp_command_pending_ && !this->avrcp_failed_.load() &&
          (!this->avrcp_ready_.load() || target != this->avrcp_last_requested_)) {
        if (this->avrcp_last_requested_ == 255)
          this->avrcp_attempts_ = 0;
        esp_err_t err = esp_avrc_ct_send_set_absolute_volume_cmd(this->avrcp_transaction_++ & 0x0F, target);
        if (err == ESP_OK) {
          this->avrcp_last_requested_ = target;
          this->avrcp_command_pending_ = true;
          this->avrcp_command_deadline_ms_ = now + 1500;
          this->avrcp_attempts_++;
        } else {
          ESP_LOGW(TAG, "AVRCP volume command failed for slot %u: %s", this->slot_, esp_err_to_name(err));
          this->avrcp_failed_.store(true);
        }
      }
    }
    if (!this->avrcp_ready_.load() && !this->avrcp_failed_.load() &&
        static_cast<int32_t>(now - this->avrcp_connect_deadline_ms_) >= 0)
      this->avrcp_failed_.store(true);
    if (this->avrcp_failed_.load() && !this->avrcp_ready_.load()) {
      ESP_LOGW(TAG, "AVRCP receiver volume unavailable for slot %u; stopping audio", this->slot_);
      this->stop();
    }
  }
  if (this->pairing_ && static_cast<int32_t>(now - this->pair_deadline_ms_) >= 0) {
    if (this->confirmation_pending_.exchange(false))
      esp_bt_gap_ssp_confirm_reply(this->confirmation_address_, false);
    this->pairing_.store(false);
    this->pairing_status_ = PairingStatus::FAILED;
    std::memset(this->pin_, 0, sizeof(this->pin_));
    ESP_LOGW(TAG, "Pairing timed out for slot %u", this->slot_);
  }
  if (this->pairing_.load() && this->connected_.load()) {
    if (this->pair_authenticated_.load() || this->is_bonded_(this->address_)) {
      this->pairing_.store(false);
      this->pairing_status_ = this->save_address_() ? PairingStatus::PAIRED : PairingStatus::FAILED;
      std::memset(this->pin_, 0, sizeof(this->pin_));
      ESP_LOGI(TAG, "Pairing finished for slot %u", this->slot_);
    } else if (!this->confirmation_pending_.load()) {
      this->pairing_status_ = PairingStatus::WAITING_FOR_AUTH;
    }
  }
  if (this->pairing_status_ == PairingStatus::FAILED && !this->connected_.load() && !this->connect_pending_) {
    std::memcpy(this->address_, this->previous_address_, 6);
  }
  if (this->connect_pending_ && static_cast<int32_t>(now - this->next_connect_attempt_ms_) >= 0) {
    this->retry_or_abort_connection_("connection timed out");
  }
  if (this->media_check_pending_ && static_cast<int32_t>(now - this->next_media_check_ms_) >= 0)
    this->media_check_pending_ = false;
  if (this->media_start_pending_ && static_cast<int32_t>(now - this->next_media_check_ms_) >= 0)
    this->media_start_pending_ = false;
  if (this->requested_.load() || this->pairing_) {
    if (!this->connected_.load() && !this->connect_pending_ &&
        static_cast<int32_t>(now - this->next_connect_attempt_ms_) >= 0) {
#ifdef USE_ESP32_BLE_TRACKER
      // stop_scan() is asynchronous. In particular, a stop requested while a
      // scan is STARTING leaves the scanner running after its start callback.
      // Wait for IDLE before paging a Classic receiver on the shared radio.
      auto *tracker = esp32_ble_tracker::global_esp32_ble_tracker;
      if (tracker != nullptr) {
        const auto scanner_state = tracker->get_scanner_state();
        if (scanner_state != esp32_ble_tracker::ScannerState::STARTING)
          this->scanner_stop_starting_requested_ = false;
        // Also disable an automatic restart when the scanner is IDLE.
        if (scanner_state == esp32_ble_tracker::ScannerState::IDLE ||
            scanner_state == esp32_ble_tracker::ScannerState::RUNNING ||
            scanner_state == esp32_ble_tracker::ScannerState::FAILED) {
          tracker->stop_scan();
        } else if (scanner_state == esp32_ble_tracker::ScannerState::STARTING &&
                   !this->scanner_stop_starting_requested_) {
          tracker->stop_scan();
          this->scanner_stop_starting_requested_ = true;
        }
        if (scanner_state != esp32_ble_tracker::ScannerState::IDLE) {
          if (this->scanner_wait_started_ms_ == 0)
            this->scanner_wait_started_ms_ = now;
          if (static_cast<int32_t>(now - this->scanner_wait_started_ms_) >= 5000)
            this->abort_connection_("BLE scan did not stop");
          return;
        }
        if (this->scanner_wait_started_ms_ != 0) {
          ESP_LOGI(TAG, "A2DP slot %u: BLE scan stopped after %u ms", this->slot_,
                   static_cast<unsigned>(now - this->scanner_wait_started_ms_));
          this->scanner_wait_started_ms_ = 0;
        }
      }
#endif
      this->connect_attempts_.fetch_add(1);
      esp_err_t err = esp_a2d_source_connect(this->address_);
      this->next_connect_attempt_ms_ = now + 15000;
      if (err == ESP_OK)
        this->connect_pending_ = true;
      else {
        ESP_LOGW(TAG, "A2DP connect failed: %s", esp_err_to_name(err));
        this->retry_or_abort_connection_("A2DP connect call failed");
      }
    } else if (this->requested_.load() && this->connected_.load() && this->avrcp_ready_.load() &&
               !this->streaming_.load() && !this->media_check_pending_ && !this->media_start_pending_ &&
               static_cast<int32_t>(now - this->next_media_check_ms_) >= 0) {
      if (esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_CHECK_SRC_RDY) == ESP_OK)
        this->media_check_pending_ = true;
      this->next_media_check_ms_ = now + 1000;
    }
  } else if (this->connected_.load() && !this->disconnect_pending_) {
    if (this->streaming_.load())
      esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_SUSPEND);
    if (esp_a2d_source_disconnect(this->address_) == ESP_OK)
      this->disconnect_pending_ = true;
  }

  if (this->finish_pending_ && !this->has_buffered_data())
    this->stop();
}

void A2DPSourceSpeaker::dump_config() {
  ESP_LOGCONFIG(TAG, "A2DP Source Speaker:");
  ESP_LOGCONFIG(TAG, "  Connected: %s", YESNO(this->connected_.load()));
  ESP_LOGCONFIG(TAG, "  Streaming: %s", YESNO(this->streaming_.load()));
  ESP_LOGCONFIG(TAG, "  PCM bytes: %" PRIu32, this->pcm_bytes_.load());
  ESP_LOGCONFIG(TAG, "  Underrun bytes: %" PRIu32, this->underrun_bytes_.load());
}

void A2DPSourceSpeaker::set_volume(float volume) {
  this->volume_ = std::clamp(volume, 0.0f, 1.0f);
  this->avrcp_target_volume_.store(this->muted_.load() ? 0 : static_cast<uint8_t>(this->volume_ * 127.0f + 0.5f));
}

void A2DPSourceSpeaker::set_mute_state(bool mute_state) {
  this->mute_state_ = mute_state;
  this->muted_.store(mute_state);
  this->avrcp_target_volume_.store(mute_state ? 0 : static_cast<uint8_t>(this->volume_ * 127.0f + 0.5f));
}

const char *A2DPSourceSpeaker::avrcp_status() const {
  if (this->avrcp_failed_.load())
    return this->avrcp_ready_.load() ? "volume update failed" : "unavailable";
  if (!this->connected_.load() && this->avrcp_connected_.load())
    return "closing AVRCP";
  if (this->avrcp_command_pending_.load())
    return "setting receiver volume";
  if (this->avrcp_ready_.load())
    return "receiver volume confirmed";
  if (this->avrcp_connected_.load())
    return "setting receiver volume";
  if (this->connected_.load())
    return "waiting for AVRCP";
  return "disconnected";
}

void A2DPSourceSpeaker::start() {
  if (this->preempted_.load() || this->connection_failed_latched_.load()) {
    this->state_ = speaker::STATE_STOPPED;
    return;
  }
  if (!this->has_address_()) {
    ESP_LOGW(TAG, "Speaker address is not configured");
    this->state_ = speaker::STATE_STOPPED;
    return;
  }
  auto *desired = desired_speaker.load();
  if (desired && desired != this) {
    this->state_ = speaker::STATE_STOPPED;
    return;
  }
  auto *current = active_speaker.load();
  if (current && current != this && current->pairing_.load()) {
    this->state_ = speaker::STATE_STOPPED;
    return;
  }
  if (classic_discovery_running.load())
    esp_bt_gap_cancel_discovery();
  if (!this->pcm_buffer_) {
    this->pcm_buffer_ = ring_buffer::RingBuffer::create(PCM_BUFFER_BYTES);
    if (!this->pcm_buffer_) {
      ESP_LOGE(TAG, "Unable to allocate PCM buffer");
      return;
    }
  }
  this->pcm_buffer_->reset();
  this->finish_pending_ = false;
  this->received_first_data_.store(false);
  this->previous_callback_us_ = 0;
  this->requested_.store(true);
  this->connection_session_active_.store(true);
  this->avrcp_failed_.store(false);
  this->avrcp_ready_.store(false);
  this->avrcp_connect_deadline_ms_ = 0;
  this->avrcp_command_pending_ = false;
  this->avrcp_last_requested_ = 255;
  this->avrcp_attempts_ = 0;
  this->scanner_wait_started_ms_ = 0;
  this->scanner_stop_starting_requested_ = false;
  // Buffer PCM immediately, including while Classic paging is still pending.
  this->state_ = speaker::STATE_RUNNING;
  desired_speaker.store(this);
}

void A2DPSourceSpeaker::stop() {
  ESP_LOGD(TAG, "A2DP slot %u stop requested after %" PRIu32 " PCM bytes", this->slot_, this->pcm_bytes_.load());
  this->requested_.store(false);
  this->finish_pending_ = false;
  this->state_ = speaker::STATE_STOPPED;
  this->preempted_.store(false);
}

void A2DPSourceSpeaker::finish() {
  ESP_LOGD(TAG, "A2DP slot %u finish requested after %" PRIu32 " PCM bytes", this->slot_, this->pcm_bytes_.load());
  this->finish_pending_ = true;
}

bool A2DPSourceSpeaker::has_buffered_data() const { return this->pcm_buffer_ && this->pcm_buffer_->available() != 0; }

size_t A2DPSourceSpeaker::play(const uint8_t *data, size_t length, TickType_t ticks_to_wait) {
  if (!this->requested_.load())
    this->start();
  if (!this->requested_.load() || !this->pcm_buffer_)
    return 0;
  size_t written = this->pcm_buffer_->write_without_replacement(data, length, ticks_to_wait);
  if (written)
    this->received_first_data_.store(true);
  return written;
}

void A2DPSourceSpeaker::a2dp_event_(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param) {
  if (auto *speaker = active_speaker.load())
    speaker->process_event_(event, param);
}

void A2DPSourceSpeaker::avrcp_event_(esp_avrc_ct_cb_event_t event, esp_avrc_ct_cb_param_t *param) {
  if (auto *speaker = active_speaker.load())
    speaker->process_avrcp_event_(event, param);
}

void A2DPSourceSpeaker::process_avrcp_event_(esp_avrc_ct_cb_event_t event, esp_avrc_ct_cb_param_t *param) {
  if (event == ESP_AVRC_CT_CONNECTION_STATE_EVT) {
    if (std::memcmp(param->conn_stat.remote_bda, this->address_, 6) != 0)
      return;
    bool connected = param->conn_stat.connected;
    this->avrcp_connected_.store(connected);
    this->avrcp_ready_.store(false);
    this->avrcp_response_volume_.store(-1);
    this->avrcp_actual_volume_.store(-1);
    this->avrcp_last_requested_ = 255;
    this->avrcp_command_pending_ = false;
    ESP_LOGI(TAG, "AVRCP slot %u %s", this->slot_, connected ? "connected" : "disconnected");
  } else if (event == ESP_AVRC_CT_REMOTE_FEATURES_EVT) {
    if (std::memcmp(param->rmt_feats.remote_bda, this->address_, 6) == 0)
      ESP_LOGI(TAG, "AVRCP slot %u remote features 0x%" PRIx32, this->slot_, param->rmt_feats.feat_mask);
  } else if (event == ESP_AVRC_CT_SET_ABSOLUTE_VOLUME_RSP_EVT && this->avrcp_connected_.load()) {
    this->avrcp_response_volume_.store(param->set_volume_rsp.volume);
  }
}

void A2DPSourceSpeaker::gap_event_(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param) {
  if (event == ESP_BT_GAP_REMOVE_BOND_DEV_COMPLETE_EVT) {
    if (owner_speaker != nullptr && owner_speaker->forget_pending_.load() &&
        std::memcmp(owner_speaker->forget_address_, param->remove_bond_dev_cmpl.bda, 6) == 0)
      owner_speaker->forget_result_.store(param->remove_bond_dev_cmpl.status == ESP_BT_STATUS_SUCCESS ? 1 : 2);
    for (auto *speaker : registered_speakers) {
      if (speaker != nullptr && speaker->remove_bond_pending_.load() &&
          std::memcmp(speaker->address_, param->remove_bond_dev_cmpl.bda, 6) == 0) {
        speaker->remove_bond_result_.store(param->remove_bond_dev_cmpl.status == ESP_BT_STATUS_SUCCESS ? 1 : 2);
        break;
      }
    }
  } else if (event == ESP_BT_GAP_DISC_STATE_CHANGED_EVT) {
    classic_discovery_running.store(param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STARTED);
  } else if (event == ESP_BT_GAP_DISC_RES_EVT) {
    const auto &result = param->disc_res;
    DiscoveredDevice device{};
    std::memcpy(device.address, result.bda, 6);
    for (int i = 0; i < result.num_prop; i++) {
      const auto &prop = result.prop[i];
      if (prop.type == ESP_BT_GAP_DEV_PROP_BDNAME && prop.val != nullptr && prop.len > 0) {
        size_t length = std::min<size_t>(prop.len, sizeof(device.name) - 1);
        std::memcpy(device.name, prop.val, length);
        device.name[length] = '\0';
      } else if (prop.type == ESP_BT_GAP_DEV_PROP_RSSI && prop.val != nullptr && prop.len >= 1) {
        device.rssi = *static_cast<int8_t *>(prop.val);
      } else if (prop.type == ESP_BT_GAP_DEV_PROP_EIR && prop.val != nullptr) {
        uint8_t length = 0;
        auto *name =
            esp_bt_gap_resolve_eir_data(static_cast<uint8_t *>(prop.val), ESP_BT_EIR_TYPE_CMPL_LOCAL_NAME, &length);
        if (name == nullptr)
          name =
              esp_bt_gap_resolve_eir_data(static_cast<uint8_t *>(prop.val), ESP_BT_EIR_TYPE_SHORT_LOCAL_NAME, &length);
        if (name != nullptr && length > 0) {
          size_t copy_length = std::min<size_t>(length, sizeof(device.name) - 1);
          std::memcpy(device.name, name, copy_length);
          device.name[copy_length] = '\0';
        }
      }
    }
    portENTER_CRITICAL(&discovery_mux);
    size_t index = 0;
    while (index < discovered_count && std::memcmp(discovered[index].address, device.address, 6) != 0)
      index++;
    if (index < MAX_DISCOVERED) {
      if (index == discovered_count)
        discovered_count++;
      if (!device.name[0] && discovered[index].name[0])
        std::memcpy(device.name, discovered[index].name, sizeof(device.name));
      // Inquiry may report the same address again without an RSSI property.
      if (device.rssi == -128 && discovered[index].rssi != -128)
        device.rssi = discovered[index].rssi;
      discovered[index] = device;
    }
    portEXIT_CRITICAL(&discovery_mux);
  }
  if (auto *speaker = active_speaker.load())
    speaker->process_gap_event_(event, param);
}

void A2DPSourceSpeaker::process_gap_event_(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param) {
  if (event == ESP_BT_GAP_ACL_CONN_CMPL_STAT_EVT) {
    if (std::memcmp(param->acl_conn_cmpl_stat.bda, this->address_, 6) != 0)
      return;
    const bool connected = param->acl_conn_cmpl_stat.stat == ESP_BT_STATUS_SUCCESS;
    this->acl_connected_.store(connected);
    if (!connected && this->connect_pending_.load())
      this->connection_failed_.store(true);
    ESP_LOGI(TAG, "Classic ACL slot %u connection %s (status 0x%02x, handle 0x%04x)", this->slot_,
             connected ? "open" : "failed", param->acl_conn_cmpl_stat.stat, param->acl_conn_cmpl_stat.handle);
  } else if (event == ESP_BT_GAP_ACL_DISCONN_CMPL_STAT_EVT) {
    if (std::memcmp(param->acl_disconn_cmpl_stat.bda, this->address_, 6) != 0)
      return;
    this->acl_connected_.store(false);
    ESP_LOGI(TAG, "Classic ACL slot %u closed (handle 0x%04x, reason 0x%02x)", this->slot_,
             param->acl_disconn_cmpl_stat.handle, param->acl_disconn_cmpl_stat.reason);
  } else if (event == ESP_BT_GAP_AUTH_CMPL_EVT) {
    if (std::memcmp(param->auth_cmpl.bda, this->address_, 6) != 0)
      return;
    if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS) {
      if (this->pairing_.load())
        this->pair_authenticated_.store(true);
      ESP_LOGI(TAG, "Bluetooth Classic speaker authenticated");
    } else {
      ESP_LOGW(TAG, "Bluetooth Classic authentication failed: %d", param->auth_cmpl.stat);
      if (this->pairing_.load()) {
        this->pairing_.store(false);
        this->pairing_status_ = PairingStatus::FAILED;
      }
    }
  } else if (event == ESP_BT_GAP_CFM_REQ_EVT) {
    bool expected = this->pairing_.load() && std::memcmp(param->cfm_req.bda, this->address_, 6) == 0;
    if (!expected) {
      esp_bt_gap_ssp_confirm_reply(param->cfm_req.bda, false);
    } else {
      std::memcpy(this->confirmation_address_, param->cfm_req.bda, 6);
      this->confirmation_number_ = param->cfm_req.num_val;
      this->pairing_status_ = PairingStatus::WAITING_CONFIRM;
      this->confirmation_pending_.store(true);
    }
  } else if (event == ESP_BT_GAP_PIN_REQ_EVT) {
    bool expected =
        this->pairing_.load() && std::memcmp(param->pin_req.bda, this->address_, 6) == 0 && this->pin_[0] != '\0';
    uint8_t pin_length = expected ? std::strlen(this->pin_) : 0;
    if (param->pin_req.min_16_digit && pin_length != 16)
      expected = false;
    esp_bt_pin_code_t pin{};
    if (expected)
      std::memcpy(pin, this->pin_, pin_length);
    esp_bt_gap_pin_reply(param->pin_req.bda, expected, expected ? pin_length : 0, pin);
  } else if (event == ESP_BT_GAP_KEY_REQ_EVT) {
    bool expected = this->pairing_.load() && std::memcmp(param->key_req.bda, this->address_, 6) == 0 &&
                    std::strlen(this->pin_) == 6;
    esp_bt_gap_ssp_passkey_reply(param->key_req.bda, expected, expected ? std::strtoul(this->pin_, nullptr, 10) : 0);
  }
}

void A2DPSourceSpeaker::process_event_(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param) {
  if (event == ESP_A2D_PROF_STATE_EVT) {
    classic_profile_ready.store(param->a2d_prof_stat.init_state == ESP_A2D_INIT_SUCCESS);
  } else if (event == ESP_A2D_CONNECTION_STATE_EVT) {
    ESP_LOGI(TAG, "A2DP slot %u connection state %d for %s", this->slot_, param->conn_stat.state,
             format_address(param->conn_stat.remote_bda).c_str());
    if (std::memcmp(param->conn_stat.remote_bda, this->address_, 6) != 0)
      return;
    const auto state = param->conn_stat.state;
    if (state == ESP_A2D_CONNECTION_STATE_CONNECTING) {
      this->connect_pending_ = true;
    } else if (state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
      this->connected_ = true;
      this->connect_pending_ = false;
      this->disconnect_pending_ = false;
      this->next_connect_attempt_ms_ = 0;
      this->connect_attempts_.store(0);
    } else if (state == ESP_A2D_CONNECTION_STATE_DISCONNECTING) {
      // Keep the old sink active until A2DP, AVRCP, and Classic ACL close.
      this->disconnect_pending_ = true;
    } else if (state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
      const bool failed_connect = this->connect_pending_.exchange(false);
      const bool dropped_while_requested = this->connected_.exchange(false) && this->requested_.load();
      this->disconnect_pending_ = false;
      if (failed_connect || dropped_while_requested) {
        this->connection_failed_.store(true);
      }
    }
    if (state != ESP_A2D_CONNECTION_STATE_CONNECTED) {
      this->streaming_.store(false);
      this->media_check_pending_ = false;
      this->media_start_pending_ = false;
      this->avrcp_ready_.store(false);
    }
  } else if (event == ESP_A2D_AUDIO_STATE_EVT) {
    bool streaming = param->audio_stat.state == ESP_A2D_AUDIO_STATE_STARTED;
    this->streaming_.store(streaming);
  } else if (event == ESP_A2D_MEDIA_CTRL_ACK_EVT) {
    if (param->media_ctrl_stat.cmd == ESP_A2D_MEDIA_CTRL_CHECK_SRC_RDY) {
      this->media_check_pending_ = false;
      if (param->media_ctrl_stat.status == ESP_A2D_MEDIA_CTRL_ACK_SUCCESS && this->requested_.load()) {
        if (esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_START) == ESP_OK)
          this->media_start_pending_ = true;
      }
    } else if (param->media_ctrl_stat.cmd == ESP_A2D_MEDIA_CTRL_START) {
      this->media_start_pending_ = false;
    }
  }
}

int32_t A2DPSourceSpeaker::audio_data_(uint8_t *data, int32_t length) {
  auto *speaker = active_speaker.load();
  if (!speaker || !data || length <= 0)
    return 0;
  speaker->callback_count_.fetch_add(1);
  const int64_t now_us = esp_timer_get_time();
  if (speaker->previous_callback_us_ > 0) {
    const int64_t gap = now_us - speaker->previous_callback_us_;
    if (gap > 0 && gap < UINT32_MAX) {
      uint32_t previous_max = speaker->max_callback_gap_us_.load();
      while (static_cast<uint32_t>(gap) > previous_max &&
             !speaker->max_callback_gap_us_.compare_exchange_weak(previous_max, static_cast<uint32_t>(gap))) {
      }
    }
  }
  speaker->previous_callback_us_ = now_us;
  size_t bytes = speaker->requested_.load() && speaker->pcm_buffer_ ? speaker->pcm_buffer_->read(data, length) : 0;
  if (bytes < static_cast<size_t>(length)) {
    std::memset(data + bytes, 0, length - bytes);
    // The ESP-IDF callback can continue while playback is finishing and after
    // it has stopped. Those intentional silence frames are not underruns.
    if (speaker->requested_.load() && !speaker->finish_pending_.load() && speaker->received_first_data_.load()) {
      speaker->underrun_bytes_.fetch_add(length - bytes);
      speaker->underrun_events_.fetch_add(1);
    }
  }
  speaker->pcm_bytes_.fetch_add(bytes);
  if (bytes != 0)
    speaker->audio_output_callback_(bytes / 4, now_us);
  if (!speaker->avrcp_ready_.load() || speaker->avrcp_failed_.load() || speaker->avrcp_command_pending_.load() ||
      speaker->avrcp_target_volume_.load() != speaker->avrcp_last_requested_.load() || speaker->muted_.load() ||
      speaker->avrcp_target_volume_.load() == 0)
    std::memset(data, 0, length);
  return length;
}

}  // namespace esphome::a2dp_source

#endif  // USE_ESP32
