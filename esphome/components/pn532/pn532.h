#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/nfc/nfc_tag.h"
#include "esphome/components/nfc/nfc.h"
#include "esphome/components/nfc/automation.h"

#include <cinttypes>
#include <vector>

namespace esphome::pn532 {

static const uint8_t PN532_COMMAND_VERSION_DATA = 0x02;
static const uint8_t PN532_COMMAND_SAMCONFIGURATION = 0x14;
static const uint8_t PN532_COMMAND_RFCONFIGURATION = 0x32;
static const uint8_t PN532_COMMAND_INDATAEXCHANGE = 0x40;
static const uint8_t PN532_COMMAND_INLISTPASSIVETARGET = 0x4A;
static const uint8_t PN532_COMMAND_POWERDOWN = 0x16;

enum PN532ReadReady : uint8_t {
  WOULDBLOCK = 0,
  TIMEOUT,
  READY,
};

// SEL_RES (SAK) bits, as reported by InListPassiveTarget for ISO/IEC 14443 type A targets (NXP AN10833)
static constexpr uint8_t SEL_RES_MIFARE_CLASSIC = 0x08;
static constexpr uint8_t SEL_RES_ISO_DEP = 0x20;
static constexpr uint8_t SEL_RES_TNP3XXX = 0x01;  // MIFARE Classic 1K compatible

/// Tag type (nfc::TAG_TYPE_*) from a type A target's SEL_RES byte
inline uint8_t tag_type_from_sel_res(uint8_t sel_res) {
  if ((sel_res & SEL_RES_MIFARE_CLASSIC) || sel_res == SEL_RES_TNP3XXX)
    return nfc::TAG_TYPE_MIFARE_CLASSIC;
  if (sel_res & SEL_RES_ISO_DEP)
    return nfc::TAG_TYPE_4;
  if (sel_res == 0x00)
    return nfc::TAG_TYPE_2;
  return nfc::TAG_TYPE_UNKNOWN;
}

class PN532BinarySensor;

class PN532 : public PollingComponent {
 public:
  void setup() override;

  void dump_config() override;

  void update() override;

  void loop() override;
  void on_powerdown() override { powerdown(); }

  void register_tag(PN532BinarySensor *tag) { this->binary_sensors_.push_back(tag); }
  void register_ontag_trigger(nfc::NfcOnTagTrigger *trig) { this->triggers_ontag_.push_back(trig); }
  void register_ontagremoved_trigger(nfc::NfcOnTagTrigger *trig) { this->triggers_ontagremoved_.push_back(trig); }

  template<typename F> void add_on_finished_write_callback(F &&callback) {
    this->on_finished_write_callback_.add(std::forward<F>(callback));
  }

  bool is_writing() { return this->next_task_ != READ; };

  void read_mode();
  void clean_mode();
  void format_mode();
  void write_mode(nfc::NdefMessage *message);
  bool powerdown();

 protected:
  void turn_off_rf_();
  bool write_command_(const std::vector<uint8_t> &data);
  bool read_ack_();
  void send_ack_();
  void send_nack_();

  enum PN532ReadReady read_ready_(bool block);
  virtual bool is_read_ready() = 0;
  virtual bool write_data(const std::vector<uint8_t> &data) = 0;
  virtual bool read_data(std::vector<uint8_t> &data, uint8_t len) = 0;
  virtual bool read_response(uint8_t command, std::vector<uint8_t> &data) = 0;

  std::unique_ptr<nfc::NfcTag> read_tag_(nfc::NfcTagUid &uid, uint8_t tag_type);

  bool format_tag_(nfc::NfcTagUid &uid, uint8_t tag_type);
  bool clean_tag_(nfc::NfcTagUid &uid, uint8_t tag_type);
  bool write_tag_(nfc::NfcTagUid &uid, uint8_t tag_type, nfc::NdefMessage *message);
  /// Sends an InDataExchange command and reads the response; returns false unless the status byte reports success.
  /// On success, `response` holds the data returned by the target, without the status byte.
  bool in_data_exchange_(const std::vector<uint8_t> &command, std::vector<uint8_t> &response);

  std::unique_ptr<nfc::NfcTag> read_mifare_classic_tag_(nfc::NfcTagUid &uid);
  bool read_mifare_classic_block_(uint8_t block_num, std::vector<uint8_t> &data);
  bool write_mifare_classic_block_(uint8_t block_num, const uint8_t *data, size_t len);
  bool auth_mifare_classic_block_(nfc::NfcTagUid &uid, uint8_t block_num, uint8_t key_num, const uint8_t *key);
  bool format_mifare_classic_mifare_(nfc::NfcTagUid &uid);
  bool format_mifare_classic_ndef_(nfc::NfcTagUid &uid);
  bool write_mifare_classic_tag_(nfc::NfcTagUid &uid, nfc::NdefMessage *message);

  std::unique_ptr<nfc::NfcTag> read_mifare_ultralight_tag_(nfc::NfcTagUid &uid);
  bool read_mifare_ultralight_bytes_(uint8_t start_page, uint16_t num_bytes, std::vector<uint8_t> &data);
  bool is_mifare_ultralight_formatted_(const std::vector<uint8_t> &page_3_to_6);
  uint16_t read_mifare_ultralight_capacity_();
  bool find_mifare_ultralight_ndef_(const std::vector<uint8_t> &page_3_to_6, uint8_t &message_length,
                                    uint8_t &message_start_index);
  bool write_mifare_ultralight_page_(uint8_t page_num, const uint8_t *write_data, size_t len);
  bool write_mifare_ultralight_tag_(nfc::NfcTagUid &uid, nfc::NdefMessage *message);
  bool clean_mifare_ultralight_();

  enum NfcTask : uint8_t {
    READ = 0,
    CLEAN,
    FORMAT,
    WRITE,
  };
  enum PN532Error : uint8_t {
    NONE = 0,
    WAKEUP_FAILED,
    SAM_COMMAND_FAILED,
  };

  // members are ordered by alignment, widest first, to minimize padding
  CallbackManager<void()> on_finished_write_callback_;
  std::vector<PN532BinarySensor *> binary_sensors_;
  std::vector<nfc::NfcOnTagTrigger *> triggers_ontag_;
  std::vector<nfc::NfcOnTagTrigger *> triggers_ontagremoved_;
  std::unique_ptr<nfc::NdefMessage> next_task_message_to_write_;
  nfc::NfcTagUid current_uid_;
  uint32_t rd_start_time_{0};  // valid only while rd_started_ is set
  PN532ReadReady rd_ready_{WOULDBLOCK};
  NfcTask next_task_{READ};
  PN532Error error_code_{NONE};
  bool rd_started_{false};
  bool updates_enabled_{true};
  bool requested_read_{false};
};

class PN532BinarySensor final : public binary_sensor::BinarySensor {
 public:
  void set_uid(const nfc::NfcTagUid &uid) { uid_ = uid; }

  bool process(const nfc::NfcTagUid &data);

  void on_scan_end() {
    if (!this->found_) {
      this->publish_state(false);
    }
    this->found_ = false;
  }

 protected:
  nfc::NfcTagUid uid_;
  bool found_{false};
};

template<typename... Ts> class PN532IsWritingCondition final : public Condition<Ts...>, public Parented<PN532> {
 public:
  bool check(const Ts &...x) override { return this->parent_->is_writing(); }
};

}  // namespace esphome::pn532
