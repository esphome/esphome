#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/nfc/nfc_tag.h"
#include "esphome/components/nfc/nfc.h"

namespace esphome {
namespace pn532 {

static const uint8_t PN532_COMMAND_VERSION_DATA = 0x02;
static const uint8_t PN532_COMMAND_SAMCONFIGURATION = 0x14;
static const uint8_t PN532_COMMAND_RFCONFIGURATION = 0x32;
static const uint8_t PN532_COMMAND_INDATAEXCHANGE = 0x40;
static const uint8_t PN532_COMMAND_INLISTPASSIVETARGET = 0x4A;

class PN532BinarySensor;
class PN532OnTagTrigger;

class PN532 : public PollingComponent {
 public:
  void setup() override;

  void dump_config() override;

  void update() override;
  float get_setup_priority() const override;

  void loop() override;

  void register_tag(PN532BinarySensor *tag) { this->binary_sensors_.push_back(tag); }
  void register_ontag_trigger(PN532OnTagTrigger *trig) { this->triggers_ontag_.push_back(trig); }
  void register_ontagremoved_trigger(PN532OnTagTrigger *trig) { this->triggers_ontagremoved_.push_back(trig); }

  void add_on_finished_write_callback(std::function<void()> callback) {
    this->on_finished_write_callback_.add(std::move(callback));
  }

  bool is_writing() { return this->next_task_ != READ; };

  void read_mode();
  void clean_mode();
  void format_mode();
  void write_mode(nfc::NdefMessage *message);

 protected:
  void turn_off_rf_();
  bool write_command_(const std::vector<uint8_t> &data);
  bool read_ack_();
  void send_nack_();

  virtual bool write_data(const std::vector<uint8_t> &data) = 0;
  virtual bool read_data(std::vector<uint8_t> &data, uint8_t len) = 0;
  virtual bool read_response(uint8_t command, std::vector<uint8_t> &data) = 0;

  std::unique_ptr<nfc::NfcTag> read_tag_(std::vector<uint8_t> &uid);

  bool format_tag_(std::vector<uint8_t> &uid);
  bool clean_tag_(std::vector<uint8_t> &uid);
  bool write_tag_(std::vector<uint8_t> &uid, nfc::NdefMessage *message);

  std::unique_ptr<nfc::NfcTag> read_mifare_classic_tag_(std::vector<uint8_t> &uid);
  bool read_mifare_classic_block_(uint8_t block_num, std::vector<uint8_t> &data);
  bool write_mifare_classic_block_(uint8_t block_num, std::vector<uint8_t> &data);
  bool auth_mifare_classic_block_(std::vector<uint8_t> &uid, uint8_t block_num, uint8_t key_num, const uint8_t *key);
  bool format_mifare_classic_mifare_(std::vector<uint8_t> &uid);
  bool format_mifare_classic_ndef_(std::vector<uint8_t> &uid);
  bool write_mifare_classic_tag_(std::vector<uint8_t> &uid, nfc::NdefMessage *message);

  std::unique_ptr<nfc::NfcTag> read_mifare_ultralight_tag_(std::vector<uint8_t> &uid);
  bool read_mifare_ultralight_page_(uint8_t page_num, std::vector<uint8_t> &data);
  bool is_mifare_ultralight_formatted_();
  uint16_t read_mifare_ultralight_capacity_();
  bool find_mifare_ultralight_ndef_(uint8_t &message_length, uint8_t &message_start_index);
  bool write_mifare_ultralight_page_(uint8_t page_num, std::vector<uint8_t> &write_data);
  bool write_mifare_ultralight_tag_(std::vector<uint8_t> &uid, nfc::NdefMessage *message);
  bool clean_mifare_ultralight_();

  bool requested_read_{false};
  std::vector<PN532BinarySensor *> binary_sensors_;
  std::vector<PN532OnTagTrigger *> triggers_ontag_;
  std::vector<PN532OnTagTrigger *> triggers_ontagremoved_;
  std::vector<uint8_t> current_uid_;
  nfc::NdefMessage *next_task_message_to_write_;
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

class PN532BinarySensor : public binary_sensor::BinarySensor {
 public:
  void set_uid(const std::vector<uint8_t> &uid) { uid_ = uid; }

  bool process(std::vector<uint8_t> &data);

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

class PN532OnTagTrigger : public Trigger<std::string, nfc::NfcTag> {
 public:
  void process(const std::unique_ptr<nfc::NfcTag> &tag);
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
