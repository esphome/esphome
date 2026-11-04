#include "st25r.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include <algorithm>
#include <iterator>

namespace esphome::st25r {

ESPHOME_LOG_TAG(TAG, "st25r");

// ISO14443A SEL command for each cascade level
static constexpr uint8_t SEL_CMDS[] = {0x93, 0x95, 0x97};

// RFAL NFC-A 106 kbps register profile, as {register, value} pairs in write order
static const uint8_t NFCA_PROFILE_RX[][2] PROGMEM = {
    {ST25R_REG_MODE, 0x08},      {ST25R_REG_BIT_RATE, 0x00},   {ST25R_REG_RX_CONF1, 0x08},
    {ST25R_REG_RX_CONF2, 0x2D},  {ST25R_REG_RX_CONF3, 0x00},   {ST25R_REG_RX_CONF4, 0x00},
    {ST25R_REG_MASK_MAIN, 0x00}, {ST25R_REG_MASK_TIMER, 0x00}, {ST25R_REG_ISO14443A_CONF, 0x00},
};
static const uint8_t NFCA_PROFILE_TX[][2] PROGMEM = {
    {ST25R_REG_ANT_TUNE_A, 0x80},
    {ST25R_REG_ANT_TUNE_B, 0x40},
    {ST25R_REG_AUX_MOD, 0x10},
    {ST25R_REG_RES_AM_MOD, 0x80},
    {ST25R_REG_FIELD_THRESHOLD_ACTV, 0x11},
    {ST25R_REG_FIELD_THRESHOLD_DEACTV, 0x00},
    {ST25R_REG_PASSIVE_TARGET, 0x50},
    {ST25R_REG_PT_MOD, 0x51},
    {ST25R_REG_EMD_SUP_CONF, 0x40},
    {ST25R_REG_OVERSHOOT_CONF1, 0x40},
    {ST25R_REG_OVERSHOOT_CONF2, 0x03},
    {ST25R_REG_UNDERSHOOT_CONF1, 0x40},
    {ST25R_REG_UNDERSHOOT_CONF2, 0x03},
};
static const uint8_t NFCA_PROFILE_CORR[][2] PROGMEM = {
    {ST25R_REG_RX_CONF4, 0x00},
    {ST25R_REG_CORR_CONF1, 0x51},
    {ST25R_REG_CORR_CONF2, 0x00},
};

void IRAM_ATTR ST25R::isr(ST25R *arg) {
  arg->irq_fired_ = true;
  arg->enable_loop_soon_any_context();
}

void ST25R::setup() {
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();
    this->pulse_reset_();
  }

  if (this->irq_pin_ != nullptr) {
    this->irq_pin_->setup();
    this->irq_pin_->attach_interrupt(ST25R::isr, this, gpio::INTERRUPT_RISING_EDGE);
  }

  if (!this->reset_chip_())
    this->mark_failed(LOG_STR("Failed to reset chip"));
}

void ST25R::update() {
  if (this->is_failed() || this->state_ != STATE_IDLE)
    return;

  this->enable_loop();
  // The field was off between scans, so every tag in range restarts in the IDLE state and
  // answers the WUPA; a tag left on the reader is found again without needing a HLTA first.
  this->write_register(ST25R_REG_OP_CONTROL, 0xC8);  // en=1, rx_en=1, tx_en=1
  this->tag_found_this_scan_ = false;
  this->state_ = STATE_FIELD_ON;
  this->last_state_change_ = App.get_loop_component_start_time();
}

void ST25R::loop() {
  if (this->state_ == STATE_IDLE) {
    this->disable_loop();
    return;
  }
  // With an IRQ pin the status is only fetched after the chip raised the line; the
  // state timeouts still run on every pass
  if (this->irq_pin_ == nullptr || this->irq_fired_) {
    this->irq_fired_ = false;
    this->irq_status_ = this->read_register(ST25R_REG_IRQ_MAIN);
    // The line only drops once every pending source is read, or the next edge is missed
    this->read_register(ST25R_REG_IRQ_TIMER);
    this->read_register(ST25R_REG_IRQ_ERROR);
  } else {
    this->irq_status_ = 0;
  }
  this->process_state_();
}

