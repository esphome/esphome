#include "a2dp_source_speaker.h"

#ifdef USE_ESP32

#include <algorithm>
#include <cinttypes>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <string>

#include "esphome/components/esp32_ble/ble.h"
#ifdef USE_ESP32_BLE_TRACKER
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#endif
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <esp_a2dp_legacy_api.h>
#include <esp_avrc_api.h>
#include <esp_bt_main.h>
#include <esp_timer.h>

namespace esphome::a2dp_source {

ESPHOME_LOG_TAG(TAG, "a2dp_source");
ESPHOME_LOG_TAG(CONNECTION_TAG, "a2dp_source.connection");
ESPHOME_LOG_TAG(AVRCP_TAG, "a2dp_source.avrcp");
ESPHOME_LOG_TAG(COEX_TAG, "a2dp_source.coexistence");
ESPHOME_LOG_TAG(DISCOVERY_TAG, "a2dp_source.discovery");
ESPHOME_LOG_TAG(PAIRING_TAG, "a2dp_source.pairing");
ESPHOME_LOG_TAG(SELECTION_TAG, "a2dp_source.selection");
// Bluetooth callbacks and all speaker instances share one Classic controller.
// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
static std::atomic<A2DPSourceSpeaker *> active_speaker{nullptr};
static std::atomic<A2DPSourceSpeaker *> desired_speaker{nullptr};
static uint32_t desired_since_ms = 0;
static A2DPSourceSpeaker *owner_speaker = nullptr;
static A2DPSourceSpeaker *registered_speakers[8]{};
static bool profile_initialized = false;
static std::atomic<bool> classic_profile_ready{false};
static constexpr size_t PCM_BUFFER_BYTES = 32768;
static constexpr uint8_t MAX_CONNECT_ATTEMPTS = 3;

struct DiscoveredDevice {
  uint8_t address[6];
  char name[33];
  int8_t rssi;
};

static RAMUniquePtr<DiscoveredDevice[]> discovered{};
static size_t discovered_count = 0;
static size_t discovered_capacity = 0;
static std::atomic<bool> classic_discovery_running{false};
static portMUX_TYPE discovery_mux = portMUX_INITIALIZER_UNLOCKED;
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

static std::string format_address(const uint8_t *address) {
  char result[18];
  std::snprintf(result, sizeof(result), "%02X:%02X:%02X:%02X:%02X:%02X", address[0], address[1], address[2], address[3],
                address[4], address[5]);
  return result;
}

static bool parse_address(const char *address, uint8_t *result) {
  if (address == nullptr || std::strlen(address) != 17)
    return false;
  auto hex_value = [](char digit) -> int {
    if (digit >= '0' && digit <= '9')
      return digit - '0';
    if (digit >= 'a' && digit <= 'f')
      return digit - 'a' + 10;
    if (digit >= 'A' && digit <= 'F')
      return digit - 'A' + 10;
    return -1;
  };
  uint8_t parsed[6];
  for (size_t i = 0; i < 6; i++) {
    const int high = hex_value(address[i * 3]);
    const int low = hex_value(address[i * 3 + 1]);
    if (high < 0 || low < 0 || (i < 5 && address[i * 3 + 2] != ':'))
      return false;
    parsed[i] = static_cast<uint8_t>((high << 4) | low);
  }
  std::copy_n(parsed, 6, result);
  return std::any_of(result, result + 6, [](uint8_t octet) { return octet != 0; });
}

bool A2DPSourceSpeaker::set_address(const char *address, bool persist) {
  if (this->connected_.load() || this->requested_.load() || this->pairing_.load() ||
      this->remove_bond_pending_.load() || this->connect_pending_.load() || this->disconnect_pending_.load() ||
      this->acl_connected_.load())
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
  auto bonds =
      RAMAllocator<esp_bd_addr_t>(RAMAllocator<esp_bd_addr_t>::PREFER_INTERNAL).make_unique_array_for_overwrite(count);
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
  auto bonds =
      RAMAllocator<esp_bd_addr_t>(RAMAllocator<esp_bd_addr_t>::PREFER_INTERNAL).make_unique_array_for_overwrite(count);
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
      this->acl_connected_.load() || this->disconnect_pending_.load() || this->remove_bond_pending_.load() ||
      (owner_speaker != nullptr && owner_speaker->forget_pending_.load()))
    return false;
  std::memset(this->address_, 0, sizeof(this->address_));
  this->pairing_status_ = PairingStatus::IDLE;
  return this->save_address_();
}

bool A2DPSourceSpeaker::unpair() {
  if (!this->has_address_() || this->requested_.load() || this->connected_.load() || this->connect_pending_.load() ||
      this->acl_connected_.load() || this->disconnect_pending_.load() || this->pairing_.load() ||
      this->remove_bond_pending_.load() || (owner_speaker != nullptr && owner_speaker->forget_pending_.load()) ||
      esp_bluedroid_get_status() != ESP_BLUEDROID_STATUS_ENABLED) {
    ESP_LOGW(PAIRING_TAG, "Cannot remove pairing for slot %u in the current state", this->slot_);
    return false;
  }
  if (!this->is_bonded_(this->address_)) {
    if (!this->clear_address()) {
      ESP_LOGW(PAIRING_TAG, "Could not clear unbonded speaker from slot %u", this->slot_);
      return false;
    }
    ESP_LOGI(PAIRING_TAG, "Cleared unbonded speaker from slot %u", this->slot_);
    return true;
  }
  this->remove_bond_result_.store(0);
  this->remove_bond_pending_.store(true);
  this->remove_bond_deadline_ms_ = millis() + 10000;
  const esp_err_t err = esp_bt_gap_remove_bond_device(this->address_);
  if (err != ESP_OK) {
    this->remove_bond_pending_.store(false);
    ESP_LOGW(PAIRING_TAG, "Could not start removing pairing for slot %u: %s", this->slot_, esp_err_to_name(err));
    return false;
  }
  return true;
}

