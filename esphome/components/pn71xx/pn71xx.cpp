#include <algorithm>
#include <utility>

#include "pn71xx.h"

#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::pn71xx {

static const char *const TAG = "pn71xx";

// Builds a message with a URI record and, optionally, the Home Assistant Android app record
static std::unique_ptr<nfc::NdefMessage> build_uri_message(const std::string &uri,
                                                           const bool include_android_app_record) {
  auto ndef_message = make_unique<nfc::NdefMessage>();

  ndef_message->add_uri_record(uri);

  if (include_android_app_record) {
    auto ext_record = make_unique<nfc::NdefRecord>();
    ext_record->set_tnf(nfc::TNF_EXTERNAL_TYPE);
    ext_record->set_type(nfc::HA_TAG_ID_EXT_RECORD_TYPE);
    ext_record->set_payload(nfc::HA_TAG_ID_EXT_RECORD_PAYLOAD);
    ndef_message->add_record(std::move(ext_record));
  }
  return ndef_message;
}

void PN71xx::setup() {
  this->irq_pin_->setup();
  this->ven_pin_->setup();

  this->nci_fsm_transition_();  // kick off reset & init processes
}

void PN71xx::dump_config() {
  LOG_PIN("  IRQ pin: ", this->irq_pin_);
  LOG_PIN("  VEN pin: ", this->ven_pin_);
}

void PN71xx::loop() {
  this->nci_fsm_transition_();
  this->purge_old_tags_();
}

void PN71xx::set_tag_emulation_message(const std::shared_ptr<nfc::NdefMessage> &message) {
  if (message == nullptr) {
    return;
  }
  // encoded once here so it is validated up front and not re-encoded for every read from the reader
  auto encoded = message->encode();
  if (encoded.size() > CARD_EMU_T4T_MAX_NDEF_SIZE) {
    ESP_LOGE(TAG, "Tag emulation message too long: %zu > %u bytes", encoded.size(), CARD_EMU_T4T_MAX_NDEF_SIZE);
    return;
  }
  this->card_emulation_ndef_.init(encoded.size());
  for (const uint8_t byte : encoded) {
    this->card_emulation_ndef_.push_back(byte);
  }
  ESP_LOGD(TAG, "Tag emulation message set");
}

void PN71xx::set_tag_emulation_message(const std::string &message, const bool include_android_app_record) {
  this->set_tag_emulation_message(build_uri_message(message, include_android_app_record));
}

void PN71xx::set_tag_emulation_message(const char *message, const bool include_android_app_record) {
  this->set_tag_emulation_message(std::string(message), include_android_app_record);
}

void PN71xx::set_tag_emulation_off() {
  if (this->listening_enabled_) {
    this->listening_enabled_ = false;
    this->config_refresh_pending_ = true;
  }
  ESP_LOGD(TAG, "Tag emulation disabled");
}

void PN71xx::set_tag_emulation_on() {
  if (this->card_emulation_ndef_.empty()) {
    ESP_LOGE(TAG, "No NDEF message is set; tag emulation cannot be enabled");
    return;
  }
  if (!this->listening_enabled_) {
    this->listening_enabled_ = true;
    this->config_refresh_pending_ = true;
  }
  ESP_LOGD(TAG, "Tag emulation enabled");
}

void PN71xx::set_polling_off() {
  if (this->polling_enabled_) {
    this->polling_enabled_ = false;
    this->config_refresh_pending_ = true;
  }
  ESP_LOGD(TAG, "Tag polling disabled");
}

void PN71xx::set_polling_on() {
  if (!this->polling_enabled_) {
    this->polling_enabled_ = true;
    this->config_refresh_pending_ = true;
  }
  ESP_LOGD(TAG, "Tag polling enabled");
}

void PN71xx::read_mode() {
  this->next_task_ = EP_READ;
  ESP_LOGD(TAG, "Waiting to read next tag");
}

void PN71xx::clean_mode() {
  this->next_task_ = EP_CLEAN;
  ESP_LOGD(TAG, "Waiting to clean next tag");
}

void PN71xx::format_mode() {
  this->next_task_ = EP_FORMAT;
  ESP_LOGD(TAG, "Waiting to format next tag");
}

void PN71xx::write_mode() {
  if (this->next_task_message_to_write_ == nullptr) {
    ESP_LOGW(TAG, "Message to write must be set before setting write mode");
    return;
  }

  this->next_task_ = EP_WRITE;
  ESP_LOGD(TAG, "Waiting to write next tag");
}

void PN71xx::set_tag_write_message(std::shared_ptr<nfc::NdefMessage> message) {
  this->next_task_message_to_write_ = std::move(message);
  ESP_LOGD(TAG, "Message to write has been set");
}

void PN71xx::set_tag_write_message(const std::string &message, const bool include_android_app_record) {
  this->set_tag_write_message(build_uri_message(message, include_android_app_record));
}

uint8_t PN71xx::set_test_mode(const TestMode test_mode, const std::vector<uint8_t> &data,
                              std::vector<uint8_t> &result) {
  auto test_oid = TEST_PRBS_OID;

  switch (test_mode) {
    case TestMode::TEST_PRBS:
      // test_oid = TEST_PRBS_OID;
      break;

    case TestMode::TEST_ANTENNA:
      test_oid = TEST_ANTENNA_OID;
      break;

    case TestMode::TEST_GET_REGISTER:
      test_oid = TEST_GET_REGISTER_OID;
      break;

    case TestMode::TEST_NONE:
    default:
      ESP_LOGD(TAG, "Exiting test mode");
      this->nci_fsm_set_state_(NCIState::NFCC_RESET);
      return nfc::STATUS_OK;
  }

  if (this->reset_core_(true, true) != nfc::STATUS_OK) {
    ESP_LOGE(TAG, "Failed to reset NCI core");
    this->nci_fsm_set_error_state_(NCIState::NFCC_RESET);
    result.clear();
    return nfc::STATUS_FAILED;
  } else {
    this->nci_fsm_set_state_(NCIState::NFCC_INIT);
  }
  if (this->init_core_() != nfc::STATUS_OK) {
    ESP_LOGE(TAG, "Failed to initialise NCI core");
    this->nci_fsm_set_error_state_(NCIState::NFCC_INIT);
    result.clear();
    return nfc::STATUS_FAILED;
  } else {
    this->nci_fsm_set_state_(NCIState::TEST);
  }

  nfc::NciMessage rx;
  nfc::NciMessage tx(nfc::NCI_PKT_MT_CTRL_COMMAND, nfc::NCI_PROPRIETARY_GID, test_oid, data);

  ESP_LOGW(TAG, "Starting test mode, OID 0x%02X", test_oid);
  auto status = this->transceive_(tx, rx, NFCC_INIT_TIMEOUT);

  if (status != nfc::STATUS_OK) {
    ESP_LOGE(TAG, "Failed to start test mode, OID 0x%02X", test_oid);
    this->nci_fsm_set_state_(NCIState::NFCC_RESET);
    result.clear();
  } else {
    // the payload after the status byte, if the NFCC sent one
    const auto payload = rx.get_payload();
    result.assign(payload.begin() + std::min<size_t>(1, payload.size()), payload.end());
    if (!result.empty()) {
      char buf[nfc::FORMAT_BYTES_BUFFER_SIZE];
      ESP_LOGW(TAG, "Test results: %s", nfc::format_bytes_to(buf, result));
    }
  }
  return status;
}

