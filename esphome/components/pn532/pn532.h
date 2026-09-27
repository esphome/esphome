#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/nfc/nfc_tag.h"
#include "esphome/components/nfc/nfc.h"
#include "esphome/components/nfc/automation.h"

#include <array>
#include <cinttypes>
#include <initializer_list>
#include <span>

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

/// Most data bytes a normal information frame carries: LEN is one byte and counts the TFI byte plus the data
static constexpr size_t PN532_FRAME_MAX_DATA_SIZE = 254;
/// A received frame at its longest: the status byte the I2C bus prepends, preamble, start code (2), LEN, LCS, TFI,
/// the data, DCS and postamble
static constexpr size_t PN532_FRAME_MAX_SIZE = 1 + 6 + PN532_FRAME_MAX_DATA_SIZE + 2;
/// Holds one frame in either direction so bus traffic never allocates
using PN532Frame = StaticVector<uint8_t, PN532_FRAME_MAX_SIZE>;
/// Holds pages 3 to 6 (16 bytes) plus an NDEF message of up to 255 bytes and its TLV header, rounded up to whole reads
using UltralightReadBuffer = StaticVector<uint8_t, 272>;
/// Longest NDEF message accepted from a MIFARE Classic tag (the capacity of a 4K tag)
static constexpr uint32_t MIFARE_CLASSIC_MAX_NDEF_SIZE = 3440;
/// A MIFARE READ answers with 16 bytes: one Classic block or four Ultralight pages
static constexpr size_t MIFARE_READ_SIZE = 16;
using MifareReadData = std::array<uint8_t, MIFARE_READ_SIZE>;

class PN532BinarySensor;

class PN532 : public PollingComponent {
 public:
  void setup() override;

  void dump_config() override;

  void update() override;

  void loop() override;
  void on_powerdown() override { powerdown(); }

#ifdef PN532_BINARY_SENSOR_COUNT
  void register_tag(PN532BinarySensor *tag) { this->binary_sensors_.push_back(tag); }
#endif
#ifdef PN532_ON_TAG_TRIGGER_COUNT
  void register_ontag_trigger(nfc::NfcOnTagTrigger *trig) { this->triggers_ontag_.push_back(trig); }
#endif
#ifdef PN532_ON_TAG_REMOVED_TRIGGER_COUNT
  void register_ontagremoved_trigger(nfc::NfcOnTagTrigger *trig) { this->triggers_ontagremoved_.push_back(trig); }
#endif

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
  bool write_command_(std::span<const uint8_t> data);
  bool write_command_(std::initializer_list<uint8_t> data) {
    return this->write_command_(std::span<const uint8_t>(data.begin(), data.size()));
  }
  bool read_ack_();
  void send_ack_();
  void send_nack_();

  enum PN532ReadReady read_ready_(bool block);
  virtual bool is_read_ready() = 0;
  virtual bool write_data(std::span<const uint8_t> data) = 0;
  /// Reads `len` frame bytes into `data` behind a leading status byte, so every bus presents the I2C layout
  virtual bool read_data(PN532Frame &data, uint8_t len) = 0;
  /// Reads the response to `command`; on success `data` holds only the bytes that follow the response code
  virtual bool read_response(uint8_t command, PN532Frame &data) = 0;

  std::unique_ptr<nfc::NfcTag> read_tag_(nfc::NfcTagUid &uid, uint8_t tag_type);

  bool format_tag_(nfc::NfcTagUid &uid, uint8_t tag_type);
  bool clean_tag_(nfc::NfcTagUid &uid, uint8_t tag_type);
  bool write_tag_(nfc::NfcTagUid &uid, uint8_t tag_type, nfc::NdefMessage *message);
  /// Sends an InDataExchange command and reads the response; returns false unless the status byte reports success.
  /// On success, `response` holds the data returned by the target, without the status byte.
  bool in_data_exchange_(std::span<const uint8_t> command, PN532Frame &response);
  bool in_data_exchange_(std::initializer_list<uint8_t> command, PN532Frame &response) {
    return this->in_data_exchange_(std::span<const uint8_t>(command.begin(), command.size()), response);
  }
  /// Sends MIFARE READ for `address` and returns the 16 bytes the tag answers with
  bool mifare_read_(uint8_t address, MifareReadData &data);

  std::unique_ptr<nfc::NfcTag> read_mifare_classic_tag_(nfc::NfcTagUid &uid);
  bool read_mifare_classic_block_(uint8_t block_num, MifareReadData &data);
  bool write_mifare_classic_block_(uint8_t block_num, std::span<const uint8_t> data);
  bool auth_mifare_classic_block_(nfc::NfcTagUid &uid, uint8_t block_num, uint8_t key_num, const uint8_t *key);
  bool format_mifare_classic_mifare_(nfc::NfcTagUid &uid);
  bool format_mifare_classic_ndef_(nfc::NfcTagUid &uid);
  bool write_mifare_classic_tag_(nfc::NfcTagUid &uid, nfc::NdefMessage *message);

  std::unique_ptr<nfc::NfcTag> read_mifare_ultralight_tag_(nfc::NfcTagUid &uid);
  bool read_mifare_ultralight_bytes_(uint8_t start_page, uint16_t num_bytes, UltralightReadBuffer &data);
  bool is_mifare_ultralight_formatted_(std::span<const uint8_t> page_3_to_6);
  uint16_t read_mifare_ultralight_capacity_();
  bool find_mifare_ultralight_ndef_(std::span<const uint8_t> page_3_to_6, uint8_t &message_length,
                                    uint8_t &message_start_index);
  bool write_mifare_ultralight_page_(uint8_t page_num, std::span<const uint8_t> write_data);
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
  LazyCallbackManager<void()> on_finished_write_callback_;
#ifdef PN532_BINARY_SENSOR_COUNT
  StaticVector<PN532BinarySensor *, PN532_BINARY_SENSOR_COUNT> binary_sensors_;
#endif
#ifdef PN532_ON_TAG_TRIGGER_COUNT
  StaticVector<nfc::NfcOnTagTrigger *, PN532_ON_TAG_TRIGGER_COUNT> triggers_ontag_;
#endif
#ifdef PN532_ON_TAG_REMOVED_TRIGGER_COUNT
  StaticVector<nfc::NfcOnTagTrigger *, PN532_ON_TAG_REMOVED_TRIGGER_COUNT> triggers_ontagremoved_;
#endif
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

}  // namespace esphome::pn532