bool A2DPSourceSpeaker::select_bond(const char *address) {
  uint8_t parsed[6];
  if (!parse_address(address, parsed) || !this->slot_ || this->requested_.load() || this->connected_.load() ||
      this->connect_pending_.load() || this->acl_connected_.load() || this->disconnect_pending_.load() ||
      this->pairing_.load() || this->remove_bond_pending_.load() ||
      (owner_speaker != nullptr && owner_speaker->forget_pending_.load()) || !this->is_bonded_(parsed)) {
    ESP_LOGW(SELECTION_TAG, "Bluetooth speaker selection rejected for slot %u", this->slot_);
    return false;
  }
  for (auto *other : registered_speakers) {
    if (other != nullptr && other != this && other->has_address_() && std::memcmp(other->address_, parsed, 6) == 0) {
      ESP_LOGW(SELECTION_TAG, "Speaker is already assigned to another slot");
      return false;
    }
  }
  if (std::memcmp(this->address_, parsed, sizeof(parsed)) == 0)
    return true;
  uint8_t previous[6];
  std::memcpy(previous, this->address_, sizeof(previous));
  if (!this->set_address(address, true)) {
    std::memcpy(this->address_, previous, sizeof(previous));
    ESP_LOGW(SELECTION_TAG, "Could not save selected speaker for slot %u", this->slot_);
    return false;
  }
  this->pairing_status_ = PairingStatus::PAIRED;
  ESP_LOGI(SELECTION_TAG, "Selected bonded speaker %s for slot %u", format_address(parsed).c_str(), this->slot_);
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
  if (this != owner_speaker || this->forget_pending_.load()) {
    ESP_LOGW(SELECTION_TAG, "Bluetooth speaker forget request rejected");
    return false;
  }
  if (!parse_address(address, parsed) || esp_bluedroid_get_status() != ESP_BLUEDROID_STATUS_ENABLED) {
    this->forget_status_ = ForgetStatus::FAILED;
    ESP_LOGW(SELECTION_TAG, "Cannot forget Bluetooth speaker: invalid address or Bluetooth unavailable");
    return false;
  }
  bool assigned = false;
  for (auto *speaker : registered_speakers) {
    if (speaker == nullptr)
      continue;
    if (speaker->remove_bond_pending_.load() || speaker->pairing_.load() || speaker->requested_.load() ||
        speaker->connected_.load() || speaker->connect_pending_.load() || speaker->acl_connected_.load() ||
        speaker->disconnect_pending_.load()) {
      this->forget_status_ = ForgetStatus::FAILED;
      ESP_LOGW(SELECTION_TAG, "Cannot forget Bluetooth speaker while a receiver is active");
      return false;
    }
    if (speaker->has_address_() && std::memcmp(speaker->address_, parsed, 6) == 0) {
      assigned = true;
    }
  }
  if (!this->is_bonded_(parsed)) {
    if (!assigned) {
      this->forget_status_ = ForgetStatus::FAILED;
      ESP_LOGW(SELECTION_TAG, "Cannot forget Bluetooth speaker: no bond or slot assignment");
      return false;
    }
    bool cleared = true;
    for (auto *speaker : registered_speakers) {
      if (speaker != nullptr && std::memcmp(speaker->address_, parsed, 6) == 0)
        cleared = speaker->clear_address() && cleared;
    }
    this->forget_status_ = cleared ? ForgetStatus::REMOVED : ForgetStatus::FAILED;
    if (cleared) {
      ESP_LOGI(SELECTION_TAG, "Cleared unbonded Bluetooth speaker %s", format_address(parsed).c_str());
    } else {
      ESP_LOGW(SELECTION_TAG, "Could not clear unbonded Bluetooth speaker %s", format_address(parsed).c_str());
    }
    return cleared;
  }
  std::memcpy(this->forget_address_, parsed, sizeof(parsed));
  this->forget_result_.store(0);
  this->forget_deadline_ms_ = millis() + 10000;
  this->forget_status_ = ForgetStatus::PENDING;
  this->forget_pending_.store(true);
  const esp_err_t err = esp_bt_gap_remove_bond_device(parsed);
  if (err != ESP_OK) {
    this->forget_pending_.store(false);
    this->forget_status_ = ForgetStatus::FAILED;
    ESP_LOGW(SELECTION_TAG, "Could not start forgetting Bluetooth speaker %s: %s", format_address(parsed).c_str(),
             esp_err_to_name(err));
    return false;
  }
  return true;
}