uint8_t PN71xx::reset_core_(const bool reset_config, const bool power) {
  this->prepare_reset();

  if (power) {
    this->ven_pin_->digital_write(true);
    delay(NFCC_RESET_DELAY);
    this->ven_pin_->digital_write(false);
    delay(NFCC_RESET_DELAY);
    this->ven_pin_->digital_write(true);
    delay(NFCC_INIT_TIMEOUT);
  }

  nfc::NciMessage rx;
  nfc::NciMessage tx(nfc::NCI_PKT_MT_CTRL_COMMAND, nfc::NCI_CORE_GID, nfc::NCI_CORE_RESET_OID,
                     {(uint8_t) reset_config});

  if (this->transceive_(tx, rx, NFCC_INIT_TIMEOUT) != nfc::STATUS_OK) {
    ESP_LOGE(TAG, "Error sending reset command");
    return nfc::STATUS_FAILED;
  }

  if (!rx.simple_status_response_is(nfc::STATUS_OK)) {
    char buf[nfc::FORMAT_BYTES_BUFFER_SIZE];
    ESP_LOGE(TAG, "Invalid reset response: %s", nfc::format_bytes_to(buf, rx.get_message()));
    return rx.get_simple_status_response();
  }
  return this->verify_reset(rx, reset_config);
}

uint8_t PN71xx::init_core_() {
  nfc::NciMessage rx;
  nfc::NciMessage tx(nfc::NCI_PKT_MT_CTRL_COMMAND, nfc::NCI_CORE_GID, nfc::NCI_CORE_INIT_OID);

  if (this->transceive_(tx, rx) != nfc::STATUS_OK) {
    ESP_LOGE(TAG, "Error sending initialise command");
    return nfc::STATUS_FAILED;
  }

  if (!rx.simple_status_response_is(nfc::STATUS_OK)) {
    char buf[nfc::FORMAT_BYTES_BUFFER_SIZE];
    ESP_LOGE(TAG, "Invalid initialise response: %s", nfc::format_bytes_to(buf, rx.get_message()));
    return nfc::STATUS_FAILED;
  }

  return this->process_init_response(rx);
}

uint8_t PN71xx::send_init_config_() {
  nfc::NciMessage rx;
  nfc::NciMessage tx(nfc::NCI_PKT_MT_CTRL_COMMAND, nfc::NCI_PROPRIETARY_GID, nfc::NCI_CORE_SET_CONFIG_OID);

  if (this->transceive_(tx, rx) != nfc::STATUS_OK) {
    ESP_LOGE(TAG, "Error enabling proprietary extensions");
    return nfc::STATUS_FAILED;
  }

  tx.set_message(nfc::NCI_PKT_MT_CTRL_COMMAND, nfc::NCI_CORE_GID, nfc::NCI_CORE_SET_CONFIG_OID, this->pmu_config());

  if (this->transceive_(tx, rx) != nfc::STATUS_OK) {
    ESP_LOGE(TAG, "Error sending PMU config");
    return nfc::STATUS_FAILED;
  }

  return this->send_core_config_();
}

uint8_t PN71xx::send_core_config_() {
  std::span<const uint8_t> core_config = CORE_CONFIG_SOLO;
  this->core_config_is_solo_ = true;

  if (this->listening_enabled_ && this->polling_enabled_) {
    core_config = CORE_CONFIG_RW_CE;
    this->core_config_is_solo_ = false;
  }

  nfc::NciMessage rx;
  nfc::NciMessage tx(nfc::NCI_PKT_MT_CTRL_COMMAND, nfc::NCI_CORE_GID, nfc::NCI_CORE_SET_CONFIG_OID, core_config);

  if (this->transceive_(tx, rx) != nfc::STATUS_OK) {
    ESP_LOGW(TAG, "Error sending core config");
    return nfc::STATUS_FAILED;
  }

  return nfc::STATUS_OK;
}

uint8_t PN71xx::refresh_core_config_() {
  bool core_config_should_be_solo = !(this->listening_enabled_ && this->polling_enabled_);

  if (this->nci_state_ == NCIState::RFST_DISCOVERY) {
    if (this->stop_discovery_() != nfc::STATUS_OK) {
      this->nci_fsm_set_state_(NCIState::NFCC_RESET);
      return nfc::STATUS_FAILED;
    }
    this->nci_fsm_set_state_(NCIState::RFST_IDLE);
  }

  if (this->core_config_is_solo_ != core_config_should_be_solo) {
    if (this->send_core_config_() != nfc::STATUS_OK) {
      ESP_LOGV(TAG, "Failed to refresh core config");
      return nfc::STATUS_FAILED;
    }
  }
  this->config_refresh_pending_ = false;
  return nfc::STATUS_OK;
}

uint8_t PN71xx::set_discover_map_() {
  nfc::NciMessage rx;
  nfc::NciMessage tx(nfc::NCI_PKT_MT_CTRL_COMMAND, nfc::RF_GID, nfc::RF_DISCOVER_MAP_OID,
                     {sizeof(RF_DISCOVER_MAP_CONFIG) / 3});
  tx.append(RF_DISCOVER_MAP_CONFIG);

  if (this->transceive_(tx, rx, NFCC_INIT_TIMEOUT) != nfc::STATUS_OK) {
    ESP_LOGE(TAG, "Error sending discover map poll config");
    return nfc::STATUS_FAILED;
  }
  return nfc::STATUS_OK;
}

uint8_t PN71xx::set_listen_mode_routing_() {
  nfc::NciMessage rx;
  nfc::NciMessage tx(nfc::NCI_PKT_MT_CTRL_COMMAND, nfc::RF_GID, nfc::RF_SET_LISTEN_MODE_ROUTING_OID,
                     this->listen_mode_routing_config());

  if (this->transceive_(tx, rx, NFCC_INIT_TIMEOUT) != nfc::STATUS_OK) {
    ESP_LOGE(TAG, "Error setting listen mode routing config");
    return nfc::STATUS_FAILED;
  }
  return nfc::STATUS_OK;
}

