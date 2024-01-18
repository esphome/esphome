#include "automation.h"
#include "pn7150.h"

#include <utility>

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace pn7150 {

static const char *const TAG = "pn7150";

void PN7150::setup() {
  this->irq_pin_->setup();
  this->ven_pin_->setup();

  this->nci_fsm_transition_();  // kick off reset & init processes
}

void PN7150::dump_config() {
  ESP_LOGCONFIG(TAG, "PN7150:");
  LOG_PIN("  IRQ pin: ", this->irq_pin_);
  LOG_PIN("  VEN pin: ", this->ven_pin_);
}

void PN7150::loop() {
  this->nci_fsm_transition_();
  this->purge_old_tags_();
}

void PN7150::set_tag_emulation_message(std::shared_ptr<nfc::NdefMessage> message) {
  this->card_emulation_message_ = std::move(message);
  ESP_LOGD(TAG, "Tag emulation message set");
}

void PN7150::set_tag_emulation_message(const optional<std::string> &message,
                                       const optional<bool> include_android_app_record) {
  if (!message.has_value()) {
    return;
  }

  auto ndef_message = make_unique<nfc::NdefMessage>();

  ndef_message->add_uri_record(message.value());

  if (!include_android_app_record.has_value() || include_android_app_record.value()) {
    auto ext_record = make_unique<nfc::NdefRecord>();
    ext_record->set_tnf(nfc::TNF_EXTERNAL_TYPE);
    ext_record->set_type(nfc::HA_TAG_ID_EXT_RECORD_TYPE);
    ext_record->set_payload(nfc::HA_TAG_ID_EXT_RECORD_PAYLOAD);
    ndef_message->add_record(std::move(ext_record));
  }

  this->card_emulation_message_ = std::move(ndef_message);
  ESP_LOGD(TAG, "Tag emulation message set");
}

void PN7150::set_tag_emulation_message(const char *message, const bool include_android_app_record) {
  this->set_tag_emulation_message(std::string(message), include_android_app_record);
}

void PN7150::set_tag_emulation_off() {
  if (this->listening_enabled_) {
    this->listening_enabled_ = false;
    this->config_refresh_pending_ = true;
  }
  ESP_LOGD(TAG, "Tag emulation disabled");
}

void PN7150::set_tag_emulation_on() {
  if (this->card_emulation_message_ == nullptr) {
    ESP_LOGE(TAG, "No NDEF message is set; tag emulation cannot be enabled");
    return;
  }
  if (!this->listening_enabled_) {
    this->listening_enabled_ = true;
    this->config_refresh_pending_ = true;
  }
  ESP_LOGD(TAG, "Tag emulation enabled");
}

void PN7150::set_polling_off() {
  if (this->polling_enabled_) {
    this->polling_enabled_ = false;
    this->config_refresh_pending_ = true;
  }
  ESP_LOGD(TAG, "Tag polling disabled");
}

void PN7150::set_polling_on() {
  if (!this->polling_enabled_) {
    this->polling_enabled_ = true;
    this->config_refresh_pending_ = true;
  }
  ESP_LOGD(TAG, "Tag polling enabled");
}

void PN7150::read_mode() {
  this->next_task_ = EP_READ;
  ESP_LOGD(TAG, "Waiting to read next tag");
}

void PN7150::clean_mode() {
  this->next_task_ = EP_CLEAN;
  ESP_LOGD(TAG, "Waiting to clean next tag");
}

void PN7150::format_mode() {
  this->next_task_ = EP_FORMAT;
  ESP_LOGD(TAG, "Waiting to format next tag");
}

void PN7150::write_mode() {
  if (this->next_task_message_to_write_ == nullptr) {
    ESP_LOGW(TAG, "Message to write must be set before setting write mode");
    return;
  }

  this->next_task_ = EP_WRITE;
  ESP_LOGD(TAG, "Waiting to write next tag");
}

void PN7150::set_tag_write_message(std::shared_ptr<nfc::NdefMessage> message) {
  this->next_task_message_to_write_ = std::move(message);
  ESP_LOGD(TAG, "Message to write has been set");
}

void PN7150::set_tag_write_message(optional<std::string> message, optional<bool> include_android_app_record) {
  if (!message.has_value()) {
    return;
  }

  auto ndef_message = make_unique<nfc::NdefMessage>();

  ndef_message->add_uri_record(message.value());

  if (!include_android_app_record.has_value() || include_android_app_record.value()) {
    auto ext_record = make_unique<nfc::NdefRecord>();
    ext_record->set_tnf(nfc::TNF_EXTERNAL_TYPE);
    ext_record->set_type(nfc::HA_TAG_ID_EXT_RECORD_TYPE);
    ext_record->set_payload(nfc::HA_TAG_ID_EXT_RECORD_PAYLOAD);
    ndef_message->add_record(std::move(ext_record));
  }

  this->next_task_message_to_write_ = std::move(ndef_message);
  ESP_LOGD(TAG, "Message to write has been set");
}

uint8_t PN7150::set_test_mode(const TestMode test_mode, const std::vector<uint8_t> &data,
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
    result = rx.get_message();
    result.erase(result.begin(), result.begin() + 4);  // remove NCI header
    if (!result.empty()) {
      ESP_LOGW(TAG, "Test results: %s", nfc::format_bytes(result).c_str());
    }
  }
  return status;
}

