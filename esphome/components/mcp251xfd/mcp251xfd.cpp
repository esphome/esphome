#include "mcp251xfd.h"

#include <algorithm>
#include <cinttypes>
#include <cstring>

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::mcp251xfd {

ESPHOME_LOG_TAG(TAG, "mcp251xfd");

namespace {

uint8_t can_clock_to_mhz(CanClock clock) {
  switch (clock) {
    case CanClock::CAN_CLOCK_4MHZ:
      return 4;
    case CanClock::CAN_CLOCK_20MHZ:
      return 20;
    default:
    case CanClock::CAN_CLOCK_40MHZ:
      return 40;
  }
}

}  // namespace

void IRAM_ATTR MCP251XFD::gpio_intr(MCP251XFD *arg) { arg->enable_loop_soon_any_context(); }

bool MCP251XFD::setup_internal() {
  // A 4 MHz oscillator starts without the PLL. Keep SPI below 0.85 * SYSCLK / 2 until it locks.
  const uint32_t operating_data_rate = this->data_rate_;
  this->set_data_rate(std::min(operating_data_rate, uint32_t{1000000}));
  this->spi_setup();
  if (!this->reset_() || !this->configure_clock_())
    return false;

  if (this->data_rate_ != operating_data_rate) {
    this->spi_teardown();
    this->set_data_rate(operating_data_rate);
    this->spi_setup();
  }

  // Leave TEF, TXQ, automatic RTR responses and timestamps disabled. ESI follows the controller's error state.
  this->write_register_(REG_CON, (static_cast<uint32_t>(CanMode::CAN_MODE_CONFIGURATION) << 24) | CON_ISO_CRC_ENABLE);
  this->write_register_(REG_NBTCFG, this->nominal_bit_timing_);
  this->write_register_(REG_DBTCFG, this->data_bit_timing_);
  this->write_register_(REG_TDC, this->transmitter_delay_compensation_);
  bool timing_valid = true;
  if (uint32_t timing; !this->read_register_(REG_NBTCFG, timing) || timing != this->nominal_bit_timing_) {
    ESP_LOGE(TAG, "Nominal Bit Timing register verification failed");
    timing_valid = false;
  }
  if (uint32_t timing; !this->read_register_(REG_DBTCFG, timing) || timing != this->data_bit_timing_) {
    ESP_LOGE(TAG, "Data Bit Timing register verification failed");
    timing_valid = false;
  }
  if (uint32_t timing; !this->read_register_(REG_TDC, timing) || timing != this->transmitter_delay_compensation_) {
    ESP_LOGE(TAG, "Transmitter Delay Compensation register verification failed");
    timing_valid = false;
  }
  if (!timing_valid) {
    return false;
  }

  uint8_t zeros[32]{};
  for (uint16_t address = RAM_START; address < RAM_START + RAM_SIZE; address += sizeof(zeros))
    this->write_registers_(address, zeros, sizeof(zeros));

  uint32_t rx_interrupt_flag = (this->interrupt_pin_ != nullptr ? FIFO_RX_NOT_EMPTY_ENABLE : 0);
  this->write_register_(REG_TX_FIFO_CON, FIFO_PAYLOAD_64 | ((TX_FIFO_DEPTH - 1) << 24) | FIFO_TX_ENABLE);
  this->write_register_(REG_RX_FIFO_CON, FIFO_PAYLOAD_64 | ((RX_FIFO_DEPTH - 1) << 24) | rx_interrupt_flag);

  // Filter 0 accepts both standard and extended IDs and routes them to FIFO 2.
  this->write_registers_(REG_FLTCON, zeros, sizeof(zeros));
  this->write_register_(REG_FLTOBJ0, 0);
  this->write_register_(REG_MASK0, 0);
  this->write_byte_(REG_FLTCON, FILTER_ENABLE_RX_FIFO);

  if (!this->set_mode_(this->mcp_mode_))
    return false;

  if (!this->wait_for_register_(REG_TX_FIFO_CON + 1, FIFO_RESET, 0))
    return false;

  if (!this->wait_for_register_(REG_RX_FIFO_CON + 1, FIFO_RESET, 0))
    return false;

  if (this->interrupt_pin_ != nullptr) {
    this->interrupt_pin_->setup();
    this->interrupt_pin_->attach_interrupt(&MCP251XFD::gpio_intr, this, gpio::INTERRUPT_FALLING_EDGE);
    // Only RX-not-empty drives INT. Keep latched error diagnostics on the existing periodic poll.
    this->write_byte_(REG_INT + 2, INT_RX_ENABLE);
    if (!this->interrupt_pin_->digital_read())
      this->enable_loop_soon_any_context();
  }

  ESP_LOGD(TAG, "Setup complete");
  return true;
}

void MCP251XFD::dump_config() {
  canbus::Canbus::dump_config();
  ESP_LOGCONFIG(TAG, "MCP251XFD (CAN FD):");
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);

  [[maybe_unused]] const uint8_t clock_mhz = can_clock_to_mhz(this->mcp_clock_);
  ESP_LOGCONFIG(TAG, "  Oscillator: %u MHz", clock_mhz);
  ESP_LOGCONFIG(TAG, "  Mode: %u", static_cast<uint8_t>(this->mcp_mode_));
  ESP_LOGCONFIG(TAG, "  Nominal Bit Timing: 0x%08" PRIX32, this->nominal_bit_timing_);
  ESP_LOGCONFIG(TAG, "  Data Bit Timing: 0x%08" PRIX32, this->data_bit_timing_);
  ESP_LOGCONFIG(TAG, "  Transmitter Delay Compensation: 0x%08" PRIX32, this->transmitter_delay_compensation_);
}