uint8_t PN71xx::start_discovery_() {
  std::span<const uint8_t> rf_discovery_config = RF_DISCOVERY_CONFIG;

  if (!this->listening_enabled_) {
    rf_discovery_config = RF_DISCOVERY_POLL_CONFIG;
  } else if (!this->polling_enabled_) {
    rf_discovery_config = RF_DISCOVERY_LISTEN_CONFIG;
  }

  nfc::NciMessage rx;
  nfc::NciMessage tx(nfc::NCI_PKT_MT_CTRL_COMMAND, nfc::RF_GID, nfc::RF_DISCOVER_OID,
                     {static_cast<uint8_t>(rf_discovery_config.size())});
  for (const uint8_t mode_tech : rf_discovery_config) {
    tx.append({mode_tech, 0x01});  // RF Technology and Mode will be executed in every discovery period
  }

  if (this->transceive_(tx, rx) != nfc::STATUS_OK) {
    switch (rx.get_simple_status_response()) {
      // in any of these cases, we are either already in or will remain in discovery, which satisfies the function call
      case nfc::STATUS_OK:
      case nfc::DISCOVERY_ALREADY_STARTED:
      case nfc::DISCOVERY_TARGET_ACTIVATION_FAILED:
      case nfc::DISCOVERY_TEAR_DOWN:
        return nfc::STATUS_OK;

      default:
        ESP_LOGE(TAG, "Error starting discovery");
        return nfc::STATUS_FAILED;
    }
  }

  return nfc::STATUS_OK;
}

uint8_t PN71xx::stop_discovery_() { return this->deactivate_(nfc::DEACTIVATION_TYPE_IDLE, NFCC_TAG_WRITE_TIMEOUT); }

uint8_t PN71xx::deactivate_(const uint8_t type, const uint16_t timeout) {
  nfc::NciMessage rx;
  nfc::NciMessage tx(nfc::NCI_PKT_MT_CTRL_COMMAND, nfc::RF_GID, nfc::RF_DEACTIVATE_OID, {type});

  auto status = this->transceive_(tx, rx, timeout);
  // if (status != nfc::STATUS_OK) {
  //   ESP_LOGE(TAG, "Error sending deactivate type %u", type);
  //   return nfc::STATUS_FAILED;
  // }
  return status;
}

void PN71xx::select_endpoint_() {
  if (this->discovered_endpoint_.empty()) {
    ESP_LOGW(TAG, "No cached tags to select");
    this->stop_discovery_();
    this->nci_fsm_set_state_(NCIState::RFST_IDLE);
    return;
  }
  this->selecting_endpoint_ = 0;
  for (size_t i = 0; i < this->discovered_endpoint_.size(); i++) {
    if (!this->discovered_endpoint_[i].trig_called) {
      this->selecting_endpoint_ = i;
      break;
    }
  }
  const auto &endpoint = this->discovered_endpoint_[this->selecting_endpoint_];
  // the RF interface must match the one set for this protocol in RF_DISCOVER_MAP_CONFIG
  uint8_t interface = nfc::INTF_FRAME;
  if (endpoint.protocol == nfc::PROT_ISODEP) {
    interface = nfc::INTF_ISODEP;
  } else if (endpoint.protocol == nfc::PROT_MIFARE) {
    interface = nfc::INTF_TAGCMD;
  }
  nfc::NciMessage rx;
  nfc::NciMessage tx(nfc::NCI_PKT_MT_CTRL_COMMAND, nfc::RF_GID, nfc::RF_DISCOVER_SELECT_OID,
                     {endpoint.id, endpoint.protocol, interface});

  if (this->transceive_(tx, rx) != nfc::STATUS_OK) {
    ESP_LOGE(TAG, "Error selecting endpoint");
  } else {
    this->nci_fsm_set_state_(NCIState::EP_SELECTING);
  }
}

uint8_t PN71xx::read_endpoint_data_(const uint8_t protocol, nfc::NfcTag &tag) {
  switch (protocol) {
    case nfc::PROT_MIFARE:
      ESP_LOGV(TAG, "Reading Mifare classic");
      return this->read_mifare_classic_tag_(tag);

    case nfc::PROT_T2T:
      ESP_LOGV(TAG, "Reading Mifare ultralight");
      return this->read_mifare_ultralight_tag_(tag);

    default:
      ESP_LOGV(TAG, "Reading protocol 0x%02X is not supported", protocol);
      break;
  }
  return nfc::STATUS_FAILED;
}

uint8_t PN71xx::clean_endpoint_(const uint8_t protocol) {
  switch (protocol) {
    case nfc::PROT_MIFARE:
      return this->format_mifare_classic_mifare_();

    case nfc::PROT_T2T:
      return this->clean_mifare_ultralight_();

    default:
      ESP_LOGE(TAG, "Unsupported tag for cleaning");
      break;
  }
  return nfc::STATUS_FAILED;
}

uint8_t PN71xx::format_endpoint_(const uint8_t protocol) {
  switch (protocol) {
    case nfc::PROT_MIFARE:
      return this->format_mifare_classic_ndef_();

    case nfc::PROT_T2T:
      return this->clean_mifare_ultralight_();

    default:
      ESP_LOGE(TAG, "Unsupported tag for formatting");
      break;
  }
  return nfc::STATUS_FAILED;
}

uint8_t PN71xx::write_endpoint_(const uint8_t protocol, nfc::NfcTagUid &uid,
                                std::shared_ptr<nfc::NdefMessage> &message) {
  switch (protocol) {
    case nfc::PROT_MIFARE:
      return this->write_mifare_classic_tag_(message);

    case nfc::PROT_T2T:
      return this->write_mifare_ultralight_tag_(uid, message);

    default:
      ESP_LOGE(TAG, "Unsupported tag for writing");
      break;
  }
  return nfc::STATUS_FAILED;
}

bool PN71xx::parse_uid_(const uint8_t mode_tech, const std::span<const uint8_t> rf_tech_params, nfc::NfcTagUid &uid) {
  if (mode_tech != (nfc::MODE_POLL | nfc::TECH_PASSIVE_NFCA)) {
    return false;
  }
  // RF technology parameters: SENS_RES (2 bytes), NFCID1 length, NFCID1, ...
  if (rf_tech_params.size() < 3) {
    ESP_LOGE(TAG, "NFC-A parameters too short");
    return false;
  }
  const uint8_t uid_length = rf_tech_params[2];
  if (uid_length == 0 || uid_length > nfc::NFC_UID_MAX_LENGTH || rf_tech_params.size() < 3u + uid_length) {
    ESP_LOGE(TAG, "Invalid UID length: %u", uid_length);
    return false;
  }
  uid.assign(rf_tech_params.begin() + 3, rf_tech_params.begin() + 3 + uid_length);
  return true;
}

std::unique_ptr<nfc::NfcTag> PN71xx::build_tag_(const uint8_t protocol, const nfc::NfcTagUid &uid) {
  if (protocol == nfc::PROT_MIFARE) {
    return make_unique<nfc::NfcTag>(uid, nfc::MIFARE_CLASSIC);
  }
  if (protocol == nfc::PROT_T2T) {
    return make_unique<nfc::NfcTag>(uid, nfc::NFC_FORUM_TYPE_2);
  }
  return make_unique<nfc::NfcTag>(uid);
}

