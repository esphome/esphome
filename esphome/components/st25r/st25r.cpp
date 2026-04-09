#include "st25r.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/components/nfc/nfc_tag.h"
#include <cinttypes>
#include <algorithm>
#include <cstring>

namespace esphome {
namespace st25r {

static const char *const TAG = "st25r";

void ST25R::isr(ST25R *arg) { arg->irq_triggered_ = true; }

void ST25R::setup() {
  ESP_LOGI(TAG, "Setting up ST25R...");
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();
    this->reset_pin_->digital_write(true);
    delay(10);
    this->reset_pin_->digital_write(false);
    delay(10);
  }

  if (this->irq_pin_ != nullptr) {
    this->irq_pin_->setup();
    this->irq_pin_->attach_interrupt(ST25R::isr, this, gpio::INTERRUPT_RISING_EDGE);
  }

  if (!this->reset_chip()) {
    ESP_LOGE(TAG, "Failed to reset chip");
    this->mark_failed();
    return;
  }
  ESP_LOGI(TAG, "ST25R initialized successfully.");
}

void ST25R::update() {
  if (this->is_failed() || this->state_ != STATE_IDLE)
    return;

  this->read_register(IRQ_MAIN);
  this->read_register(IRQ_TIMER);
  this->read_register(IRQ_ERROR);
  this->write_command(ST25R_CMD_CLEAR_FIFO);
  this->write_command(ST25R_CMD_RESET_RX_GAIN);

  this->write_register(OP_CONTROL, 0xC8);  // en=1, rx_en=1, tx_en=1

  this->saved_anticol_valid_ = false;
  this->anticol_resume_ = false;

  this->irq_triggered_ = false;
  this->write_command(ST25R_CMD_TRANSMIT_WUPA);
  this->state_ = STATE_WUPA;
  this->last_state_change_ = millis();
}

bool ST25R::transceive_(const uint8_t *data, size_t len, uint8_t *resp, uint8_t &resp_len,
                        uint32_t timeout_ms) {
  return this->transceive_ex(data, len, resp, resp_len, true, timeout_ms);
}

bool ST25R::transceive_no_crc_(const uint8_t *data, size_t len, uint8_t *resp, uint8_t &resp_len,
                               uint32_t timeout_ms) {
  return this->transceive_ex(data, len, resp, resp_len, false, timeout_ms);
}

bool ST25R::transceive_ex(const uint8_t *data, size_t len, uint8_t *resp, uint8_t &resp_len, bool with_crc,
                          uint32_t timeout_ms) {
  this->write_command(ST25R_CMD_CLEAR_FIFO);
  this->write_command(ST25R_CMD_RESET_RX_GAIN);
  this->read_register(IRQ_MAIN);
  this->read_register(IRQ_TIMER);
  this->read_register(IRQ_ERROR);

  this->write_register(NUM_TX_BYTES1, (len >> 8) & 0xFF);
  if (with_crc) {
    this->write_register(NUM_TX_BYTES2, (len & 0x1F) << 3);
  } else {
    this->write_register(NUM_TX_BYTES2, 0x00);
  }

  this->write_fifo(data, len);

  this->irq_triggered_ = false;
  if (with_crc) {
    this->write_command(ST25R_CMD_TRANSMIT_WITH_CRC);
  } else {
    this->write_command(ST25R_CMD_TRANSMIT_WITHOUT_CRC);
  }

  uint32_t start = millis();
  resp_len = 0;
  bool tx_done = false;

  while (millis() - start < timeout_ms) {
    uint8_t irq;
    if (this->irq_triggered_) {
      this->irq_triggered_ = false;
      irq = this->read_register(IRQ_MAIN);
    } else {
      irq = this->read_register(IRQ_MAIN);
    }
    this->irq_status_ = irq;

    if (irq & IRQ_TXE)
      tx_done = true;

    if (tx_done) {
      uint8_t f1 = this->read_register(FIFO_STATUS1);
      if (f1 > 0) {
        uint8_t to_read = std::min((uint8_t) (64 - resp_len), f1);
        this->read_fifo(resp + resp_len, to_read);
        resp_len += to_read;
        start = millis();
      }
      if (irq & IRQ_RXE) {
        return resp_len > 0;
      }
    }
    delay(1);
  }
  return resp_len > 0;
}

std::unique_ptr<nfc::NfcTag> ST25R::read_tag(std::vector<uint8_t> &uid) {
  nfc::NfcTagUid nfc_uid(uid.begin(), uid.end());
  return make_unique<nfc::NfcTag>(nfc_uid);
}

void ST25R::loop() {
  if (this->is_failed())
    return;

  if (this->irq_triggered_) {
    this->irq_triggered_ = false;
    this->irq_status_ = this->read_register(IRQ_MAIN);
  } else if (this->state_ == STATE_WUPA || this->state_ == STATE_ANTICOL || this->state_ == STATE_SELECT) {
    this->irq_status_ = this->read_register(IRQ_MAIN);
  } else {
    this->irq_status_ = 0;
  }

  this->process_state();
}

void ST25R::process_state() {
  switch (this->state_) {
    case STATE_IDLE:
      break;

    case STATE_WUPA: {
      if (this->irq_status_ & (IRQ_RXE | IRQ_COL)) {
        this->cascade_level_ = 0;
        this->current_uid_ = "";
        if (!this->anticol_resume_) {
          this->anticol_prefix_full_ = 0;
          this->anticol_prefix_bits_ = 0;
          this->anticol_col_pos_ = 0;
          this->anticol_prefix_val_ = 0;
        }
        this->anticol_resume_ = false;

        this->send_anticol_frame();
        this->state_ = STATE_ANTICOL;
        this->last_state_change_ = millis();
      } else if (this->irq_status_ & IRQ_NRE) {
        this->irq_status_ = 0;
        this->state_ = STATE_IDLE;
        this->finalize_scan_();
      } else if (millis() - this->last_state_change_ > 100) {
        // Read all IRQ registers so pin goes LOW for next ISR edge
        this->read_register(IRQ_TIMER);
        this->read_register(IRQ_ERROR);
        this->irq_status_ = 0;
        this->irq_triggered_ = false;
        this->state_ = STATE_IDLE;
        this->finalize_scan_();
      }
      break;
    }

    case STATE_ANTICOL: {
      if (millis() - this->last_state_change_ > 20) {
        uint8_t max_prefix_val = (1 << (this->anticol_col_pos_ + 1)) - 1;
        if (this->anticol_col_pos_ > 0 && this->anticol_prefix_val_ < max_prefix_val) {
          this->anticol_prefix_val_++;
          this->apply_anticol_prefix_();
          this->anticol_resume_ = true;
          this->start_wupa();
          this->state_ = STATE_WUPA;
          this->last_state_change_ = millis();
          return;
        }
        this->state_ = STATE_IDLE;
        this->finalize_scan_();
        return;
      }

      if (this->irq_status_ != 0 && (this->irq_status_ & (IRQ_RXE | IRQ_COL | IRQ_TXE))) {
        delay(5);
        uint8_t f1 = this->read_fifo_status1();
        bool has_collision = (this->irq_status_ & IRQ_COL) != 0;

        if (has_collision) {
          uint8_t col_raw = this->read_collision_display();
          uint8_t c_byte = (col_raw >> 4) & 0x0F;
          uint8_t c_bit = (col_raw >> 1) & 0x07;
          int uid_col_pos = (int) (c_byte * 8 + c_bit) - 16;
          if (uid_col_pos < 0)
            uid_col_pos = 0;
          if (f1 > 0) {
            uint8_t tmp[8];
            this->read_fifo(tmp, std::min(f1, (uint8_t) 8));
          }

          this->anticol_col_pos_ = uid_col_pos;
          this->anticol_prefix_val_ = 0;
          this->apply_anticol_prefix_();

          this->send_anticol_frame();
          this->last_state_change_ = millis();

        } else if (f1 >= 5) {
          uint8_t resp[5];
          this->read_fifo(resp, 5);

          // Reconstruct full UID by OR-ing prefix bits back in
          uint8_t full_uid[4];
          memcpy(full_uid, resp, 4);
          for (int k = 0; k < (int) this->anticol_prefix_full_; k++) {
            full_uid[k] = this->anticol_prefix_[k];
          }
          if (this->anticol_prefix_bits_ > 0) {
            uint8_t mask = (uint8_t) ((1 << this->anticol_prefix_bits_) - 1);
            full_uid[this->anticol_prefix_full_] =
                (this->anticol_prefix_[this->anticol_prefix_full_] & mask) |
                (resp[this->anticol_prefix_full_] & (uint8_t) (~mask));
          }
          uint8_t bcc = full_uid[0] ^ full_uid[1] ^ full_uid[2] ^ full_uid[3];

          uint8_t sel_cmds[] = {0x93, 0x95, 0x97};
          uint8_t sel_pk[7] = {sel_cmds[this->cascade_level_], 0x70,       full_uid[0], full_uid[1],
                               full_uid[2],                    full_uid[3], bcc};

          if (full_uid[0] == 0x88) {
            for (int i = 1; i < 4; i++) {
              char buf[3];
              snprintf(buf, sizeof(buf), "%02X", full_uid[i]);
              this->current_uid_ += buf;
            }
          } else {
            for (unsigned char i : full_uid) {
              char buf[3];
              snprintf(buf, sizeof(buf), "%02X", i);
              this->current_uid_ += buf;
            }
          }

          this->pre_select();

          uint8_t sak_buf[3];
          uint8_t sak_len = 0;
          if (!this->transceive_(sel_pk, 7, sak_buf, sak_len) || sak_len == 0) {
            ESP_LOGW(TAG, "SELECT failed (no SAK)");
            this->state_ = STATE_IDLE;
            this->finalize_scan_();
            return;
          }
          uint8_t sak = sak_buf[0];
          this->last_sak_ = sak;

          if (sak & 0x04) {  // Cascade bit
            if (this->cascade_level_ == 0) {
              this->saved_col_pos_ = this->anticol_col_pos_;
              this->saved_prefix_val_ = this->anticol_prefix_val_;
              this->saved_anticol_valid_ =
                  (this->anticol_col_pos_ > 0 || this->anticol_prefix_bits_ > 0);
            }
            this->cascade_level_++;
            if (this->cascade_level_ > 2) {
              ESP_LOGE(TAG, "Too many cascade levels");
              this->state_ = STATE_IDLE;
              this->finalize_scan_();
              return;
            }
            this->anticol_prefix_full_ = 0;
            this->anticol_prefix_bits_ = 0;
            this->anticol_col_pos_ = 0;
            this->anticol_prefix_val_ = 0;
            this->send_anticol_frame();
            this->state_ = STATE_ANTICOL;
            this->last_state_change_ = millis();
          } else {
            // Tag fully selected - validate UID length
            size_t uid_bytes_len = this->current_uid_.length() / 2;
            if (uid_bytes_len != 4 && uid_bytes_len != 7 && uid_bytes_len != 10) {
              ESP_LOGW(TAG, "Discarding invalid UID len=%zu", uid_bytes_len);
              this->state_ = STATE_IDLE;
              this->finalize_scan_();
              return;
            }

            ESP_LOGI(TAG, "Tag selected: %s (SAK=0x%02X)", this->current_uid_.c_str(), sak);

            if (!this->present_tags_.count(this->current_uid_)) {
              std::vector<uint8_t> uid_bytes;
              for (size_t i = 0; i < this->current_uid_.length(); i += 2)
                uid_bytes.push_back(
                    (uint8_t) strtol(this->current_uid_.substr(i, 2).c_str(), nullptr, 16));
              this->tags_data_[this->current_uid_] = this->read_tag(uid_bytes);
            }

            this->tags_this_scan_.insert(this->current_uid_);
            this->send_halt();

            // Resume multi-tag tree traversal
            uint8_t resume_col_pos;
            uint8_t resume_prefix_val;
            bool can_resume;
            if (this->saved_anticol_valid_) {
              resume_col_pos = this->saved_col_pos_;
              resume_prefix_val = this->saved_prefix_val_;
              can_resume = true;
              this->saved_anticol_valid_ = false;
            } else {
              resume_col_pos = this->anticol_col_pos_;
              resume_prefix_val = this->anticol_prefix_val_;
              can_resume = (this->anticol_col_pos_ > 0 || this->anticol_prefix_bits_ > 0);
            }

            if (can_resume) {
              this->cascade_level_ = 0;
              this->current_uid_ = "";
              this->anticol_col_pos_ = resume_col_pos;
              this->anticol_prefix_val_ = resume_prefix_val + 1;
              this->apply_anticol_prefix_();

              uint8_t max_val = (1 << (resume_col_pos + 1)) - 1;
              if (this->anticol_prefix_val_ > max_val) {
                this->state_ = STATE_IDLE;
                this->finalize_scan_();
                return;
              }
              this->anticol_resume_ = true;
              this->start_wupa();
            } else {
              this->state_ = STATE_IDLE;
              this->finalize_scan_();
              return;
            }
            this->state_ = STATE_WUPA;
            this->last_state_change_ = millis();
          }
        }
      }
      break;
    }

    case STATE_REINITIALIZING:
      this->reinitialize();
      this->state_ = STATE_IDLE;
      break;

    case STATE_SELECT:
    default:
      break;
  }
}

void ST25R::finalize_scan_() {
  // Increment miss counters; fire on_tag_removed when threshold reached
  std::vector<std::string> to_remove;
  for (auto &kv : this->present_tags_) {
    if (this->tags_this_scan_.count(kv.first)) {
      kv.second = 0;
    } else {
      kv.second++;
      if (kv.second >= this->miss_threshold_) {
        to_remove.push_back(kv.first);
      }
    }
  }
  for (const auto &uid : to_remove) {
    ESP_LOGI(TAG, "Tag removed: %s", uid.c_str());

    std::vector<uint8_t> uid_bytes;
    for (size_t i = 0; i < uid.length(); i += 2) {
      uid_bytes.push_back((uint8_t) strtol(uid.substr(i, 2).c_str(), nullptr, 16));
    }
    nfc::NfcTagUid nfc_uid(uid_bytes.begin(), uid_bytes.end());
    nfc::NfcTag nfc_tag(nfc_uid);
    for (auto *listener : this->tag_listeners_) {
      listener->tag_off(nfc_tag);
    }
    for (auto *trigger : this->on_tag_removed_triggers_) {
      trigger->trigger(uid);
    }
    this->tags_data_.erase(uid);
    this->present_tags_.erase(uid);
  }

  // Fire on_tag for newly seen UIDs
  for (const auto &uid : this->tags_this_scan_) {
    if (!this->present_tags_.count(uid)) {
      this->present_tags_[uid] = 0;
      for (auto *trigger : this->on_tag_triggers_) {
        trigger->trigger(uid);
      }
      if (this->tags_data_.count(uid) && this->tags_data_[uid]) {
        for (auto *listener : this->tag_listeners_) {
          listener->tag_on(*this->tags_data_[uid]);
        }
      }
    }
  }

  this->tags_this_scan_.clear();
}

bool ST25R::wait_for_irq_(uint8_t mask, uint32_t timeout_ms) {
  uint32_t start = millis();
  while (millis() - start < timeout_ms) {
    if (this->irq_triggered_)
      return true;
    delay(1);
  }
  return false;
}

bool ST25R::reset_chip() {
  // Verify IC identity before SET_DEFAULT
  uint8_t ic_identity = this->read_register(IC_IDENTITY);
  uint8_t chip_type = ic_identity & 0xF8;
  if (chip_type != 0x28 && chip_type != 0x30) {
    ESP_LOGE(TAG, "IC identity mismatch: expected 0x28/0x30, got 0x%02X", chip_type);
    return false;
  }

  this->write_command(ST25R_CMD_SET_DEFAULT);
  delay(10);
  this->is_b_version_ = (chip_type == 0x30);
  this->has_aat_ = true;
  ESP_LOGI(TAG, "IC: 0x%02X (ST25R3916%s)", ic_identity, this->is_b_version_ ? "B" : "");

  // Enable oscillator
  this->write_register(OP_CONTROL, 0x80);
  delay(10);

  // Measure VDD for supply voltage auto-detection
  uint8_t reg_ctrl = this->read_register(REGULATOR_CONTROL);
  this->write_register(REGULATOR_CONTROL, (reg_ctrl & ~0x07) | 0x00);
  this->write_command(ST25R_CMD_MEASURE_VDD);
  delay(5);
  uint8_t vdd_raw = this->read_register(AD_CONV_RESULT);
  bool sup3v;
  if (vdd_raw == 0) {
    sup3v = true;  // Default to 3.3V if measurement fails
  } else {
    uint16_t vdd_mv = (uint16_t) vdd_raw * 23U + (((uint16_t) vdd_raw * 4U + 5U) / 10U);
    sup3v = (vdd_mv < 3600);
    ESP_LOGI(TAG, "VDD: %u mV (sup3V=%s)", vdd_mv, sup3v ? "3.3V" : "5V");
  }

  // Configure registers (RFAL NFC-A 106 kbps profile)
  this->write_register(IO_CONF1, 0x07);
  uint8_t io_conf2 = sup3v ? 0x80 : 0x00;
  io_conf2 |= 0x18;  // SPI pull-downs
  if (this->has_aat_)
    io_conf2 |= 0x20;  // Enable AAT module
  this->write_register(IO_CONF2, io_conf2);
  this->write_register(MODE, 0x08);
  this->write_register(BIT_RATE, 0x00);
  this->write_register(RX_CONF1, 0x08);
  this->write_register(RX_CONF2, 0x2D);
  this->write_register(RX_CONF3, 0x00);
  this->write_register(RX_CONF4, 0x00);
  this->write_register(MASK_MAIN, 0x00);
  this->write_register(MASK_TIMER, 0x00);
  this->write_register(ISO14443A_CONF, 0x00);

  uint8_t d_res = (15 - this->rf_power_) & 0x0F;
  this->write_register(TX_DRIVER_CONF, d_res);

  if (this->has_aat_) {
    this->write_register(ANT_TUNE_A, 0x80);
    this->write_register(ANT_TUNE_B, 0x40);
  }

  // RFAL chip-init registers
  this->write_register(AUX_MOD, 0x10);
  this->write_register(RES_AM_MOD, 0x80);
  this->write_register(FIELD_THRESHOLD_ACTV, 0x11);
  this->write_register(FIELD_THRESHOLD_DEACTV, 0x00);
  this->write_register(PASSIVE_TARGET, 0x50);
  this->write_register(PT_MOD, 0x51);
  this->write_register(EMD_SUP_CONF, 0x40);

  // Overshoot/undershoot protection
  this->write_register(OVERSHOOT_CONF1, 0x40);
  this->write_register(OVERSHOOT_CONF2, 0x03);
  this->write_register(UNDERSHOOT_CONF1, 0x40);
  this->write_register(UNDERSHOOT_CONF2, 0x03);

  // Correlator receiver
  uint8_t aux_val = this->read_register(AUX);
  this->write_register(AUX, aux_val & ~0x04);
  this->write_register(RX_CONF4, 0x00);
  this->write_register(CORR_CONF1, 0x51);
  this->write_register(CORR_CONF2, 0x00);

  this->field_on_();
  delay(10);

  return true;
}

void ST25R::reinitialize() {
  ESP_LOGW(TAG, "Reinitializing ST25R...");
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->digital_write(true);
    delay(10);
    this->reset_pin_->digital_write(false);
    delay(10);
  }
  if (!this->reset_chip()) {
    ESP_LOGE(TAG, "Reinitialize failed");
    this->mark_failed();
  }
}

