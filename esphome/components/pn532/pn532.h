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

 protected:
  void turn_off_rf_();
  bool write_command_(const std::vector<uint8_t> &data);
  bool read_response_(uint8_t command, std::vector<uint8_t> &data);
  bool read_ack_();
  uint8_t read_response_length_();

  virtual bool write_data(const std::vector<uint8_t> &data) = 0;
  virtual bool read_data(std::vector<uint8_t> &data, uint8_t len) = 0;

  nfc::NfcTag *read_tag_(std::vector<uint8_t> &uid);

  bool erase_tag_(nfc::NfcTag &tag);
  bool format_tag_(nfc::NfcTag &tag);
  bool clean_tag_(nfc::NfcTag &tag);
  bool write_tag_(nfc::NfcTag &tag);

  std::vector<uint8_t> read_mifare_classic_block_(uint8_t block_num);
  bool write_mifare_classic_block_(uint8_t block_num, std::vector<uint8_t> &data);

  bool auth_mifare_classic_block_(std::vector<uint8_t> &uid, uint8_t block_num, uint8_t key_num, uint8_t *key);
  bool format_mifare_classic_mifare_(nfc::NfcTag &tag);
  bool format_mifare_classic_ndef_(nfc::NfcTag &tag);

  bool requested_read_{false};
  std::vector<PN532BinarySensor *> binary_sensors_;
  std::vector<PN532Trigger *> triggers_;
  std::vector<uint8_t> current_uid_;
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