size_t PN71xx::find_or_add_tag_(const uint8_t protocol, const nfc::NfcTagUid &uid) {
  const auto tag_loc = this->find_tag_uid_(uid);
  if (tag_loc.has_value()) {
    ESP_LOGVV(TAG, "Tag cache updated");
    return tag_loc.value();
  }
  if (this->discovered_endpoint_.size() >= MAX_DISCOVERED_ENDPOINTS) {
    size_t oldest = 0;
    for (size_t i = 1; i < this->discovered_endpoint_.size(); i++) {
      if (this->discovered_endpoint_[i].last_seen < this->discovered_endpoint_[oldest].last_seen) {
        oldest = i;
      }
    }
    ESP_LOGW(TAG, "Tag cache full; dropping the tag seen longest ago");
    this->erase_tag_(oldest);
  }
  this->discovered_endpoint_.emplace_next() = DiscoveredEndpoint{.last_seen = App.get_loop_component_start_time(),
                                                                 .tag = this->build_tag_(protocol, uid),
                                                                 .id = 0,
                                                                 .protocol = protocol,
                                                                 .trig_called = false};
  ESP_LOGVV(TAG, "Tag added to cache");
  return this->discovered_endpoint_.size() - 1;
}

optional<size_t> PN71xx::find_tag_uid_(const nfc::NfcTagUid &uid) {
  if (!this->discovered_endpoint_.empty()) {
    for (size_t i = 0; i < this->discovered_endpoint_.size(); i++) {
      auto existing_tag_uid = this->discovered_endpoint_[i].tag->get_uid();
      bool uid_match = (uid.size() == existing_tag_uid.size());

      if (uid_match) {
        for (size_t i = 0; i < uid.size(); i++) {
          uid_match &= (uid[i] == existing_tag_uid[i]);
        }
        if (uid_match) {
          return i;
        }
      }
    }
  }
  return nullopt;
}

void PN71xx::purge_old_tags_() {
  // millis(), not the loop start time: last_seen is stamped after tag operations that may block for seconds
  const uint32_t now = millis();
  for (size_t i = this->discovered_endpoint_.size(); i > 0; i--) {
    if (now - this->discovered_endpoint_[i - 1].last_seen > this->tag_ttl_) {
      this->erase_tag_(i - 1);
    }
  }
}

void PN71xx::erase_tag_(const uint8_t tag_index) {
  if (tag_index < this->discovered_endpoint_.size()) {
#ifdef PN71XX_ON_TAG_REMOVED_TRIGGER_COUNT
    for (auto *trigger : this->triggers_ontagremoved_) {
      trigger->process(this->discovered_endpoint_[tag_index].tag);
    }
#endif
    for (auto *listener : this->tag_listeners_) {
      listener->tag_off(*this->discovered_endpoint_[tag_index].tag);
    }
    char uid_buf[nfc::FORMAT_UID_BUFFER_SIZE];
    ESP_LOGI(TAG, "Tag %s removed", nfc::format_uid_to(uid_buf, this->discovered_endpoint_[tag_index].tag->get_uid()));
    // keep the remaining entries in order; selecting_endpoint_ indexes into this list
    for (size_t i = tag_index; i + 1 < this->discovered_endpoint_.size(); i++) {
      this->discovered_endpoint_[i] = std::move(this->discovered_endpoint_[i + 1]);
    }
    // StaticVector::resize() does not destroy the dropped slot; free its tag now, not when the slot is reused
    this->discovered_endpoint_[this->discovered_endpoint_.size() - 1].tag.reset();
    this->discovered_endpoint_.resize(this->discovered_endpoint_.size() - 1);
  }
}

void PN71xx::nci_fsm_transition_() {
  switch (this->nci_state_) {
    case NCIState::NFCC_RESET:
      if (this->reset_core_(true, true) != nfc::STATUS_OK) {
        ESP_LOGE(TAG, "Failed to reset NCI core");
        this->nci_fsm_set_error_state_(NCIState::NFCC_RESET);
        return;
      } else {
        this->nci_fsm_set_state_(NCIState::NFCC_INIT);
      }
      [[fallthrough]];

    case NCIState::NFCC_INIT:
      if (this->init_core_() != nfc::STATUS_OK) {
        ESP_LOGE(TAG, "Failed to initialise NCI core");
        this->nci_fsm_set_error_state_(NCIState::NFCC_INIT);
        return;
      } else {
        this->nci_fsm_set_state_(NCIState::NFCC_CONFIG);
      }
      [[fallthrough]];

    case NCIState::NFCC_CONFIG:
      if (this->send_init_config_() != nfc::STATUS_OK) {
        ESP_LOGE(TAG, "Failed to send initial config");
        this->nci_fsm_set_error_state_(NCIState::NFCC_CONFIG);
        return;
      } else {
        this->config_refresh_pending_ = false;
        this->nci_fsm_set_state_(NCIState::NFCC_SET_DISCOVER_MAP);
      }
      [[fallthrough]];

    case NCIState::NFCC_SET_DISCOVER_MAP:
      if (this->set_discover_map_() != nfc::STATUS_OK) {
        ESP_LOGE(TAG, "Failed to set discover map");
        this->nci_fsm_set_error_state_(NCIState::NFCC_SET_DISCOVER_MAP);
        return;
      } else {
        this->nci_fsm_set_state_(NCIState::NFCC_SET_LISTEN_MODE_ROUTING);
      }
      [[fallthrough]];

    case NCIState::NFCC_SET_LISTEN_MODE_ROUTING:
      if (this->set_listen_mode_routing_() != nfc::STATUS_OK) {
        ESP_LOGE(TAG, "Failed to set listen mode routing");
        this->nci_fsm_set_error_state_(NCIState::NFCC_SET_LISTEN_MODE_ROUTING);
        return;
      } else {
        this->nci_fsm_set_state_(NCIState::RFST_IDLE);
      }
      [[fallthrough]];

    case NCIState::RFST_IDLE:
      if (this->nci_state_error_ == NCIState::RFST_DISCOVERY) {
        this->stop_discovery_();
      }

      if (this->config_refresh_pending_) {
        this->refresh_core_config_();
      }

      if (!this->listening_enabled_ && !this->polling_enabled_) {
        return;
      }

      if (this->start_discovery_() != nfc::STATUS_OK) {
        ESP_LOGV(TAG, "Failed to start discovery");
        this->nci_fsm_set_error_state_(NCIState::RFST_DISCOVERY);
      } else {
        this->nci_fsm_set_state_(NCIState::RFST_DISCOVERY);
      }
      return;

    case NCIState::RFST_W4_HOST_SELECT:
      select_endpoint_();
      [[fallthrough]];

    // All cases below are waiting for NOTIFICATION messages
    case NCIState::RFST_DISCOVERY:
      if (this->config_refresh_pending_) {
        this->refresh_core_config_();
      }
      [[fallthrough]];

    case NCIState::RFST_LISTEN_ACTIVE:
    case NCIState::RFST_LISTEN_SLEEP:
    case NCIState::RFST_POLL_ACTIVE:
    case NCIState::EP_SELECTING:
    case NCIState::EP_DEACTIVATING:
      // only a notification from the NFCC ends the EP_ states; if it was lost, recover rather than wait forever.
      // millis(), not the loop start time: the state is stamped after tag operations that may block for seconds.
      if ((this->nci_state_ == NCIState::EP_SELECTING || this->nci_state_ == NCIState::EP_DEACTIVATING) &&
          !this->irq_pin_->digital_read() && millis() - this->last_nci_state_change_ > NFCC_STATE_TIMEOUT) {
        ESP_LOGW(TAG, "Timed out waiting for notification in state %u; resetting NFCC", (uint8_t) this->nci_state_);
        this->nci_fsm_set_state_(NCIState::NFCC_RESET);
        return;
      }
      if (this->irq_pin_->digital_read()) {
        this->process_message_();
      }
      break;

    case NCIState::TEST:
    case NCIState::FAILED:
    case NCIState::NONE:
    default:
      return;
  }
}