uint8_t PN7150::reset_core_(const bool reset_config, const bool power) {
  if (power) {
    this->ven_pin_->digital_write(true);
    delay(NFCC_DEFAULT_TIMEOUT);
    this->ven_pin_->digital_write(false);
    delay(NFCC_DEFAULT_TIMEOUT);
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
    ESP_LOGE(TAG, "Invalid reset response: %s", nfc::format_bytes(rx.get_message()).c_str());
    return rx.get_simple_status_response();
  }
  // verify reset response
  if ((!rx.message_type_is(nfc::NCI_PKT_MT_CTRL_RESPONSE)) || (!rx.message_length_is(3)) ||
      (rx.get_message()[nfc::NCI_PKT_PAYLOAD_OFFSET + 1] != 0x11) ||
      (rx.get_message()[nfc::NCI_PKT_PAYLOAD_OFFSET + 2] != (uint8_t) reset_config)) {
    ESP_LOGE(TAG, "Reset response was malformed: %s", nfc::format_bytes(rx.get_message()).c_str());
    return nfc::STATUS_FAILED;
  }

  ESP_LOGD(TAG, "Configuration %s", rx.get_message()[nfc::NCI_PKT_PAYLOAD_OFFSET + 2] ? "reset" : "retained");
  ESP_LOGD(TAG, "NCI version: %s", rx.get_message()[nfc::NCI_PKT_PAYLOAD_OFFSET + 1] == 0x20 ? "2.0" : "1.0");

  return nfc::STATUS_OK;
}

uint8_t PN7150::init_core_() {
  nfc::NciMessage rx;
  nfc::NciMessage tx(nfc::NCI_PKT_MT_CTRL_COMMAND, nfc::NCI_CORE_GID, nfc::NCI_CORE_INIT_OID);

  if (this->transceive_(tx, rx) != nfc::STATUS_OK) {
    ESP_LOGE(TAG, "Error sending initialise command");
    return nfc::STATUS_FAILED;
  }

  if (!rx.simple_status_response_is(nfc::STATUS_OK)) {
    ESP_LOGE(TAG, "Invalid initialise response: %s", nfc::format_bytes(rx.get_message()).c_str());
    return nfc::STATUS_FAILED;
  }

  uint8_t manf_id = rx.get_message()[15 + rx.get_message()[8]];
  uint8_t hw_version = rx.get_message()[16 + rx.get_message()[8]];
  uint8_t rom_code_version = rx.get_message()[17 + rx.get_message()[8]];
  uint8_t flash_major_version = rx.get_message()[18 + rx.get_message()[8]];
  uint8_t flash_minor_version = rx.get_message()[19 + rx.get_message()[8]];

  ESP_LOGD(TAG, "Manufacturer ID: 0x%02X", manf_id);
  ESP_LOGD(TAG, "Hardware version: 0x%02X", hw_version);
  ESP_LOGD(TAG, "ROM code version: 0x%02X", rom_code_version);
  ESP_LOGD(TAG, "FLASH major version: 0x%02X", flash_major_version);
  ESP_LOGD(TAG, "FLASH minor version: 0x%02X", flash_minor_version);

  return rx.get_simple_status_response();
}

uint8_t PN7150::send_init_config_() {
  nfc::NciMessage rx;
  nfc::NciMessage tx(nfc::NCI_PKT_MT_CTRL_COMMAND, nfc::NCI_PROPRIETARY_GID, nfc::NCI_CORE_SET_CONFIG_OID);

  if (this->transceive_(tx, rx) != nfc::STATUS_OK) {
    ESP_LOGE(TAG, "Error enabling proprietary extensions");
    return nfc::STATUS_FAILED;
  }

  tx.set_message(nfc::NCI_PKT_MT_CTRL_COMMAND, nfc::NCI_CORE_GID, nfc::NCI_CORE_SET_CONFIG_OID,
                 std::vector<uint8_t>(std::begin(PMU_CFG), std::end(PMU_CFG)));

  if (this->transceive_(tx, rx) != nfc::STATUS_OK) {
    ESP_LOGE(TAG, "Error sending PMU config");
    return nfc::STATUS_FAILED;
  }

  return this->send_core_config_();
}

uint8_t PN7150::send_core_config_() {
  const auto *core_config_begin = std::begin(CORE_CONFIG_SOLO);
  const auto *core_config_end = std::end(CORE_CONFIG_SOLO);
  this->core_config_is_solo_ = true;

  if (this->listening_enabled_ && this->polling_enabled_) {
    core_config_begin = std::begin(CORE_CONFIG_RW_CE);
    core_config_end = std::end(CORE_CONFIG_RW_CE);
    this->core_config_is_solo_ = false;
  }

  nfc::NciMessage rx;
  nfc::NciMessage tx(nfc::NCI_PKT_MT_CTRL_COMMAND, nfc::NCI_CORE_GID, nfc::NCI_CORE_SET_CONFIG_OID,
                     std::vector<uint8_t>(core_config_begin, core_config_end));

  if (this->transceive_(tx, rx) != nfc::STATUS_OK) {
    ESP_LOGW(TAG, "Error sending core config");
    return nfc::STATUS_FAILED;
  }

  return nfc::STATUS_OK;
}