bool A2DPSourceSpeaker::pair(const char *address, const char *pin, bool require_discovered) {
  if (this->requested_.load() || this->connected_.load() || this->pairing_.load() || this->connect_pending_.load() ||
      this->disconnect_pending_.load() || this->acl_connected_.load() || this->remove_bond_pending_.load() ||
      (owner_speaker != nullptr && owner_speaker->forget_pending_.load()) || classic_discovery_running.load() ||
      !classic_profile_ready.load() || address == nullptr || pin == nullptr) {
    ESP_LOGW(PAIRING_TAG, "Pairing request rejected for slot %u", this->slot_);
    return false;
  }
  size_t pin_length = std::strlen(pin);
  if (pin_length > 16 || std::any_of(pin, pin + pin_length, [](char c) { return c < '0' || c > '9'; })) {
    ESP_LOGW(PAIRING_TAG, "Cannot start pairing for slot %u: invalid PIN", this->slot_);
    return false;
  }
  std::memcpy(this->previous_address_, this->address_, 6);
  if (!this->set_address(address)) {
    ESP_LOGW(PAIRING_TAG, "Cannot start pairing for slot %u: address could not be set", this->slot_);
    return false;
  }
  for (auto *other : registered_speakers) {
    if (other != nullptr && other != this && other->has_address_() &&
        std::memcmp(other->address_, this->address_, 6) == 0) {
      std::memcpy(this->address_, this->previous_address_, 6);
      ESP_LOGW(PAIRING_TAG, "Speaker is already assigned to another slot");
      return false;
    }
  }
  auto *current = active_speaker.load();
  if (current && current != this &&
      (current->requested_.load() || current->pairing_.load() || current->connected_.load() ||
       current->connect_pending_ || current->acl_connected_.load() || current->disconnect_pending_)) {
    std::memcpy(this->address_, this->previous_address_, 6);
    ESP_LOGW(PAIRING_TAG, "Cannot start pairing for slot %u while another receiver is active", this->slot_);
    return false;
  }
  if (this->is_bonded_(this->address_)) {
    this->pairing_status_ = this->save_address_() ? PairingStatus::PAIRED : PairingStatus::FAILED;
    if (this->pairing_status_ == PairingStatus::PAIRED) {
      ESP_LOGI(PAIRING_TAG, "Existing Classic bond selected for slot %u", this->slot_);
    } else {
      ESP_LOGW(PAIRING_TAG, "Could not save existing Classic bond for slot %u", this->slot_);
    }
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
    ESP_LOGW(PAIRING_TAG, "Pairing requires an address from the latest Classic scan");
    return false;
  }
  std::memset(this->pin_, 0, sizeof(this->pin_));
  std::memcpy(this->pin_, pin, pin_length);
  this->pair_authenticated_.store(false);
  this->incoming_pairing_.store(false);
  this->confirmation_pending_.store(false);
  this->pair_deadline_ms_ = millis() + 30000;
  this->next_connect_attempt_ms_ = 0;
  this->pairing_status_ = PairingStatus::CONNECTING;
  this->pairing_.store(true);
  desired_speaker.store(this);
  return true;
}

bool A2DPSourceSpeaker::pair_incoming(const char *address) {
  uint8_t parsed[6];
  if (!parse_address(address, parsed) || this->is_bonded_(parsed)) {
    ESP_LOGW(PAIRING_TAG, "Cannot open incoming pairing: invalid address or existing bond");
    return false;
  }
  if (!this->pair(address, "", true))
    return false;
  this->incoming_pairing_.store(true);
#ifdef USE_ESP32_BLE_TRACKER
  if (auto *tracker = esp32_ble_tracker::global_esp32_ble_tracker) {
    tracker->stop_scan();
    this->scanner_paused_for_connect_ = true;
  }
#endif
  const esp_err_t err = esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_NON_DISCOVERABLE);
  if (err != ESP_OK) {
    this->incoming_pairing_.store(false);
    this->pairing_.store(false);
    this->pairing_status_ = PairingStatus::FAILED;
    std::memcpy(this->address_, this->previous_address_, 6);
    A2DPSourceSpeaker *pending = this;
    desired_speaker.compare_exchange_strong(pending, nullptr);
    ESP_LOGW(PAIRING_TAG, "Could not open incoming Classic pairing window: %s", esp_err_to_name(err));
    return false;
  }
  ESP_LOGI(PAIRING_TAG, "Incoming Classic pairing window opened for %s", format_address(this->address_).c_str());
  return true;
}

bool A2DPSourceSpeaker::confirm_pairing(bool approve) {
  if (!this->pairing_.load() || !this->confirmation_pending_.exchange(false)) {
    ESP_LOGW(PAIRING_TAG, "No Classic pairing confirmation is pending");
    return false;
  }
  esp_err_t err = esp_bt_gap_ssp_confirm_reply(this->confirmation_address_, approve);
  if (err != ESP_OK) {
    ESP_LOGW(PAIRING_TAG, "Classic pairing confirmation failed: %s", esp_err_to_name(err));
    return false;
  }
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
      return "confirm code";
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
                   current->connect_pending_ || current->acl_connected_.load() || current->disconnect_pending_)))
    return false;
  portENTER_CRITICAL(&discovery_mux);
  discovered_count = 0;
  portEXIT_CRITICAL(&discovery_mux);
  esp_err_t err = esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, 6, 0);
  if (err != ESP_OK) {
    ESP_LOGW(DISCOVERY_TAG, "Bluetooth Classic discovery failed: %s", esp_err_to_name(err));
    return false;
  }
  classic_discovery_running.store(true);
  ESP_LOGD(DISCOVERY_TAG, "Bluetooth Classic discovery started");
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

std::string A2DPSourceSpeaker::discovery_results(uint16_t page) const {
  const size_t capacity = this->discovery_count();
  auto snapshot = RAMAllocator<DiscoveredDevice>(RAMAllocator<DiscoveredDevice>::PREFER_INTERNAL)
                      .make_unique_array_for_overwrite(capacity);
  if (capacity != 0 && snapshot == nullptr)
    return "out of memory";
  size_t count;
  portENTER_CRITICAL(&discovery_mux);
  count = std::min(discovered_count, capacity);
  if (count != 0)
    std::memcpy(snapshot.get(), discovered.get(), count * sizeof(DiscoveredDevice));
  portEXIT_CRITICAL(&discovery_mux);
  const size_t first = static_cast<size_t>(page) * 10;
  if (count <= first)
    return classic_discovery_running.load() ? "scanning" : "none";
  std::string result;
  for (size_t i = first; i < count && i < first + 10; i++) {
    if (i != first)
      result += "; ";
    result += format_address(snapshot[i].address);
    if (snapshot[i].name[0]) {
      result += ' ';
      result.append(snapshot[i].name, strnlen(snapshot[i].name, 24));
    }
    char signal[16];
    std::snprintf(signal, sizeof(signal), " (%d dBm)", snapshot[i].rssi);
    result += signal;
    if (this->is_bonded_(snapshot[i].address))
      result += " bonded";
  }
  return result;
}