bool MCP251XFD::reset_() {
  if (!this->set_mode_(CanMode::CAN_MODE_CONFIGURATION))
    return false;

  this->enable();
  this->write_byte(INSTRUCTION_RESET);
  this->write_byte(0);
  this->disable();
  delay(10);

  return this->set_mode_(CanMode::CAN_MODE_CONFIGURATION);
}

bool MCP251XFD::configure_clock_() {
  const bool use_pll = this->mcp_clock_ == CanClock::CAN_CLOCK_4MHZ;
  this->write_byte_(REG_OSC, use_pll ? OSC_PLL_ENABLE : 0);
  const uint8_t ready = OSC_READY | (use_pll ? OSC_PLL_READY : 0);
  return this->wait_for_register_(REG_OSC + 1, ready, ready);
}

bool MCP251XFD::wait_for_register_(uint16_t address, uint8_t mask, uint8_t expected) {
  for (uint8_t attempt = 0; attempt < 100; attempt++) {
    uint8_t value;
    if (!this->read_registers_(address, &value, 1))
      return false;
    if ((value & mask) == expected)
      return true;
    delay(1);
  }
  ESP_LOGE(TAG, "Timeout waiting for register 0x%03X", address);
  return false;
}

bool MCP251XFD::set_mode_(CanMode mode) {
  uint8_t control;
  if (!this->read_registers_(REG_CON + 3, &control, 1)) {
    return false;
  }
  this->write_byte_(REG_CON + 3, (control & ~CON_MODE_MASK) | static_cast<uint8_t>(mode));
  return this->wait_for_register_(REG_CON + 2, CON_MODE_MASK << CON_MODE_SHIFT,
                                  static_cast<uint8_t>(mode) << CON_MODE_SHIFT);
}

