#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/automation.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/nfc/nfc.h"
#include "crypto1.h"
#include <map>
#include <set>
#include <vector>
#include <string>

namespace esphome {
namespace st25r {

class ST25RBinarySensor;

// ST25R Register Definitions
enum ST25RRegister : uint8_t {
  IO_CONF1 = 0x00,
  IO_CONF2 = 0x01,
  OP_CONTROL = 0x02,
  MODE = 0x03,
  BIT_RATE = 0x04,
  RX_CONF1 = 0x0B,
  RX_CONF2 = 0x0C,
  RX_CONF3 = 0x0D,
  RX_CONF4 = 0x0E,
  ISO14443A_CONF = 0x05,
  PASSIVE_TARGET = 0x08,  // Passive target definition (Space A): FDT alignment
  STREAM_MODE = 0x09,     // Stream mode config (Space A): din, dout, report_period
  AUX = 0x0A,             // Auxiliary definition (Space A): dis_corr bit for correlator
  MASK_MAIN = 0x16,
  MASK_TIMER = 0x17,
  IRQ_MAIN = 0x1A,
  IRQ_TIMER = 0x1B,
  IRQ_ERROR = 0x1C,
  FIFO_STATUS1 = 0x1E,
  FIFO_STATUS2 = 0x1F,
  COLLISION_DISPLAY = 0x20,
  NUM_TX_BYTES1 = 0x22,
  NUM_TX_BYTES2 = 0x23,
  AD_CONV_RESULT = 0x25,
  ANT_TUNE_A = 0x26,   // Antenna tuning DAC A (Space A): V = (0.044 + 0.868 * val/255) * VDD_A; default 0x80
  ANT_TUNE_B = 0x27,   // Antenna tuning DAC B (Space A): same formula; default 0x80
  TX_DRIVER_CONF = 0x28,
  PT_MOD = 0x29,        // PT modulation (Space A): RFO resistance in modulated state
  FIELD_THRESHOLD_ACTV = 0x2A,    // External field detector activation threshold
  FIELD_THRESHOLD_DEACTV = 0x2B,  // External field detector deactivation threshold
  IC_IDENTITY = 0x3F,
  // Space B registers (bit 0x40 set → SPI prefix 0xFB before address)
  EMD_SUP_CONF = 0x45,      // (Space B 0x05) EMD suppression config
  CORR_CONF1 = 0x4C,        // (Space B 0x0C) Correlator configuration 1
  CORR_CONF2 = 0x4D,        // (Space B 0x0D) Correlator configuration 2
  AUX_MOD = 0x68,            // (Space B 0x28) Aux modulation setting
  RES_AM_MOD = 0x6A,         // (Space B 0x2A) Resistive AM modulation
  OVERSHOOT_CONF1 = 0x70,   // (Space B 0x30) Overshoot protection config 1
  OVERSHOOT_CONF2 = 0x71,   // (Space B 0x31) Overshoot protection config 2
  UNDERSHOOT_CONF1 = 0x72,  // (Space B 0x32) Undershoot protection config 1
  UNDERSHOOT_CONF2 = 0x73,  // (Space B 0x33) Undershoot protection config 2
  REGULATOR_CONTROL = 0x2C, // Regulated voltage control (Space A)
};

// ST25R Commands
enum ST25RCommand : uint8_t {
  ST25R_CMD_SET_DEFAULT = 0xC1,
  ST25R_CMD_READ_FIFO = 0x9F,
  ST25R_CMD_STOP_ALL = 0xC2,
  ST25R_CMD_CLEAR_FIFO = 0xC3,  // Table 13: 0xC2/0xC3 = Stop all activities (clears FIFO state)
  ST25R_CMD_TRANSMIT_WITH_CRC = 0xC4,
  ST25R_CMD_TRANSMIT_WITHOUT_CRC = 0xC5,
  ST25R_CMD_TRANSMIT_REQA = 0xC6,
  ST25R_CMD_TRANSMIT_WUPA = 0xC7,
  ST25R_CMD_FIELD_ON = 0xC8,
  ST25R_CMD_FIELD_OFF = 0xC9,
  ST25R_CMD_MEASURE_AMPLITUDE = 0xD3,
  ST25R_CMD_RESET_RX_GAIN = 0xD5,
  ST25R_CMD_ADJUST_REGULATORS = 0xD6,
  ST25R_CMD_MEASURE_VDD = 0xDF,
};

class ST25R;

class ST25RTagTrigger : public Trigger<std::string> {
 public:
  explicit ST25RTagTrigger(ST25R *parent) : parent_(parent) {}