void A2DPSourceSpeaker::reset_connection_state_() {
  this->connected_.store(false);
  this->streaming_.store(false);
  this->acl_connected_.store(false);
  this->avrcp_connected_.store(false);
  this->avrcp_ready_.store(false);
  this->connect_pending_.store(false);
  this->disconnect_pending_.store(false);
  this->media_check_pending_.store(false);
  this->media_start_pending_.store(false);
  this->next_connect_attempt_ms_ = 0;
  this->next_disconnect_attempt_ms_ = 0;
  this->connection_failed_.store(false);
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

void A2DPSourceSpeaker::setup() {
  this->audio_stream_info_ = audio::AudioStreamInfo(16, 2, 44100);
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
      classic_profile_ready.store(false);
      classic_discovery_running.store(false);
    }
    return;
  }

  if (this == owner_speaker && !profile_initialized) {
    esp_err_t err = esp_bt_gap_register_callback(A2DPSourceSpeaker::gap_event);
    if (err == ESP_OK) {
      uint8_t iocap = ESP_BT_IO_CAP_IO;
      err = esp_bt_gap_set_security_param(ESP_BT_SP_IOCAP_MODE, &iocap, sizeof(iocap));
    }
    if (err == ESP_OK) {
      esp_bt_pin_code_t pin_code{};
      err = esp_bt_gap_set_pin(ESP_BT_PIN_TYPE_VARIABLE, 0, pin_code);
    }
    if (err == ESP_OK) {
      const esp_err_t page_err = esp_bt_gap_set_page_timeout(0x2000);
      if (page_err != ESP_OK) {
        ESP_LOGW(CONNECTION_TAG, "Could not shorten Classic page timeout: %s", esp_err_to_name(page_err));
      }
    }
    if (err == ESP_OK)
      err = esp_avrc_ct_init();
    if (err == ESP_OK)
      err = esp_avrc_ct_register_callback(A2DPSourceSpeaker::avrcp_event);
    if (err == ESP_OK)
      err = esp_a2d_source_init();
    if (err == ESP_OK)
      err = esp_a2d_register_callback(A2DPSourceSpeaker::a2dp_event);
    if (err == ESP_OK)
      err = esp_a2d_source_register_data_callback(A2DPSourceSpeaker::audio_data);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "A2DP init failed: %s", esp_err_to_name(err));
      this->mark_failed();
      return;
    }
    profile_initialized = true;
    ESP_LOGD(TAG, "A2DP source initialized");
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
      if (cleared) {
        ESP_LOGI(SELECTION_TAG, "Forgot Bluetooth speaker %s", format_address(this->forget_address_).c_str());
      } else {
        ESP_LOGW(SELECTION_TAG, "Could not forget Bluetooth speaker %s", format_address(this->forget_address_).c_str());
      }
    } else if (static_cast<int32_t>(millis() - this->forget_deadline_ms_) >= 0) {
      this->forget_pending_.store(false);
      this->forget_status_ = ForgetStatus::FAILED;
      ESP_LOGW(SELECTION_TAG, "Forgetting Bluetooth speaker timed out");
    }
  }

  if (this->remove_bond_pending_.load()) {
    const uint8_t result = this->remove_bond_result_.exchange(0);
    if (result != 0) {
      this->remove_bond_pending_.store(false);
      if (result == 1 && this->clear_address()) {
        ESP_LOGI(PAIRING_TAG, "Removed pairing for slot %u", this->slot_);
      } else {
        ESP_LOGW(PAIRING_TAG, "Could not remove pairing for slot %u", this->slot_);
      }
    } else if (static_cast<int32_t>(millis() - this->remove_bond_deadline_ms_) >= 0) {
      this->remove_bond_pending_.store(false);
      ESP_LOGW(PAIRING_TAG, "Removing pairing timed out for slot %u", this->slot_);
    }
  }

  if (this == owner_speaker) {
    auto *current = active_speaker.load();
    auto *desired = desired_speaker.load();
    if (desired && current == desired) {
      desired_speaker.store(nullptr);
      desired_since_ms = 0;
    } else if (desired) {
      if (desired_since_ms == 0)
        desired_since_ms = millis();
      if (static_cast<int32_t>(millis() - desired_since_ms) >= 30000) {
        ESP_LOGW(SELECTION_TAG, "A2DP receiver handoff timed out");
        if (desired->pairing_.exchange(false)) {
          desired->pairing_status_ = PairingStatus::FAILED;
          std::memcpy(desired->address_, desired->previous_address_, 6);
          std::memset(desired->pin_, 0, sizeof(desired->pin_));
        }
        desired->stop();
        desired_speaker.store(nullptr);
        desired_since_ms = 0;
        return;
      }
      if (classic_discovery_running.load())
        esp_bt_gap_cancel_discovery();
      if (current && current->requested_.load()) {
        current->preempted_ = true;
        current->stop();
        current->preempt_trigger_.trigger();
      }
      if (!current || (!current->connect_pending_ && !current->disconnect_pending_ && !current->acl_connected_.load() &&
                       !current->connected_.load() &&
                       (current->acl_disconnected_ms_.load() == 0 ||
                        static_cast<int32_t>(millis() - current->acl_disconnected_ms_.load()) >= 5000))) {
        active_speaker.store(desired);
        desired_speaker.store(nullptr);
        desired_since_ms = 0;
      }
    } else {
      desired_since_ms = 0;
    }
  }

  if (active_speaker.load() != this)
    return;

  if (this->requested_.load())
    this->state_ = this->prepared_start_ ? speaker::STATE_STARTING : speaker::STATE_RUNNING;

  const uint32_t now = millis();
  if (this->pairing_ && static_cast<int32_t>(now - this->pair_deadline_ms_) >= 0) {
    if (this->confirmation_pending_.exchange(false))
      esp_bt_gap_ssp_confirm_reply(this->confirmation_address_, false);
    this->pairing_.store(false);
    this->pairing_status_ = PairingStatus::FAILED;
    std::memset(this->pin_, 0, sizeof(this->pin_));
    ESP_LOGW(PAIRING_TAG, "Pairing timed out for slot %u", this->slot_);
  }
  if (this->incoming_pairing_.load() && this->pairing_.load() && this->pair_authenticated_.load() &&
      this->is_bonded_(this->address_)) {
    this->pairing_.store(false);
    this->pairing_status_ = this->save_address_() ? PairingStatus::PAIRED : PairingStatus::FAILED;
    if (this->pairing_status_ == PairingStatus::PAIRED) {
      ESP_LOGI(PAIRING_TAG, "Incoming Classic bond saved for slot %u", this->slot_);
    } else {
      ESP_LOGW(PAIRING_TAG, "Could not save incoming Classic bond for slot %u", this->slot_);
    }
  }
  if (this->incoming_pairing_.load() && !this->pairing_.load()) {
    esp_bt_gap_set_scan_mode(ESP_BT_NON_CONNECTABLE, ESP_BT_NON_DISCOVERABLE);
    this->incoming_pairing_.store(false);
  }
  if (this->pairing_.load() && this->connected_.load()) {
    if (this->pair_authenticated_.load() || this->is_bonded_(this->address_)) {
      this->pairing_.store(false);
      this->pairing_status_ = this->save_address_() ? PairingStatus::PAIRED : PairingStatus::FAILED;
      std::memset(this->pin_, 0, sizeof(this->pin_));
      if (this->pairing_status_ == PairingStatus::PAIRED) {
        ESP_LOGI(PAIRING_TAG, "Pairing finished for slot %u", this->slot_);
      } else {
        ESP_LOGW(PAIRING_TAG, "Could not save pairing for slot %u", this->slot_);
      }
    } else if (!this->confirmation_pending_.load()) {
      this->pairing_status_ = PairingStatus::WAITING_FOR_AUTH;
    }
  }
  if (this->pairing_status_ == PairingStatus::FAILED && !this->connected_.load() && !this->connect_pending_ &&
      !this->acl_connected_.load()) {
    std::memcpy(this->address_, this->previous_address_, 6);
  }
