#include "st25r300.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/components/nfc/nfc_tag.h"
#include "esphome/components/nfc/nfc_helpers.h"
#include <algorithm>
#include <cstring>

namespace esphome {
namespace st25r300 {

static const char *const TAG = "st25r300";

void ST25R300::isr(ST25R300 *arg) {
  arg->irq_triggered_ = true;
}

void ST25R300::setup() {
  ESP_LOGI(TAG, "Setting up ST25R300...");
  if (this->reset_pin_ != nullptr) {
    ESP_LOGI(TAG, "Cycling reset pin...");
    this->reset_pin_->setup();
    this->reset_pin_->digital_write(true);   // assert reset
    delay(10);
    this->reset_pin_->digital_write(false);  // release reset
    delay(10);
  }

  if (this->irq_pin_ != nullptr) {
    ESP_LOGI(TAG, "Configuring IRQ pin...");
    this->irq_pin_->setup();
    this->irq_pin_->attach_interrupt(ST25R300::isr, this, gpio::INTERRUPT_RISING_EDGE);
  }

  if (this->status_binary_sensor_ != nullptr)
    this->status_binary_sensor_->publish_initial_state(false);

  ESP_LOGI(TAG, "Starting reset_chip()...");
  if (!this->reset_chip()) {
    ESP_LOGE(TAG, "Failed to reset/init ST25R300");
    this->mark_failed();
    return;
  }
  ESP_LOGI(TAG, "ST25R300 initialized successfully.");
}

void ST25R300::update() {
  if (this->is_failed() || this->state_ != STATE_IDLE) return;

  // Health check: verify chip identity at the configured interval (default 60s),
  // independent of the tag scan rate.  Uses ST25R300-specific IC identity value
  // (chip_type=0xB0) but otherwise mirrors ST25R::update() so that all four
  // configurable options (health_check_enabled, health_check_interval,
  // max_failed_checks, auto_reset_on_failure) are honoured.
  if (this->health_check_enabled_) {
    uint32_t now = millis();
    if (now - this->last_health_check_ms_ >= this->health_check_interval_ms_) {
      this->last_health_check_ms_ = now;
      uint8_t ic_identity = this->read_register(ST25R300_REG_IC_IDENTITY);
      if ((ic_identity & ST25R300_IC_TYPE_MASK) != ST25R300_IC_TYPE_VAL) {
        ESP_LOGW(TAG, "Health check failed: IC identity 0x%02X (failures: %u/%u)",
                 ic_identity, this->health_check_failures_ + 1, this->max_failed_checks_);
        this->health_check_failures_++;
        if (this->status_binary_sensor_ != nullptr)
          this->status_binary_sensor_->publish_state(false);
        if (this->health_check_failures_ >= this->max_failed_checks_) {
          if (this->auto_reset_on_failure_) {
            ESP_LOGW(TAG, "Health check: max failures reached, triggering reinit");
            this->state_ = STATE_REINITIALIZING;
          } else {
            ESP_LOGE(TAG, "Health check: max failures reached, auto-reset disabled");
          }
        }
        return;
      }
      this->health_check_failures_ = 0;
      if (this->status_binary_sensor_ != nullptr)
        this->status_binary_sensor_->publish_state(true);
    }
  }

  // Clear all IRQ registers (read-to-clear) and FIFO
  this->read_register(ST25R300_REG_IRQ_STATUS1);
  this->read_register(ST25R300_REG_IRQ_STATUS2);
  this->read_register(ST25R300_REG_IRQ_STATUS3);
  this->write_command(ST25R300_CMD_CLEAR_FIFO);
  this->write_command(ST25R300_CMD_CLEAR_RX_GAIN);

  if (this->rf_field_enabled_)
    this->write_register(ST25R300_REG_OPERATION, ST25R300_OP_ALL_ON);

  if (this->rf_field_enabled_ && this->field_strength_sensor_ != nullptr) {
    uint8_t sense = this->read_register(ST25R300_REG_SENSE_RF);
    this->field_strength_sensor_->publish_state(sense);
  }

  // NFC-V (ISO 15693) blocking inventory — runs before NFC-A scan
  if (this->nfcv_enabled_)
    this->nfcv_scan_();

  // NFC-B (ISO 14443B) blocking SENSB — runs before NFC-A scan
  if (this->nfcb_enabled_)
    this->nfcb_scan_();

  this->saved_anticol_valid_ = false;
  this->anticol_resume_ = false;

  this->irq_status1_ = 0;
  this->irq_status2_ = 0;
  this->irq_status_ = 0;
  this->irq_triggered_ = false;

  // ST25R300 has no dedicated WUPA command — send 7-bit short frame via FIFO
  uint8_t resp[2];
  uint8_t resp_len = 0;
  this->send_short_frame_(0x52, resp, resp_len, 10);  // 0x52 = WUPA
  ESP_LOGI(TAG, "Sent WUPA");
  delay(1);
  this->state_ = STATE_WUPA;
  this->last_state_change_ = millis();
}

// ── loop ──────────────────────────────────────────────────────────────────────
// Overrides base to translate ST25R300's three IRQ registers into the normalized
// irq_status_ uint8_t used by the shared process_state() state machine.
void ST25R300::loop() {
  if (this->is_failed()) return;

  if (this->irq_triggered_) {
    this->irq_triggered_ = false;
    uint8_t r1 = this->read_register(ST25R300_REG_IRQ_STATUS1);
    uint8_t r2 = this->read_register(ST25R300_REG_IRQ_STATUS2);
    this->irq_status1_ |= r1;
    this->irq_status2_ |= r2;
    ESP_LOGV(TAG, "IRQ triggered, IRQ1=0x%02X IRQ2=0x%02X state=%d", r1, r2, this->state_);
  } else if (this->state_ == STATE_WUPA || this->state_ == STATE_ANTICOL) {
    this->irq_status1_ |= this->read_register(ST25R300_REG_IRQ_STATUS1);
    this->irq_status2_ |= this->read_register(ST25R300_REG_IRQ_STATUS2);
    if (this->irq_status1_ != 0 || this->irq_status2_ != 0) {
      ESP_LOGV(TAG, "IRQ polled, IRQ1=0x%02X IRQ2=0x%02X state=%d",
               this->irq_status1_, this->irq_status2_, this->state_);
    }
  } else {
    this->irq_status1_ = 0;
    this->irq_status2_ = 0;
  }

  // Translate accumulated ST25R300 IRQ bits into the base class normalized format
  uint8_t n = 0;
  if (this->irq_status1_ & ST25R300_IRQ1_RXE) n |= IRQ_RXE;
  if (this->irq_status1_ & ST25R300_IRQ1_TXE) n |= IRQ_TXE;
  if (this->irq_status1_ & ST25R300_IRQ1_COL) n |= IRQ_COL;
  if (this->irq_status2_ & ST25R300_IRQ2_NRE) n |= IRQ_NRE;
  this->irq_status_ = n;

  this->process_state();
}

// ── send_short_frame_ ─────────────────────────────────────────────────────────
bool ST25R300::send_short_frame_(uint8_t byte7, uint8_t *resp, uint8_t &resp_len, uint32_t timeout_ms) {
  this->write_register(ST25R300_REG_TX_PROTOCOL1, 0x00);  // no CRC, no parity
  // b_rx_sof+b_rx_eof MUST be set for Manchester framing; no CRC for ATQA
  this->write_register(ST25R300_REG_RX_PROTOCOL1,
                       ST25R300_RX_PROT1_B_RX_SOF | ST25R300_RX_PROT1_B_RX_EOF |
                       ST25R300_RX_PROT1_A_RX_PAR);  // 0x38

  // CLEAR_FIFO (not STOP_ALL): STOP_ALL disables the RX decoder, preventing ATQA reception
  this->write_command(ST25R300_CMD_CLEAR_FIFO);
  this->read_register(ST25R300_REG_IRQ_STATUS1);
  this->read_register(ST25R300_REG_IRQ_STATUS2);
  this->read_register(ST25R300_REG_IRQ_STATUS3);
  this->write_command(ST25R300_CMD_CLEAR_RX_GAIN);
  this->irq_triggered_ = false;

  // TX_FRAME must be written before FIFO data
  this->write_register(ST25R300_REG_TX_FRAME1, 0x00);
  this->write_register(ST25R300_REG_TX_FRAME2, 0x07);  // 0 full bytes + 7 partial bits

  uint8_t data = byte7 & 0x7F;
  this->write_fifo(&data, 1);
  this->write_command(ST25R300_CMD_TRANSMIT_DATA);

  // Wait past ATQA window, then sweep IRQs into accumulator
  delayMicroseconds(700);
  this->irq_status1_ |= this->read_register(ST25R300_REG_IRQ_STATUS1);
  this->irq_status2_ |= this->read_register(ST25R300_REG_IRQ_STATUS2);
  ESP_LOGV(TAG, "ssf @700us: FIFO=%u IRQ1=0x%02X IRQ2=0x%02X",
           this->read_register(ST25R300_REG_FIFO_STATUS1), this->irq_status1_, this->irq_status2_);

  // Wait out the NRT (5ms) then do a final IRQ sweep to catch late arrivals
  delay(10);
  this->irq_status1_ |= this->read_register(ST25R300_REG_IRQ_STATUS1);
  this->irq_status2_ |= this->read_register(ST25R300_REG_IRQ_STATUS2);
  resp_len = 0;
  return true;
}

// ── transceive_ / transceive_no_crc_ ─────────────────────────────────────────
bool ST25R300::transceive_(const uint8_t *data, size_t len, uint8_t *resp, uint8_t &resp_len,
                            uint32_t timeout_ms) {
  return this->transceive_ex(data, len, resp, resp_len, true, timeout_ms);
}
bool ST25R300::transceive_no_crc_(const uint8_t *data, size_t len, uint8_t *resp, uint8_t &resp_len,
                                   uint32_t timeout_ms) {
  return this->transceive_ex(data, len, resp, resp_len, false, timeout_ms);
}

// ── transceive_ex_ ────────────────────────────────────────────────────────────
bool ST25R300::transceive_ex(const uint8_t *data, size_t len, uint8_t *resp, uint8_t &resp_len,
                               bool with_crc, uint32_t timeout_ms) {
  this->write_command(ST25R300_CMD_CLEAR_FIFO);
  this->write_command(ST25R300_CMD_CLEAR_RX_GAIN);
  this->read_register(ST25R300_REG_IRQ_STATUS1);
  this->read_register(ST25R300_REG_IRQ_STATUS2);
  this->read_register(ST25R300_REG_IRQ_STATUS3);

  this->write_register(ST25R300_REG_TX_FRAME1, (uint8_t)((len >> 5) & 0xFF));
  this->write_register(ST25R300_REG_TX_FRAME2, (uint8_t)((len & 0x1F) << 3));  // full bytes, 0 partial bits

  if (with_crc) {
    this->write_register(ST25R300_REG_TX_PROTOCOL1,
                         ST25R300_TX_PROT1_A_TX_PAR | ST25R300_TX_PROT1_TX_CRC);  // 0x60
    this->write_register(ST25R300_REG_RX_PROTOCOL1,
                         ST25R300_RX_PROT1_B_RX_SOF | ST25R300_RX_PROT1_B_RX_EOF |
                         ST25R300_RX_PROT1_A_RX_PAR | ST25R300_RX_PROT1_RX_CRC);  // 0x3C
  } else {
    this->write_register(ST25R300_REG_TX_PROTOCOL1, 0x00);
    this->write_register(ST25R300_REG_RX_PROTOCOL1,
                         ST25R300_RX_PROT1_B_RX_SOF | ST25R300_RX_PROT1_B_RX_EOF |
                         ST25R300_RX_PROT1_A_RX_PAR);  // 0x38, no CRC
  }

  this->write_fifo(data, len);
  this->irq_triggered_ = false;
  this->write_command(ST25R300_CMD_TRANSMIT_DATA);

  uint32_t start = millis();
  resp_len = 0;
  bool tx_done = false;

  while (millis() - start < timeout_ms) {
    this->irq_triggered_ = false;
    uint8_t irq1 = this->read_register(ST25R300_REG_IRQ_STATUS1);
    uint8_t irq2 = this->read_register(ST25R300_REG_IRQ_STATUS2);

    if (irq1 & ST25R300_IRQ1_TXE) tx_done = true;

    if (tx_done) {
      uint8_t f1 = this->read_register(ST25R300_REG_FIFO_STATUS1);
      if (f1 > 0) {
        uint8_t to_read = std::min((uint8_t)(64 - resp_len), f1);
        this->read_fifo(resp + resp_len, to_read);
        resp_len += to_read;
        start = millis();
      }
      if (irq1 & ST25R300_IRQ1_RXE) return resp_len > 0;
      if (irq2 & ST25R300_IRQ2_NRE) return resp_len > 0;
    }
    delay(1);
  }
  return resp_len > 0;
}

// ── read_tag_ ─────────────────────────────────────────────────────────────────
// ST25R300 supports NFC Forum Type 2 (NTAG) only.
// Mifare Classic authentication is not implemented for ST25R300 (different MAC engine).
std::unique_ptr<nfc::NfcTag> ST25R300::read_tag(std::vector<uint8_t> &uid) {
  uint8_t type = nfc::guess_tag_type(uid.size());
  ESP_LOGI(TAG, "read_tag_: UID length=%zu, guessed type=%d", uid.size(), type);

  if (type == nfc::TAG_TYPE_2) {
    uint8_t buffer[16];
    uint8_t len;
    std::vector<uint8_t> data;

    uint8_t read_cmd[2] = {0x30, 0x00};
    if (this->transceive_(read_cmd, 2, buffer, len) && len >= 16) {
      data.insert(data.end(), buffer, buffer + 16);

      size_t tlv_index = 0;
      bool found = false;
      bool terminator_found = false;

      for (size_t i = 0; i < 16; i++) {
        if (data[i] == 0x03) { tlv_index = i; found = true; break; }
        if (data[i] == 0xFE) { terminator_found = true; break; }
      }

      if (!found && !terminator_found) {
        for (uint8_t p = 4; p < 16; p += 4) {
          delay(10);
          read_cmd[1] = p;
          if (this->transceive_(read_cmd, 2, buffer, len) && len >= 16) {
            data.insert(data.end(), buffer, buffer + 16);
            for (size_t i = data.size() - 16; i < data.size(); i++) {
              if (data[i] == 0x03) { tlv_index = i; found = true; break; }
              if (data[i] == 0xFE) { terminator_found = true; break; }
            }
          }
          if (found || terminator_found) break;
        }
      }

      if (found) {
        while (data.size() <= tlv_index + 3) {
          read_cmd[1] = (uint8_t)(data.size() / 4);
          delay(10);
          if (!this->transceive_(read_cmd, 2, buffer, len) || len < 16) break;
          data.insert(data.end(), buffer, buffer + 16);
        }
        if (tlv_index + 1 < data.size()) {
          size_t msg_len, msg_start;
          if (data[tlv_index + 1] == 0xFF && tlv_index + 3 < data.size()) {
            msg_len   = ((size_t)data[tlv_index + 2] << 8) | data[tlv_index + 3];
            msg_start = tlv_index + 4;
          } else {
            msg_len   = data[tlv_index + 1];
            msg_start = tlv_index + 2;
          }
          while (data.size() < msg_start + msg_len) {
            read_cmd[1] = (uint8_t)(data.size() / 4);
            delay(10);
            if (!this->transceive_(read_cmd, 2, buffer, len) || len < 16) break;
            data.insert(data.end(), buffer, buffer + 16);
          }
          if (data.size() >= msg_start + msg_len) {
            std::vector<uint8_t> ndef_data(data.begin() + (int)msg_start,
                                           data.begin() + (int)msg_start + (int)msg_len);
            nfc::NfcTagUid nfc_uid(uid.begin(), uid.end());
            if (msg_len > 0) {
              ESP_LOGD(TAG, "read_tag_: NDEF found, %zu bytes at TLV offset %zu", msg_len, tlv_index);
              return make_unique<nfc::NfcTag>(nfc_uid, nfc::NFC_FORUM_TYPE_2, ndef_data);
            }
            ESP_LOGD(TAG, "read_tag_: NDEF TLV present but empty");
            return make_unique<nfc::NfcTag>(nfc_uid, nfc::NFC_FORUM_TYPE_2);
          }
        }
      }
      ESP_LOGD(TAG, "read_tag_: no NDEF TLV found (found=%d terminator=%d)", found, terminator_found);
    } else {
      ESP_LOGW(TAG, "Failed to read page 0, len=%d", len);
    }
  }

  nfc::NfcTagUid nfc_uid(uid.begin(), uid.end());
  return make_unique<nfc::NfcTag>(nfc_uid);
}

// ── Virtual helper overrides ───────────────────────────────────────────────────

uint8_t ST25R300::read_fifo_status1() {
  return this->read_register(ST25R300_REG_FIFO_STATUS1);
}

uint8_t ST25R300::read_collision_display() {
  return this->read_register(ST25R300_REG_COLLISION_DISPLAY);
}

void ST25R300::pre_select() {
  // No-op: ST25R300 manages RX mode (antcl) via RX_PROTOCOL1 inside transceive_ex().
  // There is no separate "anticol mode" register to clear before SELECT.
}

void ST25R300::send_halt() {
  uint8_t halt_cmd[2] = {0x50, 0x00};
  this->write_command(ST25R300_CMD_CLEAR_FIFO);
  this->read_register(ST25R300_REG_IRQ_STATUS1);
  this->read_register(ST25R300_REG_IRQ_STATUS2);
  this->read_register(ST25R300_REG_IRQ_STATUS3);
  // TX_FRAME must be written before FIFO data (per datasheet)
  this->write_register(ST25R300_REG_TX_FRAME1, 0x00);
  this->write_register(ST25R300_REG_TX_FRAME2, 0x10);  // 2 full bytes
  this->write_register(ST25R300_REG_TX_PROTOCOL1,
                       ST25R300_TX_PROT1_A_TX_PAR | ST25R300_TX_PROT1_TX_CRC);
  this->write_fifo(halt_cmd, 2);
  this->write_command(ST25R300_CMD_TRANSMIT_DATA);
  delay(10);
}

void ST25R300::start_wupa() {
  // Clear FIFO + IRQs + chip accumulators, then transmit WUPA as short frame
  this->write_command(ST25R300_CMD_CLEAR_FIFO);
  this->read_register(ST25R300_REG_IRQ_STATUS1);
  this->read_register(ST25R300_REG_IRQ_STATUS2);
  this->read_register(ST25R300_REG_IRQ_STATUS3);
  this->irq_triggered_ = false;
  this->irq_status1_ = 0;
  this->irq_status2_ = 0;
  this->irq_status_ = 0;
  uint8_t dummy[2];
  uint8_t dummy_len = 0;
  this->send_short_frame_(0x52, dummy, dummy_len, 10);
}

// ── send_anticol_frame_ ───────────────────────────────────────────────────────
void ST25R300::send_anticol_frame() {
  uint8_t sel_cmds[] = {0x93, 0x95, 0x97};
  uint8_t sel = sel_cmds[this->cascade_level_];

  uint8_t nvb_high = 2 + this->anticol_prefix_full_;
  uint8_t nvb = (nvb_high << 4) | this->anticol_prefix_bits_;

  uint8_t frame[7];
  frame[0] = sel;
  frame[1] = nvb;
  uint8_t frame_len = 2;
  for (int i = 0; i < this->anticol_prefix_full_; i++)
    frame[frame_len++] = this->anticol_prefix_[i];
  if (this->anticol_prefix_bits_ > 0)
    frame[frame_len++] = this->anticol_prefix_[this->anticol_prefix_full_];

  uint8_t ntx_n = 2 + this->anticol_prefix_full_;
  uint8_t ntx_b = this->anticol_prefix_bits_;

  // a_rx_par MUST be set: without it, parity bits from 9-bit NFC-A bytes are packed into FIFO
  // alongside data bits, garbling the UID. a_rx_par strips parity before FIFO write.
  this->write_register(ST25R300_REG_RX_PROTOCOL1,
                       ST25R300_RX_PROT1_B_RX_SOF | ST25R300_RX_PROT1_B_RX_EOF |
                       ST25R300_RX_PROT1_A_RX_PAR | ST25R300_RX_PROT1_ANTCL);  // 0x39
  this->write_register(ST25R300_REG_TX_PROTOCOL1, ST25R300_TX_PROT1_A_TX_PAR);  // parity, no CRC

  // CLEAR_FIFO (not STOP_ALL): STOP_ALL disables the RX decoder, preventing collision detection.
  this->write_command(ST25R300_CMD_CLEAR_FIFO);
  this->read_register(ST25R300_REG_IRQ_STATUS1);
  this->read_register(ST25R300_REG_IRQ_STATUS2);
  this->read_register(ST25R300_REG_IRQ_STATUS3);
  this->write_command(ST25R300_CMD_CLEAR_RX_GAIN);
  this->irq_triggered_ = false;
  this->irq_status1_ = 0;
  this->irq_status2_ = 0;
  // TX_FRAME must be written before FIFO data
  this->write_register(ST25R300_REG_TX_FRAME1, ntx_n >> 5);
  this->write_register(ST25R300_REG_TX_FRAME2, (uint8_t)(((ntx_n & 0x1F) << 3) | (ntx_b & 0x07)));
  this->write_fifo(frame, frame_len);
  this->write_command(ST25R300_CMD_TRANSMIT_DATA);
}

// ── reset_ ────────────────────────────────────────────────────────────────────
bool ST25R300::reset_chip() {
  // Verify IC identity BEFORE SET_DEFAULT — if bus is down, don't clear registers.
  uint8_t ic_identity = this->read_register(ST25R300_REG_IC_IDENTITY);
  ESP_LOGD(TAG, "  reset_: IC identity read: 0x%02X", ic_identity);
  if ((ic_identity & ST25R300_IC_TYPE_MASK) != ST25R300_IC_TYPE_VAL) {
    ESP_LOGE(TAG, "  reset_: IC identity mismatch! Expected 0xB0, got chip_type=0x%02X",
             ic_identity & ST25R300_IC_TYPE_MASK);
    return false;
  }

  ESP_LOGV(TAG, "  reset_: Sending SET_DEFAULT");
  this->write_command(ST25R300_CMD_SET_DEFAULT);
  delay(10);
  ESP_LOGI(TAG, "IC identity match: 0x%02X (ST25R300 rev %u)", ic_identity, ic_identity & 0x07);

  // Step 1: Enable oscillator → enter Ready mode
  this->write_register(ST25R300_REG_OPERATION, ST25R300_OP_EN);
  delay(5);

  // Wait for oscillator stable (I_osc in IRQ_STATUS3, bit0)
  uint32_t osc_start = millis();
  while (millis() - osc_start < 50) {
    if (this->read_register(ST25R300_REG_IRQ_STATUS3) & ST25R300_IRQ3_OSC) break;
    delay(1);
  }

  // Step 2: Enable VDDDR regulator, wait ≥10µs
  this->write_register(ST25R300_REG_OPERATION, ST25R300_OP_EN | ST25R300_OP_VDDDR_EN);
  delay(1);

  // Step 3: Enable TX + RX
  this->write_register(ST25R300_REG_OPERATION, ST25R300_OP_ALL_ON);

  // Step 4: Adjust regulators
  this->write_command(ST25R300_CMD_ADJUST_REGS);
  delay(5);

  // Step 5: Configure for NFC-A / ISO14443A
  this->write_register(ST25R300_REG_GENERAL_CONF, 0x00);  // differential antenna, no RFO2
  this->write_register(ST25R300_REG_PROTOCOL1, 0x01);      // om=1: NFC-A reader/initiator
  this->write_register(ST25R300_REG_TX_PROTOCOL1,
                       ST25R300_TX_PROT1_A_TX_PAR | ST25R300_TX_PROT1_TX_CRC);  // 0x60
  this->write_register(ST25R300_REG_RX_PROTOCOL1,
                       ST25R300_RX_PROT1_B_RX_SOF | ST25R300_RX_PROT1_B_RX_EOF |
                       ST25R300_RX_PROT1_A_RX_PAR | ST25R300_RX_PROT1_RX_CRC);  // 0x3C

  // RX path analog and correlator — tuned for NFC-A 106kbps (RFAL ST25R500 analogConfigTbl)
  uint8_t rx_ana1 = 0x73 | (this->rx_gain_boost_ ? 0x04 : 0x00);
  this->write_register(ST25R300_REG_RX_ANALOG1, rx_ana1);
  this->write_register(0x0A, 0x22);  // afe_gain_rw=2 (6dB), afe_gain_td=2
  this->write_register(0x0B, 0x85);  // RX Ana3: ook thresholds (factory default)
  this->write_register(0x0D, 0xCC);  // RX Digital: agc_en=1, lpf/hpf coef (factory default)
  // Correlator settings critical for NFC-A reception (factory defaults do NOT work)
  this->write_register(0x0E, 0xF8);  // CORR1: iir_coef2=F, iir_coef1=8
  this->write_register(0x0F, 0x2E);  // CORR2: squelch_thr=2, agc_thr=E
  this->write_register(0x10, 0x0F);  // CORR3: start_wait=15 (prevents false subc_start from TX)
  this->write_register(0x11, 0x88);  // CORR4: coll_lvl=8, data_lvl=8
  this->write_register(0x12, 0x32);  // CORR5: dis_soft_sq=1, dis_agc_noise_meas=1, dec_f=2
  this->write_register(0x13, 0x20);  // CORR6: init_noise_lvl=2

  // TX driver: d_res controls power (0=max, 15=min)
  uint8_t d_res = (15 - this->rf_power_) & 0x0F;
  this->write_register(ST25R300_REG_TX_DRIVER_CONF, d_res);
  this->write_register(ST25R300_REG_TX_MOD1, 0x70);  // am_mod=7 (20%)

  // Unmask all IRQs
  this->write_register(ST25R300_REG_IRQ_MASK1, 0x00);
  this->write_register(ST25R300_REG_IRQ_MASK2, 0x00);
  this->write_register(ST25R300_REG_IRQ_MASK3, 0x00);

  // NRT: ~5ms window to capture ATQA (~86µs after TX-end)
  this->write_register(ST25R300_REG_NRT_CONF1, 0x00);  // nrt_step=0 → 4.72µs per step
  this->write_register(ST25R300_REG_NRT_CONF2, 0x04);  // nrt[15:8]
  this->write_register(ST25R300_REG_NRT_CONF3, 0x23);  // nrt[7:0] = 0x0423 = 1059 steps ≈ 5ms

  if (this->rf_field_enabled_) {
    this->write_register(ST25R300_REG_OPERATION,
                         ST25R300_OP_EN | ST25R300_OP_VDDDR_EN | ST25R300_OP_TX_EN);
    delay(5);
    this->write_command(ST25R300_CMD_FIELD_ON);
    delay(10);
    this->write_register(ST25R300_REG_OPERATION, ST25R300_OP_ALL_ON);
  }
  delay(10);
  return true;
}

// ── reinitialize_ ─────────────────────────────────────────────────────────────
void ST25R300::reinitialize() {
  this->reinitialization_attempts_++;
  ESP_LOGW(TAG, "Reinitializing ST25R300 (attempt %u)...", this->reinitialization_attempts_);
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->digital_write(true);
    delay(10);
    this->reset_pin_->digital_write(false);
    delay(10);
  }
  if (this->reset_chip()) {
    ESP_LOGI(TAG, "Reinitialize succeeded after %u attempt(s)", this->reinitialization_attempts_);
    this->health_check_failures_ = 0;
    this->reinitialization_attempts_ = 0;
    // Force an immediate health check on the next update() so the status sensor
    // reflects recovery without waiting the full health_check_interval.
    this->last_health_check_ms_ = 0;
  } else {
    ESP_LOGE(TAG, "Reinitialize attempt %u failed", this->reinitialization_attempts_);
    if (this->reinitialization_attempts_ >= 5) {
      ESP_LOGE(TAG, "Reinitialize: too many failures, marking component failed");
      this->mark_failed();
    }
  }
}

// ── dump_config ───────────────────────────────────────────────────────────────
void ST25R300::dump_config() {
  ESP_LOGCONFIG(TAG, "ST25R300:");
  LOG_PIN("  IRQ Pin: ", this->irq_pin_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  ESP_LOGCONFIG(TAG, "  RF Power: %u", this->rf_power_);
  ESP_LOGCONFIG(TAG, "  RF Field Enabled: %s", YESNO(this->rf_field_enabled_));
  ESP_LOGCONFIG(TAG, "  Health Check: %s", YESNO(this->health_check_enabled_));
  if (this->health_check_enabled_) {
    ESP_LOGCONFIG(TAG, "  Health Check Interval: %u ms", this->health_check_interval_ms_);
    ESP_LOGCONFIG(TAG, "  Max Failed Checks: %u", this->max_failed_checks_);
    ESP_LOGCONFIG(TAG, "  Auto Reset on Failure: %s", YESNO(this->auto_reset_on_failure_));
  }
  LOG_UPDATE_INTERVAL(this);
  uint8_t ic_id = this->read_register(ST25R300_REG_IC_IDENTITY);
  ESP_LOGCONFIG(TAG, "  IC Identity (live): 0x%02X (chip_type=0x%02X)", ic_id,
                ic_id & ST25R300_IC_TYPE_MASK);
}

// ── NFC-V (ISO 15693) support ────────────────────────────────────────────────

void ST25R300::configure_nfcv_mode_() {
  this->write_register(ST25R300_REG_GENERAL_CONF, 0x02);    // nfc_n=2 (NFC-V subcarrier config)
  this->write_register(ST25R300_REG_PROTOCOL1, 0x05);      // om=5: NFC-V/ISO15693
  this->write_register(ST25R300_REG_TX_PROTOCOL1, 0x20);   // tx_crc only (no parity)
  this->write_register(ST25R300_REG_RX_PROTOCOL1,
                       ST25R300_RX_PROT1_B_RX_SOF | ST25R300_RX_PROT1_B_RX_EOF |
                       ST25R300_RX_PROT1_RX_CRC);  // 0x34: SOF+EOF+CRC (no parity, no antcl)
  // TX modulation: OOK (97% ASK) for NFC-V 26kbps — RFAL analogConfigTbl
  this->write_register(ST25R300_REG_TX_MOD1, 0x00);  // am_mod=0 → OOK
  // RX analog + correlator: RFAL ST25R500 NFC-V 26kbps profile
  this->write_register(ST25R300_REG_RX_ANALOG1, 0x71);  // dig_clk_dly=7, hpf_ctrl=1
  this->write_register(0x0D, 0x50);  // RX_DIG: lpf_coef + hpf_coef for NFC-V
  this->write_register(0x0E, 0xD0);  // CORR1
  this->write_register(0x0F, 0x2A);  // CORR2
  this->write_register(0x10, 0x08);  // CORR3: start_wait=8
  this->write_register(0x11, 0xAA);  // CORR4: coll_lvl=A, data_lvl=A
  this->write_register(0x12, 0xCC);  // CORR5
  this->write_register(0x13, 0x10);  // CORR6
  // NRT: ~20ms for NFC-V (4233 steps at 4.72µs/step)
  this->write_register(ST25R300_REG_NRT_CONF2, 0x10);
  this->write_register(ST25R300_REG_NRT_CONF3, 0x89);
}

void ST25R300::configure_nfca_mode_() {
  this->write_register(ST25R300_REG_GENERAL_CONF, 0x00);    // nfc_n=0 (NFC-A)
  this->write_register(ST25R300_REG_PROTOCOL1, 0x01);      // om=1: NFC-A
  this->write_register(ST25R300_REG_TX_PROTOCOL1,
                       ST25R300_TX_PROT1_A_TX_PAR | ST25R300_TX_PROT1_TX_CRC);  // 0x60
  this->write_register(ST25R300_REG_RX_PROTOCOL1,
                       ST25R300_RX_PROT1_B_RX_SOF | ST25R300_RX_PROT1_B_RX_EOF |
                       ST25R300_RX_PROT1_A_RX_PAR | ST25R300_RX_PROT1_RX_CRC);  // 0x3C
  // Restore NFC-A TX modulation + RX analog + correlator
  this->write_register(ST25R300_REG_TX_MOD1, 0x70);  // am_mod=7 (20% ASK)
  uint8_t rx_ana1 = 0x73 | (this->rx_gain_boost_ ? 0x04 : 0x00);
  this->write_register(ST25R300_REG_RX_ANALOG1, rx_ana1);
  this->write_register(0x0D, 0xCC);
  this->write_register(0x0E, 0xF8);
  this->write_register(0x0F, 0x2E);
  this->write_register(0x10, 0x0F);
  this->write_register(0x11, 0x88);
  this->write_register(0x12, 0x32);
  this->write_register(0x13, 0x20);
  // NRT: ~5ms for NFC-A
  this->write_register(ST25R300_REG_NRT_CONF2, 0x04);
  this->write_register(ST25R300_REG_NRT_CONF3, 0x23);
}

bool ST25R300::transceive_nfcv_(const uint8_t *data, size_t len, uint8_t *resp, uint8_t &resp_len,
                                 uint32_t timeout_ms) {
  this->write_command(ST25R300_CMD_CLEAR_FIFO);
  this->write_command(ST25R300_CMD_CLEAR_RX_GAIN);

  // Set TX frame length (full bytes, 0 partial bits)
  this->write_register(ST25R300_REG_TX_FRAME1, len >> 5);
  this->write_register(ST25R300_REG_TX_FRAME2, (len & 0x1F) << 3);

  // Load FIFO and transmit
  this->write_fifo(data, len);

  this->read_register(ST25R300_REG_IRQ_STATUS1);
  this->read_register(ST25R300_REG_IRQ_STATUS2);
  this->irq_triggered_ = false;
  this->write_command(ST25R300_CMD_TRANSMIT_DATA);

  // Wait for response
  uint32_t start = millis();
  while (millis() - start < timeout_ms) {
    if (this->irq_triggered_) {
      this->irq_triggered_ = false;
      uint8_t r1 = this->read_register(ST25R300_REG_IRQ_STATUS1);
      uint8_t r2 = this->read_register(ST25R300_REG_IRQ_STATUS2);
      ESP_LOGV(TAG, "NFC-V IRQ: r1=0x%02X r2=0x%02X", r1, r2);
      if (r1 & ST25R300_IRQ1_RXE) {
        // Read FIFO
        uint8_t fifo_len = this->read_register(ST25R300_REG_FIFO_STATUS1);
        if (fifo_len > 0 && fifo_len <= 64) {
          resp_len = fifo_len;
          this->read_fifo(resp, fifo_len);
          return true;
        }
        return false;
      }
      if (r2 & ST25R300_IRQ2_NRE) {
        return false;  // No response
      }
    }
    delay(1);
  }
  return false;
}

void ST25R300::nfcv_scan_() {
  // Switch to NFC-V mode
  this->configure_nfcv_mode_();

  ESP_LOGV(TAG, "NFC-V: starting inventory scan");

  // 1-slot INVENTORY: [flags=0x26, cmd=0x01, mask_len=0x00]
  uint8_t inv_req[] = {NFCV_INV_FLAG_1SLOT, NFCV_CMD_INVENTORY, 0x00};
  uint8_t resp[12];
  uint8_t resp_len = 0;

  // Single-tag inventory (multi-tag requires STAY_QUIET + field cycling)
  {
    resp_len = 0;
    if (!this->transceive_nfcv_(inv_req, sizeof(inv_req), resp, resp_len, 25)) {
      break;  // No more tags
    }

    // Response (CRC stripped by chip): flags(1) + DSFID(1) + UID(8) = 10 bytes
    if (resp_len < 10) {
      ESP_LOGW(TAG, "NFC-V: short inventory response (%u bytes)", resp_len);
      break;
    }

    // Check flags uint8_t — bit0=error
    if (resp[0] & 0x01) {
      ESP_LOGD(TAG, "NFC-V: inventory error flag set (0x%02X)", resp[0]);
      break;
    }

    // UID is bytes 2-9, transmitted LSB-first — reverse for display
    char uid_str[17];
    for (int j = 0; j < 8; j++) {
      snprintf(uid_str + j * 2, 3, "%02X", resp[9 - j]);  // reverse: resp[9]..resp[2]
    }
    uid_str[16] = '\0';
    ESP_LOGI(TAG, "NFC-V tag: %s (DSFID=0x%02X)", uid_str, resp[1]);

    // Add to tags_this_scan_ — finalize_scan_() handles on_tag_removed
    std::string uid_string(uid_str);
    this->tags_this_scan_.insert(uid_string);

    // Try to read NDEF (Type 5 tag) via READ_SINGLE_BLOCK
    std::vector<uint8_t> uid_bytes;
    uid_bytes.reserve(8);
    for (int j = 0; j < 8; j++)
      uid_bytes.push_back(resp[9 - j]);

    // Read block 0 (Capability Container)
    uint8_t blk_req[] = {0x02, 0x20, 0x00};  // flags, READ_SINGLE_BLOCK, block=0
    uint8_t blk_resp[8];
    uint8_t blk_len = 0;
    std::vector<uint8_t> ndef_data;

    if (this->transceive_nfcv_(blk_req, sizeof(blk_req), blk_resp, blk_len, 20) &&
        blk_len >= 5 && !(blk_resp[0] & 0x01)) {
      if (blk_resp[1] == 0xE1) {  // NDEF magic uint8_t in CC
        uint8_t cc_size = blk_resp[3];
        uint8_t total_blocks = (cc_size * 8) / 4;
        if (total_blocks > 64) total_blocks = 64;

        for (uint8_t blk = 1; blk <= total_blocks; blk++) {
          blk_req[2] = blk;
          blk_len = 0;
          if (this->transceive_nfcv_(blk_req, sizeof(blk_req), blk_resp, blk_len, 20) &&
              blk_len >= 5 && !(blk_resp[0] & 0x01)) {
            for (int k = 1; k < 5 && k < blk_len; k++)
              ndef_data.push_back(blk_resp[k]);
          } else {
            break;
          }
        }
      }
    }

    // Fire on_tag immediately for new tags
    if (this->present_tags_.find(uid_string) == this->present_tags_.end()) {
      this->present_tags_[uid_string] = 0;
      nfc::NfcTagUid nfc_uid(uid_bytes.begin(), uid_bytes.end());

      if (!ndef_data.empty()) {
        for (size_t i = 0; i < ndef_data.size(); i++) {
          if (ndef_data[i] == 0x03 && i + 1 < ndef_data.size()) {
            uint8_t ndef_len = ndef_data[i + 1];
            size_t ndef_start = i + 2;
            if (ndef_start + ndef_len <= ndef_data.size()) {
              ESP_LOGI(TAG, "NFC-V NDEF: %u bytes", ndef_len);
              auto tag = make_unique<nfc::NfcTag>(nfc_uid);
              this->tags_data_[uid_string] = std::move(tag);
            }
            break;
          }
          if (ndef_data[i] == 0xFE) break;
        }
      }

      if (this->tags_data_.find(uid_string) == this->tags_data_.end()) {
        auto tag = make_unique<nfc::NfcTag>(nfc_uid);
        this->tags_data_[uid_string] = std::move(tag);
      }

      for (auto *listener : this->tag_listeners_)
        listener->tag_on(*this->tags_data_[uid_string]);
      for (auto *trigger : this->on_tag_triggers_)
        trigger->trigger(uid_string);
    }

  }

  // Switch back to NFC-A mode for the main scan
  this->configure_nfca_mode_();
}

bool ST25R300::nfcv_ndef_write(nfc::NdefMessage *message) {
  this->configure_nfcv_mode_();

  std::vector<uint8_t> payload;
  if (message != nullptr) {
    std::vector<uint8_t> ndef_data = message->encode();
    payload.push_back(0x03);
    if (ndef_data.size() < 255) {
      payload.push_back((uint8_t)ndef_data.size());
    } else {
      payload.push_back(0xFF);
      payload.push_back((uint8_t)((ndef_data.size() >> 8) & 0xFF));
      payload.push_back((uint8_t)(ndef_data.size() & 0xFF));
    }
    payload.insert(payload.end(), ndef_data.begin(), ndef_data.end());
  }
  payload.push_back(0xFE);
  while (payload.size() % 4 != 0) payload.push_back(0);

  ESP_LOGD(TAG, "NFC-V NDEF write: %zu bytes in %zu blocks", payload.size(), payload.size() / 4);

  // Write CC to block 0
  uint8_t cc_size = (payload.size() + 4) / 8;
  if (cc_size == 0) cc_size = 1;
  uint8_t write_req[7] = {0x02, 0x21, 0x00, 0xE1, 0x40, cc_size, 0x00};
  uint8_t resp[4];
  uint8_t resp_len = 0;

  if (!this->transceive_nfcv_(write_req, sizeof(write_req), resp, resp_len, 25)) {
    ESP_LOGE(TAG, "NFC-V: failed to write CC (block 0)");
    this->configure_nfca_mode_();
    return false;
  }

  for (size_t i = 0; i < payload.size(); i += 4) {
    uint8_t blk = 1 + (i / 4);
    write_req[2] = blk;
    write_req[3] = payload[i];
    write_req[4] = payload[i + 1];
    write_req[5] = payload[i + 2];
    write_req[6] = payload[i + 3];
    resp_len = 0;

    bool success = false;
    for (uint8_t retry = 0; retry < 3; retry++) {
      if (this->transceive_nfcv_(write_req, sizeof(write_req), resp, resp_len, 25)) {
        success = true;
        break;
      }
      delay(10);
    }
    if (!success) {
      ESP_LOGE(TAG, "NFC-V: failed to write block %u", blk);
      this->configure_nfca_mode_();
      return false;
    }
  }

  ESP_LOGI(TAG, "NFC-V NDEF write successful!");
  this->configure_nfca_mode_();
  return true;
}

// ── NFC-B (ISO 14443B) support ───────────────────────────────────────────────

void ST25R300::configure_nfcb_mode_() {
  this->write_register(ST25R300_REG_PROTOCOL1, 0x02);      // om=2: NFC-B/ISO14443B
  this->write_register(ST25R300_REG_TX_PROTOCOL1, 0x20);   // tx_crc only (no parity for NFC-B)
  this->write_register(ST25R300_REG_RX_PROTOCOL1,
                       ST25R300_RX_PROT1_B_RX_SOF | ST25R300_RX_PROT1_B_RX_EOF |
                       ST25R300_RX_PROT1_RX_CRC);  // 0x34: SOF+EOF+CRC (no parity)
  // NFC-B uses AM modulation (10% ASK), not OOK
  this->write_register(ST25R300_REG_TX_MOD1, 0x70);  // am_mod=7 (12% ASK)
  // NRT: ~20ms for NFC-B response
  this->write_register(ST25R300_REG_NRT_CONF2, 0x10);
  this->write_register(ST25R300_REG_NRT_CONF3, 0x89);
}

void ST25R300::nfcb_scan_() {
  this->configure_nfcb_mode_();

  // SENSB_REQ (ALLB): cmd=0x05, AFI=0x00, PARAM=0x08 (1 slot + WUPB)
  uint8_t sensb_req[] = {0x05, 0x00, 0x08};
  uint8_t resp[16];
  uint8_t resp_len = 0;

  if (this->transceive_nfcv_(sensb_req, sizeof(sensb_req), resp, resp_len, 20) &&
      resp_len >= 12 && resp[0] == 0x50) {
    // ATQB: uint8_t 0 = 0x50, bytes 1-4 = PUPI
    char uid_str[9];
    snprintf(uid_str, sizeof(uid_str), "%02X%02X%02X%02X", resp[1], resp[2], resp[3], resp[4]);
    ESP_LOGI(TAG, "NFC-B tag: %s (ATQB len=%u)", uid_str, resp_len);

    std::string uid_string(uid_str);
    this->tags_this_scan_.insert(uid_string);

    if (this->present_tags_.find(uid_string) == this->present_tags_.end()) {
      this->present_tags_[uid_string] = 0;
      std::vector<uint8_t> uid_bytes = {resp[1], resp[2], resp[3], resp[4]};
      nfc::NfcTagUid nfc_uid(uid_bytes.begin(), uid_bytes.end());
      auto tag = make_unique<nfc::NfcTag>(nfc_uid);
      for (auto *listener : this->tag_listeners_)
        listener->tag_on(*tag);
      for (auto *trigger : this->on_tag_triggers_)
        trigger->trigger(uid_string);
    }
  }

  // Restore NFC-A mode
  this->configure_nfca_mode_();
}

}  // namespace st25r300
}  // namespace esphome