void ST25R::apply_anticol_prefix_() {
  int total_bits = this->anticol_col_pos_ + 1;
  this->anticol_prefix_full_ = total_bits >> 3;
  this->anticol_prefix_bits_ = total_bits & 7;
  memset(this->anticol_prefix_, 0, sizeof(this->anticol_prefix_));
  for (int i = 0; i < total_bits; i++) {
    int byte_idx = i >> 3;
    int bit_idx = i & 7;
    if ((this->anticol_prefix_val_ >> i) & 1)
      this->anticol_prefix_[byte_idx] |= (1 << bit_idx);
  }
}

void ST25R::send_anticol_frame() {
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

  this->write_register(ISO14443A_CONF, 0x01);  // antcl=1
  this->write_command(ST25R_CMD_CLEAR_FIFO);
  this->read_register(IRQ_MAIN);
  this->read_register(IRQ_TIMER);
  this->read_register(IRQ_ERROR);
  this->irq_triggered_ = false;
  this->write_fifo(frame, frame_len);
  this->write_register(NUM_TX_BYTES1, ntx_n >> 5);
  this->write_register(NUM_TX_BYTES2, ((ntx_n & 0x1F) << 3) | (ntx_b & 0x07));
  this->write_command(ST25R_CMD_TRANSMIT_WITHOUT_CRC);
}

