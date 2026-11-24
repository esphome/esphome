#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/components/nfc/nfc.h"
#include "esphome/components/nfc/nfc_tag.h"
#include <memory>
#include <string>

namespace esphome::st25r {

// Space B registers have bit 0x40 set; the transport sends the 0xFB prefix before their address
enum ST25RRegister : uint8_t {
  ST25R_REG_IO_CONF1 = 0x00,
  ST25R_REG_IO_CONF2 = 0x01,
  ST25R_REG_OP_CONTROL = 0x02,
  ST25R_REG_MODE = 0x03,
  ST25R_REG_BIT_RATE = 0x04,
  ST25R_REG_ISO14443A_CONF = 0x05,
  ST25R_REG_PASSIVE_TARGET = 0x08,
  ST25R_REG_AUX = 0x0A,
  ST25R_REG_RX_CONF1 = 0x0B,
  ST25R_REG_RX_CONF2 = 0x0C,
  ST25R_REG_RX_CONF3 = 0x0D,
  ST25R_REG_RX_CONF4 = 0x0E,
  ST25R_REG_MASK_MAIN = 0x16,
  ST25R_REG_MASK_TIMER = 0x17,
  ST25R_REG_IRQ_MAIN = 0x1A,
  ST25R_REG_IRQ_TIMER = 0x1B,
  ST25R_REG_IRQ_ERROR = 0x1C,
  ST25R_REG_FIFO_STATUS1 = 0x1E,
  ST25R_REG_NUM_TX_BYTES1 = 0x22,
  ST25R_REG_NUM_TX_BYTES2 = 0x23,
  ST25R_REG_AD_CONV_RESULT = 0x25,
  ST25R_REG_ANT_TUNE_A = 0x26,
  ST25R_REG_ANT_TUNE_B = 0x27,
  ST25R_REG_TX_DRIVER_CONF = 0x28,
  ST25R_REG_PT_MOD = 0x29,
  ST25R_REG_FIELD_THRESHOLD_ACTV = 0x2A,
  ST25R_REG_FIELD_THRESHOLD_DEACTV = 0x2B,
  ST25R_REG_REGULATOR_CONTROL = 0x2C,
  ST25R_REG_IC_IDENTITY = 0x3F,
  ST25R_REG_EMD_SUP_CONF = 0x45,
  ST25R_REG_CORR_CONF1 = 0x4C,
  ST25R_REG_CORR_CONF2 = 0x4D,
  ST25R_REG_AUX_MOD = 0x68,
  ST25R_REG_RES_AM_MOD = 0x6A,
  ST25R_REG_OVERSHOOT_CONF1 = 0x70,
  ST25R_REG_OVERSHOOT_CONF2 = 0x71,
  ST25R_REG_UNDERSHOOT_CONF1 = 0x72,
  ST25R_REG_UNDERSHOOT_CONF2 = 0x73,
};

enum ST25RCommand : uint8_t {
  ST25R_CMD_SET_DEFAULT = 0xC1,
  ST25R_CMD_CLEAR_FIFO = 0xC3,
  ST25R_CMD_TRANSMIT_WITH_CRC = 0xC4,
  ST25R_CMD_TRANSMIT_WITHOUT_CRC = 0xC5,
  ST25R_CMD_TRANSMIT_WUPA = 0xC7,
  ST25R_CMD_FIELD_ON = 0xC8,
  ST25R_CMD_RESET_RX_GAIN = 0xD5,
  ST25R_CMD_ADJUST_REGULATORS = 0xD6,
  ST25R_CMD_MEASURE_VDD = 0xDF,
};

class ST25R : public PollingComponent, public nfc::Nfcc {
 public:
  enum State : uint8_t {
    STATE_IDLE,
    STATE_FIELD_ON,
    STATE_WUPA,
    STATE_ANTICOL,
  };

  void setup() override;
  void dump_config() override;
  void update() override;
  void loop() override;

  void set_reset_pin(GPIOPin *reset_pin) { this->reset_pin_ = reset_pin; }
  void set_irq_pin(InternalGPIOPin *irq_pin) { this->irq_pin_ = irq_pin; }
  void set_rf_power(uint8_t power) { this->rf_power_ = power; }

  template<typename F> void add_on_tag_callback(F &&callback) { this->on_tag_callback_.add(std::forward<F>(callback)); }
  template<typename F> void add_on_tag_removed_callback(F &&callback) {
    this->on_tag_removed_callback_.add(std::forward<F>(callback));
  }

 protected:
  virtual uint8_t read_register(uint8_t reg) = 0;
  virtual void write_register(uint8_t reg, uint8_t value) = 0;
  virtual void write_command(uint8_t command) = 0;
  virtual void write_fifo(const uint8_t *data, size_t len) = 0;
  virtual void read_fifo(uint8_t *data, size_t len) = 0;

  bool reset_chip_();
  void pulse_reset_();
  void clear_irqs_();
  void write_register_table_(const uint8_t (*table)[2], size_t count);
  void field_on_();
  void send_anticol_frame_();
  bool transceive_ex_(const uint8_t *data, size_t len, uint8_t *resp, uint8_t resp_cap, uint8_t &resp_len,
                      uint32_t timeout_ms);
  void process_state_();
  void finish_scan_();
  void finalize_scan_();
  void remove_tag_();
  static void isr(ST25R *arg);

  GPIOPin *reset_pin_{nullptr};
  InternalGPIOPin *irq_pin_{nullptr};

  // Consecutive empty polls before a tag counts as removed
  static constexpr uint8_t MISS_THRESHOLD = 3;
  static constexpr uint32_t FIELD_GUARD_MS = 10;

  // IRQ_MAIN bit definitions
  static constexpr uint8_t IRQ_RXE = 0x10;
  static constexpr uint8_t IRQ_TXE = 0x08;
  static constexpr uint8_t IRQ_COL = 0x04;

  std::unique_ptr<nfc::NfcTag> current_tag_;
  nfc::NfcTagUid scan_uid_;
  uint32_t last_state_change_{0};
  State state_{STATE_IDLE};
  uint8_t rf_power_{15};
  uint8_t miss_count_{0};
  uint8_t irq_status_{0};
  uint8_t cascade_level_{0};
  bool tag_found_this_scan_{false};
  volatile bool irq_fired_{false};

  LazyCallbackManager<void(const std::string &, const nfc::NfcTag &)> on_tag_callback_;
  LazyCallbackManager<void(const std::string &, const nfc::NfcTag &)> on_tag_removed_callback_;
};

}  // namespace esphome::st25r
