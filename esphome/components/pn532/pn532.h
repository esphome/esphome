#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

#include <cinttypes>
#include <vector>
#include <type_traits>

#include "esphome/components/nfc/nfc_tag.h"
#include "esphome/components/nfc/nfc.h"
#include "esphome/components/nfc/automation.h"

namespace esphome {
namespace pn532 {

/**
 * Robustly identify the UID type used by the NFC component.
 * Newer versions use StaticVector, older ones use std::vector.
 */
template<typename T> struct NfcUidTypeExtractor {
  using type = std::vector<uint8_t>;
};

template<> struct NfcUidTypeExtractor<nfc::NfcTag> {
  // Try to extract type from NfcTag::get_uid() return type
  using type = typename std::remove_cv<typename std::remove_reference<decltype(
      std::declval<nfc::NfcTag>().get_uid())>::type>::type;
};

using NfcTagUid = typename NfcUidTypeExtractor<nfc::NfcTag>::type;

static const uint8_t PN532_COMMAND_VERSION_DATA = 0x02;
static const uint8_t PN532_COMMAND_SAMCONFIGURATION = 0x14;
static const uint8_t PN532_COMMAND_RFCONFIGURATION = 0x32;
static const uint8_t PN532_COMMAND_INDATAEXCHANGE = 0x40;
static const uint8_t PN532_COMMAND_INLISTPASSIVETARGET = 0x4A;
static const uint8_t PN532_COMMAND_POWERDOWN = 0x16;

enum PN532ReadReady {
  WOULDBLOCK = 0,
  TIMEOUT,
  READY,
};

class PN532BinarySensor : public binary_sensor::BinarySensor {
 public:
  void set_uid(const std::vector<uint8_t> &uid) { uid_ = uid; }

  bool process(const std::vector<uint8_t> &data);

  void on_scan_end() {
    if (!this->found_) {
      this->publish_state(false);
    }
    this->found_ = false;
  }

 protected:
  std::vector<uint8_t> uid_;
  bool found_{false};
};

class PN532 : public PollingComponent {
 public:
  virtual ~PN532() = default;
  void setup() override;
  void set_rf_field_enabled(bool enabled) { this->rf_field_enabled_ = enabled; }

  void dump_config() override;

  void update() override;

  void loop() override;
  void on_shutdown() override { powerdown(); }

  void register_tag(PN532BinarySensor *tag) { this->binary_sensors_.push_back(tag); }
  void register_ontag_trigger(nfc::NfcOnTagTrigger *trig) { this->triggers_ontag_.push_back(trig); }
  void register_ontagremoved_trigger(nfc::NfcOnTagTrigger *trig) { this->triggers_ontagremoved_.push_back(trig); }

  void add_on_finished_write_callback(std::function<void()> callback) {
    this->on_finished_write_callback_.add(std::move(callback));
  }

  bool is_writing() { return this->next_task_ != READ; };

  void read_mode();
  void clean_mode();
  void format_mode();
  void write_mode(nfc::NdefMessage *message);
  bool powerdown();
  void set_max_failed_checks(uint8_t n) { this->max_failed_checks_ = n; }
  void set_auto_reset(bool v) { this->auto_reset_ = v; }
  void set_health_check_enabled(bool v) { this->health_check_enabled_ = v; }
  void set_health_check_interval(uint32_t ms) { this->health_check_interval_ = ms; }
  void set_user_update_interval(uint32_t ms) { this->user_update_interval_ = ms; }
  uint32_t user_update_interval_{1000};
  uint32_t backoff_ms_{0};

 protected:
  void turn_off_rf_();
  bool write_command_(const std::vector<uint8_t> &data);
  bool read_ack_();
  void send_ack_();
  void send_nack_();
  bool reinit_();
  bool sam_configured_{false};
  uint8_t reinit_attempts_{0};
  uint32_t last_reinit_attempt_{0};
  uint8_t consecutive_failures_{0};
  uint8_t max_failed_checks_{3};
  bool auto_reset_{true};
  bool rf_field_enabled_{false};
  bool health_check_enabled_{true};
  uint32_t health_check_interval_{60000};
  uint32_t last_health_check_{0};

  void process_removed_tags_(const std::vector<std::vector<uint8_t>> &new_uids);
  void process_binary_sensors_();