void ST25R::pre_select() { this->write_register(ISO14443A_CONF, 0x00); }

uint8_t ST25R::read_fifo_status1() { return this->read_register(FIFO_STATUS1); }

uint8_t ST25R::read_collision_display() { return this->read_register(COLLISION_DISPLAY); }

void ST25R::send_halt() {
  uint8_t halt_cmd[2] = {0x50, 0x00};
  this->write_command(ST25R_CMD_CLEAR_FIFO);
  this->read_register(IRQ_MAIN);
  this->read_register(IRQ_TIMER);
  this->read_register(IRQ_ERROR);
  this->write_fifo(halt_cmd, 2);
  this->write_register(NUM_TX_BYTES1, 0x00);
  this->write_register(NUM_TX_BYTES2, 0x10);
  this->write_command(ST25R_CMD_TRANSMIT_WITH_CRC);
  delay(10);
}

void ST25R::start_wupa() {
  this->write_command(ST25R_CMD_CLEAR_FIFO);
  this->read_register(IRQ_MAIN);
  this->read_register(IRQ_TIMER);
  this->read_register(IRQ_ERROR);
  this->irq_triggered_ = false;
  this->irq_status_ = 0;
  this->write_command(ST25R_CMD_TRANSMIT_WUPA);
}

void ST25R::field_on_() {
  this->write_register(OP_CONTROL, 0x88);  // en=1, tx_en=1
  delay(10);
  this->write_command(ST25R_CMD_FIELD_ON);
  delay(10);
  this->write_register(OP_CONTROL, 0xC8);  // en=1, rx_en=1, tx_en=1
  this->write_command(ST25R_CMD_ADJUST_REGULATORS);
}

void ST25R::dump_config() {
  ESP_LOGCONFIG(TAG, "ST25R:");
  LOG_PIN("  IRQ Pin: ", this->irq_pin_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  ESP_LOGCONFIG(TAG, "  RF Power: %u", this->rf_power_);
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace st25r
}  // namespace esphome
