#include "rc522_spi.h"
#include "esphome/core/log.h"

// Based on:
// - https://github.com/miguelbalboa/rfid

namespace esphome {
namespace rc522_spi {

static const char *TAG = "rc522_spi";

static const uint8_t RESET_COUNT = 5;

void format_uid(char *buf, const uint8_t *uid, uint8_t uid_length) {
  int offset = 0;
  for (uint8_t i = 0; i < uid_length; i++) {
    const char *format = "%02X";
    if (i + 1 < uid_length)
      format = "%02X-";
    offset += sprintf(buf + offset, format, uid[i]);
  }
}

void RC522::setup() {
  spi_setup();
  initialize_pending_ = true;
  // Pull device out of power down / reset state.

  // First set the resetPowerDownPin as digital input, to check the MFRC522 power down mode.
  if (reset_pin_ != nullptr) {
    reset_pin_->pin_mode(INPUT);

    if (reset_pin_->digital_read() == LOW) {  // The MFRC522 chip is in power down mode.
      ESP_LOGV(TAG, "Power down mode detected. Hard resetting...");
      reset_pin_->pin_mode(OUTPUT);     // Now set the resetPowerDownPin as digital output.
      reset_pin_->digital_write(LOW);   // Make sure we have a clean LOW state.
      delayMicroseconds(2);             // 8.8.1 Reset timing requirements says about 100ns. Let us be generous: 2μsl
      reset_pin_->digital_write(HIGH);  // Exit power down mode. This triggers a hard reset.
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
  // Per originall code, wait 50 ms
  if (millis() - reset_timeout_ < 50)
    return;

  // Reset baud rates
  ESP_LOGV(TAG, "Initialize");

  pcd_write_register_(TX_MODE_REG, 0x00);
  pcd_write_register_(RX_MODE_REG, 0x00);
  // Reset ModWidthReg
  pcd_write_register_(MOD_WIDTH_REG, 0x26);

  // When communicating with a PICC we need a timeout if something goes wrong.
  // f_timer = 13.56 MHz / (2*TPreScaler+1) where TPreScaler = [TPrescaler_Hi:TPrescaler_Lo].
  // TPrescaler_Hi are the four low bits in TModeReg. TPrescaler_Lo is TPrescalerReg.
  pcd_write_register_(T_MODE_REG, 0x80);  // TAuto=1; timer starts automatically at the end of the transmission in all
                                          // communication modes at all speeds

  // TPreScaler = TModeReg[3..0]:TPrescalerReg, ie 0x0A9 = 169 => f_timer=40kHz, ie a timer period of 25μs.
  pcd_write_register_(T_PRESCALER_REG, 0xA9);
  pcd_write_register_(T_RELOAD_REG_H, 0x03);  // Reload timer with 0x3E8 = 1000, ie 25ms before timeout.
  pcd_write_register_(T_RELOAD_REG_L, 0xE8);

  // Default 0x00. Force a 100 % ASK modulation independent of the ModGsPReg register setting
  pcd_write_register_(TX_ASK_REG, 0x40);
  pcd_write_register_(MODE_REG, 0x3D);  // Default 0x3F. Set the preset value for the CRC coprocessor for the CalcCRC
                                        // command to 0x6363 (ISO 14443-3 part 6.2.4)
  pcd_antenna_on_();                    // Enable the antenna driver pins TX1 and TX2 (they were disabled by the reset)

  initialize_pending_ = false;
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

  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_PIN("  RESET Pin: ", this->reset_pin_);

  LOG_UPDATE_INTERVAL(this);

  for (auto *child : this->binary_sensors_) {
    LOG_BINARY_SENSOR("  ", "Tag", child);
  }
}

void RC522::loop() {
  // First check reset is needed
  if (reset_count_ > 0) {
    pcd_reset_();
    return;
  }
  if (initialize_pending_) {
    initialize_();
    return;
  }

  if (millis() - update_wait_ < this->update_interval_)
    return;

  auto status = picc_is_new_card_present_();

  if (status == STATUS_ERROR)  // No card
  {
    ESP_LOGE(TAG, "Error");
    // mark_failed();
    return;
  }

  if (status != STATUS_OK)  // We can receive STATUS_TIMEOUT when no card, or unexpected status.
    return;

  // Try process card
  if (!picc_read_card_serial_()) {
    ESP_LOGW(TAG, "Requesting tag read failed!");
    return;
  };

  if (uid_.size < 4) {
    return;
    ESP_LOGW(TAG, "Read serial size: %d", uid_.size);
  }

  update_wait_ = millis();

  bool report = true;
  // 1. Go through all triggers
  for (auto *trigger : this->triggers_)
    trigger->process(uid_.uiduint8_t, uid_.size);

  // 2. Find a binary sensor
  for (auto *tag : this->binary_sensors_) {
    if (tag->process(uid_.uiduint8_t, uid_.size)) {
      // 2.1 if found, do not dump
      report = false;
    }
  }

  if (report) {
    char buf[32];
    format_uid(buf, uid_.uiduint8_t, uid_.size);
    ESP_LOGD(TAG, "Found new tag '%s'", buf);
  }
}

void RC522::update() {
  for (auto *obj : this->binary_sensors_)
    obj->on_scan_end();
}

/**
 * Performs a soft reset on the MFRC522 chip and waits for it to be ready again.
 */
void RC522::pcd_reset_() {
  // The datasheet does not mention how long the SoftRest command takes to complete.
  // But the MFRC522 might have been in soft power-down mode (triggered by bit 4 of CommandReg)
  // Section 8.8.2 in the datasheet says the oscillator start-up time is the start up time of the crystal + 37,74μs. Let
  // us be generous: 50ms.

  if (millis() - reset_timeout_ < 50)
    return;

  if (reset_count_ == RESET_COUNT) {
    ESP_LOGV(TAG, "Soft reset...");
    // Issue the SoftReset command.
    pcd_write_register_(COMMAND_REG, PCD_SOFT_RESET);
  }

  // Expect the PowerDown bit in CommandReg to be cleared (max 3x50ms)
  if ((pcd_read_register_(COMMAND_REG) & (1 << 4)) == 0) {
    reset_count_ = 0;
    ESP_LOGI(TAG, "Device online.");
    // Wait for initialize
    reset_timeout_ = millis();
    return;
  }

  if (--reset_count_ == 0) {
    ESP_LOGE(TAG, "Unable to reset RC522.");
    mark_failed();
  }
}

/**
 * Turns the antenna on by enabling pins TX1 and TX2.
 * After a reset these pins are disabled.
 */
void RC522::pcd_antenna_on_() {
  uint8_t value = pcd_read_register_(TX_CONTROL_REG);
  if ((value & 0x03) != 0x03) {
    pcd_write_register_(TX_CONTROL_REG, value | 0x03);
  }
}

/**
 * Reads a uint8_t from the specified register in the MFRC522 chip.
 * The interface is described in the datasheet section 8.1.2.
 */
uint8_t RC522::pcd_read_register_(PcdRegister reg  ///< The register to read from. One of the PCD_Register enums.
) {
  uint8_t value;
  enable();
  transfer_byte(0x80 | reg);
  value = read_byte();
  disable();
  ESP_LOGVV(TAG, "read_register_(%x) -> %x", reg, value);
  return value;
}

/**
 * Reads a number of uint8_ts from the specified register in the MFRC522 chip.
 * The interface is described in the datasheet section 8.1.2.
 */
void RC522::pcd_read_register_(PcdRegister reg,  ///< The register to read from. One of the PCD_Register enums.
                               uint8_t count,    ///< The number of uint8_ts to read
                               uint8_t *values,  ///< uint8_t array to store the values in.
                               uint8_t rx_align  ///< Only bit positions rxAlign..7 in values[0] are updated.
) {
  std::string buf;
  buf = "Rx";
  char cstrb[20];
  if (count == 0) {
    return;
  }

  // Serial.print(F("Reading "));   Serial.print(count); Serial.println(F(" uint8_ts from register."));
  uint8_t address = 0x80 | reg;  // MSB == 1 is for reading. LSB is not used in address. Datasheet section 8.1.2.3.
  uint8_t index = 0;             // Index in values array.
  enable();
  count--;  // One read is performed outside of the loop

  write_byte(address);  // Tell MFRC522 which address we want to read
  if (rx_align) {       // Only update bit positions rxAlign..7 in values[0]
    // Create bit mask for bit positions rxAlign..7
    uint8_t mask = 0xFF << rx_align;
    // Read value and tell that we want to read the same address again.
    uint8_t value = transfer_byte(address);
    // Apply mask to both current value of values[0] and the new data in value.
    values[0] = (values[0] & ~mask) | (value & mask);
    index++;

    sprintf(cstrb, " %x", values[0]);
    buf.append(cstrb);
  }
  while (index < count) {
    values[index] = transfer_byte(address);  // Read value and tell that we want to read the same address again.

    sprintf(cstrb, " %x", values[index]);
    buf.append(cstrb);

    index++;
  }
  values[index] = transfer_byte(0);  // Read the final uint8_t. Send 0 to stop reading.

  buf = buf + " ";
  sprintf(cstrb, "%x", values[index]);
  buf.append(cstrb);

  ESP_LOGVV(TAG, "read_register_array_(%x, %d, , %d) -> %s", reg, count, rx_align, buf.c_str());

  disable();
}

void RC522::pcd_write_register_(PcdRegister reg,  ///< The register to write to. One of the PCD_Register enums.
                                uint8_t value     ///< The value to write.
) {
  enable();
  // MSB == 0 is for writing. LSB is not used in address. Datasheet section 8.1.2.3.
  transfer_byte(reg);
  transfer_byte(value);
  disable();
}

/**
 * Writes a number of uint8_ts to the specified register in the MFRC522 chip.
 * The interface is described in the datasheet section 8.1.2.
 */
void RC522::pcd_write_register_(PcdRegister reg,  ///< The register to write to. One of the PCD_Register enums.
                                uint8_t count,    ///< The number of uint8_ts to write to the register
                                uint8_t *values   ///< The values to write. uint8_t array.
) {
  std::string buf;
  buf = "Tx";

  enable();
  transfer_byte(reg);
  char cstrb[20];

  for (uint8_t index = 0; index < count; index++) {
    transfer_byte(values[index]);

    sprintf(cstrb, " %x", values[index]);
    buf.append(cstrb);
  }
  disable();
  ESP_LOGVV(TAG, "write_register_(%x, %d) -> %s", reg, count, buf.c_str());
}

/**
 * Transmits a REQuest command, Type A. Invites PICCs in state IDLE to go to READY and prepare for anticollision or
 * selection. 7 bit frame. Beware: When two PICCs are in the field at the same time I often get STATUS_TIMEOUT -
 * probably due do bad antenna design.
 *
 * @return STATUS_OK on success, STATUS_??? otherwise.
 */
RC522::StatusCode RC522::picc_request_a_(
    uint8_t *buffer_atqa,  ///< The buffer to store the ATQA (Answer to request) in
    uint8_t *buffer_size   ///< Buffer size, at least two uint8_ts. Also number of uint8_ts returned if STATUS_OK.
) {
  return picc_reqa_or_wupa_(PICC_CMD_REQA, buffer_atqa, buffer_size);
}

/**
 * Transmits REQA or WUPA commands.
 * Beware: When two PICCs are in the field at the same time I often get STATUS_TIMEOUT - probably due do bad antenna
 * design.
 *
 * @return STATUS_OK on success, STATUS_??? otherwise.
 */
RC522::StatusCode RC522::picc_reqa_or_wupa_(
    uint8_t command,       ///< The command to send - PICC_CMD_REQA or PICC_CMD_WUPA
    uint8_t *buffer_atqa,  ///< The buffer to store the ATQA (Answer to request) in
    uint8_t *buffer_size   ///< Buffer size, at least two uint8_ts. Also number of uint8_ts returned if STATUS_OK.
) {
  uint8_t valid_bits;
  RC522::StatusCode status;

  if (buffer_atqa == nullptr || *buffer_size < 2) {  // The ATQA response is 2 uint8_ts long.
    return STATUS_NO_ROOM;
  }
  pcd_clear_register_bit_mask_(COLL_REG, 0x80);  // ValuesAfterColl=1 => Bits received after collision are cleared.
  valid_bits = 7;  // For REQA and WUPA we need the short frame format - transmit only 7 bits of the last (and only)
                   // uint8_t. TxLastBits = BitFramingReg[2..0]
  status = pcd_transceive_data_(&command, 1, buffer_atqa, buffer_size, &valid_bits);
  if (status != STATUS_OK)
    return status;
  if (*buffer_size != 2 || valid_bits != 0) {  // ATQA must be exactly 16 bits.
    ESP_LOGVV(TAG, "picc_reqa_or_wupa_() -> STATUS_ERROR");
    return STATUS_ERROR;
  }

  return STATUS_OK;
}

/**
 * Sets the bits given in mask in register reg.
 */
void RC522::pcd_set_register_bit_mask_(PcdRegister reg,  ///< The register to update. One of the PCD_Register enums.
                                       uint8_t mask      ///< The bits to set.
) {
  uint8_t tmp = pcd_read_register_(reg);
  pcd_write_register_(reg, tmp | mask);  // set bit mask
}

/**
 * Clears the bits given in mask from register reg.
 */
void RC522::pcd_clear_register_bit_mask_(PcdRegister reg,  ///< The register to update. One of the PCD_Register enums.
                                         uint8_t mask      ///< The bits to clear.
) {
  uint8_t tmp = pcd_read_register_(reg);
  pcd_write_register_(reg, tmp & (~mask));  // clear bit mask
}

/**
 * Executes the Transceive command.
 * CRC validation can only be done if backData and backLen are specified.
 *
 * @return STATUS_OK on success, STATUS_??? otherwise.
 */
RC522::StatusCode RC522::pcd_transceive_data_(
    uint8_t *send_data,  ///< Pointer to the data to transfer to the FIFO.
    uint8_t send_len,    ///< Number of uint8_ts to transfer to the FIFO.
    uint8_t *back_data,  ///< nullptr or pointer to buffer if data should be read back after executing the command.
    uint8_t *back_len,   ///< In: Max number of uint8_ts to write to *backData. Out: The number of uint8_ts returned.
    uint8_t
        *valid_bits,   ///< In/Out: The number of valid bits in the last uint8_t. 0 for 8 valid bits. Default nullptr.
    uint8_t rx_align,  ///< In: Defines the bit position in backData[0] for the first bit received. Default 0.
    bool check_crc     ///< In: True => The last two uint8_ts of the response is assumed to be a CRC_A that must be
                       ///< validated.
) {
  uint8_t wait_i_rq = 0x30;  // RxIRq and IdleIRq
  auto ret = pcd_communicate_with_picc_(PCD_TRANSCEIVE, wait_i_rq, send_data, send_len, back_data, back_len, valid_bits,
                                        rx_align, check_crc);

  if (ret == STATUS_OK && *back_len == 5)
    ESP_LOGVV(TAG, "pcd_transceive_data_(..., %d,  ) -> %d [%x, %x, %x, %x, %x]", send_len, ret, back_data[0],
              back_data[1], back_data[2], back_data[3], back_data[4]);
  else
    ESP_LOGVV(TAG, "pcd_transceive_data_(..., %d, ... ) -> %d", send_len, ret);
  return ret;
}

/**
 * Transfers data to the MFRC522 FIFO, executes a command, waits for completion and transfers data back from the FIFO.
 * CRC validation can only be done if backData and backLen are specified.
 *
 * @return STATUS_OK on success, STATUS_??? otherwise.
 */
RC522::StatusCode RC522::pcd_communicate_with_picc_(
    uint8_t command,      ///< The command to execute. One of the PCD_Command enums.
    uint8_t wait_i_rq,    ///< The bits in the ComIrqReg register that signals successful completion of the command.
    uint8_t *send_data,   ///< Pointer to the data to transfer to the FIFO.
    uint8_t send_len,     ///< Number of uint8_ts to transfer to the FIFO.
    uint8_t *back_data,   ///< nullptr or pointer to buffer if data should be read back after executing the command.
    uint8_t *back_len,    ///< In: Max number of uint8_ts to write to *backData. Out: The number of uint8_ts returned.
    uint8_t *valid_bits,  ///< In/Out: The number of valid bits in the last uint8_t. 0 for 8 valid bits.
    uint8_t rx_align,     ///< In: Defines the bit position in backData[0] for the first bit received. Default 0.
    bool check_crc        ///< In: True => The last two uint8_ts of the response is assumed to be a CRC_A that must be
                          ///< validated.
) {
  ESP_LOGVV(TAG, "pcd_communicate_with_picc_(%d, %d,... %d)", command, wait_i_rq, check_crc);

  // Prepare values for BitFramingReg
  uint8_t tx_last_bits = valid_bits ? *valid_bits : 0;
  uint8_t bit_framing =
      (rx_align << 4) + tx_last_bits;  // RxAlign = BitFramingReg[6..4]. TxLastBits = BitFramingReg[2..0]

  pcd_write_register_(COMMAND_REG, PCD_IDLE);               // Stop any active command.
  pcd_write_register_(COM_IRQ_REG, 0x7F);                   // Clear all seven interrupt request bits
  pcd_write_register_(FIFO_LEVEL_REG, 0x80);                // FlushBuffer = 1, FIFO initialization
  pcd_write_register_(FIFO_DATA_REG, send_len, send_data);  // Write sendData to the FIFO
  pcd_write_register_(BIT_FRAMING_REG, bit_framing);        // Bit adjustments
  pcd_write_register_(COMMAND_REG, command);                // Execute the command
  if (command == PCD_TRANSCEIVE) {
    pcd_set_register_bit_mask_(BIT_FRAMING_REG, 0x80);  // StartSend=1, transmission of data starts
  }

  // Wait for the command to complete.
  // In PCD_Init() we set the TAuto flag in TModeReg. This means the timer automatically starts when the PCD stops
  // transmitting. Each iteration of the do-while-loop takes 17.86μs.
  // TODO check/modify for other architectures than Arduino Uno 16bit
  uint16_t i;
  for (i = 2000; i > 0; i--) {
    uint8_t n = pcd_read_register_(
        COM_IRQ_REG);     // ComIrqReg[7..0] bits are: Set1 TxIRq RxIRq IdleIRq HiAlertIRq LoAlertIRq ErrIRq TimerIRq
    if (n & wait_i_rq) {  // One of the interrupts that signal success has been set.
      break;
    }
    if (n & 0x01) {  // Timer interrupt - nothing received in 25ms
      return STATUS_TIMEOUT;
    }
  }
  // 35.7ms and nothing happend. Communication with the MFRC522 might be down.
  if (i == 0) {
    return STATUS_TIMEOUT;
  }

  // Stop now if any errors except collisions were detected.
  uint8_t error_reg_value = pcd_read_register_(
      ERROR_REG);  // ErrorReg[7..0] bits are: WrErr TempErr reserved BufferOvfl CollErr CRCErr ParityErr ProtocolErr
  if (error_reg_value & 0x13) {  // BufferOvfl ParityErr ProtocolErr
    return STATUS_ERROR;
  }

  uint8_t valid_bits_local = 0;

  // If the caller wants data back, get it from the MFRC522.
  if (back_data && back_len) {
    uint8_t n = pcd_read_register_(FIFO_LEVEL_REG);  // Number of uint8_ts in the FIFO
    if (n > *back_len) {
      return STATUS_NO_ROOM;
    }
    *back_len = n;                                              // Number of uint8_ts returned
    pcd_read_register_(FIFO_DATA_REG, n, back_data, rx_align);  // Get received data from FIFO
    valid_bits_local =
        pcd_read_register_(CONTROL_REG) & 0x07;  // RxLastBits[2:0] indicates the number of valid bits in the last
                                                 // received uint8_t. If this value is 000b, the whole uint8_t is valid.
    if (valid_bits) {
      *valid_bits = valid_bits_local;
    }
  }

  // Tell about collisions
  if (error_reg_value & 0x08) {  // CollErr
    return STATUS_COLLISION;
  }

  // Perform CRC_A validation if requested.
  if (back_data && back_len && check_crc) {
    // In this case a MIFARE Classic NAK is not OK.
    if (*back_len == 1 && valid_bits_local == 4) {
      return STATUS_MIFARE_NACK;
    }
    // We need at least the CRC_A value and all 8 bits of the last uint8_t must be received.
    if (*back_len < 2 || valid_bits_local != 0) {
      return STATUS_CRC_WRONG;
    }
    // Verify CRC_A - do our own calculation and store the control in controlBuffer.
    uint8_t control_buffer[2];
    RC522::StatusCode status = pcd_calculate_crc_(&back_data[0], *back_len - 2, &control_buffer[0]);
    if (status != STATUS_OK) {
      return status;
    }
    if ((back_data[*back_len - 2] != control_buffer[0]) || (back_data[*back_len - 1] != control_buffer[1])) {
      return STATUS_CRC_WRONG;
    }
  }

  return STATUS_OK;
}

/**
 * Use the CRC coprocessor in the MFRC522 to calculate a CRC_A.
 *
 * @return STATUS_OK on success, STATUS_??? otherwise.
 */

RC522::StatusCode RC522::pcd_calculate_crc_(
    uint8_t *data,   ///< In: Pointer to the data to transfer to the FIFO for CRC calculation.
    uint8_t length,  ///< In: The number of uint8_ts to transfer.
    uint8_t *result  ///< Out: Pointer to result buffer. Result is written to result[0..1], low uint8_t first.
) {
  ESP_LOGVV(TAG, "pcd_calculate_crc_(..., %d, ...)", length);
  pcd_write_register_(COMMAND_REG, PCD_IDLE);        // Stop any active command.
  pcd_write_register_(DIV_IRQ_REG, 0x04);            // Clear the CRCIRq interrupt request bit
  pcd_write_register_(FIFO_LEVEL_REG, 0x80);         // FlushBuffer = 1, FIFO initialization
  pcd_write_register_(FIFO_DATA_REG, length, data);  // Write data to the FIFO
  pcd_write_register_(COMMAND_REG, PCD_CALC_CRC);    // Start the calculation

  // Wait for the CRC calculation to complete. Each iteration of the while-loop takes 17.73μs.
  // TODO check/modify for other architectures than Arduino Uno 16bit

  // Wait for the CRC calculation to complete. Each iteration of the while-loop takes 17.73us.
  for (uint16_t i = 5000; i > 0; i--) {
    // DivIrqReg[7..0] bits are: Set2 reserved reserved MfinActIRq reserved CRCIRq reserved reserved
    uint8_t n = pcd_read_register_(DIV_IRQ_REG);
    if (n & 0x04) {                                // CRCIRq bit set - calculation done
      pcd_write_register_(COMMAND_REG, PCD_IDLE);  // Stop calculating CRC for new content in the FIFO.
      // Transfer the result from the registers to the result buffer
      result[0] = pcd_read_register_(CRC_RESULT_REG_L);
      result[1] = pcd_read_register_(CRC_RESULT_REG_H);

      ESP_LOGVV(TAG, "pcd_calculate_crc_() STATUS_OK");
      return STATUS_OK;
    }
  }
  ESP_LOGVV(TAG, "pcd_calculate_crc_() TIMEOUT");
  // 89ms passed and nothing happend. Communication with the MFRC522 might be down.
  return STATUS_TIMEOUT;
}
/**
 * Returns STATUS_OK if a PICC responds to PICC_CMD_REQA.
 * Only "new" cards in state IDLE are invited. Sleeping cards in state HALT are ignored.
 *
 * @return  STATUS_OK on success, STATUS_??? otherwise.
 */

RC522::StatusCode RC522::picc_is_new_card_present_() {
  uint8_t buffer_atqa[2];
  uint8_t buffer_size = sizeof(buffer_atqa);

  // Reset baud rates
  pcd_write_register_(TX_MODE_REG, 0x00);
  pcd_write_register_(RX_MODE_REG, 0x00);
  // Reset ModWidthReg
  pcd_write_register_(MOD_WIDTH_REG, 0x26);

  auto result = picc_request_a_(buffer_atqa, &buffer_size);

  ESP_LOGV(TAG, "picc_is_new_card_present_() -> %d", result);
  return result;
}

/**
 * Simple wrapper around PICC_Select.
 * Returns true if a UID could be read.
 * Remember to call PICC_IsNewCardPresent(), PICC_RequestA() or PICC_WakeupA() first.
 * The read UID is available in the class variable uid.
 *
 * @return bool
 */
bool RC522::picc_read_card_serial_() {
  RC522::StatusCode result = picc_select_(&this->uid_);
  ESP_LOGVV(TAG, "picc_select_(...) -> %d", result);
  return (result == STATUS_OK);
}

/**
 * Transmits SELECT/ANTICOLLISION commands to select a single PICC.
 * Before calling this function the PICCs must be placed in the READY(*) state by calling PICC_RequestA() or
 * PICC_WakeupA(). On success:
 *     - The chosen PICC is in state ACTIVE(*) and all other PICCs have returned to state IDLE/HALT. (Figure 7 of the
 * ISO/IEC 14443-3 draft.)
 *     - The UID size and value of the chosen PICC is returned in *uid along with the SAK.
 *
 * A PICC UID consists of 4, 7 or 10 uint8_ts.
 * Only 4 uint8_ts can be specified in a SELECT command, so for the longer UIDs two or three iterations are used:
 *     UID size  Number of UID uint8_ts    Cascade levels    Example of PICC
 *     ========  ===================    ==============    ===============
 *     single         4            1        MIFARE Classic
 *     double         7            2        MIFARE Ultralight
 *     triple        10            3        Not currently in use?
 *
 * @return STATUS_OK on success, STATUS_??? otherwise.
 */
RC522::StatusCode RC522::picc_select_(
    Uid *uid,           ///< Pointer to Uid struct. Normally output, but can also be used to supply a known UID.
    uint8_t valid_bits  ///< The number of known UID bits supplied in *uid. Normally 0. If set you must also supply
                        ///< uid->size.
) {
  bool uid_complete;
  bool select_done;
  bool use_cascade_tag;
  uint8_t cascade_level = 1;
  RC522::StatusCode result;
  uint8_t count;
  uint8_t check_bit;
  uint8_t index;
  uint8_t uid_index;                // The first index in uid->uiduint8_t[] that is used in the current Cascade Level.
  int8_t current_level_known_bits;  // The number of known UID bits in the current Cascade Level.
  uint8_t buffer[9];    // The SELECT/ANTICOLLISION commands uses a 7 uint8_t standard frame + 2 uint8_ts CRC_A
  uint8_t buffer_used;  // The number of uint8_ts used in the buffer, ie the number of uint8_ts to transfer to the FIFO.
  uint8_t rx_align;     // Used in BitFramingReg. Defines the bit position for the first bit received.
  uint8_t tx_last_bits;  // Used in BitFramingReg. The number of valid bits in the last transmitted uint8_t.
  uint8_t *response_buffer;
  uint8_t response_length;

  // Description of buffer structure:
  //    uint8_t 0: SEL         Indicates the Cascade Level: PICC_CMD_SEL_CL1, PICC_CMD_SEL_CL2 or PICC_CMD_SEL_CL3
  //    uint8_t 1: NVB          Number of Valid Bits (in complete command, not just the UID): High nibble: complete
  // uint8_ts,
  // Low nibble: Extra bits.     uint8_t 2: UID-data or CT    See explanation below. CT means Cascade Tag.     uint8_t
  // 3: UID-data uint8_t 4: UID-data     uint8_t 5: UID-data     uint8_t 6: BCC          Block Check Character - XOR of
  // uint8_ts 2-5 uint8_t 7: CRC_A uint8_t 8: CRC_A The BCC and CRC_A are only transmitted if we know all the UID bits
  // of the current Cascade Level.
  //
  // Description of uint8_ts 2-5: (Section 6.5.4 of the ISO/IEC 14443-3 draft: UID contents and cascade levels)
  //    UID size  Cascade level  uint8_t2  uint8_t3  uint8_t4  uint8_t5
  //    ========  =============  =====  =====  =====  =====
  //     4 uint8_ts    1      uid0  uid1  uid2  uid3
  //     7 uint8_ts    1      CT    uid0  uid1  uid2
  //            2      uid3  uid4  uid5  uid6
  //    10 uint8_ts    1      CT    uid0  uid1  uid2
  //            2      CT    uid3  uid4  uid5
  //            3      uid6  uid7  uid8  uid9

  // Sanity checks
  if (valid_bits > 80) {
    return STATUS_INVALID;
  }

  ESP_LOGVV(TAG, "picc_select_(&, %d)", valid_bits);

  // Prepare MFRC522
  pcd_clear_register_bit_mask_(COLL_REG, 0x80);  // ValuesAfterColl=1 => Bits received after collision are cleared.

  // Repeat Cascade Level loop until we have a complete UID.
  uid_complete = false;
  while (!uid_complete) {
    // Set the Cascade Level in the SEL uint8_t, find out if we need to use the Cascade Tag in uint8_t 2.
    switch (cascade_level) {
      case 1:
        buffer[0] = PICC_CMD_SEL_CL1;
        uid_index = 0;
        use_cascade_tag = valid_bits && uid->size > 4;  // When we know that the UID has more than 4 uint8_ts
        break;

      case 2:
        buffer[0] = PICC_CMD_SEL_CL2;
        uid_index = 3;
        use_cascade_tag = valid_bits && uid->size > 7;  // When we know that the UID has more than 7 uint8_ts
        break;

      case 3:
        buffer[0] = PICC_CMD_SEL_CL3;
        uid_index = 6;
        use_cascade_tag = false;  // Never used in CL3.
        break;

      default:
        return STATUS_INTERNAL_ERROR;
        break;
    }

    // How many UID bits are known in this Cascade Level?
    current_level_known_bits = valid_bits - (8 * uid_index);
    if (current_level_known_bits < 0) {
      current_level_known_bits = 0;
    }
    // Copy the known bits from uid->uiduint8_t[] to buffer[]
    index = 2;  // destination index in buffer[]
    if (use_cascade_tag) {
      buffer[index++] = PICC_CMD_CT;
    }
    uint8_t uint8_ts_to_copy = current_level_known_bits / 8 +
                               (current_level_known_bits % 8
                                    ? 1
                                    : 0);  // The number of uint8_ts needed to represent the known bits for this level.
    if (uint8_ts_to_copy) {
      uint8_t maxuint8_ts =
          use_cascade_tag ? 3 : 4;  // Max 4 uint8_ts in each Cascade Level. Only 3 left if we use the Cascade Tag
      if (uint8_ts_to_copy > maxuint8_ts) {
        uint8_ts_to_copy = maxuint8_ts;
      }
      for (count = 0; count < uint8_ts_to_copy; count++) {
        buffer[index++] = uid->uiduint8_t[uid_index + count];
      }
    }
    // Now that the data has been copied we need to include the 8 bits in CT in currentLevelKnownBits
    if (use_cascade_tag) {
      current_level_known_bits += 8;
    }

    // Repeat anti collision loop until we can transmit all UID bits + BCC and receive a SAK - max 32 iterations.
    select_done = false;
    while (!select_done) {
      // Find out how many bits and uint8_ts to send and receive.
      if (current_level_known_bits >= 32) {  // All UID bits in this Cascade Level are known. This is a SELECT.

        if (response_length < 4) {
          ESP_LOGW(TAG, "Not enough data received.");
          return STATUS_INVALID;
        }

        // Serial.print(F("SELECT: currentLevelKnownBits=")); Serial.println(currentLevelKnownBits, DEC);
        buffer[1] = 0x70;  // NVB - Number of Valid Bits: Seven whole uint8_ts
        // Calculate BCC - Block Check Character
        buffer[6] = buffer[2] ^ buffer[3] ^ buffer[4] ^ buffer[5];
        // Calculate CRC_A
        result = pcd_calculate_crc_(buffer, 7, &buffer[7]);
        if (result != STATUS_OK) {
          return result;
        }
        tx_last_bits = 0;  // 0 => All 8 bits are valid.
        buffer_used = 9;
        // Store response in the last 3 uint8_ts of buffer (BCC and CRC_A - not needed after tx)
        response_buffer = &buffer[6];
        response_length = 3;
      } else {  // This is an ANTICOLLISION.
        // Serial.print(F("ANTICOLLISION: currentLevelKnownBits=")); Serial.println(currentLevelKnownBits, DEC);
        tx_last_bits = current_level_known_bits % 8;
        count = current_level_known_bits / 8;     // Number of whole uint8_ts in the UID part.
        index = 2 + count;                        // Number of whole uint8_ts: SEL + NVB + UIDs
        buffer[1] = (index << 4) + tx_last_bits;  // NVB - Number of Valid Bits
        buffer_used = index + (tx_last_bits ? 1 : 0);
        // Store response in the unused part of buffer
        response_buffer = &buffer[index];
        response_length = sizeof(buffer) - index;
      }

      // Set bit adjustments
      rx_align = tx_last_bits;  // Having a separate variable is overkill. But it makes the next line easier to read.
      pcd_write_register_(
          BIT_FRAMING_REG,
          (rx_align << 4) + tx_last_bits);  // RxAlign = BitFramingReg[6..4]. TxLastBits = BitFramingReg[2..0]

      // Transmit the buffer and receive the response.
      result = pcd_transceive_data_(buffer, buffer_used, response_buffer, &response_length, &tx_last_bits, rx_align);
      if (result == STATUS_COLLISION) {  // More than one PICC in the field => collision.
        uint8_t value_of_coll_reg = pcd_read_register_(
            COLL_REG);  // CollReg[7..0] bits are: ValuesAfterColl reserved CollPosNotValid CollPos[4:0]
        if (value_of_coll_reg & 0x20) {  // CollPosNotValid
          return STATUS_COLLISION;       // Without a valid collision position we cannot continue
        }
        uint8_t collision_pos = value_of_coll_reg & 0x1F;  // Values 0-31, 0 means bit 32.
        if (collision_pos == 0) {
          collision_pos = 32;
        }
        if (collision_pos <= current_level_known_bits) {  // No progress - should not happen
          return STATUS_INTERNAL_ERROR;
        }
        // Choose the PICC with the bit set.
        current_level_known_bits = collision_pos;
        count = current_level_known_bits % 8;  // The bit to modify
        check_bit = (current_level_known_bits - 1) % 8;
        index = 1 + (current_level_known_bits / 8) + (count ? 1 : 0);  // First uint8_t is index 0.
        if (response_length > 2)  // Note: Otherwise buffer[index] might be not initialized
          buffer[index] |= (1 << check_bit);
      } else if (result != STATUS_OK) {
        return result;
      } else {                                 // STATUS_OK
        if (current_level_known_bits >= 32) {  // This was a SELECT.
          select_done = true;                  // No more anticollision
                                               // We continue below outside the while.
        } else {                               // This was an ANTICOLLISION.
          // We now have all 32 bits of the UID in this Cascade Level
          current_level_known_bits = 32;
          // Run loop again to do the SELECT.
        }
      }
    }  // End of while (!selectDone)

    // We do not check the CBB - it was constructed by us above.

    // Copy the found UID uint8_ts from buffer[] to uid->uiduint8_t[]
    index = (buffer[2] == PICC_CMD_CT) ? 3 : 2;  // source index in buffer[]
    uint8_ts_to_copy = (buffer[2] == PICC_CMD_CT) ? 3 : 4;
    for (count = 0; count < uint8_ts_to_copy; count++) {
      uid->uiduint8_t[uid_index + count] = buffer[index++];
    }

    // Check response SAK (Select Acknowledge)
    if (response_length != 3 || tx_last_bits != 0) {  // SAK must be exactly 24 bits (1 uint8_t + CRC_A).
      return STATUS_ERROR;
    }
    // Verify CRC_A - do our own calculation and store the control in buffer[2..3] - those uint8_ts are not needed
    // anymore.
    result = pcd_calculate_crc_(response_buffer, 1, &buffer[2]);
    if (result != STATUS_OK) {
      return result;
    }
    if ((buffer[2] != response_buffer[1]) || (buffer[3] != response_buffer[2])) {
      return STATUS_CRC_WRONG;
    }
    if (response_buffer[0] & 0x04) {  // Cascade bit set - UID not complete yes
      cascade_level++;
    } else {
      uid_complete = true;
      uid->sak = response_buffer[0];
    }
  }  // End of while (!uidComplete)

  // Set correct uid->size
  uid->size = 3 * cascade_level + 1;

  return STATUS_OK;
}

bool RC522BinarySensor::process(const uint8_t *data, uint8_t len) {
  if (len != this->uid_.size())
    return false;

  for (uint8_t i = 0; i < len; i++) {
    if (data[i] != this->uid_[i])
      return false;
  }

  this->publish_state(true);
  this->found_ = true;
  return true;
}
void RC522Trigger::process(const uint8_t *uid, uint8_t uid_length) {
  char buf[32];
  format_uid(buf, uid, uid_length);
  this->trigger(std::string(buf));
}

}  // namespace rc522_spi
}  // namespace esphome