void PN71xx::nci_fsm_set_state_(NCIState new_state) {
  ESP_LOGVV(TAG, "nci_fsm_set_state_(%u)", (uint8_t) new_state);
  this->nci_state_ = new_state;
  this->nci_state_error_ = NCIState::NONE;
  this->error_count_ = 0;
  this->last_nci_state_change_ = millis();
}

bool PN71xx::nci_fsm_set_error_state_(NCIState new_state) {
  ESP_LOGVV(TAG, "nci_fsm_set_error_state_(%u); error_count_ = %u", (uint8_t) new_state, this->error_count_);
  this->nci_state_error_ = new_state;
  if (this->error_count_++ > NFCC_MAX_ERROR_COUNT) {
    if ((this->nci_state_error_ == NCIState::NFCC_RESET) || (this->nci_state_error_ == NCIState::NFCC_INIT) ||
        (this->nci_state_error_ == NCIState::NFCC_CONFIG)) {
      ESP_LOGE(TAG, "Too many initialization failures -- check device connections");
      this->mark_failed();
      this->nci_fsm_set_state_(NCIState::FAILED);
    } else {
      ESP_LOGW(TAG, "Too many errors transitioning to state %u; resetting NFCC", (uint8_t) this->nci_state_error_);
      this->nci_fsm_set_state_(NCIState::NFCC_RESET);
    }
  }
  return this->error_count_ > NFCC_MAX_ERROR_COUNT;
}

void PN71xx::process_message_() {
  nfc::NciMessage rx;
  if (this->read_nfcc(rx, NFCC_DEFAULT_TIMEOUT) != nfc::STATUS_OK) {
    return;  // No data
  }

  switch (rx.get_message_type()) {
    case nfc::NCI_PKT_MT_CTRL_NOTIFICATION:
      if (rx.get_gid() == nfc::RF_GID) {
        switch (rx.get_oid()) {
          case nfc::RF_INTF_ACTIVATED_OID:
            ESP_LOGVV(TAG, "RF_INTF_ACTIVATED_OID");
            this->process_rf_intf_activated_oid_(rx);
            return;

          case nfc::RF_DISCOVER_OID:
            ESP_LOGVV(TAG, "RF_DISCOVER_OID");
            this->process_rf_discover_oid_(rx);
            return;

          case nfc::RF_DEACTIVATE_OID:
            ESP_LOGVV(TAG, "RF_DEACTIVATE_OID: type: 0x%02X, reason: 0x%02X", rx.get_message()[3], rx.get_message()[4]);
            this->process_rf_deactivate_oid_(rx);
            return;

          default:
            ESP_LOGV(TAG, "Unimplemented RF OID received: 0x%02X", rx.get_oid());
        }
      } else if (rx.get_gid() == nfc::NCI_CORE_GID) {
        switch (rx.get_oid()) {
          case nfc::NCI_CORE_GENERIC_ERROR_OID:
            ESP_LOGV(TAG, "NCI_CORE_GENERIC_ERROR_OID:");
            switch (rx.get_simple_status_response()) {
              case nfc::DISCOVERY_ALREADY_STARTED:
                ESP_LOGV(TAG, "  DISCOVERY_ALREADY_STARTED");
                break;

              case nfc::DISCOVERY_TARGET_ACTIVATION_FAILED:
                // Tag removed too soon
                ESP_LOGV(TAG, "  DISCOVERY_TARGET_ACTIVATION_FAILED");
                if (this->nci_state_ == NCIState::EP_SELECTING) {
                  this->nci_fsm_set_state_(NCIState::RFST_W4_HOST_SELECT);
                  if (!this->discovered_endpoint_.empty()) {
                    this->erase_tag_(this->selecting_endpoint_);
                  }
                } else {
                  this->stop_discovery_();
                  this->nci_fsm_set_state_(NCIState::RFST_IDLE);
                }
                break;

              case nfc::DISCOVERY_TEAR_DOWN:
                ESP_LOGV(TAG, "  DISCOVERY_TEAR_DOWN");
                break;

              default:
                ESP_LOGW(TAG, "Unknown error: 0x%02X", rx.get_simple_status_response());
                break;
            }
            break;

          default:
            ESP_LOGV(TAG, "Unimplemented NCI Core OID received: 0x%02X", rx.get_oid());
        }
      } else {
        char buf[nfc::FORMAT_BYTES_BUFFER_SIZE];
        ESP_LOGV(TAG, "Unimplemented notification: %s", nfc::format_bytes_to(buf, rx.get_message()));
      }
      break;

    case nfc::NCI_PKT_MT_CTRL_RESPONSE: {
      char buf[nfc::FORMAT_BYTES_BUFFER_SIZE];
      ESP_LOGV(TAG, "Unimplemented GID: 0x%02X  OID: 0x%02X  Full response: %s", rx.get_gid(), rx.get_oid(),
               nfc::format_bytes_to(buf, rx.get_message()));
      break;
    }

    case nfc::NCI_PKT_MT_CTRL_COMMAND: {
      char buf[nfc::FORMAT_BYTES_BUFFER_SIZE];
      ESP_LOGV(TAG, "Unimplemented command: %s", nfc::format_bytes_to(buf, rx.get_message()));
      break;
    }

    case nfc::NCI_PKT_MT_DATA:
      this->process_data_message_(rx);
      break;

    default: {
      char buf[nfc::FORMAT_BYTES_BUFFER_SIZE];
      ESP_LOGV(TAG, "Unimplemented message type: %s", nfc::format_bytes_to(buf, rx.get_message()));
      break;
    }
  }
}