#ifdef USE_ESP32_BLE_TRACKER
  if (this->scanner_paused_for_connect_ &&
      ((this->avrcp_ready_.load() && this->streaming_.load() &&
        static_cast<int32_t>(now - this->streaming_started_ms_.load()) >= 3000) ||
       (!this->requested_.load() && !this->pairing_.load() && !this->connected_.load() &&
        !this->acl_connected_.load() && !this->connect_pending_ && !this->disconnect_pending_ &&
        static_cast<int32_t>(now - this->acl_disconnected_ms_.load()) >= 5000))) {
    auto *tracker = esp32_ble_tracker::global_esp32_ble_tracker;
    if (tracker != nullptr && tracker->get_scanner_state() == esp32_ble_tracker::ScannerState::IDLE) {
      tracker->start_scan();
      this->scanner_paused_for_connect_ = false;
      ESP_LOGD(COEX_TAG, "BLE scan resumed after A2DP transition");
    }
  }
#endif
  if (this->requested_.load() && this->connected_.load()) {
    if (this->avrcp_connect_deadline_ms_ == 0)
      this->avrcp_connect_deadline_ms_ = now + 15000;
    int response = this->avrcp_response_volume_.exchange(-1);
    if (response >= 0 && this->avrcp_command_pending_) {
      this->avrcp_ready_.store(true);
      this->avrcp_command_pending_ = false;
      this->avrcp_attempts_ = 0;
      ESP_LOGD(AVRCP_TAG, "AVRCP receiver accepted volume %d/127", response);
    }
    if (this->avrcp_command_pending_ && static_cast<int32_t>(now - this->avrcp_command_deadline_ms_) >= 0)
      this->avrcp_command_pending_ = false;
    if (this->avrcp_connected_.load() && !this->avrcp_command_pending_ &&
        (!this->avrcp_ready_.load() || this->avrcp_last_requested_ != this->avrcp_target_volume_.load())) {
      if (this->avrcp_attempts_ >= 3) {
        ESP_LOGW(AVRCP_TAG, "AVRCP volume was not acknowledged");
        this->stop();
      } else {
        uint8_t target = this->avrcp_target_volume_.load();
        if (esp_avrc_ct_send_set_absolute_volume_cmd(this->avrcp_transaction_++ & 0x0F, target) == ESP_OK) {
          this->avrcp_last_requested_ = target;
          this->avrcp_command_pending_ = true;
          this->avrcp_command_deadline_ms_ = now + 1500;
          this->avrcp_attempts_++;
        } else {
          ESP_LOGW(AVRCP_TAG, "AVRCP volume command failed");
          this->stop();
        }
      }
    }
    if (!this->avrcp_ready_.load() && static_cast<int32_t>(now - this->avrcp_connect_deadline_ms_) >= 0) {
      ESP_LOGW(AVRCP_TAG, "AVRCP preparation timed out");
      this->stop();
    }
  }
  if (this->connection_failed_.exchange(false)) {
    if ((this->requested_.load() || this->pairing_.load()) && this->connect_attempts_ >= MAX_CONNECT_ATTEMPTS) {
      ESP_LOGW(CONNECTION_TAG, "A2DP connection failed after %u attempts", this->connect_attempts_);
      if (this->pairing_.exchange(false)) {
        this->pairing_status_ = PairingStatus::FAILED;
      }
      this->stop();
    } else if (this->requested_.load() || this->pairing_.load()) {
      this->next_connect_attempt_ms_ = now + 3000 + (now % 2000);
    }
  }
  if (this->connect_pending_ && static_cast<int32_t>(now - this->next_connect_attempt_ms_) >= 0) {
    ESP_LOGW(CONNECTION_TAG, "A2DP connection timed out");
    this->connect_pending_ = false;
    esp_a2d_source_disconnect(this->address_);
    this->connection_failed_ = true;
  }
  if (this->media_check_pending_ && static_cast<int32_t>(now - this->next_media_check_ms_) >= 0)
    this->media_check_pending_ = false;
  if (this->media_start_pending_ && static_cast<int32_t>(now - this->next_media_check_ms_) >= 0)
    this->media_start_pending_ = false;
  if (this->requested_.load() || (this->pairing_.load() && !this->incoming_pairing_.load())) {
    const uint32_t last_acl_disconnect = this->acl_disconnected_ms_.load();
    if (!this->connected_.load() && !this->acl_connected_.load() && !this->connect_pending_ &&
        !this->disconnect_pending_ && static_cast<int32_t>(now - this->next_connect_attempt_ms_) >= 0 &&
        (last_acl_disconnect == 0 || static_cast<int32_t>(now - last_acl_disconnect) >= 5000)) {
#ifdef USE_ESP32_BLE_TRACKER
      auto *tracker = esp32_ble_tracker::global_esp32_ble_tracker;
      if (tracker != nullptr) {
        const auto scanner_state = tracker->get_scanner_state();
        if (scanner_state != esp32_ble_tracker::ScannerState::STARTING)
          this->scanner_stop_starting_requested_ = false;
        if (scanner_state == esp32_ble_tracker::ScannerState::IDLE ||
            scanner_state == esp32_ble_tracker::ScannerState::RUNNING ||
            scanner_state == esp32_ble_tracker::ScannerState::FAILED) {
          if (!this->scanner_paused_for_connect_) {
            ESP_LOGD(COEX_TAG, "Pausing BLE scan for A2DP connection");
          }
          tracker->stop_scan();
          this->scanner_paused_for_connect_ = true;
        } else if (scanner_state == esp32_ble_tracker::ScannerState::STARTING &&
                   !this->scanner_stop_starting_requested_) {
          tracker->stop_scan();
          this->scanner_stop_starting_requested_ = true;
          this->scanner_paused_for_connect_ = true;
        }
        if (scanner_state != esp32_ble_tracker::ScannerState::IDLE) {
          if (this->scanner_wait_started_ms_ == 0)
            this->scanner_wait_started_ms_ = now;
          if (static_cast<int32_t>(now - this->scanner_wait_started_ms_) >= 5000) {
            ESP_LOGW(COEX_TAG, "BLE scan did not stop before A2DP connect");
            this->stop();
          }
          return;
        }
        this->scanner_wait_started_ms_ = 0;
      }
#endif
      this->connect_attempts_++;
      esp_err_t err = esp_a2d_source_connect(this->address_);
      this->next_connect_attempt_ms_ = now + 15000;
      if (err == ESP_OK) {
        this->connect_pending_ = true;
      } else {
        ESP_LOGW(CONNECTION_TAG, "A2DP connect failed: %s", esp_err_to_name(err));
        this->connection_failed_ = true;
      }
    } else if (this->connected_.load() && this->avrcp_ready_.load() && this->received_first_data_.load() &&
               !this->streaming_.load() && !this->media_check_pending_ && !this->media_start_pending_ &&
               static_cast<int32_t>(now - this->next_media_check_ms_) >= 0) {
      if (esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_CHECK_SRC_RDY) == ESP_OK)
        this->media_check_pending_ = true;
      this->next_media_check_ms_ = now + 1000;
    }
  } else if ((this->connected_.load() || this->connect_pending_ || this->acl_connected_.load()) &&
             !this->disconnect_pending_ && static_cast<int32_t>(now - this->next_disconnect_attempt_ms_) >= 0) {
    this->next_disconnect_attempt_ms_ = now + 1000;
    if (this->streaming_.load())
      esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_SUSPEND);
    const esp_err_t err = esp_a2d_source_disconnect(this->address_);
    if (err == ESP_OK) {
      this->disconnect_pending_ = true;
    } else {
      ESP_LOGW(CONNECTION_TAG, "A2DP disconnect request failed: %s", esp_err_to_name(err));
    }
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
  this->avrcp_target_volume_.store(static_cast<uint8_t>(std::lround(this->volume_ * 127.0f)));
}

