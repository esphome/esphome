#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/automation.h"
#include "esphome/components/nfc/nfc.h"
#include <map>
#include <set>
#include <vector>
#include <string>

namespace esphome {
namespace st25r {

// ST25R Register Definitions
enum ST25RRegister : uint8_t {
  IO_CONF1 = 0x00,
  IO_CONF2 = 0x01,
  OP_CONTROL = 0x02,
  MODE = 0x03,
  BIT_RATE = 0x04,
  ISO14443A_CONF = 0x05,
  PASSIVE_TARGET = 0x08,
  STREAM_MODE = 0x09,
  AUX = 0x0A,
  RX_CONF1 = 0x0B,
  RX_CONF2 = 0x0C,
  RX_CONF3 = 0x0D,
  RX_CONF4 = 0x0E,
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
  ANT_TUNE_A = 0x26,
  ANT_TUNE_B = 0x27,
  TX_DRIVER_CONF = 0x28,
  PT_MOD = 0x29,
  FIELD_THRESHOLD_ACTV = 0x2A,
  FIELD_THRESHOLD_DEACTV = 0x2B,
  REGULATOR_CONTROL = 0x2C,
  IC_IDENTITY = 0x3F,
  // Space B registers (bit 0x40 set -> SPI prefix 0xFB before address)
  EMD_SUP_CONF = 0x45,
  CORR_CONF1 = 0x4C,
  CORR_CONF2 = 0x4D,
  AUX_MOD = 0x68,
  RES_AM_MOD = 0x6A,
  OVERSHOOT_CONF1 = 0x70,
  OVERSHOOT_CONF2 = 0x71,
  UNDERSHOOT_CONF1 = 0x72,
  UNDERSHOOT_CONF2 = 0x73,
};

// ST25R Commands
enum ST25RCommand : uint8_t {
  ST25R_CMD_SET_DEFAULT = 0xC1,
  ST25R_CMD_STOP_ALL = 0xC2,
  ST25R_CMD_CLEAR_FIFO = 0xC3,
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

  void set_reset_pin(GPIOPin *reset_pin) { this->reset_pin_ = reset_pin; }
  void set_irq_pin(InternalGPIOPin *irq_pin) { this->irq_pin_ = irq_pin; }
  void set_rf_power(uint8_t power) { this->rf_power_ = power; }

  void register_on_tag_trigger(ST25RTagTrigger *trig) { this->on_tag_triggers_.push_back(trig); }
  void register_on_tag_removed_trigger(ST25RTagRemovedTrigger *trig) {
    this->on_tag_removed_triggers_.push_back(trig);
  }

  bool is_tag_present() const { return !this->present_tags_.empty(); }

 protected:
  virtual uint8_t read_register(uint8_t reg) = 0;
  virtual void write_register(uint8_t reg, uint8_t value) = 0;
  virtual void write_command(uint8_t command) = 0;
  virtual void write_fifo(const uint8_t *data, size_t len) = 0;
  virtual void read_fifo(uint8_t *data, size_t len) = 0;

  virtual bool reset_chip();
  virtual void reinitialize();
  virtual void send_anticol_frame();
  virtual bool transceive_ex(const uint8_t *data, size_t len, uint8_t *resp, uint8_t &resp_len, bool with_crc,
                             uint32_t timeout_ms);
  virtual std::unique_ptr<nfc::NfcTag> read_tag(std::vector<uint8_t> &uid);

  virtual void send_halt();
  virtual void start_wupa();
  virtual void pre_select();
  virtual uint8_t read_fifo_status1();
  virtual uint8_t read_collision_display();

  void field_on_();
  void finalize_scan_();
  void apply_anticol_prefix_();
  bool wait_for_irq_(uint8_t mask, uint32_t timeout_ms);
  bool transceive_(const uint8_t *data, size_t len, uint8_t *resp, uint8_t &resp_len,
                   uint32_t timeout_ms = 150);
  bool transceive_no_crc_(const uint8_t *data, size_t len, uint8_t *resp, uint8_t &resp_len,
                          uint32_t timeout_ms = 150);
  static void isr(ST25R *arg);

  GPIOPin *reset_pin_{nullptr};
  InternalGPIOPin *irq_pin_{nullptr};

  uint8_t rf_power_{15};
  uint8_t miss_threshold_{3};
  bool is_b_version_{false};
  bool has_aat_{false};

  volatile bool irq_triggered_{false};
  volatile uint8_t irq_status_{0};

  // Multi-tag tracking
  std::map<std::string, uint8_t> present_tags_;
  std::set<std::string> tags_this_scan_;
  std::map<std::string, std::unique_ptr<nfc::NfcTag>> tags_data_;

  // IRQ_MAIN bit definitions
  static const uint8_t IRQ_RXE = 0x10;
  static const uint8_t IRQ_TXE = 0x08;
  static const uint8_t IRQ_COL = 0x04;
  static const uint8_t IRQ_NRE = 0x01;

  State state_{STATE_IDLE};
  uint32_t last_state_change_{0};
  uint8_t cascade_level_{0};
  std::string current_uid_;
  uint8_t last_sak_{0};

  // Anticollision state
  uint8_t anticol_prefix_[5]{};
  uint8_t anticol_prefix_full_;
  uint8_t anticol_prefix_bits_;
  uint8_t anticol_col_pos_{0};
  uint8_t anticol_prefix_val_{0};

  // Saved CL1 state for cascade CL2 resume
  uint8_t saved_col_pos_{0};
  uint8_t saved_prefix_val_{0};
  bool saved_anticol_valid_{false};
  bool anticol_resume_{false};

  std::vector<ST25RTagTrigger *> on_tag_triggers_;
  std::vector<ST25RTagRemovedTrigger *> on_tag_removed_triggers_;
};

}  // namespace st25r
}  // namespace esphome