  enum PN532ReadReady read_ready_(bool block);
  virtual bool is_read_ready() = 0;
  virtual bool write_data(const std::vector<uint8_t> &data) = 0;
  virtual bool read_data(std::vector<uint8_t> &data, uint8_t len) = 0;
  virtual bool read_response(uint8_t command, std::vector<uint8_t> &data) = 0;

  std::unique_ptr<nfc::NfcTag> read_tag_(uint8_t tg, std::vector<uint8_t> &uid);

  bool format_tag_(uint8_t tg, std::vector<uint8_t> &uid);
  bool clean_tag_(uint8_t tg, std::vector<uint8_t> &uid);
  bool write_tag_(uint8_t tg, std::vector<uint8_t> &uid, nfc::NdefMessage *message);

  std::unique_ptr<nfc::NfcTag> read_mifare_classic_tag_(uint8_t tg, std::vector<uint8_t> &uid);
  bool read_mifare_classic_block_(uint8_t tg, uint8_t block_num, std::vector<uint8_t> &data);
  bool write_mifare_classic_block_(uint8_t tg, uint8_t block_num, std::vector<uint8_t> &data);
  bool auth_mifare_classic_block_(uint8_t tg, std::vector<uint8_t> &uid, uint8_t block_num, uint8_t key_num, const uint8_t *key);
  bool format_mifare_classic_mifare_(uint8_t tg, std::vector<uint8_t> &uid);
  bool format_mifare_classic_ndef_(uint8_t tg, std::vector<uint8_t> &uid);
  bool write_mifare_classic_tag_(uint8_t tg, std::vector<uint8_t> &uid, nfc::NdefMessage *message);

  std::unique_ptr<nfc::NfcTag> read_mifare_ultralight_tag_(uint8_t tg, std::vector<uint8_t> &uid);
  bool read_mifare_ultralight_bytes_(uint8_t tg, uint8_t start_page, uint16_t num_bytes, std::vector<uint8_t> &data);
  bool is_mifare_ultralight_formatted_(const std::vector<uint8_t> &page_3_to_6);
  uint16_t read_mifare_ultralight_capacity_(uint8_t tg);
  bool find_mifare_ultralight_ndef_(const std::vector<uint8_t> &page_3_to_6, uint8_t &message_length,
                                    uint8_t &message_start_index);
  bool write_mifare_ultralight_page_(uint8_t tg, uint8_t page_num, std::vector<uint8_t> &write_data);
  bool write_mifare_ultralight_tag_(uint8_t tg, std::vector<uint8_t> &uid, nfc::NdefMessage *message);
  bool clean_mifare_ultralight_(uint8_t tg);

  bool updates_enabled_{true};
  bool requested_read_{false};
  std::vector<PN532BinarySensor *> binary_sensors_;
  std::vector<nfc::NfcOnTagTrigger *> triggers_ontag_;
  std::vector<nfc::NfcOnTagTrigger *> triggers_ontagremoved_;
  std::vector<std::vector<uint8_t>> current_uids_;
  struct PersistentTag {
    std::vector<uint8_t> uid;
    uint8_t missing_count;
  };
  std::vector<PersistentTag> persistent_tags_;
  nfc::NdefMessage *next_task_message_to_write_;
  uint32_t rd_start_time_{0};
  uint32_t rd_latency_ms_{0};
  enum PN532ReadReady rd_ready_{WOULDBLOCK};
  enum NfcTask {
    READ = 0,
    CLEAN,
    FORMAT,
    WRITE,
  } next_task_{READ};
  enum PN532Error {
    NONE = 0,
    WAKEUP_FAILED,
    SAM_COMMAND_FAILED,
  } error_code_{NONE};
  CallbackManager<void()> on_finished_write_callback_;
};

class PN532OnFinishedWriteTrigger : public Trigger<> {
 public:
  explicit PN532OnFinishedWriteTrigger(PN532 *parent) {
    parent->add_on_finished_write_callback([this]() { this->trigger(); });
  }
};

template<typename... Ts> class PN532IsWritingCondition : public Condition<Ts...>, public Parented<PN532> {
 public:
  bool check(Ts... x) override { return this->parent_->is_writing(); }
};

}  // namespace pn532
}  // namespace esphome