 protected:
  ST25R *parent_;
};

class ST25RTagRemovedTrigger : public Trigger<std::string> {
 public:
  explicit ST25RTagRemovedTrigger(ST25R *parent) : parent_(parent) {}

 protected:
  ST25R *parent_;
};

template<typename... Ts> class NDEFWriteAction : public Action<Ts...> {
 public:
  void set_parent(ST25R *parent) { parent_ = parent; }
  void set_message(std::function<nfc::NdefMessage *(Ts...)> func) { message_func_ = func; }
  void set_format(bool format) { format_ = format; }
  void play(const Ts &...x) override;

 protected:
  ST25R *parent_;
  std::function<nfc::NdefMessage *(Ts...)> message_func_;
  bool format_{false};
};

class ST25R : public PollingComponent, public nfc::Nfcc {
 public:
  enum State {
    STATE_IDLE,
    STATE_WUPA,
    STATE_ANTICOL,
    STATE_SELECT,
    STATE_REINITIALIZING,
  };

  void setup() override;
  void dump_config() override;
  void update() override;
  void loop() override;
  void process_state();
  float get_setup_priority() const override { return setup_priority::DATA; }

  bool ndef_write(nfc::NdefMessage *message, bool format = false);
  virtual bool nfcv_ndef_write(nfc::NdefMessage *message);  // NFC-V Type 5 WRITE_SINGLE_BLOCK path
  bool clean_tag();

  /// Send an APDU via ISO-DEP (ISO 14443-4) I-Block framing.
  /// Requires the tag to be ISO-DEP activated (SAK & 0x20, RATS already sent).
  /// Call multiple times for conversational APDU exchanges (block number toggles automatically).
  /// Returns true if a response was received; resp/resp_len contain the response data + SW1 SW2.
  /// Example from on_tag lambda:
  ///   uint8_t select[] = {0x00, 0xA4, 0x04, 0x00, 0x07, 0xA0, 0x00, 0x00, 0x05, 0x27, 0x20, 0x01};
  ///   uint8_t resp[64]; uint8_t len = 0;
  ///   if (id(reader).send_apdu(select, sizeof(select), resp, len)) { /* check resp */ }
  bool send_apdu(const uint8_t *apdu, size_t apdu_len, uint8_t *resp, uint8_t &resp_len);

  /// Check if the most recently selected tag supports ISO-DEP (SAK & 0x20).
  bool is_isodep_active() const { return this->last_sak_ & 0x20; }

  void set_reset_pin(GPIOPin *reset_pin) { this->reset_pin_ = reset_pin; }
  void set_irq_pin(InternalGPIOPin *irq_pin) { this->irq_pin_ = irq_pin; }
  void set_rf_field_enabled(bool enabled) { this->rf_field_enabled_ = enabled; }
  void set_rf_power(uint8_t power) { this->rf_power_ = power; }
  void set_supply_3v3(bool supply_3v3) { this->supply_3v3_ = supply_3v3; }
  void set_rx_gain_boost(bool boost) { this->rx_gain_boost_ = boost; }
  void set_mifare_key_a(uint64_t key) { this->mifare_key_a_ = key; }
  void set_mifare_key_b(uint64_t key) { this->mifare_key_b_ = key; }
  void set_miss_threshold(uint8_t t) { this->miss_threshold_ = t; }
  void set_ant_tune_a(uint8_t v) { this->ant_tune_a_ = v; }
  void set_ant_tune_b(uint8_t v) { this->ant_tune_b_ = v; }
  void set_health_check_enabled(bool v) { this->health_check_enabled_ = v; }
  void set_health_check_interval(uint32_t ms) { this->health_check_interval_ms_ = ms; }
  void set_max_failed_checks(uint8_t n) { this->max_failed_checks_ = n; }
  void set_auto_reset_on_failure(bool v) { this->auto_reset_on_failure_ = v; }
  void set_nfcv_enabled(bool v) { this->nfcv_enabled_ = v; }
  void set_nfcb_enabled(bool v) { this->nfcb_enabled_ = v; }
  void set_aat_enabled(bool v) { this->aat_enabled_ = v; }

  void register_on_tag_trigger(ST25RTagTrigger *trig) { this->on_tag_triggers_.push_back(trig); }
  void register_on_tag_removed_trigger(ST25RTagRemovedTrigger *trig) {
    this->on_tag_removed_triggers_.push_back(trig);
  }
  void register_tag(ST25RBinarySensor *tag) { this->binary_sensors_.push_back(tag); }
  void set_status_binary_sensor(binary_sensor::BinarySensor *sensor) { this->status_binary_sensor_ = sensor; }
  void set_field_strength_sensor(sensor::Sensor *sensor) { this->field_strength_sensor_ = sensor; }

