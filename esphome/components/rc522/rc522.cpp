#include "rc522.h"
#include "esphome/core/log.h"

// Based on:
// - https://github.com/miguelbalboa/rfid

namespace esphome {
namespace rc522 {

static const uint8_t WAIT_I_RQ = 0x30;  // RxIRq and IdleIRq

static const char *const TAG = "rc522";

static const uint8_t RESET_COUNT = 5;

std::string format_buffer(uint8_t *b, uint8_t len) {
  char buf[32];
  int offset = 0;
  for (uint8_t i = 0; i < len; i++) {
    const char *format = "%02X";
    if (i + 1 < len)
      format = "%02X-";
    offset += sprintf(buf + offset, format, b[i]);
  }
  return std::string(buf);
}

std::string format_uid(std::vector<uint8_t> &uid) {
  char buf[32];
  int offset = 0;
  for (size_t i = 0; i < uid.size(); i++) {
    const char *format = "%02X";
    if (i + 1 < uid.size())
      format = "%02X-";
    offset += sprintf(buf + offset, format, uid[i]);
  }
  return std::string(buf);
}

void RC522::setup() {
  state_ = STATE_SETUP;
  // Pull device out of power down / reset state.

  // First set the resetPowerDownPin as digital input, to check the MFRC522 power down mode.
  if (reset_pin_ != nullptr) {
    reset_pin_->pin_mode(gpio::FLAG_INPUT);

    if (!reset_pin_->digital_read()) {  // The MFRC522 chip is in power down mode.
      ESP_LOGV(TAG, "Power down mode detected. Hard resetting...");
      reset_pin_->pin_mode(gpio::FLAG_OUTPUT);  // Now set the resetPowerDownPin as digital output.
      reset_pin_->digital_write(false);         // Make sure we have a clean LOW state.
      delayMicroseconds(2);             // 8.8.1 Reset timing requirements says about 100ns. Let us be generous: 2μsl
      reset_pin_->digital_write(true);  // Exit power down mode. This triggers a hard reset.
      // Section 8.8.2 in the datasheet says the oscillator start-up time is the start up time of the crystal + 37,74μs.
      // Let us be generous: 50ms.
      reset_timeout_ = millis();
      return;
    }
  }

  // Setup a soft reset
  reset_count_ = RESET_COUNT;
  reset_timeout_ = millis();
}

void RC522::initialize_() {
  // Per original code, wait 50 ms
  if (millis() - reset_timeout_ < 50)
    return;

  // Reset baud rates
  ESP_LOGV(TAG, "Initialize");

  pcd_write_register(TX_MODE_REG, 0x00);
  pcd_write_register(RX_MODE_REG, 0x00);
  // Reset ModWidthReg
  pcd_write_register(MOD_WIDTH_REG, 0x26);

  // When communicating with a PICC we need a timeout if something goes wrong.
  // f_timer = 13.56 MHz / (2*TPreScaler+1) where TPreScaler = [TPrescaler_Hi:TPrescaler_Lo].
  // TPrescaler_Hi are the four low bits in TModeReg. TPrescaler_Lo is TPrescalerReg.
  pcd_write_register(T_MODE_REG, 0x80);  // TAuto=1; timer starts automatically at the end of the transmission in all
                                         // communication modes at all speeds

  // TPreScaler = TModeReg[3..0]:TPrescalerReg, ie 0x0A9 = 169 => f_timer=40kHz, ie a timer period of 25μs.
  pcd_write_register(T_PRESCALER_REG, 0xA9);
  pcd_write_register(T_RELOAD_REG_H, 0x03);  // Reload timer with 0x3E8 = 1000, ie 25ms before timeout.
  pcd_write_register(T_RELOAD_REG_L, 0xE8);

  // Default 0x00. Force a 100 % ASK modulation independent of the ModGsPReg register setting
  pcd_write_register(TX_ASK_REG, 0x40);
  pcd_write_register(MODE_REG, 0x3D);  // Default 0x3F. Set the preset value for the CRC coprocessor for the CalcCRC
                                       // command to 0x6363 (ISO 14443-3 part 6.2.4)

  state_ = STATE_INIT;
}

void RC522::dump_config() {
  ESP_LOGCONFIG(TAG, "RC522:");
  switch (this->error_code_) {
    case NONE:
      break;
    case RESET_FAILED:
      ESP_LOGE(TAG, "Reset command failed!");
      break;
  }

  LOG_PIN("  RESET Pin: ", this->reset_pin_);

  LOG_UPDATE_INTERVAL(this);

  for (auto *child : this->binary_sensors_) {
    LOG_BINARY_SENSOR("  ", "Tag", child);
  }
}

void RC522::update() {
  if (state_ == STATE_INIT) {
    pcd_antenna_on_();
    pcd_clear_register_bit_mask_(COLL_REG, 0x80);  // ValuesAfterColl=1 => Bits received after collision are cleared.
    buffer_[0] = PICC_CMD_REQA;
    pcd_transceive_data_(1);
    state_ = STATE_PICC_REQUEST_A;
  } else {
    ESP_LOGW(TAG, "Communication takes longer than update interval: %d", state_);
  }
}

void RC522::loop() {
  // First check reset is needed
  if (reset_count_ > 0) {
    pcd_reset_();
    return;
  }
  if (state_ == STATE_SETUP) {
    initialize_();
    return;
  }

  StatusCode status = STATUS_ERROR;  // For lint passing. TODO: refactor this
  if (awaiting_comm_) {
    if (state_ == STATE_SELECT_SERIAL_DONE) {
      status = await_crc_();
    } else {
      status = await_transceive_();
    }

    if (status == STATUS_WAITING) {
      return;
    }
    awaiting_comm_ = false;
    ESP_LOGV(TAG, "finished communication status: %d, state: %d", status, state_);
  }

  switch (state_) {
    case STATE_PICC_REQUEST_A: {
      if (status == STATUS_TIMEOUT) {  // no tag present
        for (auto *obj : this->binary_sensors_)
          obj->on_scan_end();  // reset the binary sensors
        ESP_LOGV(TAG, "CMD_REQA -> TIMEOUT (no tag present) %d", status);
        state_ = STATE_DONE;
      } else if (status != STATUS_OK) {
        ESP_LOGW(TAG, "CMD_REQA -> Not OK %d", status);
        state_ = STATE_DONE;
      } else if (back_length_ != 2) {  // || *valid_bits_ != 0) {  // ATQA must be exactly 16 bits.
        ESP_LOGW(TAG, "CMD_REQA -> OK, but unexpected back_length_ of %d", back_length_);
        state_ = STATE_DONE;
      } else {
        state_ = STATE_READ_SERIAL;
      }
      if (state_ == STATE_DONE) {
        // Don't wait another loop cycle
        pcd_antenna_off_();
      }
      break;
    }
    case STATE_READ_SERIAL: {
      ESP_LOGV(TAG, "STATE_READ_SERIAL (%d)", status);
      switch (uid_idx_) {
        case 0:
          buffer_[0] = PICC_CMD_SEL_CL1;
          break;
        case 3:
          buffer_[0] = PICC_CMD_SEL_CL2;
          break;
        case 6:
          buffer_[0] = PICC_CMD_SEL_CL3;
          break;
        default:
          ESP_LOGE(TAG, "uid_idx_ invalid, uid_idx_ = %d", uid_idx_);
          state_ = STATE_DONE;
      }
      buffer_[1] = 32;
      pcd_transceive_data_(2);
      state_ = STATE_SELECT_SERIAL;
      break;
    }
    case STATE_SELECT_SERIAL: {
      buffer_[1] = 0x70;  // select
      // todo: set CRC
      buffer_[6] = buffer_[2] ^ buffer_[3] ^ buffer_[4] ^ buffer_[5];
      pcd_calculate_crc_(buffer_, 7);
      state_ = STATE_SELECT_SERIAL_DONE;
      break;
    }
    case STATE_SELECT_SERIAL_DONE: {
      send_len_ = 6;
      pcd_transceive_data_(9);
      state_ = STATE_READ_SERIAL_DONE;
      break;
    }
    case STATE_READ_SERIAL_DONE: {
      if (status != STATUS_OK || back_length_ != 3) {
        if (status == STATUS_TIMEOUT) {
          ESP_LOGV(TAG, "STATE_READ_SERIAL_DONE -> TIMEOUT (no tag present) %d", status);
        } else {
          ESP_LOGW(TAG, "Unexpected response. Read status is %d. Read bytes: %d (%s)", status, back_length_,
                   format_buffer(buffer_, 9).c_str());
        }

        state_ = STATE_DONE;
        uid_idx_ = 0;

        pcd_antenna_off_();
        return;
      }

      // copy the uid
      bool cascade = buffer_[2] == PICC_CMD_CT;  // todo: should be determined based on select response (buffer[6])
      for (uint8_t i = 2 + cascade; i < 6; i++)
        uid_buffer_[uid_idx_++] = buffer_[i];
      ESP_LOGVV(TAG, "copied uid to idx %d last byte is 0x%x, cascade is %d", uid_idx_, uid_buffer_[uid_idx_ - 1],
                cascade);

      if (cascade) {  // there is more bytes in the UID
        state_ = STATE_READ_SERIAL;
        return;
      }

      std::vector<uint8_t> rfid_uid(std::begin(uid_buffer_), std::begin(uid_buffer_) + uid_idx_);
      uid_idx_ = 0;
      // ESP_LOGD(TAG, "Processing '%s'", format_uid(rfid_uid).c_str());
      pcd_antenna_off_();
      state_ = STATE_INIT;  // scan again on next update
      bool report = true;

      for (auto *tag : this->binary_sensors_) {
        if (tag->process(rfid_uid)) {
          report = false;
        }
      }

      if (this->current_uid_ == rfid_uid) {
        return;
      }

      this->current_uid_ = rfid_uid;

      for (auto *trigger : this->triggers_ontag_)
        trigger->process(rfid_uid);

      if (report) {
        ESP_LOGD(TAG, "Found new tag '%s'", format_uid(rfid_uid).c_str());
      }
      break;
    }
    case STATE_DONE: {
      if (!this->current_uid_.empty()) {
        ESP_LOGV(TAG, "Tag '%s' removed", format_uid(this->current_uid_).c_str());
        for (auto *trigger : this->triggers_ontagremoved_)
          trigger->process(this->current_uid_);
      }
      this->current_uid_ = {};
      state_ = STATE_INIT;
      break;
    }
    default:
      break;
  }
}  // namespace rc522

/**
 * Performs a soft reset on the MFRC522 chip and waits for it to be ready again.
 */
void RC522::pcd_reset_() {
  // The datasheet does not mention how long the SoftRest command takes to complete.
  // But the MFRC522 might have been in soft power-down mode (triggered by bit 4 of CommandReg)
  // Section 8.8.2 in the datasheet says the oscillator start-up time is the start up time of the crystal + 37,74μs.
  // Let us be generous: 50ms.

  if (millis() - reset_timeout_ < 50)
    return;

  if (reset_count_ == RESET_COUNT) {
    ESP_LOGI(TAG, "Soft reset...");
    // Issue the SoftReset command.
    pcd_write_register(COMMAND_REG, PCD_SOFT_RESET);
  }

  // Expect the PowerDown bit in CommandReg to be cleared (max 3x50ms)
  if ((pcd_read_register(COMMAND_REG) & (1 << 4)) == 0) {
    reset_count_ = 0;
    ESP_LOGI(TAG, "Device online.");
    // Wait for initialize
    reset_timeout_ = millis();
    return;
  }

  if (--reset_count_ == 0) {
    ESP_LOGE(TAG, "Unable to reset RC522.");
    this->error_code_ = RESET_FAILED;
    mark_failed();
  }
}

/**
 * Turns the antenna on by enabling pins TX1 and TX2.
 * After a reset these pins are disabled.
 */
void RC522::pcd_antenna_on_() {
  uint8_t value = pcd_read_register(TX_CONTROL_REG);
  if ((value & 0x03) != 0x03) {
    pcd_write_register(TX_CONTROL_REG, value | 0x03);
  }
}

/**
 * Turns the antenna off by disabling pins TX1 and TX2.
 */
void RC522::pcd_antenna_off_() {
  uint8_t value = pcd_read_register(TX_CONTROL_REG);
  if ((value & 0x03) != 0x00) {
    pcd_write_register(TX_CONTROL_REG, value & ~0x03);
  }
}

/**
 * Sets the bits given in mask in register reg.
 */
void RC522::pcd_set_register_bit_mask_(PcdRegister reg,  ///< The register to update. One of the PCD_Register enums.
                                       uint8_t mask      ///< The bits to set.
) {
  uint8_t tmp = pcd_read_register(reg);
  pcd_write_register(reg, tmp | mask);  // set bit mask
}

/**
 * Clears the bits given in mask from register reg.
 */
void RC522::pcd_clear_register_bit_mask_(PcdRegister reg,  ///< The register to update. One of the PCD_Register enums.
                                         uint8_t mask      ///< The bits to clear.
) {
  uint8_t tmp = pcd_read_register(reg);
  pcd_write_register(reg, tmp & (~mask));  // clear bit mask
}

/**
 * Transfers data to the MFRC522 FIFO, executes a command, waits for completion and transfers data back from the FIFO.
 * CRC validation can only be done if backData and backLen are specified.
 *
 * @return STATUS_OK on success, STATUS_??? otherwise.
 */
void RC522::pcd_transceive_data_(uint8_t send_len) {
  ESP_LOGV(TAG, "PCD TRANSCEIVE: RX: %s", format_buffer(buffer_, send_len).c_str());
  delayMicroseconds(1000);  // we need 1 ms delay between antenna on and those communication commands
  send_len_ = send_len;
  // Prepare values for BitFramingReg
  // For REQA and WUPA we need the short frame format - transmit only 7 bits of the last (and only)
  // uint8_t. TxLastBits = BitFramingReg[2..0]
  uint8_t bit_framing = (buffer_[0] == PICC_CMD_REQA) ? 7 : 0;

  pcd_write_register(COMMAND_REG, PCD_IDLE);              // Stop any active command.
  pcd_write_register(COM_IRQ_REG, 0x7F);                  // Clear all seven interrupt request bits
  pcd_write_register(FIFO_LEVEL_REG, 0x80);               // FlushBuffer = 1, FIFO initialization
  pcd_write_register(FIFO_DATA_REG, send_len_, buffer_);  // Write sendData to the FIFO
  pcd_write_register(BIT_FRAMING_REG, bit_framing);       // Bit adjustments
  pcd_write_register(COMMAND_REG, PCD_TRANSCEIVE);        // Execute the command
  pcd_set_register_bit_mask_(BIT_FRAMING_REG, 0x80);      // StartSend=1, transmission of data starts
  awaiting_comm_ = true;
  awaiting_comm_time_ = millis();
}

RC522::StatusCode RC522::await_transceive_() {
  if (millis() - awaiting_comm_time_ < 2)  // wait at least 2 ms
    return STATUS_WAITING;
  uint8_t n = pcd_read_register(
      COM_IRQ_REG);  // ComIrqReg[7..0] bits are: Set1 TxIRq RxIRq IdleIRq HiAlertIRq LoAlertIRq ErrIRq TimerIRq
  if (n & 0x01) {    // Timer interrupt - nothing received in 25ms
    back_length_ = 0;
    error_counter_ = 0;  // reset the error counter
    return STATUS_TIMEOUT;
  }
  if (!(n & WAIT_I_RQ)) {  // None of the interrupts that signal success has been set.
                           // Wait for the command to complete.
    if (millis() - awaiting_comm_time_ < 40)
      return STATUS_WAITING;
    back_length_ = 0;
    ESP_LOGW(TAG, "Communication with the MFRC522 might be down, reset in %d",
             10 - error_counter_);  // todo: trigger reset?
    if (error_counter_++ >= 10) {
      setup();
      error_counter_ = 0;  // reset the error counter
    }

    return STATUS_TIMEOUT;
  }
  // Stop now if any errors except collisions were detected.
  uint8_t error_reg_value = pcd_read_register(
      ERROR_REG);  // ErrorReg[7..0] bits are: WrErr TempErr reserved BufferOvfl CollErr CRCErr ParityErr ProtocolErr
  if (error_reg_value & 0x13) {  // BufferOvfl ParityErr ProtocolErr
    return STATUS_ERROR;
  }
  error_counter_ = 0;  // reset the error counter

  n = pcd_read_register(FIFO_LEVEL_REG);  // Number of uint8_ts in the FIFO
  if (n > sizeof(buffer_))
    return STATUS_NO_ROOM;
  if (n > sizeof(buffer_) - send_len_)
    send_len_ = sizeof(buffer_) - n;                                    // simply overwrite the sent values
  back_length_ = n;                                                     // Number of uint8_ts returned
  pcd_read_register(FIFO_DATA_REG, n, buffer_ + send_len_, rx_align_);  // Get received data from FIFO
  uint8_t valid_bits_local =
      pcd_read_register(CONTROL_REG) & 0x07;  // RxLastBits[2:0] indicates the number of valid bits in the last
                                              // received uint8_t. If this value is 000b, the whole uint8_t is valid.

  // Tell about collisions
  if (error_reg_value & 0x08) {  // CollErr
    ESP_LOGW(TAG, "collision error, received %d bytes + %d bits (but anticollision not implemented)",
             back_length_ - (valid_bits_local > 0), valid_bits_local);
    return STATUS_COLLISION;
  }
  // Tell about collisions
  if (valid_bits_local) {
    ESP_LOGW(TAG, "only %d valid bits received, tag distance to high? Error code is 0x%x", valid_bits_local,
             error_reg_value);  // TODO: is this always due to collissions?
    return STATUS_ERROR;
  }
  ESP_LOGV(TAG, "received %d bytes: %s", back_length_, format_buffer(buffer_ + send_len_, back_length_).c_str());

  return STATUS_OK;
}

/**
 * Use the CRC coprocessor in the MFRC522 to calculate a CRC_A.
 *
 * @return STATUS_OK on success, STATUS_??? otherwise.
 */

void RC522::pcd_calculate_crc_(uint8_t *data,  ///< In: Pointer to the data to transfer to the FIFO for CRC calculation.
                               uint8_t length  ///< In: The number of uint8_ts to transfer.
) {
  ESP_LOGVV(TAG, "pcd_calculate_crc_(..., %d, ...)", length);
  pcd_write_register(COMMAND_REG, PCD_IDLE);        // Stop any active command.
  pcd_write_register(DIV_IRQ_REG, 0x04);            // Clear the CRCIRq interrupt request bit
  pcd_write_register(FIFO_LEVEL_REG, 0x80);         // FlushBuffer = 1, FIFO initialization
  pcd_write_register(FIFO_DATA_REG, length, data);  // Write data to the FIFO
  pcd_write_register(COMMAND_REG, PCD_CALC_CRC);    // Start the calculation

  awaiting_comm_ = true;
  awaiting_comm_time_ = millis();
}

RC522::StatusCode RC522::await_crc_() {
  if (millis() - awaiting_comm_time_ < 2)  // wait at least 2 ms
    return STATUS_WAITING;

  // DivIrqReg[7..0] bits are: Set2 reserved reserved MfinActIRq reserved CRCIRq reserved reserved
  uint8_t n = pcd_read_register(DIV_IRQ_REG);
  if (n & 0x04) {                               // CRCIRq bit set - calculation done
    pcd_write_register(COMMAND_REG, PCD_IDLE);  // Stop calculating CRC for new content in the FIFO.
    // Transfer the result from the registers to the result buffer
    buffer_[7] = pcd_read_register(CRC_RESULT_REG_L);
    buffer_[8] = pcd_read_register(CRC_RESULT_REG_H);

    ESP_LOGVV(TAG, "pcd_calculate_crc_() STATUS_OK");
    return STATUS_OK;
  }
  if (millis() - awaiting_comm_time_ < 89)
    return STATUS_WAITING;

  ESP_LOGD(TAG, "pcd_calculate_crc_() TIMEOUT");
  // 89ms passed and nothing happened. Communication with the MFRC522 might be down.
  return STATUS_TIMEOUT;
}

bool RC522BinarySensor::process(std::vector<uint8_t> &data) {
  bool result = true;
  if (data.size() != this->uid_.size()) {
    result = false;
  } else {
    for (size_t i = 0; i < data.size(); i++) {
      if (data[i] != this->uid_[i]) {
        result = false;
        break;
      }
    }
  }
  this->publish_state(result);
  this->found_ = result;
  return result;
}
void RC522Trigger::process(std::vector<uint8_t> &data) { this->trigger(format_uid(data)); }

}  // namespace rc522
}  // namespace esphome