void A2DPSourceSpeaker::set_mute_state(bool mute_state) {
  this->mute_state_ = mute_state;
  this->muted_.store(mute_state);
}

void A2DPSourceSpeaker::start() {
  if (this->preempted_) {
    this->state_ = speaker::STATE_STOPPED;
    return;
  }
  if (this->prepared_start_ && this->requested_.load()) {
    this->prepared_start_ = false;
    this->state_ = speaker::STATE_RUNNING;
    return;
  }
  if (!this->has_address_()) {
    ESP_LOGW(CONNECTION_TAG, "Speaker address is not configured");
    this->state_ = speaker::STATE_STOPPED;
    return;
  }
  auto *desired = desired_speaker.load();
  if (desired != nullptr && desired != this) {
    this->state_ = speaker::STATE_STOPPED;
    return;
  }
  auto *current = active_speaker.load();
  if (current != nullptr && current != this && current->pairing_.load()) {
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
  this->connect_attempts_ = 0;
  this->connection_failed_ = false;
  this->avrcp_ready_.store(false);
  this->avrcp_last_requested_ = 255;
  this->avrcp_connect_deadline_ms_ = 0;
  this->avrcp_attempts_ = 0;
  this->avrcp_command_pending_ = false;
  this->avrcp_response_volume_.store(-1);
  this->scanner_wait_started_ms_ = 0;
  this->next_connect_attempt_ms_ = 0;
  this->next_disconnect_attempt_ms_ = 0;
  this->finish_pending_ = false;
  this->received_first_data_.store(false);
  this->streaming_started_ms_.store(0);
  this->previous_callback_us_ = 0;
  this->requested_.store(true);
  this->state_ = speaker::STATE_STARTING;
  desired_speaker.store(this);
}

void A2DPSourceSpeaker::prepare_for_media() {
  this->preempted_ = false;
  if (this->requested_.load())
    return;
  this->prepared_start_ = true;
  this->start();
}

void A2DPSourceSpeaker::stop() {
  A2DPSourceSpeaker *pending = this;
  desired_speaker.compare_exchange_strong(pending, nullptr);
  this->requested_.store(false);
  this->prepared_start_ = false;
  this->avrcp_ready_.store(false);
  this->finish_pending_ = false;
  this->state_ = speaker::STATE_STOPPED;
}

void A2DPSourceSpeaker::finish() { this->finish_pending_ = true; }

bool A2DPSourceSpeaker::has_buffered_data() const { return this->pcm_buffer_ && this->pcm_buffer_->available() != 0; }

size_t A2DPSourceSpeaker::play(const uint8_t *data, size_t length, TickType_t ticks_to_wait) {
  if (!this->requested_.load() || !this->pcm_buffer_)
    return 0;
  size_t written = this->pcm_buffer_->write_without_replacement(data, length, ticks_to_wait);
  if (written)
    this->received_first_data_.store(true);
  return written;
}

void A2DPSourceSpeaker::a2dp_event(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param) {
  if (auto *speaker = active_speaker.load())
    speaker->process_event_(event, param);
}

void A2DPSourceSpeaker::avrcp_event(esp_avrc_ct_cb_event_t event, esp_avrc_ct_cb_param_t *param) {
  if (auto *speaker = active_speaker.load())
    speaker->process_avrcp_event_(event, param);
}

void A2DPSourceSpeaker::process_avrcp_event_(esp_avrc_ct_cb_event_t event, esp_avrc_ct_cb_param_t *param) {
  if (event == ESP_AVRC_CT_CONNECTION_STATE_EVT && std::memcmp(param->conn_stat.remote_bda, this->address_, 6) == 0) {
    this->avrcp_connected_.store(param->conn_stat.connected);
    if (param->conn_stat.connected) {
      ESP_LOGD(AVRCP_TAG, "AVRCP connected");
    } else {
      ESP_LOGD(AVRCP_TAG, "AVRCP disconnected");
    }
    this->avrcp_ready_.store(false);
    this->avrcp_command_pending_ = false;
    this->avrcp_last_requested_ = 255;
  } else if (event == ESP_AVRC_CT_SET_ABSOLUTE_VOLUME_RSP_EVT && this->avrcp_connected_.load()) {
    this->avrcp_response_volume_.store(param->set_volume_rsp.volume);
  }
}

void A2DPSourceSpeaker::gap_event(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param) {
  if (event == ESP_BT_GAP_ACL_CONN_CMPL_STAT_EVT) {
    if (auto *speaker = active_speaker.load();
        speaker != nullptr && std::memcmp(param->acl_conn_cmpl_stat.bda, speaker->address_, 6) == 0) {
      speaker->acl_connected_.store(param->acl_conn_cmpl_stat.stat == ESP_BT_STATUS_SUCCESS);
      ESP_LOGD(CONNECTION_TAG, "Classic ACL connect status 0x%02x", param->acl_conn_cmpl_stat.stat);
    }
  } else if (event == ESP_BT_GAP_ACL_DISCONN_CMPL_STAT_EVT) {
    if (auto *speaker = active_speaker.load();
        speaker != nullptr && std::memcmp(param->acl_disconn_cmpl_stat.bda, speaker->address_, 6) == 0) {
      speaker->acl_connected_.store(false);
      speaker->acl_disconnected_ms_.store(millis());
      ESP_LOGD(CONNECTION_TAG, "Classic ACL disconnected (reason 0x%02x)", param->acl_disconn_cmpl_stat.reason);
    }
  } else if (event == ESP_BT_GAP_REMOVE_BOND_DEV_COMPLETE_EVT) {
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
    if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STOPPED && owner_speaker != nullptr) {
      ESP_LOGD(DISCOVERY_TAG, "Bluetooth Classic discovery stopped with %u results",
               static_cast<unsigned>(owner_speaker->discovery_count()));
    }
  } else if (event == ESP_BT_GAP_DISC_RES_EVT) {
    const auto &result = param->disc_res;
    DiscoveredDevice device{};
    device.rssi = -128;
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
        if (name == nullptr) {
          name =
              esp_bt_gap_resolve_eir_data(static_cast<uint8_t *>(prop.val), ESP_BT_EIR_TYPE_SHORT_LOCAL_NAME, &length);
        }
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
    if (index < discovered_count || discovered_count < discovered_capacity) {
      if (index < discovered_count && !device.name[0] && discovered[index].name[0])
        std::memcpy(device.name, discovered[index].name, sizeof(device.name));
      discovered[index] = device;
      if (index == discovered_count)
        discovered_count++;
      portEXIT_CRITICAL(&discovery_mux);
    } else {
      const size_t next_capacity = discovered_capacity == 0 ? 8 : discovered_capacity * 2;
      portEXIT_CRITICAL(&discovery_mux);
      auto expanded = RAMAllocator<DiscoveredDevice>(RAMAllocator<DiscoveredDevice>::PREFER_INTERNAL)
                          .make_unique_array_for_overwrite(next_capacity);
      if (expanded == nullptr) {
        ESP_LOGW(DISCOVERY_TAG, "No memory for another Classic discovery result");
      } else {
        RAMUniquePtr<DiscoveredDevice[]> old;
        portENTER_CRITICAL(&discovery_mux);
        if (discovered_count != 0)
          std::memcpy(expanded.get(), discovered.get(), discovered_count * sizeof(DiscoveredDevice));
        old = std::move(discovered);
        discovered = std::move(expanded);
        discovered_capacity = next_capacity;
        discovered[discovered_count++] = device;
        portEXIT_CRITICAL(&discovery_mux);
      }
    }
  }
  if (auto *speaker = active_speaker.load())
    speaker->process_gap_event_(event, param);
}