  bool is_tag_present() const { return !this->present_tags_.empty(); }

 protected:
  virtual uint8_t read_register(uint8_t reg) = 0;
  virtual void write_register(uint8_t reg, uint8_t value) = 0;
  virtual void write_command(uint8_t command) = 0;
  virtual void write_fifo(const uint8_t *data, size_t len) = 0;
  virtual void read_fifo(uint8_t *data, size_t len) = 0;

  // Chip-specific overrides — each uses the ST25R3916 default or can be overridden
  virtual bool reset_chip();
  virtual void reinitialize();
  virtual void send_anticol_frame();
  virtual bool transceive_ex(const uint8_t *data, size_t len, uint8_t *resp, uint8_t &resp_len, bool with_crc, uint32_t timeout_ms);
  virtual std::unique_ptr<nfc::NfcTag> read_tag(std::vector<uint8_t> &uid);

  // Virtual helpers used by the shared process_state() to abstract chip differences
  virtual void send_halt();                  // encode and transmit the ISO14443A HALT command
  virtual void start_wupa();                 // clear chip state and transmit WUPA
  virtual void pre_select();                 // clear anticol mode before SELECT (chip-specific register)
  virtual uint8_t read_fifo_status1();       // read the chip's FIFO byte count register
  virtual uint8_t read_collision_display();  // read the chip's collision position register

  void field_on_();
  void finalize_scan_();

  // Automatic Antenna Tuning — hill-climbing optimizer for ANT_TUNE_A/B
  void aat_tune_();

  // ISO 14443-4 (ISO-DEP / Type 4 tags)
  bool isodep_activate_(uint8_t *ats, uint8_t &ats_len);  // Send RATS, parse ATS
  bool isodep_transceive_(const uint8_t *apdu, size_t apdu_len, uint8_t *resp, uint8_t &resp_len);
  std::unique_ptr<nfc::NfcTag> read_tag_type4_(std::vector<uint8_t> &uid);
  uint8_t isodep_block_number_{0};

  // NFC-B (ISO 14443B) support
  void nfcb_scan_();
  void configure_nfcb_mode_();

  // NFC-V (ISO 15693) support for ST25R3916 — streaming mode
  void nfcv_scan_();
  void configure_nfcv_stream_mode_();
  void restore_nfca_mode_();
  bool transceive_nfcv_stream_(const uint8_t *data, size_t len, uint8_t *resp, uint8_t &resp_len,
                               uint32_t timeout_ms = 30);
  // ISO 15693 encoding/decoding helpers
  static uint16_t iso15693_crc(const uint8_t *data, size_t len);
  static size_t iso15693_encode_1of4(const uint8_t *data, size_t len, bool add_crc, uint8_t *out, size_t out_max);
  static size_t iso15693_decode_manchester(const uint8_t *in, size_t in_len, uint8_t *out, size_t out_max);
  void apply_anticol_prefix_();
  bool wait_for_irq_(uint8_t mask, uint32_t timeout_ms);
  bool transceive_(const uint8_t *data, size_t len, uint8_t *resp, uint8_t &resp_len, uint32_t timeout_ms = 150);
  bool transceive_no_crc_(const uint8_t *data, size_t len, uint8_t *resp, uint8_t &resp_len, uint32_t timeout_ms = 150);
  // Mifare Classic — raw transceive with manual parity (no chip CRC/parity).
  // Adapted from mf1.c (MIT, github.com/suut/rfal-mifare-classic)
  bool transceive_mifare_(const uint8_t *data, const uint8_t *parity, uint8_t len,
                          uint8_t *resp, uint8_t *resp_parity, uint8_t &resp_len,
                          uint32_t timeout_ms = 15);
  bool mifare_authenticate_(uint8_t block, bool key_b, uint64_t key,
                            const uint8_t *uid, uint8_t uid_len,
                            struct Crypto1State *cs);
  bool mifare_read_block_(uint8_t block, uint8_t *data, struct Crypto1State *cs);
  static void isr(ST25R *arg);

  GPIOPin *reset_pin_{nullptr};
  InternalGPIOPin *irq_pin_{nullptr};