bool MCP251XFD::read_registers_(uint16_t address, uint8_t *data, size_t length) {
  const uint8_t command[] = {
      static_cast<uint8_t>(INSTRUCTION_READ_CRC | (address >> 8)), static_cast<uint8_t>(address),
      static_cast<uint8_t>(address >= RAM_START && address < RAM_START + RAM_SIZE ? length / 4 : length)};
  // MCP2517FD errata DS80000792, section 1: RAM reads can block CAN RAM access during SPI pauses.
  // Byte gaps and the last-byte-to-CS-high delay must stay below the phase-dependent TSPIMAXDLY.
  // Separate SPI calls and preemption can exceed it. RX MAB overflow discards the frame and sets SERRIF.
  // TX MAB underflow sets SERRIF/MODIF and enters Restricted Operation or Listen Only (CiCON.SERR2LOM).
  // Software must request Normal mode to resume TX. FIFO space and SPI CRC retries do not prevent this.
  // MCP251XFD errata: ordinary READ can silently return corrupt data. Retry READ_CRC on a mismatch.
  for (uint8_t attempt = 0; attempt < 3; attempt++) {
    this->enable();
    this->write_array(command, sizeof(command));
    this->read_array(data, length);
    const uint16_t crc_high = this->read_byte();
    const uint16_t received_crc = (crc_high << 8) | this->read_byte();
    this->disable();
    uint16_t crc = crc16be(command, sizeof(command), 0xFFFF, 0x8005);
    crc = crc16be(data, length, crc, 0x8005);
    if (crc == received_crc) {
      if (this->spi_error_) {
        this->spi_error_ = false;
        if (!this->bus_off_)
          this->status_clear_warning();
      }
      return true;
    }
  }
  this->spi_error_ = true;
  this->status_set_warning(LOG_STR("SPI CRC mismatch"));
  return false;
}

bool MCP251XFD::read_register_(uint16_t address, uint32_t &value) {
  uint8_t data[4];
  if (!this->read_registers_(address, data, sizeof(data)))
    return false;
  value = uint32_t{data[0]} | (uint32_t{data[1]} << 8) | (uint32_t{data[2]} << 16) | (uint32_t{data[3]} << 24);
  return true;
}

void MCP251XFD::write_registers_(uint16_t address, const uint8_t *data, size_t length) {
  // Keep the CS-low window tight on every model. Bus ownership does not prevent task preemption.
  // A single buffered transfer reduces gaps but still needs prompt CS release after the last byte.
  this->enable();
  this->write_byte(INSTRUCTION_WRITE | (address >> 8));
  this->write_byte(address);
  this->write_array(data, length);
  this->disable();
}

void MCP251XFD::write_register_(uint16_t address, uint32_t value) {
  const uint8_t data[] = {static_cast<uint8_t>(value), static_cast<uint8_t>(value >> 8),
                          static_cast<uint8_t>(value >> 16), static_cast<uint8_t>(value >> 24)};
  this->write_registers_(address, data, sizeof(data));
}

void MCP251XFD::write_byte_(uint16_t address, uint8_t value) { this->write_registers_(address, &value, 1); }

bool MCP251XFD::read_fifo_address_(uint16_t reg, uint32_t start, uint32_t size, uint16_t &address) {
  uint32_t offset;
  if (!this->read_register_(reg, offset))
    return false;
  if (offset < start || offset >= start + size || (offset - start) % MESSAGE_SIZE != 0) {
    this->status_set_warning(LOG_STR("Invalid FIFO address"));
    return false;
  }
  address = RAM_START + offset;
  if (!this->bus_off_)
    this->status_clear_warning();
  return true;
}

