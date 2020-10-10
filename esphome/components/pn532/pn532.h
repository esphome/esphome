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
class PN532Trigger;

class PN532 : public PollingComponent {
 public:
  void setup() override;

  void dump_config() override;

  void update() override;
  float get_setup_priority() const override;

  void loop() override;

  void register_tag(PN532BinarySensor *tag) { this->binary_sensors_.push_back(tag); }
  void register_trigger(PN532Trigger *trig) { this->triggers_.push_back(trig); }

  void read_mode();
  void clean_mode(bool continuous = false);
  void erase_mode(bool continuous = false);
  void format_mode(bool continuous = false);
  void write_mode(nfc::NdefMessage *message, bool continuous = false);

 protected:
  void turn_off_rf_();
  bool write_command_(const std::vector<uint8_t> &data);
  bool read_response_(uint8_t command, std::vector<uint8_t> &data);
  bool read_ack_();
  uint8_t read_response_length_();

  virtual bool write_data(const std::vector<uint8_t> &data) = 0;
  virtual bool read_data(std::vector<uint8_t> &data, uint8_t len) = 0;

  nfc::NfcTag *read_tag_(std::vector<uint8_t> &uid);

  bool erase_tag_(std::vector<uint8_t> &uid);
  bool format_tag_(std::vector<uint8_t> &uid);
  bool clean_tag_(std::vector<uint8_t> &uid);
  bool write_tag_(std::vector<uint8_t> &uid, nfc::NdefMessage *message);

  nfc::NfcTag *read_mifare_classic_tag_(std::vector<uint8_t> &uid);
  std::vector<uint8_t> read_mifare_classic_block_(uint8_t block_num);
  bool write_mifare_classic_block_(uint8_t block_num, std::vector<uint8_t> &data);
  bool auth_mifare_classic_block_(std::vector<uint8_t> &uid, uint8_t block_num, uint8_t key_num, const uint8_t *key);
  bool format_mifare_classic_mifare_(std::vector<uint8_t> &uid);
  bool format_mifare_classic_ndef_(std::vector<uint8_t> &uid);
  bool write_mifare_classic_tag_(std::vector<uint8_t> &uid, nfc::NdefMessage *message);

  nfc::NfcTag *read_mifare_ultralight_tag_(std::vector<uint8_t> &uid);
  bool read_mifare_ultralight_page_(uint8_t page_num, std::vector<uint8_t> &data);
  bool is_mifare_ultralight_formatted_();
  uint16_t read_mifare_ultralight_capacity();
  bool find_mifare_ultralight_ndef_(uint8_t &message_length, uint8_t &message_start_index);
  bool write_mifare_ultralight_page_(uint8_t page_num, std::vector<uint8_t> &write_data);
  bool write_mifare_ultralight_tag_(std::vector<uint8_t> &uid, nfc::NdefMessage *message);
  bool clean_mifare_ultralight_();


  bool requested_read_{false};
  bool next_task_continuous_{false};
  std::vector<PN532BinarySensor *> binary_sensors_;
  std::vector<PN532Trigger *> triggers_;
  std::vector<uint8_t> current_uid_;
  nfc::NdefMessage *next_task_message_to_write_;
  enum NfcTask {
    READ = 0,
    CLEAN,
    FORMAT,
    ERASE,
    WRITE,
  } next_task_{READ};
  enum PN532Error {
    NONE = 0,
    WAKEUP_FAILED,
    SAM_COMMAND_FAILED,
  } error_code_{NONE};
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

class PN532Trigger : public Trigger<std::string, nfc::NfcTag> {
 public:
  void process(nfc::NfcTag *tag);
};

}  // namespace pn532
}  // namespace esphome