  bool rf_field_enabled_{true};
  uint8_t rf_power_{15};
  bool supply_3v3_{true};
  bool rx_gain_boost_{false};
  uint64_t mifare_key_a_{0xFFFFFFFFFFFFULL};
  uint64_t mifare_key_b_{0xFFFFFFFFFFFFULL};
  uint8_t miss_threshold_{3};
  uint8_t ant_tune_a_{0x80};  // AAT DAC A: 0x80 = mid-range (~0.48V on 3.3V / ~0.87V on 5V)
  uint8_t ant_tune_b_{0x40};  // RFAL default (not chip default 0x80)  // AAT DAC B: same
  bool is_b_version_{false};
  bool has_aat_{false};  // Automatic Antenna Tuning / varicap DAC (ANT_TUNE_A/B) available
  // Health check (chip liveness check, separate from tag scan interval)
  bool health_check_enabled_{true};
  uint32_t health_check_interval_ms_{60000};
  uint32_t last_health_check_ms_{0};
  uint8_t max_failed_checks_{3};
  bool auto_reset_on_failure_{true};
  bool nfcv_enabled_{true};
  bool nfcb_enabled_{true};
  bool aat_enabled_{true};  // AAT hill-climbing — improves range on boards with varicaps
  uint8_t health_check_failures_{0};
  uint8_t reinitialization_attempts_{0};
  volatile bool irq_triggered_{false};
  volatile uint8_t irq_status_{0};

  // Multi-tag tracking
  // present_tags_: UID → consecutive miss count (0 = seen this or prior scan)
  std::map<std::string, uint8_t> present_tags_;
  std::set<std::string> tags_this_scan_;  // UIDs found in current scan cycle
  std::map<std::string, std::unique_ptr<nfc::NfcTag>> tags_data_;  // UID → last read NfcTag

  // IRQ_MAIN (0x1A) bit definitions per Table 62
  static const uint8_t IRQ_OSC     = 0x80;  // bit7: oscillator stable
  static const uint8_t IRQ_WL      = 0x40;  // bit6: FIFO water level
  static const uint8_t IRQ_RXS     = 0x20;  // bit5: start of receive
  static const uint8_t IRQ_RXE     = 0x10;  // bit4: end of receive ← tag response
  static const uint8_t IRQ_TXE     = 0x08;  // bit3: end of transmission
  static const uint8_t IRQ_COL     = 0x04;  // bit2: bit collision ← anticollision
  static const uint8_t IRQ_RX_REST = 0x02;  // bit1: automatic reception restart
  // NRE (no-response timer expired): set in irq_status_ by derived class overrides
  // (e.g. ST25R300 translates its IRQ_STATUS2 NRE bit here). ST25R3916 uses millis() timeout.
  static const uint8_t IRQ_NRE = 0x01;

  State state_{STATE_IDLE};
  uint32_t last_state_change_{0};
  uint8_t cascade_level_{0};
  std::string current_uid_;
  uint8_t last_sak_{0};  // SAK from most recent SELECT (bit5=0x20 → ISO-DEP capable)

  // Anticollision loop state
  uint8_t anticol_prefix_[5]{};   // UID prefix bytes being used to narrow search
  uint8_t anticol_prefix_full_;   // complete prefix bytes
  uint8_t anticol_prefix_bits_;   // partial bits in last prefix byte
  uint8_t anticol_col_pos_{0};    // collision bit position (bits 0..col_pos are prefix)
  uint8_t anticol_prefix_val_{0}; // current prefix value being tried (brute-forced)

  // Multi-tag tree traversal: saved CL1 collision state for resuming after cascade CL2
  uint8_t saved_col_pos_{0};
  uint8_t saved_prefix_val_{0};
  bool saved_anticol_valid_{false};
  bool anticol_resume_{false};    // when true: STATE_WUPA uses saved prefix instead of resetting

  std::vector<ST25RTagTrigger *> on_tag_triggers_;
  std::vector<ST25RTagRemovedTrigger *> on_tag_removed_triggers_;
  std::vector<ST25RBinarySensor *> binary_sensors_;
  binary_sensor::BinarySensor *status_binary_sensor_{nullptr};
  sensor::Sensor *field_strength_sensor_{nullptr};
};

template<typename... Ts> void NDEFWriteAction<Ts...>::play(const Ts &...x) {
  auto *message = this->message_func_(x...);
  if (message != nullptr) {
    this->parent_->ndef_write(message, this->format_);
  }
}

template<typename... Ts> class CleanTagAction : public Action<Ts...> {
 public:
  void set_parent(ST25R *parent) { parent_ = parent; }
  void play(const Ts &...x) override { this->parent_->clean_tag(); }

 protected:
  ST25R *parent_;
};

class ST25RBinarySensor : public binary_sensor::BinarySensor {
 public:
  void set_uid(const std::vector<uint8_t> &uid) { uid_ = uid; }
  bool process(const std::string &uid);
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

}  // namespace st25r
}  // namespace esphome