void PN71xx::process_rf_intf_activated_oid_(nfc::NciMessage &rx) {  // an endpoint was activated
  uint8_t discovery_id = rx.get_message_byte(nfc::RF_INTF_ACTIVATED_NTF_DISCOVERY_ID);
  uint8_t interface = rx.get_message_byte(nfc::RF_INTF_ACTIVATED_NTF_INTERFACE);
  uint8_t protocol = rx.get_message_byte(nfc::RF_INTF_ACTIVATED_NTF_PROTOCOL);
  uint8_t mode_tech = rx.get_message_byte(nfc::RF_INTF_ACTIVATED_NTF_MODE_TECH);
  uint8_t max_size = rx.get_message_byte(nfc::RF_INTF_ACTIVATED_NTF_MAX_SIZE);

  ESP_LOGVV(TAG, "Endpoint activated -- interface: 0x%02X, protocol: 0x%02X, mode&tech: 0x%02X, max payload: %u",
            interface, protocol, mode_tech, max_size);

  if (mode_tech & nfc::MODE_LISTEN_MASK) {
    ESP_LOGVV(TAG, "Tag activated in listen mode");
    this->nci_fsm_set_state_(NCIState::RFST_LISTEN_ACTIVE);
    return;
  }

  this->nci_fsm_set_state_(NCIState::RFST_POLL_ACTIVE);
  if (rx.get_message().size() < nfc::RF_INTF_ACTIVATED_NTF_RF_TECH_PARAMS) {
    ESP_LOGE(TAG, "RF_INTF_ACTIVATED_NTF too short");
    this->stop_discovery_();
    this->nci_fsm_set_state_(NCIState::EP_DEACTIVATING);
    return;
  }
  nfc::NfcTagUid uid;
  if (!this->parse_uid_(mode_tech,
                        std::span<const uint8_t>(rx.get_message()).subspan(nfc::RF_INTF_ACTIVATED_NTF_RF_TECH_PARAMS),
                        uid)) {
    ESP_LOGE(TAG, "Could not build tag");
  } else {
    auto &working_endpoint = this->discovered_endpoint_[this->find_or_add_tag_(protocol, uid)];
    working_endpoint.id = discovery_id;
    working_endpoint.protocol = protocol;
    working_endpoint.last_seen = App.get_loop_component_start_time();

    switch (this->next_task_) {
      case EP_CLEAN:
        ESP_LOGD(TAG, "  Tag cleaning");
        if (this->clean_endpoint_(working_endpoint.protocol) != nfc::STATUS_OK) {
          ESP_LOGE(TAG, "  Tag cleaning incomplete");
        }
        ESP_LOGD(TAG, "  Tag cleaned!");
        break;

      case EP_FORMAT:
        ESP_LOGD(TAG, "  Tag formatting");
        if (this->format_endpoint_(working_endpoint.protocol) != nfc::STATUS_OK) {
          ESP_LOGE(TAG, "Error formatting tag as NDEF");
        }
        ESP_LOGD(TAG, "  Tag formatted!");
        break;

      case EP_WRITE:
        if (this->next_task_message_to_write_ != nullptr) {
          ESP_LOGD(TAG, "  Tag writing\n"
                        "  Tag formatting");
          if (this->format_endpoint_(working_endpoint.protocol) != nfc::STATUS_OK) {
            ESP_LOGE(TAG, "  Tag could not be formatted for writing");
          } else {
            ESP_LOGD(TAG, "  Writing NDEF data");
            if (this->write_endpoint_(working_endpoint.protocol, working_endpoint.tag->get_uid(),
                                      this->next_task_message_to_write_) != nfc::STATUS_OK) {
              ESP_LOGE(TAG, "  Failed to write message to tag");
            }
            ESP_LOGD(TAG, "  Finished writing NDEF data");
            this->next_task_message_to_write_ = nullptr;
            this->on_finished_write_callback_.call();
          }
        }
        break;

      case EP_READ:
      default:
        if (!working_endpoint.trig_called) {
          char uid_buf[nfc::FORMAT_UID_BUFFER_SIZE];
          ESP_LOGI(TAG, "Read tag type %s with UID %s", working_endpoint.tag->get_tag_type().c_str(),
                   nfc::format_uid_to(uid_buf, working_endpoint.tag->get_uid()));
          if (this->read_endpoint_data_(working_endpoint.protocol, *working_endpoint.tag) != nfc::STATUS_OK) {
            ESP_LOGW(TAG, "  Unable to read NDEF record(s)");
          } else if (working_endpoint.tag->has_ndef_message()) {
            const auto &message = working_endpoint.tag->get_ndef_message();
            const auto &records = message->get_records();
            ESP_LOGD(TAG, "  NDEF record(s):");
            for (const auto &record : records) {
              ESP_LOGD(TAG, "    %s - %s", record->get_type().c_str(), record->get_payload().c_str());
            }
          } else {
            ESP_LOGW(TAG, "  No NDEF records found");
          }
#ifdef PN71XX_ON_TAG_TRIGGER_COUNT
          for (auto *trigger : this->triggers_ontag_) {
            trigger->process(working_endpoint.tag);
          }
#endif
          for (auto *listener : this->tag_listeners_) {
            listener->tag_on(*working_endpoint.tag);
          }
          working_endpoint.trig_called = true;
          break;
        }
    }
    // the tag was present for the whole operation, which may have taken longer than tag_ttl
    working_endpoint.last_seen = millis();
    if (working_endpoint.protocol == nfc::PROT_MIFARE) {
      this->halt_mifare_classic_tag_();
    }
  }
  if (this->next_task_ != EP_READ) {
    this->read_mode();
  }

  this->stop_discovery_();
  this->nci_fsm_set_state_(NCIState::EP_DEACTIVATING);
}

void PN71xx::process_rf_discover_oid_(nfc::NciMessage &rx) {
  if (rx.get_message().size() < nfc::RF_DISCOVER_NTF_RF_TECH_PARAMS) {
    ESP_LOGE(TAG, "RF_DISCOVER_NTF too short");
    return;
  }
  const uint8_t protocol = rx.get_message_byte(nfc::RF_DISCOVER_NTF_PROTOCOL);
  nfc::NfcTagUid uid;
  if (!this->parse_uid_(rx.get_message_byte(nfc::RF_DISCOVER_NTF_MODE_TECH),
                        std::span<const uint8_t>(rx.get_message()).subspan(nfc::RF_DISCOVER_NTF_RF_TECH_PARAMS), uid)) {
    ESP_LOGE(TAG, "Could not build tag!");
  } else {
    auto &endpoint = this->discovered_endpoint_[this->find_or_add_tag_(protocol, uid)];
    endpoint.id = rx.get_message_byte(nfc::RF_DISCOVER_NTF_DISCOVERY_ID);
    endpoint.protocol = protocol;
    endpoint.last_seen = App.get_loop_component_start_time();
  }

  const auto &ntf = rx.get_message();
  if (ntf[ntf.size() - 1] != nfc::RF_DISCOVER_NTF_NT_MORE) {
    this->nci_fsm_set_state_(NCIState::RFST_W4_HOST_SELECT);
    ESP_LOGVV(TAG, "Discovered %zu endpoints", this->discovered_endpoint_.size());
  }
}