void A2DPSourceSpeaker::process_gap_event_(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param) {
  if (event == ESP_BT_GAP_AUTH_CMPL_EVT) {
    if (std::memcmp(param->auth_cmpl.bda, this->address_, 6) != 0)
      return;
    if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS) {
      if (this->pairing_.load())
        this->pair_authenticated_.store(true);
      ESP_LOGD(PAIRING_TAG, "Bluetooth Classic speaker authenticated");
    } else {
      ESP_LOGW(PAIRING_TAG, "Bluetooth Classic authentication failed: %d", param->auth_cmpl.stat);
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
    if (std::memcmp(param->conn_stat.remote_bda, this->address_, 6) != 0)
      return;
    ESP_LOGD(CONNECTION_TAG, "A2DP connection state %d", param->conn_stat.state);
    if (param->conn_stat.state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
      if (!this->connected_.exchange(true)) {
        ESP_LOGI(CONNECTION_TAG, "A2DP connected to %02X:%02X:%02X:%02X:%02X:%02X", this->address_[0],
                 this->address_[1], this->address_[2], this->address_[3], this->address_[4], this->address_[5]);
      }
      this->connect_pending_ = false;
      this->disconnect_pending_ = false;
      this->connection_failed_ = false;
    } else if (param->conn_stat.state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
      if (this->connected_.exchange(false)) {
        ESP_LOGI(CONNECTION_TAG, "A2DP disconnected from %02X:%02X:%02X:%02X:%02X:%02X", this->address_[0],
                 this->address_[1], this->address_[2], this->address_[3], this->address_[4], this->address_[5]);
      }
      this->connect_pending_ = false;
      this->disconnect_pending_ = false;
      this->avrcp_connected_.store(false);
      this->avrcp_ready_.store(false);
      this->streaming_.store(false);
      this->media_check_pending_ = false;
      this->media_start_pending_ = false;
      if (this->requested_.load() || this->pairing_.load())
        this->connection_failed_ = true;
    }
  } else if (event == ESP_A2D_AUDIO_STATE_EVT) {
    bool streaming = param->audio_stat.state == ESP_A2D_AUDIO_STATE_STARTED;
    if (streaming)
      this->streaming_started_ms_.store(millis());
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

int32_t A2DPSourceSpeaker::audio_data(uint8_t *data, int32_t length) {
  auto *speaker = active_speaker.load();
  if (!speaker || !data || length <= 0)
    return 0;
#if defined(USE_LOGGER) && defined(ESPHOME_LOG_HAS_DEBUG)
  speaker->callback_count_.fetch_add(1);
#endif
  const int64_t now_us = esp_timer_get_time();
#if defined(USE_LOGGER) && defined(ESPHOME_LOG_HAS_DEBUG)
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
#endif
  size_t bytes = speaker->requested_.load() && speaker->pcm_buffer_ ? speaker->pcm_buffer_->read(data, length) : 0;
  if (bytes < static_cast<size_t>(length)) {
    std::memset(data + bytes, 0, length - bytes);
#if defined(USE_LOGGER) && defined(ESPHOME_LOG_HAS_DEBUG)
    if (speaker->requested_.load() && !speaker->finish_pending_.load() && speaker->received_first_data_.load()) {
      speaker->underrun_bytes_.fetch_add(length - bytes);
      speaker->underrun_events_.fetch_add(1);
    }
#endif
  }
#if defined(USE_LOGGER) && defined(ESPHOME_LOG_HAS_DEBUG)
  speaker->pcm_bytes_.fetch_add(bytes);
#endif
  const bool muted = !speaker->avrcp_ready_.load() || speaker->avrcp_command_pending_.load() ||
                     speaker->avrcp_target_volume_.load() != speaker->avrcp_last_requested_.load() ||
                     speaker->muted_.load() || speaker->avrcp_target_volume_.load() == 0;
  if (muted) {
    std::memset(data, 0, length);
  } else if (bytes != 0) {
    speaker->audio_output_callback_(bytes / 4, now_us);
  }
  return length;
}

}  // namespace esphome::a2dp_source

#endif  // USE_ESP32