void ST25R::pulse_reset_() {
  this->reset_pin_->digital_write(true);
  delay(10);
  this->reset_pin_->digital_write(false);
  delay(10);
}

void ST25R::clear_irqs_() {
  this->write_command(ST25R_CMD_CLEAR_FIFO);
  this->read_register(ST25R_REG_IRQ_MAIN);
  this->read_register(ST25R_REG_IRQ_TIMER);
  this->read_register(ST25R_REG_IRQ_ERROR);
}

void ST25R::write_register_table_(const uint8_t (*table)[2], size_t count) {
  for (size_t i = 0; i < count; i++)
    this->write_register(progmem_read_byte(&table[i][0]), progmem_read_byte(&table[i][1]));
}

bool ST25R::transceive_ex_(const uint8_t *data, size_t len, uint8_t *resp, uint8_t resp_cap, uint8_t &resp_len,
                           uint32_t timeout_ms) {
  this->clear_irqs_();
  this->write_command(ST25R_CMD_RESET_RX_GAIN);

  // The byte count spans both registers: ntx[12:5] in the first, ntx[4:0] plus the bit count in the second
  this->write_register(ST25R_REG_NUM_TX_BYTES1, (len >> 5) & 0xFF);
  this->write_register(ST25R_REG_NUM_TX_BYTES2, (len & 0x1F) << 3);

  this->write_fifo(data, len);
  this->write_command(ST25R_CMD_TRANSMIT_WITH_CRC);

  uint32_t start = millis();
  resp_len = 0;
  bool tx_done = false;

  while (millis() - start < timeout_ms) {
    uint8_t irq = this->read_register(ST25R_REG_IRQ_MAIN);
    this->irq_status_ = irq;

    if (irq & IRQ_TXE)
      tx_done = true;

    if (tx_done) {
      uint8_t f1 = this->read_register(ST25R_REG_FIFO_STATUS1);
      if (f1 > 0) {
        uint8_t to_read = std::min<uint8_t>(resp_cap - resp_len, f1);
        this->read_fifo(resp + resp_len, to_read);
        resp_len += to_read;
        if (resp_len >= resp_cap)
          return true;
        start = millis();
      }
      if (irq & IRQ_RXE)
        return resp_len > 0;
    }
    delay(1);
  }
  return resp_len > 0;
}