canbus::Error MCP251XFD::send_message(canbus::CanFrame *frame) {
  if (this->is_failed() || this->mcp_mode_ == CanMode::CAN_MODE_LISTEN_ONLY)
    return canbus::ERROR_FAILTX;

  const uint8_t length = canbus::can_dlc_to_len(frame->can_data_length_code);
  if (length == canbus::CAN_INVALID_DLC)
    return canbus::ERROR_FAILTX;
  if (!frame->fd) {
    if (length > canbus::CAN_MAX_DATA_LENGTH)
      return canbus::ERROR_FAILTX;
  }
  if (frame->fd) {
    if (frame->remote_transmission_request)
      return canbus::ERROR_FAILTX;
    if (this->mcp_mode_ == CanMode::CAN_MODE_CLASSIC)
      return canbus::ERROR_FAILTX;
  } else if (frame->bit_rate_switch) {
    return canbus::ERROR_FAILTX;
  }
  if (frame->use_extended_id) {
    if (frame->can_id > EXTENDED_ID_MASK)
      return canbus::ERROR_FAILTX;
  } else if (frame->can_id > STANDARD_ID_MASK) {
    return canbus::ERROR_FAILTX;
  }

  uint8_t status;
  if (!this->read_registers_(REG_TX_FIFO_STA, &status, 1))
    return canbus::ERROR_FAILTX;
  if ((status & FIFO_NOT_FULL_NOT_EMPTY) == 0)
    return canbus::ERROR_ALLTXBUSY;

  uint16_t address;
  if (!this->read_fifo_address_(REG_TX_FIFO_UA, 0, TX_FIFO_DEPTH * MESSAGE_SIZE, address))
    return canbus::ERROR_FAILTX;

  uint8_t message[MESSAGE_SIZE]{};
  uint32_t id;
  ;
  if (frame->use_extended_id)
    id = (frame->can_id >> 18) | ((frame->can_id & EXTENDED_ID_LOW_MASK) << 11);
  else
    id = frame->can_id;
  for (uint8_t i = 0; i < 4; i++)
    message[i] = id >> (8 * i);
  message[4] = frame->can_data_length_code;
  if (frame->use_extended_id)
    message[4] |= MESSAGE_IDE;
  if (frame->remote_transmission_request)
    message[4] |= MESSAGE_RTR;
  if (frame->fd)
    message[4] |= MESSAGE_FDF;
  if (frame->bit_rate_switch)
    message[4] |= MESSAGE_BRS;
  if (!frame->remote_transmission_request)
    std::memcpy(message + 8, frame->data, length);

  this->write_registers_(address, message, sizeof(message));
  this->write_byte_(REG_TX_FIFO_CON + 1, FIFO_INCREMENT | FIFO_TX_REQUEST);
  return canbus::ERROR_OK;
}

canbus::Error MCP251XFD::read_message(canbus::CanFrame *frame) {
  if (this->is_failed())
    return canbus::ERROR_FAIL;
  // INT stays low until the FIFO is empty. Keep the loop enabled for frame budgets and SPI retries.
  if (this->interrupt_pin_ != nullptr && this->interrupt_pin_->digital_read())
    return canbus::ERROR_NOMSG;
  uint8_t status;
  if (!this->read_registers_(REG_RX_FIFO_STA, &status, 1))
    return canbus::ERROR_FAIL;
  if ((status & FIFO_NOT_FULL_NOT_EMPTY) == 0)
    return canbus::ERROR_NOMSG;

  uint16_t address;
  if (!this->read_fifo_address_(REG_RX_FIFO_UA, RX_FIFO_OFFSET, RX_FIFO_DEPTH * MESSAGE_SIZE, address))
    return canbus::ERROR_FAIL;

  uint8_t message[MESSAGE_SIZE];
  if (!this->read_registers_(address, message, sizeof(message)))
    return canbus::ERROR_FAIL;
  this->write_byte_(REG_RX_FIFO_CON + 1, FIFO_INCREMENT);

  const uint32_t id = message[0] | static_cast<uint32_t>(message[1]) << 8 | static_cast<uint32_t>(message[2]) << 16 |
                      static_cast<uint32_t>(message[3]) << 24;
  frame->use_extended_id = (message[4] & MESSAGE_IDE) != 0;
  if (frame->use_extended_id)
    frame->can_id = ((id & STANDARD_ID_MASK) << 18) | ((id >> 11) & EXTENDED_ID_LOW_MASK);
  else
    frame->can_id = id & STANDARD_ID_MASK;
  frame->remote_transmission_request = (message[4] & MESSAGE_RTR) != 0;
  frame->fd = (message[4] & MESSAGE_FDF) != 0;
  frame->bit_rate_switch = false;
  frame->error_state_indicator = false;
  if (frame->fd) {
    if (frame->remote_transmission_request) {
      this->status_set_warning(LOG_STR("Invalid CAN FD frame: RTR is not supported"));
      return canbus::ERROR_FAIL;
    }
    frame->bit_rate_switch = (message[4] & MESSAGE_BRS) != 0;
    frame->error_state_indicator = (message[5] & MESSAGE_ESI) != 0;
  }
  frame->can_data_length_code = message[4] & MESSAGE_DLC_MASK;
  // Classic CAN DLC values 9..15 still represent eight data bytes.
  uint8_t length;
  if (frame->fd)
    length = canbus::can_dlc_to_len(frame->can_data_length_code);
  else
    length = std::min(frame->can_data_length_code, canbus::CAN_MAX_DATA_LENGTH);
  std::memset(frame->data, 0, sizeof(frame->data));
  if (!frame->remote_transmission_request)
    std::memcpy(frame->data, message + 8, length);
  return canbus::ERROR_OK;
}