uint8_t PN7150::refresh_core_config_() {
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

uint8_t PN7150::set_discover_map_() {
  std::vector<uint8_t> discover_map = {sizeof(RF_DISCOVER_MAP_CONFIG) / 3};
  discover_map.insert(discover_map.end(), std::begin(RF_DISCOVER_MAP_CONFIG), std::end(RF_DISCOVER_MAP_CONFIG));

  nfc::NciMessage rx;
  nfc::NciMessage tx(nfc::NCI_PKT_MT_CTRL_COMMAND, nfc::RF_GID, nfc::RF_DISCOVER_MAP_OID, discover_map);

  if (this->transceive_(tx, rx, NFCC_INIT_TIMEOUT) != nfc::STATUS_OK) {
    ESP_LOGE(TAG, "Error sending discover map poll config");
    return nfc::STATUS_FAILED;
  }
  return nfc::STATUS_OK;
}

uint8_t PN7150::set_listen_mode_routing_() {
  nfc::NciMessage rx;
  nfc::NciMessage tx(
      nfc::NCI_PKT_MT_CTRL_COMMAND, nfc::RF_GID, nfc::RF_SET_LISTEN_MODE_ROUTING_OID,
      std::vector<uint8_t>(std::begin(RF_LISTEN_MODE_ROUTING_CONFIG), std::end(RF_LISTEN_MODE_ROUTING_CONFIG)));

  if (this->transceive_(tx, rx, NFCC_INIT_TIMEOUT) != nfc::STATUS_OK) {
    ESP_LOGE(TAG, "Error setting listen mode routing config");
    return nfc::STATUS_FAILED;
  }
  return nfc::STATUS_OK;
}

uint8_t PN7150::start_discovery_() {
  const uint8_t *rf_discovery_config = RF_DISCOVERY_CONFIG;
  uint8_t length = sizeof(RF_DISCOVERY_CONFIG);

  if (!this->listening_enabled_) {
    length = sizeof(RF_DISCOVERY_POLL_CONFIG);
    rf_discovery_config = RF_DISCOVERY_POLL_CONFIG;
  } else if (!this->polling_enabled_) {
    length = sizeof(RF_DISCOVERY_LISTEN_CONFIG);
    rf_discovery_config = RF_DISCOVERY_LISTEN_CONFIG;
  }

  std::vector<uint8_t> discover_config = std::vector<uint8_t>((length * 2) + 1);

  discover_config[0] = length;
  for (uint8_t i = 0; i < length; i++) {
    discover_config[(i * 2) + 1] = rf_discovery_config[i];
    discover_config[(i * 2) + 2] = 0x01;  // RF Technology and Mode will be executed in every discovery period
  }

  nfc::NciMessage rx;
  nfc::NciMessage tx(nfc::NCI_PKT_MT_CTRL_COMMAND, nfc::RF_GID, nfc::RF_DISCOVER_OID, discover_config);

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

uint8_t PN7150::stop_discovery_() { return this->deactivate_(nfc::DEACTIVATION_TYPE_IDLE, NFCC_TAG_WRITE_TIMEOUT); }

uint8_t PN7150::deactivate_(const uint8_t type, const uint16_t timeout) {
  nfc::NciMessage rx;
  nfc::NciMessage tx(nfc::NCI_PKT_MT_CTRL_COMMAND, nfc::RF_GID, nfc::RF_DEACTIVATE_OID, {type});

  auto status = this->transceive_(tx, rx, timeout);
  // if (status != nfc::STATUS_OK) {
  //   ESP_LOGE(TAG, "Error sending deactivate type %u", type);
  //   return nfc::STATUS_FAILED;
  // }
  return status;
}

void PN7150::select_endpoint_() {
  if (this->discovered_endpoint_.empty()) {
    ESP_LOGW(TAG, "No cached tags to select");
    this->stop_discovery_();
    this->nci_fsm_set_state_(NCIState::RFST_IDLE);
    return;
  }
  std::vector<uint8_t> endpoint_data = {this->discovered_endpoint_[0].id, this->discovered_endpoint_[0].protocol,
                                        0x01};  // that last byte is the interface ID
  for (size_t i = 0; i < this->discovered_endpoint_.size(); i++) {
    if (!this->discovered_endpoint_[i].trig_called) {
      endpoint_data = {this->discovered_endpoint_[i].id, this->discovered_endpoint_[i].protocol,
                       0x01};  // that last byte is the interface ID
      this->selecting_endpoint_ = i;
      break;
    }
  }

  nfc::NciMessage rx;
  nfc::NciMessage tx(nfc::NCI_PKT_MT_CTRL_COMMAND, nfc::RF_GID, nfc::RF_DISCOVER_SELECT_OID, endpoint_data);

  if (this->transceive_(tx, rx) != nfc::STATUS_OK) {
    ESP_LOGE(TAG, "Error selecting endpoint");
  } else {
    this->nci_fsm_set_state_(NCIState::EP_SELECTING);
  }
}

uint8_t PN7150::read_endpoint_data_(nfc::NfcTag &tag) {
  uint8_t type = nfc::guess_tag_type(tag.get_uid().size());

  switch (type) {
    case nfc::TAG_TYPE_MIFARE_CLASSIC:
      ESP_LOGV(TAG, "Reading Mifare classic");
      return this->read_mifare_classic_tag_(tag);

    case nfc::TAG_TYPE_2:
      ESP_LOGV(TAG, "Reading Mifare ultralight");
      return this->read_mifare_ultralight_tag_(tag);

    case nfc::TAG_TYPE_UNKNOWN:
    default:
      ESP_LOGV(TAG, "Cannot determine tag type");
      break;
  }
  return nfc::STATUS_FAILED;
}

uint8_t PN7150::clean_endpoint_(std::vector<uint8_t> &uid) {
  uint8_t type = nfc::guess_tag_type(uid.size());
  switch (type) {
    case nfc::TAG_TYPE_MIFARE_CLASSIC:
      return this->format_mifare_classic_mifare_();

    case nfc::TAG_TYPE_2:
      return this->clean_mifare_ultralight_();

    default:
      ESP_LOGE(TAG, "Unsupported tag for cleaning");
      break;
  }
  return nfc::STATUS_FAILED;
}

uint8_t PN7150::format_endpoint_(std::vector<uint8_t> &uid) {
  uint8_t type = nfc::guess_tag_type(uid.size());
  switch (type) {
    case nfc::TAG_TYPE_MIFARE_CLASSIC:
      return this->format_mifare_classic_ndef_();

    case nfc::TAG_TYPE_2:
      return this->clean_mifare_ultralight_();

    default:
      ESP_LOGE(TAG, "Unsupported tag for formatting");
      break;
  }
  return nfc::STATUS_FAILED;
}

uint8_t PN7150::write_endpoint_(std::vector<uint8_t> &uid, std::shared_ptr<nfc::NdefMessage> &message) {
  uint8_t type = nfc::guess_tag_type(uid.size());
  switch (type) {
    case nfc::TAG_TYPE_MIFARE_CLASSIC:
      return this->write_mifare_classic_tag_(message);

    case nfc::TAG_TYPE_2:
      return this->write_mifare_ultralight_tag_(uid, message);

    default:
      ESP_LOGE(TAG, "Unsupported tag for writing");
      break;
  }
  return nfc::STATUS_FAILED;
}

std::unique_ptr<nfc::NfcTag> PN7150::build_tag_(const uint8_t mode_tech, const std::vector<uint8_t> &data) {
  switch (mode_tech) {
    case (nfc::MODE_POLL | nfc::TECH_PASSIVE_NFCA): {
      uint8_t uid_length = data[2];
      if (!uid_length) {
        ESP_LOGE(TAG, "UID length cannot be zero");
        return nullptr;
      }
      std::vector<uint8_t> uid(data.begin() + 3, data.begin() + 3 + uid_length);
      const auto *tag_type_str =
          nfc::guess_tag_type(uid_length) == nfc::TAG_TYPE_MIFARE_CLASSIC ? nfc::MIFARE_CLASSIC : nfc::NFC_FORUM_TYPE_2;
      return make_unique<nfc::NfcTag>(uid, tag_type_str);
    }
  }
  return nullptr;
}

optional<size_t> PN7150::find_tag_uid_(const std::vector<uint8_t> &uid) {
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

void PN7150::purge_old_tags_() {
  for (size_t i = 0; i < this->discovered_endpoint_.size(); i++) {
    if (millis() - this->discovered_endpoint_[i].last_seen > this->tag_ttl_) {
      this->erase_tag_(i);
    }
  }
}

void PN7150::erase_tag_(const uint8_t tag_index) {
  if (tag_index < this->discovered_endpoint_.size()) {
    for (auto *trigger : this->triggers_ontagremoved_) {
      trigger->process(this->discovered_endpoint_[tag_index].tag);
    }
    for (auto *listener : this->tag_listeners_) {
      listener->tag_off(*this->discovered_endpoint_[tag_index].tag);
    }
    ESP_LOGI(TAG, "Tag %s removed", nfc::format_uid(this->discovered_endpoint_[tag_index].tag->get_uid()).c_str());
    this->discovered_endpoint_.erase(this->discovered_endpoint_.begin() + tag_index);
  }
}

void PN7150::nci_fsm_transition_() {
  switch (this->nci_state_) {
    case NCIState::NFCC_RESET:
      if (this->reset_core_(true, true) != nfc::STATUS_OK) {
        ESP_LOGE(TAG, "Failed to reset NCI core");
        this->nci_fsm_set_error_state_(NCIState::NFCC_RESET);
        return;
      } else {
        this->nci_fsm_set_state_(NCIState::NFCC_INIT);
      }
      // fall through

    case NCIState::NFCC_INIT:
      if (this->init_core_() != nfc::STATUS_OK) {
        ESP_LOGE(TAG, "Failed to initialise NCI core");
        this->nci_fsm_set_error_state_(NCIState::NFCC_INIT);
        return;
      } else {
        this->nci_fsm_set_state_(NCIState::NFCC_CONFIG);
      }
      // fall through

    case NCIState::NFCC_CONFIG:
      if (this->send_init_config_() != nfc::STATUS_OK) {
        ESP_LOGE(TAG, "Failed to send initial config");
        this->nci_fsm_set_error_state_(NCIState::NFCC_CONFIG);
        return;
      } else {
        this->config_refresh_pending_ = false;
        this->nci_fsm_set_state_(NCIState::NFCC_SET_DISCOVER_MAP);
      }
      // fall through

    case NCIState::NFCC_SET_DISCOVER_MAP:
      if (this->set_discover_map_() != nfc::STATUS_OK) {
        ESP_LOGE(TAG, "Failed to set discover map");
        this->nci_fsm_set_error_state_(NCIState::NFCC_SET_LISTEN_MODE_ROUTING);
        return;
      } else {
        this->nci_fsm_set_state_(NCIState::NFCC_SET_LISTEN_MODE_ROUTING);
      }
      // fall through

    case NCIState::NFCC_SET_LISTEN_MODE_ROUTING:
      if (this->set_listen_mode_routing_() != nfc::STATUS_OK) {
        ESP_LOGE(TAG, "Failed to set listen mode routing");
        this->nci_fsm_set_error_state_(NCIState::RFST_IDLE);
        return;
      } else {
        this->nci_fsm_set_state_(NCIState::RFST_IDLE);
      }
      // fall through

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
      // fall through

    // All cases below are waiting for NOTIFICATION messages
    case NCIState::RFST_DISCOVERY:
      if (this->config_refresh_pending_) {
        this->refresh_core_config_();
      }
      // fall through

    case NCIState::RFST_LISTEN_ACTIVE:
    case NCIState::RFST_LISTEN_SLEEP:
    case NCIState::RFST_POLL_ACTIVE:
    case NCIState::EP_SELECTING:
    case NCIState::EP_DEACTIVATING:
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

void PN7150::nci_fsm_set_state_(NCIState new_state) {
  ESP_LOGVV(TAG, "nci_fsm_set_state_(%u)", (uint8_t) new_state);
  this->nci_state_ = new_state;
  this->nci_state_error_ = NCIState::NONE;
  this->error_count_ = 0;
  this->last_nci_state_change_ = millis();
}

bool PN7150::nci_fsm_set_error_state_(NCIState new_state) {
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

void PN7150::process_message_() {
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
        ESP_LOGV(TAG, "Unimplemented notification: %s", nfc::format_bytes(rx.get_message()).c_str());
      }
      break;

    case nfc::NCI_PKT_MT_CTRL_RESPONSE:
      ESP_LOGV(TAG, "Unimplemented GID: 0x%02X  OID: 0x%02X  Full response: %s", rx.get_gid(), rx.get_oid(),
               nfc::format_bytes(rx.get_message()).c_str());
      break;

    case nfc::NCI_PKT_MT_CTRL_COMMAND:
      ESP_LOGV(TAG, "Unimplemented command: %s", nfc::format_bytes(rx.get_message()).c_str());
      break;

    case nfc::NCI_PKT_MT_DATA:
      this->process_data_message_(rx);
      break;

    default:
      ESP_LOGV(TAG, "Unimplemented message type: %s", nfc::format_bytes(rx.get_message()).c_str());
      break;
  }
}

void PN7150::process_rf_intf_activated_oid_(nfc::NciMessage &rx) {  // an endpoint was activated
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
  auto incoming_tag =
      this->build_tag_(mode_tech, std::vector<uint8_t>(rx.get_message().begin() + 10, rx.get_message().end()));

  if (incoming_tag == nullptr) {
    ESP_LOGE(TAG, "Could not build tag");
  } else {
    auto tag_loc = this->find_tag_uid_(incoming_tag->get_uid());
    if (tag_loc.has_value()) {
      this->discovered_endpoint_[tag_loc.value()].id = discovery_id;
      this->discovered_endpoint_[tag_loc.value()].protocol = protocol;
      this->discovered_endpoint_[tag_loc.value()].last_seen = millis();
      ESP_LOGVV(TAG, "Tag cache updated");
    } else {
      this->discovered_endpoint_.emplace_back(
          DiscoveredEndpoint{discovery_id, protocol, millis(), std::move(incoming_tag), false});
      tag_loc = this->discovered_endpoint_.size() - 1;
      ESP_LOGVV(TAG, "Tag added to cache");
    }

    auto &working_endpoint = this->discovered_endpoint_[tag_loc.value()];

    switch (this->next_task_) {
      case EP_CLEAN:
        ESP_LOGD(TAG, "  Tag cleaning...");
        if (this->clean_endpoint_(working_endpoint.tag->get_uid()) != nfc::STATUS_OK) {
          ESP_LOGE(TAG, "  Tag cleaning incomplete");
        }
        ESP_LOGD(TAG, "  Tag cleaned!");
        break;

      case EP_FORMAT:
        ESP_LOGD(TAG, "  Tag formatting...");
        if (this->format_endpoint_(working_endpoint.tag->get_uid()) != nfc::STATUS_OK) {
          ESP_LOGE(TAG, "Error formatting tag as NDEF");
        }
        ESP_LOGD(TAG, "  Tag formatted!");
        break;

      case EP_WRITE:
        if (this->next_task_message_to_write_ != nullptr) {
          ESP_LOGD(TAG, "  Tag writing...");
          ESP_LOGD(TAG, "  Tag formatting...");
          if (this->format_endpoint_(working_endpoint.tag->get_uid()) != nfc::STATUS_OK) {
            ESP_LOGE(TAG, "  Tag could not be formatted for writing");
          } else {
            ESP_LOGD(TAG, "  Writing NDEF data");
            if (this->write_endpoint_(working_endpoint.tag->get_uid(), this->next_task_message_to_write_) !=
                nfc::STATUS_OK) {
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
          ESP_LOGI(TAG, "Read tag type %s with UID %s", working_endpoint.tag->get_tag_type().c_str(),
                   nfc::format_uid(working_endpoint.tag->get_uid()).c_str());
          if (this->read_endpoint_data_(*working_endpoint.tag) != nfc::STATUS_OK) {
            ESP_LOGW(TAG, "  Unable to read NDEF record(s)");
          } else if (working_endpoint.tag->has_ndef_message()) {
            const auto message = working_endpoint.tag->get_ndef_message();
            const auto records = message->get_records();
            ESP_LOGD(TAG, "  NDEF record(s):");
            for (const auto &record : records) {
              ESP_LOGD(TAG, "    %s - %s", record->get_type().c_str(), record->get_payload().c_str());
            }
          } else {
            ESP_LOGW(TAG, "  No NDEF records found");
          }
          for (auto *trigger : this->triggers_ontag_) {
            trigger->process(working_endpoint.tag);
          }
          for (auto *listener : this->tag_listeners_) {
            listener->tag_on(*working_endpoint.tag);
          }
          working_endpoint.trig_called = true;
          break;
        }
    }
    if (working_endpoint.tag->get_tag_type() == nfc::MIFARE_CLASSIC) {
      this->halt_mifare_classic_tag_();
    }
  }
  if (this->next_task_ != EP_READ) {
    this->read_mode();
  }

  this->stop_discovery_();
  this->nci_fsm_set_state_(NCIState::EP_DEACTIVATING);
}

void PN7150::process_rf_discover_oid_(nfc::NciMessage &rx) {
  auto incoming_tag = this->build_tag_(rx.get_message_byte(nfc::RF_DISCOVER_NTF_MODE_TECH),
                                       std::vector<uint8_t>(rx.get_message().begin() + 7, rx.get_message().end()));

  if (incoming_tag == nullptr) {
    ESP_LOGE(TAG, "Could not build tag!");
  } else {
    auto tag_loc = this->find_tag_uid_(incoming_tag->get_uid());
    if (tag_loc.has_value()) {
      this->discovered_endpoint_[tag_loc.value()].id = rx.get_message_byte(nfc::RF_DISCOVER_NTF_DISCOVERY_ID);
      this->discovered_endpoint_[tag_loc.value()].protocol = rx.get_message_byte(nfc::RF_DISCOVER_NTF_PROTOCOL);
      this->discovered_endpoint_[tag_loc.value()].last_seen = millis();
      ESP_LOGVV(TAG, "Tag found & updated");
    } else {
      this->discovered_endpoint_.emplace_back(DiscoveredEndpoint{rx.get_message_byte(nfc::RF_DISCOVER_NTF_DISCOVERY_ID),
                                                                 rx.get_message_byte(nfc::RF_DISCOVER_NTF_PROTOCOL),
                                                                 millis(), std::move(incoming_tag), false});
      ESP_LOGVV(TAG, "Tag saved");
    }
  }

  if (rx.get_message().back() != nfc::RF_DISCOVER_NTF_NT_MORE) {
    this->nci_fsm_set_state_(NCIState::RFST_W4_HOST_SELECT);
    ESP_LOGVV(TAG, "Discovered %u endpoints", this->discovered_endpoint_.size());
  }
}

void PN7150::process_rf_deactivate_oid_(nfc::NciMessage &rx) {
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

void PN7150::process_data_message_(nfc::NciMessage &rx) {
  ESP_LOGVV(TAG, "Received data message: %s", nfc::format_bytes(rx.get_message()).c_str());

  std::vector<uint8_t> ndef_response;
  this->card_emu_t4t_get_response_(rx.get_message(), ndef_response);

  uint16_t ndef_response_size = ndef_response.size();
  if (!ndef_response_size) {
    return;  // no message returned, we cannot respond
  }

  std::vector<uint8_t> tx_msg = {nfc::NCI_PKT_MT_DATA, uint8_t((ndef_response_size & 0xFF00) >> 8),
                                 uint8_t(ndef_response_size & 0x00FF)};
  tx_msg.insert(tx_msg.end(), ndef_response.begin(), ndef_response.end());
  nfc::NciMessage tx(tx_msg);
  ESP_LOGVV(TAG, "Sending data message: %s", nfc::format_bytes(tx.get_message()).c_str());
  if (this->transceive_(tx, rx, NFCC_DEFAULT_TIMEOUT, false) != nfc::STATUS_OK) {
    ESP_LOGE(TAG, "Sending reply for card emulation failed");
  }
}

void PN7150::card_emu_t4t_get_response_(std::vector<uint8_t> &response, std::vector<uint8_t> &ndef_response) {
  if (this->card_emulation_message_ == nullptr) {
    ESP_LOGE(TAG, "No NDEF message is set; tag emulation not possible");
    ndef_response.clear();
    return;
  }

  if (equal(response.begin() + nfc::NCI_PKT_HEADER_SIZE, response.end(), std::begin(CARD_EMU_T4T_APP_SELECT))) {
    // CARD_EMU_T4T_APP_SELECT
    ESP_LOGVV(TAG, "CARD_EMU_NDEF_APP_SELECTED");
    this->ce_state_ = CardEmulationState::CARD_EMU_NDEF_APP_SELECTED;
    ndef_response.insert(ndef_response.begin(), std::begin(CARD_EMU_T4T_OK), std::end(CARD_EMU_T4T_OK));
  } else if (equal(response.begin() + nfc::NCI_PKT_HEADER_SIZE, response.end(), std::begin(CARD_EMU_T4T_CC_SELECT))) {
    // CARD_EMU_T4T_CC_SELECT
    if (this->ce_state_ == CardEmulationState::CARD_EMU_NDEF_APP_SELECTED) {
      ESP_LOGVV(TAG, "CARD_EMU_CC_SELECTED");
      this->ce_state_ = CardEmulationState::CARD_EMU_CC_SELECTED;
      ndef_response.insert(ndef_response.begin(), std::begin(CARD_EMU_T4T_OK), std::end(CARD_EMU_T4T_OK));
    }
  } else if (equal(response.begin() + nfc::NCI_PKT_HEADER_SIZE, response.end(), std::begin(CARD_EMU_T4T_NDEF_SELECT))) {
    // CARD_EMU_T4T_NDEF_SELECT
    ESP_LOGVV(TAG, "CARD_EMU_NDEF_SELECTED");
    this->ce_state_ = CardEmulationState::CARD_EMU_NDEF_SELECTED;
    ndef_response.insert(ndef_response.begin(), std::begin(CARD_EMU_T4T_OK), std::end(CARD_EMU_T4T_OK));
  } else if (equal(response.begin() + nfc::NCI_PKT_HEADER_SIZE,
                   response.begin() + nfc::NCI_PKT_HEADER_SIZE + sizeof(CARD_EMU_T4T_READ),
                   std::begin(CARD_EMU_T4T_READ))) {
    // CARD_EMU_T4T_READ
    if (this->ce_state_ == CardEmulationState::CARD_EMU_CC_SELECTED) {
      // CARD_EMU_T4T_READ with CARD_EMU_CC_SELECTED
      ESP_LOGVV(TAG, "CARD_EMU_T4T_READ with CARD_EMU_CC_SELECTED");
      uint16_t offset = (response[nfc::NCI_PKT_HEADER_SIZE + 2] << 8) + response[nfc::NCI_PKT_HEADER_SIZE + 3];
      uint8_t length = response[nfc::NCI_PKT_HEADER_SIZE + 4];

      if (length <= (sizeof(CARD_EMU_T4T_CC) + offset + 2)) {
        ndef_response.insert(ndef_response.begin(), std::begin(CARD_EMU_T4T_CC) + offset,
                             std::begin(CARD_EMU_T4T_CC) + offset + length);
        ndef_response.insert(ndef_response.end(), std::begin(CARD_EMU_T4T_OK), std::end(CARD_EMU_T4T_OK));
      }
    } else if (this->ce_state_ == CardEmulationState::CARD_EMU_NDEF_SELECTED) {
      // CARD_EMU_T4T_READ with CARD_EMU_NDEF_SELECTED
      ESP_LOGVV(TAG, "CARD_EMU_T4T_READ with CARD_EMU_NDEF_SELECTED");
      auto ndef_message = this->card_emulation_message_->encode();
      uint16_t ndef_msg_size = ndef_message.size();
      uint16_t offset = (response[nfc::NCI_PKT_HEADER_SIZE + 2] << 8) + response[nfc::NCI_PKT_HEADER_SIZE + 3];
      uint8_t length = response[nfc::NCI_PKT_HEADER_SIZE + 4];

      ESP_LOGVV(TAG, "Encoded NDEF message: %s", nfc::format_bytes(ndef_message).c_str());

      if (length <= (ndef_msg_size + offset + 2)) {
        if (offset == 0) {
          ndef_response.resize(2);
          ndef_response[0] = (ndef_msg_size & 0xFF00) >> 8;
          ndef_response[1] = (ndef_msg_size & 0x00FF);
          if (length > 2) {
            ndef_response.insert(ndef_response.end(), ndef_message.begin(), ndef_message.begin() + length - 2);
          }
        } else if (offset == 1) {
          ndef_response.resize(1);
          ndef_response[0] = (ndef_msg_size & 0x00FF);
          if (length > 1) {
            ndef_response.insert(ndef_response.end(), ndef_message.begin(), ndef_message.begin() + length - 1);
          }
        } else {
          ndef_response.insert(ndef_response.end(), ndef_message.begin(), ndef_message.begin() + length);
        }

        ndef_response.insert(ndef_response.end(), std::begin(CARD_EMU_T4T_OK), std::end(CARD_EMU_T4T_OK));

        if ((offset + length) >= (ndef_msg_size + 2)) {
          ESP_LOGD(TAG, "NDEF message sent");
          this->on_emulated_tag_scan_callback_.call();
        }
      }
    }
  } else if (equal(response.begin() + nfc::NCI_PKT_HEADER_SIZE,
                   response.begin() + nfc::NCI_PKT_HEADER_SIZE + sizeof(CARD_EMU_T4T_WRITE),
                   std::begin(CARD_EMU_T4T_WRITE))) {
    // CARD_EMU_T4T_WRITE
    if (this->ce_state_ == CardEmulationState::CARD_EMU_NDEF_SELECTED) {
      ESP_LOGVV(TAG, "CARD_EMU_T4T_WRITE");
      uint8_t length = response[nfc::NCI_PKT_HEADER_SIZE + 4];
      std::vector<uint8_t> ndef_msg_written;

      ndef_msg_written.insert(ndef_msg_written.end(), response.begin() + nfc::NCI_PKT_HEADER_SIZE + 5,
                              response.begin() + nfc::NCI_PKT_HEADER_SIZE + 5 + length);
      ESP_LOGD(TAG, "Received %u-byte NDEF message: %s", length, nfc::format_bytes(ndef_msg_written).c_str());
      ndef_response.insert(ndef_response.end(), std::begin(CARD_EMU_T4T_OK), std::end(CARD_EMU_T4T_OK));
    }
  }
}

uint8_t PN7150::transceive_(nfc::NciMessage &tx, nfc::NciMessage &rx, const uint16_t timeout,
                            const bool expect_notification) {
  uint8_t retries = NFCC_MAX_COMM_FAILS;

  while (retries) {
    // first, send the message we need to send
    if (this->write_nfcc(tx) != nfc::STATUS_OK) {
      ESP_LOGE(TAG, "Error sending message");
      return nfc::STATUS_FAILED;
    }
    ESP_LOGVV(TAG, "Wrote: %s", nfc::format_bytes(tx.get_message()).c_str());
    // next, the NFCC should send back a response
    if (this->read_nfcc(rx, timeout) != nfc::STATUS_OK) {
      ESP_LOGW(TAG, "Error receiving message");
      if (!retries--) {
        ESP_LOGE(TAG, "  ...giving up");
        return nfc::STATUS_FAILED;
      }
    } else {
      break;
    }
  }
  ESP_LOGVV(TAG, "Read: %s", nfc::format_bytes(rx.get_message()).c_str());
  // validate the response based on the message type that was sent (command vs. data)
  if (!tx.message_type_is(nfc::NCI_PKT_MT_DATA)) {
    // for commands, the GID and OID should match and the status should be OK
    if ((rx.get_gid() != tx.get_gid()) || (rx.get_oid()) != tx.get_oid()) {
      ESP_LOGE(TAG, "Incorrect response to command: %s", nfc::format_bytes(rx.get_message()).c_str());
      return nfc::STATUS_FAILED;
    }

    if (!rx.simple_status_response_is(nfc::STATUS_OK)) {
      ESP_LOGE(TAG, "Error in response to command: %s", nfc::format_bytes(rx.get_message()).c_str());
    }
    return rx.get_simple_status_response();
  } else {
    // when requesting data from the endpoint, the first response is from the NFCC; we must validate this, first
    if ((!rx.message_type_is(nfc::NCI_PKT_MT_CTRL_NOTIFICATION)) || (!rx.gid_is(nfc::NCI_CORE_GID)) ||
        (!rx.oid_is(nfc::NCI_CORE_CONN_CREDITS_OID)) || (!rx.message_length_is(3))) {
      ESP_LOGE(TAG, "Incorrect response to data message: %s", nfc::format_bytes(rx.get_message()).c_str());
      return nfc::STATUS_FAILED;
    }

    if (expect_notification) {
      // if the NFCC said "OK", there will be additional data to read; this comes back in a notification message
      if (this->read_nfcc(rx, timeout) != nfc::STATUS_OK) {
        ESP_LOGE(TAG, "Error receiving data from endpoint");
        return nfc::STATUS_FAILED;
      }
      ESP_LOGVV(TAG, "Read: %s", nfc::format_bytes(rx.get_message()).c_str());
    }

    return nfc::STATUS_OK;
  }
}

uint8_t PN7150::wait_for_irq_(uint16_t timeout, bool pin_state) {
  auto start_time = millis();

  while (millis() - start_time < timeout) {
    if (this->irq_pin_->digital_read() == pin_state) {
      return nfc::STATUS_OK;
    }
  }
  ESP_LOGW(TAG, "Timed out waiting for IRQ state");
  return nfc::STATUS_FAILED;
}

}  // namespace pn7150
}  // namespace esphome