void ST25R::process_state_() {
  const uint32_t now = App.get_loop_component_start_time();
  switch (this->state_) {
    case STATE_IDLE:
      break;

    case STATE_FIELD_ON: {
      // ISO 14443-3 guard time: a tag needs 5 ms of field before the first command
      if (now - this->last_state_change_ < FIELD_GUARD_MS)
        break;
      this->clear_irqs_();
      this->write_command(ST25R_CMD_RESET_RX_GAIN);
      this->write_command(ST25R_CMD_TRANSMIT_WUPA);
      this->state_ = STATE_WUPA;
      this->last_state_change_ = now;
      break;
    }

    case STATE_WUPA: {
      if (this->irq_status_ & (IRQ_RXE | IRQ_COL)) {
        this->cascade_level_ = 0;
        this->scan_uid_.clear();
        this->send_anticol_frame_();
        this->state_ = STATE_ANTICOL;
        this->last_state_change_ = now;
      } else if (now - this->last_state_change_ > 100) {
        this->finish_scan_();
      }
      break;
    }

    case STATE_ANTICOL: {
      if (!(this->irq_status_ & (IRQ_RXE | IRQ_COL | IRQ_TXE))) {
        if (now - this->last_state_change_ > 20)
          this->finish_scan_();
        break;
      }

      uint8_t f1 = this->read_register(ST25R_REG_FIFO_STATUS1);

      if (this->irq_status_ & IRQ_COL) {
        if (f1 > 0) {
          uint8_t tmp[8];
          this->read_fifo(tmp, std::min<uint8_t>(f1, sizeof(tmp)));
        }
        this->finish_scan_();
        break;
      }

      if (f1 < 5) {
        if (now - this->last_state_change_ > 20)
          this->finish_scan_();
        break;
      }

      uint8_t resp[5];
      this->read_fifo(resp, 5);
      uint8_t bcc = xor_bytes(resp, 4);
      if (bcc != resp[4]) {
        ESP_LOGV(TAG, "Anticollision BCC mismatch");
        this->finish_scan_();
        break;
      }
      uint8_t sel_pk[7] = {SEL_CMDS[this->cascade_level_], 0x70, resp[0], resp[1], resp[2], resp[3], bcc};

      // Skip the cascade tag (0x88)
      const uint8_t *uid_part = resp[0] == 0x88 ? resp + 1 : resp;
      const uint8_t *uid_end = resp + 4;
      for (; uid_part < uid_end && this->scan_uid_.size() < nfc::NFC_UID_MAX_LENGTH; uid_part++)
        this->scan_uid_.push_back(*uid_part);

      this->write_register(ST25R_REG_ISO14443A_CONF, 0x00);  // antcl=0

      uint8_t sak_buf[3];
      uint8_t sak_len = 0;
      if (!this->transceive_ex_(sel_pk, sizeof(sel_pk), sak_buf, sizeof(sak_buf), sak_len, 150)) {
        ESP_LOGW(TAG, "SELECT failed (no SAK)");
        this->finish_scan_();
        break;
      }
      uint8_t sak = sak_buf[0];

      if (sak & 0x04) {  // Cascade bit
        this->cascade_level_++;
        if (this->cascade_level_ >= std::size(SEL_CMDS)) {
          ESP_LOGE(TAG, "Too many cascade levels");
          this->finish_scan_();
          break;
        }
        this->send_anticol_frame_();
        this->state_ = STATE_ANTICOL;
        this->last_state_change_ = now;
        break;
      }

      size_t uid_len = this->scan_uid_.size();
      if (uid_len != 4 && uid_len != 7 && uid_len != 10) {
        ESP_LOGW(TAG, "Discarding invalid UID len=%u", static_cast<unsigned>(uid_len));
        this->finish_scan_();
        break;
      }

#ifdef ESPHOME_LOG_HAS_VERBOSE
      char uid_buf[nfc::FORMAT_UID_BUFFER_SIZE];
      ESP_LOGV(TAG, "Tag selected: %s (SAK=0x%02X)", nfc::format_uid_to(uid_buf, this->scan_uid_), sak);
#endif
      this->tag_found_this_scan_ = true;
      this->finish_scan_();
      break;
    }
  }
}

void ST25R::finish_scan_() {
  this->write_register(ST25R_REG_OP_CONTROL, 0x80);  // en=1: field off until the next scan
  this->state_ = STATE_IDLE;
  this->irq_status_ = 0;
  this->finalize_scan_();
  this->disable_loop();
}

void ST25R::finalize_scan_() {
  if (this->current_tag_ != nullptr) {
    const auto &uid = this->current_tag_->get_uid();
    if (this->tag_found_this_scan_ && uid.size() == this->scan_uid_.size() &&
        std::equal(uid.begin(), uid.end(), this->scan_uid_.begin())) {
      this->miss_count_ = 0;
      return;
    }
    if (!this->tag_found_this_scan_ && ++this->miss_count_ < MISS_THRESHOLD)
      return;
    this->remove_tag_();
  }

  if (!this->tag_found_this_scan_)
    return;

  this->current_tag_ = make_unique<nfc::NfcTag>(this->scan_uid_);
  this->miss_count_ = 0;
  char uid_buf[nfc::FORMAT_UID_BUFFER_SIZE];
  nfc::format_uid_to(uid_buf, this->scan_uid_);
  ESP_LOGI(TAG, "Tag detected: %s", uid_buf);
  if (!this->on_tag_callback_.empty())
    this->on_tag_callback_.call(std::string(uid_buf), *this->current_tag_);
#ifdef NFC_TAG_LISTENER_COUNT
  for (auto *listener : this->tag_listeners_)
    listener->tag_on(*this->current_tag_);
#endif
}

void ST25R::remove_tag_() {
  char uid_buf[nfc::FORMAT_UID_BUFFER_SIZE];
  nfc::format_uid_to(uid_buf, this->current_tag_->get_uid());
  ESP_LOGI(TAG, "Tag removed: %s", uid_buf);
  if (!this->on_tag_removed_callback_.empty())
    this->on_tag_removed_callback_.call(std::string(uid_buf), *this->current_tag_);
#ifdef NFC_TAG_LISTENER_COUNT
  for (auto *listener : this->tag_listeners_)
    listener->tag_off(*this->current_tag_);
#endif
  this->current_tag_.reset();
}