void PN71xx::process_rf_deactivate_oid_(nfc::NciMessage &rx) {
  this->ce_state_ = CardEmulationState::CARD_EMU_IDLE;

  switch (rx.get_simple_status_response()) {
    case nfc::DEACTIVATION_TYPE_DISCOVERY:
      this->nci_fsm_set_state_(NCIState::RFST_DISCOVERY);
      break;

    case nfc::DEACTIVATION_TYPE_IDLE:
      this->nci_fsm_set_state_(NCIState::RFST_IDLE);
      break;

    case nfc::DEACTIVATION_TYPE_SLEEP:
    case nfc::DEACTIVATION_TYPE_SLEEP_AF:
      if (this->nci_state_ == NCIState::RFST_LISTEN_ACTIVE) {
        this->nci_fsm_set_state_(NCIState::RFST_LISTEN_SLEEP);
      } else if (this->nci_state_ == NCIState::RFST_POLL_ACTIVE) {
        this->nci_fsm_set_state_(NCIState::RFST_W4_HOST_SELECT);
      } else {
        this->nci_fsm_set_state_(NCIState::RFST_IDLE);
      }
      break;

    default:
      break;
  }
}

void PN71xx::process_data_message_(nfc::NciMessage &rx) {
  char buf[nfc::FORMAT_BYTES_BUFFER_SIZE];
  ESP_LOGVV(TAG, "Received data message: %s", nfc::format_bytes_to(buf, rx.get_message()));

  CardEmuResponse ndef_response;
  this->card_emu_t4t_get_response_(rx.get_message(), ndef_response);

  if (ndef_response.empty()) {
    return;  // no message returned, we cannot respond
  }

  nfc::NciMessage tx(nfc::NCI_PKT_MT_DATA, ndef_response);
  ESP_LOGVV(TAG, "Sending data message: %s", nfc::format_bytes_to(buf, tx.get_message()));
  if (this->transceive_(tx, rx, NFCC_DEFAULT_TIMEOUT, false) != nfc::STATUS_OK) {
    ESP_LOGE(TAG, "Sending reply for card emulation failed");
  }
}

bool PN71xx::card_emu_t4t_read_ndef_(const uint16_t offset, const uint8_t length, CardEmuResponse &ndef_response) {
  const auto &ndef_message = this->card_emulation_ndef_;
  // the NDEF file is a two-byte big-endian length (NLEN) followed by the message
  const uint16_t ndef_msg_size = ndef_message.size();
  const uint32_t file_size = ndef_msg_size + 2;
  // the reply must also hold the two status bytes; the CC's MLe keeps well-behaved readers below this
  if (offset + static_cast<uint32_t>(length) > file_size ||
      length + sizeof(CARD_EMU_T4T_OK) > CardEmuResponse::capacity()) {
    return false;
  }
  for (uint32_t i = offset; i < offset + static_cast<uint32_t>(length); i++) {
    if (i == 0) {
      ndef_response.push_back(ndef_msg_size >> 8);
    } else if (i == 1) {
      ndef_response.push_back(ndef_msg_size & 0xFF);
    } else {
      ndef_response.push_back(ndef_message[i - 2]);
    }
  }
  if (offset + static_cast<uint32_t>(length) == file_size) {
    ESP_LOGD(TAG, "NDEF message sent");
    this->on_emulated_tag_scan_callback_.call();
  }
  return true;
}

void PN71xx::card_emu_t4t_get_response_(const std::span<const uint8_t> response, CardEmuResponse &ndef_response) {
  ndef_response.clear();
  if (this->card_emulation_ndef_.empty()) {
    ESP_LOGE(TAG, "No NDEF message is set; tag emulation not possible");
    return;
  }
  if (response.size() < nfc::NCI_PKT_HEADER_SIZE) {
    return;
  }

  const auto apdu_begin = response.begin() + nfc::NCI_PKT_HEADER_SIZE;
  const size_t apdu_size = response.size() - nfc::NCI_PKT_HEADER_SIZE;
  auto apdu_is = [&](const uint8_t *cmd, size_t cmd_size) {
    return apdu_size == cmd_size && std::equal(apdu_begin, response.end(), cmd);
  };
  auto apdu_starts_with = [&](const uint8_t *cmd, size_t cmd_size) {
    return apdu_size >= cmd_size && std::equal(cmd, cmd + cmd_size, apdu_begin);
  };
  auto append = [&](std::span<const uint8_t> bytes) {
    for (const uint8_t byte : bytes) {
      ndef_response.push_back(byte);
    }
  };
  bool ok = false;

  if (apdu_is(CARD_EMU_T4T_APP_SELECT, sizeof(CARD_EMU_T4T_APP_SELECT)) ||
      apdu_is(CARD_EMU_T4T_APP_SELECT, sizeof(CARD_EMU_T4T_APP_SELECT) - 1)) {  // Le is optional
    ESP_LOGVV(TAG, "CARD_EMU_NDEF_APP_SELECTED");
    this->ce_state_ = CardEmulationState::CARD_EMU_NDEF_APP_SELECTED;
    ok = true;
  } else if (apdu_is(CARD_EMU_T4T_CC_SELECT, sizeof(CARD_EMU_T4T_CC_SELECT))) {
    if (this->ce_state_ == CardEmulationState::CARD_EMU_NDEF_APP_SELECTED) {
      ESP_LOGVV(TAG, "CARD_EMU_CC_SELECTED");
      this->ce_state_ = CardEmulationState::CARD_EMU_CC_SELECTED;
      ok = true;
    }
  } else if (apdu_is(CARD_EMU_T4T_NDEF_SELECT, sizeof(CARD_EMU_T4T_NDEF_SELECT))) {
    ESP_LOGVV(TAG, "CARD_EMU_NDEF_SELECTED");
    this->ce_state_ = CardEmulationState::CARD_EMU_NDEF_SELECTED;
    ok = true;
  } else if (apdu_starts_with(CARD_EMU_T4T_READ, sizeof(CARD_EMU_T4T_READ)) && apdu_size == 5) {
    // READ BINARY: CLA INS P1 P2 Le, where P1-P2 is the offset
    const uint16_t offset = (apdu_begin[2] << 8) | apdu_begin[3];
    const uint8_t length = apdu_begin[4];
    if (this->ce_state_ == CardEmulationState::CARD_EMU_CC_SELECTED) {
      ESP_LOGVV(TAG, "CARD_EMU_T4T_READ with CARD_EMU_CC_SELECTED");
      if (offset + static_cast<uint32_t>(length) <= sizeof(CARD_EMU_T4T_CC)) {
        append(std::span<const uint8_t>(CARD_EMU_T4T_CC).subspan(offset, length));
        ok = true;
      }
    } else if (this->ce_state_ == CardEmulationState::CARD_EMU_NDEF_SELECTED) {
      ESP_LOGVV(TAG, "CARD_EMU_T4T_READ with CARD_EMU_NDEF_SELECTED");
      ok = this->card_emu_t4t_read_ndef_(offset, length, ndef_response);
    }
  } else if (apdu_starts_with(CARD_EMU_T4T_WRITE, sizeof(CARD_EMU_T4T_WRITE)) && apdu_size >= 5) {
    // UPDATE BINARY: CLA INS P1 P2 Lc data
    const uint8_t length = apdu_begin[4];
    if (this->ce_state_ == CardEmulationState::CARD_EMU_NDEF_SELECTED && apdu_size >= 5u + length) {
      ESP_LOGVV(TAG, "CARD_EMU_T4T_WRITE");
      char write_buf[nfc::FORMAT_BYTES_BUFFER_SIZE];
      ESP_LOGD(TAG, "Received %u-byte NDEF message: %s", length,
               nfc::format_bytes_to(write_buf, response.subspan(nfc::NCI_PKT_HEADER_SIZE + 5, length)));
      ok = true;
    }
  }

  if (ok) {
    append(CARD_EMU_T4T_OK);
  } else {
    ndef_response.clear();
    append(CARD_EMU_T4T_NOK);
    this->ce_state_ = CardEmulationState::CARD_EMU_IDLE;
  }
}

