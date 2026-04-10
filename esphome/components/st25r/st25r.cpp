#include "st25r.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/components/nfc/nfc_tag.h"
#include <cinttypes>
#include <algorithm>
#include <cstring>

namespace esphome::st25r {

static const char *const TAG = "st25r";

void IRAM_ATTR ST25R::isr(ST25R *arg) { arg->irq_triggered_ = true; }

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

  this->irq_triggered_ = false;
  this->write_command(ST25R_CMD_TRANSMIT_WUPA);
  this->state_ = STATE_WUPA;
  this->last_state_change_ = millis();
}

bool ST25R::transceive_(const uint8_t *data, size_t len, uint8_t *resp, uint8_t &resp_len, uint32_t timeout_ms) {
  return this->transceive_ex(data, len, resp, resp_len, true, timeout_ms);
}

bool ST25R::transceive_no_crc_(const uint8_t *data, size_t len, uint8_t *resp, uint8_t &resp_len, uint32_t timeout_ms) {
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
  this->write_register(NUM_TX_BYTES2, with_crc ? ((len & 0x1F) << 3) : 0x00);

  this->write_fifo(data, len);

  this->irq_triggered_ = false;
  this->write_command(with_crc ? ST25R_CMD_TRANSMIT_WITH_CRC : ST25R_CMD_TRANSMIT_WITHOUT_CRC);

  uint32_t start = millis();
  resp_len = 0;
  bool tx_done = false;

  while (millis() - start < timeout_ms) {
    this->irq_triggered_ = false;
    uint8_t irq = this->read_register(IRQ_MAIN);
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
      if (irq & IRQ_RXE)
        return resp_len > 0;
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
  } else if (this->state_ != STATE_IDLE) {
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
        this->send_anticol_frame();
        this->state_ = STATE_ANTICOL;
        this->last_state_change_ = millis();
      } else if (this->irq_status_ & IRQ_NRE) {
        this->irq_status_ = 0;
        this->state_ = STATE_IDLE;
        this->finalize_scan_();
      } else if (millis() - this->last_state_change_ > 100) {
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
        this->state_ = STATE_IDLE;
        this->finalize_scan_();
        return;
      }

      if (!(this->irq_status_ & (IRQ_RXE | IRQ_COL | IRQ_TXE)))
        break;

      delay(5);
      uint8_t f1 = this->read_fifo_status1();

      if (this->irq_status_ & IRQ_COL) {
        // Collision: drain FIFO, scan will retry next update()
        if (f1 > 0) {
          uint8_t tmp[8];
          this->read_fifo(tmp, std::min(f1, (uint8_t) 8));
        }
        this->state_ = STATE_IDLE;
        this->finalize_scan_();
        return;
      }

      if (f1 < 5)
        break;

      uint8_t resp[5];
      this->read_fifo(resp, 5);
      uint8_t bcc = resp[0] ^ resp[1] ^ resp[2] ^ resp[3];

      uint8_t sel_cmds[] = {0x93, 0x95, 0x97};
      uint8_t sel_pk[7] = {sel_cmds[this->cascade_level_], 0x70, resp[0], resp[1], resp[2], resp[3], bcc};

      // Build UID string: skip cascade tag (0x88)
      if (resp[0] == 0x88) {
        for (int i = 1; i < 4; i++) {
          char buf[3];
          snprintf(buf, sizeof(buf), "%02X", resp[i]);
          this->current_uid_ += buf;
        }
      } else {
        for (int i = 0; i < 4; i++) {
          char buf[3];
          snprintf(buf, sizeof(buf), "%02X", resp[i]);
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

      if (sak & 0x04) {  // Cascade bit — need another level
        this->cascade_level_++;
        if (this->cascade_level_ > 2) {
          ESP_LOGE(TAG, "Too many cascade levels");
          this->state_ = STATE_IDLE;
          this->finalize_scan_();
          return;
        }
        this->send_anticol_frame();
        this->state_ = STATE_ANTICOL;
        this->last_state_change_ = millis();
      } else {
        // Tag fully selected
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
            uid_bytes.push_back((uint8_t) strtol(this->current_uid_.substr(i, 2).c_str(), nullptr, 16));
          this->tags_data_[this->current_uid_] = this->read_tag(uid_bytes);
        }

        this->tags_this_scan_.insert(this->current_uid_);
        this->send_halt();
        this->state_ = STATE_IDLE;
        this->finalize_scan_();
      }
      break;
    }

    case STATE_REINITIALIZING:
      this->reinitialize();
      this->state_ = STATE_IDLE;
      break;

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
      if (kv.second >= this->miss_threshold_)
        to_remove.push_back(kv.first);
    }
  }
  for (const auto &uid : to_remove) {
    ESP_LOGI(TAG, "Tag removed: %s", uid.c_str());
    this->on_tag_removed_callback_.call(uid);
    this->tags_data_.erase(uid);
    this->present_tags_.erase(uid);
  }

  // Fire on_tag for newly seen UIDs
  for (const auto &uid : this->tags_this_scan_) {
    if (!this->present_tags_.count(uid)) {
      this->present_tags_[uid] = 0;
      this->on_tag_callback_.call(uid);
      if (this->tags_data_.count(uid) && this->tags_data_[uid]) {
        for (auto *listener : this->tag_listeners_)
          listener->tag_on(*this->tags_data_[uid]);
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

  this->write_register(OP_CONTROL, 0x80);  // Enable oscillator
  delay(10);

  // Measure VDD for supply voltage auto-detection
  uint8_t reg_ctrl = this->read_register(REGULATOR_CONTROL);
  this->write_register(REGULATOR_CONTROL, (reg_ctrl & ~0x07) | 0x00);
  this->write_command(ST25R_CMD_MEASURE_VDD);
  delay(5);
  uint8_t vdd_raw = this->read_register(AD_CONV_RESULT);
  bool sup3v;
  if (vdd_raw == 0) {
    sup3v = true;
  } else {
    uint16_t vdd_mv = (uint16_t) vdd_raw * 23U + (((uint16_t) vdd_raw * 4U + 5U) / 10U);
    sup3v = (vdd_mv < 3600);
    ESP_LOGI(TAG, "VDD: %u mV (sup3V=%s)", vdd_mv, sup3v ? "3.3V" : "5V");
  }

  // RFAL NFC-A 106 kbps register profile
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

  this->write_register(AUX_MOD, 0x10);
  this->write_register(RES_AM_MOD, 0x80);
  this->write_register(FIELD_THRESHOLD_ACTV, 0x11);
  this->write_register(FIELD_THRESHOLD_DEACTV, 0x00);
  this->write_register(PASSIVE_TARGET, 0x50);
  this->write_register(PT_MOD, 0x51);
  this->write_register(EMD_SUP_CONF, 0x40);
  this->write_register(OVERSHOOT_CONF1, 0x40);
  this->write_register(OVERSHOOT_CONF2, 0x03);
  this->write_register(UNDERSHOOT_CONF1, 0x40);
  this->write_register(UNDERSHOOT_CONF2, 0x03);

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

void ST25R::send_anticol_frame() {
  uint8_t sel_cmds[] = {0x93, 0x95, 0x97};
  uint8_t frame[2] = {sel_cmds[this->cascade_level_], 0x20};  // NVB=0x20: no prefix

  this->write_register(ISO14443A_CONF, 0x01);  // antcl=1
  this->write_command(ST25R_CMD_CLEAR_FIFO);
  this->read_register(IRQ_MAIN);
  this->read_register(IRQ_TIMER);
  this->read_register(IRQ_ERROR);
  this->irq_triggered_ = false;
  this->write_fifo(frame, 2);
  this->write_register(NUM_TX_BYTES1, 0x00);
  this->write_register(NUM_TX_BYTES2, 0x10);  // 2 bytes, 0 bits
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

}  // namespace esphome::st25r