bool ST25R::reset_chip_() {
  uint8_t ic_identity = this->read_register(ST25R_REG_IC_IDENTITY);
  uint8_t chip_type = ic_identity & 0xF8;
  if (chip_type != 0x28 && chip_type != 0x30) {
    ESP_LOGE(TAG, "IC identity mismatch: expected 0x28/0x30, got 0x%02X", chip_type);
    return false;
  }

  this->write_command(ST25R_CMD_SET_DEFAULT);
  delay(10);
  const bool is_b_version = chip_type == 0x30;
  ESP_LOGI(TAG, "IC: 0x%02X (ST25R3916%s)", ic_identity, is_b_version ? LOG_STR_LITERAL("B") : LOG_STR_LITERAL(""));

  this->write_register(ST25R_REG_OP_CONTROL, 0x80);  // Enable oscillator
  delay(10);

  // Measure VDD for supply voltage auto-detection
  uint8_t reg_ctrl = this->read_register(ST25R_REG_REGULATOR_CONTROL);
  this->write_register(ST25R_REG_REGULATOR_CONTROL, reg_ctrl & ~0x07);
  this->write_command(ST25R_CMD_MEASURE_VDD);
  delay(5);
  uint8_t vdd_raw = this->read_register(ST25R_REG_AD_CONV_RESULT);
  bool sup3v;
  if (vdd_raw == 0) {
    sup3v = true;
  } else {
    uint16_t vdd_mv = (uint16_t) vdd_raw * 23U + (((uint16_t) vdd_raw * 4U + 5U) / 10U);
    sup3v = (vdd_mv < 3600);
    ESP_LOGI(TAG, "VDD: %u mV (sup3V=%s)", vdd_mv, sup3v ? LOG_STR_LITERAL("3.3V") : LOG_STR_LITERAL("5V"));
  }

  this->write_register(ST25R_REG_IO_CONF1, 0x07);
  // SPI pull-downs and the AAT module enabled
  this->write_register(ST25R_REG_IO_CONF2, (sup3v ? 0x80 : 0x00) | 0x18 | 0x20);
  this->write_register_table_(NFCA_PROFILE_RX, std::size(NFCA_PROFILE_RX));
  this->write_register(ST25R_REG_TX_DRIVER_CONF, (15 - this->rf_power_) & 0x0F);
  this->write_register_table_(NFCA_PROFILE_TX, std::size(NFCA_PROFILE_TX));
  uint8_t aux_val = this->read_register(ST25R_REG_AUX);
  this->write_register(ST25R_REG_AUX, aux_val & ~0x04);
  this->write_register_table_(NFCA_PROFILE_CORR, std::size(NFCA_PROFILE_CORR));

  this->field_on_();
  delay(10);
  return true;
}

void ST25R::send_anticol_frame_() {
  uint8_t frame[2] = {SEL_CMDS[this->cascade_level_], 0x20};

  this->write_register(ST25R_REG_ISO14443A_CONF, 0x01);  // antcl=1
  this->clear_irqs_();
  this->write_fifo(frame, sizeof(frame));
  this->write_register(ST25R_REG_NUM_TX_BYTES1, 0x00);
  this->write_register(ST25R_REG_NUM_TX_BYTES2, 0x10);
  this->write_command(ST25R_CMD_TRANSMIT_WITHOUT_CRC);
}

void ST25R::field_on_() {
  this->write_register(ST25R_REG_OP_CONTROL, 0x88);  // en=1, tx_en=1
  delay(10);
  this->write_command(ST25R_CMD_FIELD_ON);
  delay(10);
  this->write_register(ST25R_REG_OP_CONTROL, 0xC8);  // en=1, rx_en=1, tx_en=1
  this->write_command(ST25R_CMD_ADJUST_REGULATORS);
}

void ST25R::dump_config() {
  ESP_LOGCONFIG(TAG,
                "ST25R:\n"
                "  RF Power: %u",
                this->rf_power_);
  LOG_PIN("  IRQ Pin: ", this->irq_pin_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace esphome::st25r