uint8_t PN71xx::transceive_(nfc::NciMessage &tx, nfc::NciMessage &rx, const uint16_t timeout,
                            const bool expect_notification) {
  char buf[nfc::FORMAT_BYTES_BUFFER_SIZE];

  // The NFCC may refuse a frame while it wakes from standby; the whole frame must then be sent again.
  // A command must never be re-sent once it was accepted, as the NFCC will answer it (NCI 1.0, 3.2.1).
  uint8_t attempts = 0;
  while (this->write_nfcc(tx) != nfc::STATUS_OK) {
    if (++attempts >= NFCC_MAX_COMM_FAILS) {
      ESP_LOGE(TAG, "Error sending message");
      return nfc::STATUS_FAILED;
    }
    delay(NFCC_WRITE_RETRY_DELAY);
  }
  ESP_LOGVV(TAG, "Wrote: %s", nfc::format_bytes_to(buf, tx.get_message()));

  if (!tx.message_type_is(nfc::NCI_PKT_MT_DATA)) {
    // Notifications may already be queued ahead of the response; skip them. They carry the same GID and OID
    // as some responses (e.g. RF_DEACTIVATE_NTF), so the message type must be checked. A response to an earlier
    // command whose read timed out (a tag leaving the field delays RF_DEACTIVATE_RSP) is skipped the same way.
    for (uint8_t i = 0; i < NFCC_MAX_COMM_FAILS; i++) {
      if (this->read_nfcc(rx, timeout) != nfc::STATUS_OK) {
        ESP_LOGW(TAG, "Error receiving response");
        return nfc::STATUS_FAILED;
      }
      ESP_LOGVV(TAG, "Read: %s", nfc::format_bytes_to(buf, rx.get_message()));
      if (rx.message_type_is(nfc::NCI_PKT_MT_CTRL_RESPONSE) && rx.get_gid() == tx.get_gid() &&
          rx.get_oid() == tx.get_oid()) {
        break;
      }
      ESP_LOGW(TAG, "Discarding message received while waiting for response: %s",
               nfc::format_bytes_to(buf, rx.get_message()));
    }
    // for commands, the GID and OID should match and the status should be OK
    if (!rx.message_type_is(nfc::NCI_PKT_MT_CTRL_RESPONSE) || (rx.get_gid() != tx.get_gid()) ||
        (rx.get_oid() != tx.get_oid())) {
      ESP_LOGE(TAG, "Incorrect response to command: %s", nfc::format_bytes_to(buf, rx.get_message()));
      return nfc::STATUS_FAILED;
    }

    if (!rx.simple_status_response_is(nfc::STATUS_OK)) {
      ESP_LOGE(TAG, "Error in response to command: %s", nfc::format_bytes_to(buf, rx.get_message()));
    }
    return rx.get_simple_status_response();
  }

  // when sending data to the endpoint, the first message is the credit notification from the NFCC
  if (this->read_nfcc(rx, timeout) != nfc::STATUS_OK) {
    ESP_LOGW(TAG, "Error receiving credit notification");
    return nfc::STATUS_FAILED;
  }
  ESP_LOGVV(TAG, "Read: %s", nfc::format_bytes_to(buf, rx.get_message()));
  if ((!rx.message_type_is(nfc::NCI_PKT_MT_CTRL_NOTIFICATION)) || (!rx.gid_is(nfc::NCI_CORE_GID)) ||
      (!rx.oid_is(nfc::NCI_CORE_CONN_CREDITS_OID)) || (!rx.message_length_is(3))) {
    ESP_LOGE(TAG, "Incorrect response to data message: %s", nfc::format_bytes_to(buf, rx.get_message()));
    return nfc::STATUS_FAILED;
  }

  if (expect_notification) {
    // the endpoint's answer follows in a data message
    if (this->read_nfcc(rx, timeout) != nfc::STATUS_OK) {
      ESP_LOGE(TAG, "Error receiving data from endpoint");
      return nfc::STATUS_FAILED;
    }
    ESP_LOGVV(TAG, "Read: %s", nfc::format_bytes_to(buf, rx.get_message()));
  }

  return nfc::STATUS_OK;
}

void fill_ndef_tlv(const std::vector<uint8_t> &message, const uint32_t buffer_length, FixedVector<uint8_t> &buffer) {
  buffer.init(buffer_length);
  buffer.push_back(0x03);
  if (message.size() < 255) {
    buffer.push_back(message.size());
  } else {
    buffer.push_back(0xFF);
    buffer.push_back((message.size() >> 8) & 0xFF);
    buffer.push_back(message.size() & 0xFF);
  }
  for (const uint8_t byte : message) {
    buffer.push_back(byte);
  }
  buffer.push_back(0xFE);
  while (buffer.size() < buffer_length) {
    buffer.push_back(0x00);
  }
}

uint8_t PN71xx::wait_for_irq_(uint16_t timeout, bool pin_state) {
  auto start_time = millis();

  while (millis() - start_time < timeout) {
    if (this->irq_pin_->digital_read() == pin_state) {
      return nfc::STATUS_OK;
    }
  }
  return nfc::STATUS_FAILED;
}

}  // namespace esphome::pn71xx
