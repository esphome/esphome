#pragma once

#include "esphome/components/st25r/st25r.h"
#include "st25r300_registers.h"

namespace esphome {
namespace st25r300 {

// ST25R300 inherits the full shared state machine, NDEF, multi-tag, and
// finalize_scan_ logic from st25r::ST25R. Only chip-specific hardware
// differences (init, IRQ mapping, anticol framing, transceive) are overridden here.

class ST25R300 : public st25r::ST25R {
 public:
  void setup() override;
  void dump_config() override;
  void update() override;
  void loop() override;

 protected:
  // Hardware abstraction — implemented by st25r300_spi (pure virtual here, inherited from ST25R)
  virtual uint8_t read_register(uint8_t reg) = 0;
  virtual void write_register(uint8_t reg, uint8_t value) = 0;
  virtual void write_command(uint8_t command) = 0;
  virtual void write_fifo(const uint8_t *data, size_t len) = 0;
  virtual void read_fifo(uint8_t *data, size_t len) = 0;

  // Chip-specific overrides
  bool reset_chip() override;
  void reinitialize() override;
  void send_anticol_frame() override;
  bool transceive_ex(const uint8_t *data, size_t len, uint8_t *resp, uint8_t &resp_len,
                      bool with_crc, uint32_t timeout_ms = 150) override;
  std::unique_ptr<nfc::NfcTag> read_tag(std::vector<uint8_t> &uid) override;
  bool nfcv_ndef_write(nfc::NdefMessage *message) override;
  void send_halt() override;
  void start_wupa() override;
  void pre_select() override;  // no-op: ST25R300 manages mode in transceive_ex()
  uint8_t read_fifo_status1() override;
  uint8_t read_collision_display() override;

  // ST25R300-specific: send a 7-bit ISO14443A short frame (WUPA=0x52 or REQA=0x26).
  // ST25R300 has no dedicated WUPA/REQA command; must be done via FIFO + TRANSMIT_DATA.
  bool send_short_frame(uint8_t byte7, uint8_t *resp, uint8_t &resp_len, uint32_t timeout_ms = 10);

  bool transceive_(const uint8_t *data, size_t len, uint8_t *resp, uint8_t &resp_len,
                   uint32_t timeout_ms = 150);
  bool transceive_no_crc_(const uint8_t *data, size_t len, uint8_t *resp, uint8_t &resp_len,
                           uint32_t timeout_ms = 150);

  // NFC-V (ISO 15693) blocking inventory scan — called from update() before NFC-A
  void nfcv_scan();
  // NFC-B (ISO 14443B) blocking SENSB scan
  void nfcb_scan();
  void configure_nfcb_mode();
  void configure_nfcv_mode();
  void configure_nfca_mode();
  bool transceive_nfcv(const uint8_t *data, size_t len, uint8_t *resp, uint8_t &resp_len,
                        uint32_t timeout_ms = 25);

  static void isr(ST25R300 *arg);

  // ST25R300-specific accumulated IRQ state (3 registers → 2 bytes).
  // loop() translates these to the base class irq_status_ using base class bit constants.
  volatile uint8_t irq_status1_{0};
  uint8_t irq_status2_{0};
};

// ── Trigger and Action wrappers ───────────────────────────────────────────────
// Thin subclasses so the st25r300 Python component can reference its own type names
// while still using the shared trigger/action infrastructure from st25r::ST25R.

class ST25R300TagTrigger : public st25r::ST25RTagTrigger {
 public:
  explicit ST25R300TagTrigger(st25r::ST25R *parent) : st25r::ST25RTagTrigger(parent) {}
};

class ST25R300TagRemovedTrigger : public st25r::ST25RTagRemovedTrigger {
 public:
  explicit ST25R300TagRemovedTrigger(st25r::ST25R *parent) : st25r::ST25RTagRemovedTrigger(parent) {}
};

template<typename... Ts> class NDEFWriteAction : public st25r::NDEFWriteAction<Ts...> {
 public:
  void set_parent(st25r::ST25R *parent) { this->parent_ = parent; }
};

template<typename... Ts> class CleanTagAction : public st25r::CleanTagAction<Ts...> {
 public:
  void set_parent(st25r::ST25R *parent) { this->parent_ = parent; }
};

}  // namespace st25r300
}  // namespace esphome