canbus::CanStatus MCP251XFD::get_status() {
  canbus::CanStatus status{};
  if (!this->is_failed())
    this->read_register_(REG_TREC, this->last_trec_);
  status.rx_error_counter = this->last_trec_;
  status.tx_error_counter = this->last_trec_ >> 8;
  status.bus_off = ((this->last_trec_ >> 16) & ERROR_BUS_OFF) != 0;
  return status;
}

canbus::CanEventFlags MCP251XFD::get_events() {
  if (this->is_failed())
    return canbus::CanEventFlags{};

  uint8_t events = 0;
  if (!this->read_register_(REG_TREC, this->last_trec_)) {
    if (this->last_error_flags_ & ERROR_BUS_OFF)
      return static_cast<canbus::CanEventFlags>(canbus::CAN_EVENT_BUS_OFF);
    return static_cast<canbus::CanEventFlags>(0);
  }
  const uint8_t error_flags = this->last_trec_ >> 16;
  const bool was_passive = (this->last_error_flags_ & (ERROR_RX_PASSIVE | ERROR_TX_PASSIVE)) != 0;
  const bool is_passive = (error_flags & (ERROR_RX_PASSIVE | ERROR_TX_PASSIVE)) != 0;
  if (was_passive != is_passive) {
    if (is_passive)
      events |= canbus::CAN_EVENT_PASSIVE;
    else
      events |= canbus::CAN_EVENT_ACTIVE;
  }
  if (error_flags & ERROR_BUS_OFF)
    events |= canbus::CAN_EVENT_BUS_OFF;
  this->last_error_flags_ = error_flags;

  uint8_t fifo_status;
  const bool fifo_status_read = this->read_registers_(REG_RX_FIFO_STA, &fifo_status, 1);
  if (fifo_status_read) {
    if (fifo_status & FIFO_RX_OVERFLOW) {
      events |= canbus::CAN_EVENT_RX_QUEUE_FULL;
      this->write_byte_(REG_RX_FIFO_STA, static_cast<uint8_t>(~FIFO_RX_OVERFLOW));
    }
  }
  uint8_t interrupt_flags;
  const bool interrupt_flags_read = this->read_registers_(REG_INT + 1, &interrupt_flags, 1);
  if (interrupt_flags_read) {
    if (interrupt_flags & INT_SYSTEM_ERROR) {
      ESP_LOGW(TAG, "CAN system error (SERRIF): RX frame loss or TX halted. Check SPI RAM read timing");
      // SERRIF is latched, not a frame-loss counter. Clear only this flag using write-zero-to-clear.
      this->write_byte_(REG_INT + 1, static_cast<uint8_t>(~INT_SYSTEM_ERROR));
    }
  }
  return static_cast<canbus::CanEventFlags>(events);
}

}  // namespace esphome::mcp251xfd
