#include "mcp251xfd.h"

#include <algorithm>
#include <array>
#include <cinttypes>
#include <cstring>

#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::mcp251xfd {

ESPHOME_LOG_TAG(TAG, "mcp251xfd");

namespace {

static constexpr uint32_t FAULT_LOG_INTERVAL_MS = 5000;
static constexpr uint32_t MODE_RECOVERY_INTERVAL_MS = 5000;

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

  std::array<uint8_t, 32> zeros{};
  for (uint16_t address = RAM_START; address < RAM_START + RAM_SIZE; address += zeros.size())
    this->write_registers_(address, zeros.data(), zeros.size());

  uint32_t rx_interrupt_flag = (this->interrupt_pin_ != nullptr ? FIFO_RX_NOT_EMPTY_ENABLE : 0);
  this->write_register_(REG_TX_FIFO_CON, FIFO_PAYLOAD_64 | ((TX_FIFO_DEPTH - 1) << 24) | FIFO_TX_ENABLE);
  this->write_register_(REG_RX_FIFO_CON, FIFO_PAYLOAD_64 | ((RX_FIFO_DEPTH - 1) << 24) | rx_interrupt_flag);

  // Filter 0 accepts both standard and extended IDs and routes them to FIFO 2.
  this->write_registers_(REG_FLTCON, zeros.data(), zeros.size());
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

  const std::array<uint8_t, 2> command{INSTRUCTION_RESET, 0};
  this->enable();
  this->write_array(command.data(), command.size());
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
  if (length > MESSAGE_SIZE) {
    this->status_set_warning(LOG_STR("SPI read length exceeds message buffer"));
    return false;
  }

  const std::array<uint8_t, 3> command{
      static_cast<uint8_t>(INSTRUCTION_READ_CRC | (address >> 8)), static_cast<uint8_t>(address),
      static_cast<uint8_t>(address >= RAM_START && address < RAM_START + RAM_SIZE ? length / 4 : length)};

  // MCP2517FD errata DS80000792, section 1: RAM reads can block CAN RAM access during SPI pauses.
  // Byte gaps and the last-byte-to-CS-high delay must stay below the phase-dependent TSPIMAXDLY.
  // Separate SPI calls and preemption can exceed it. RX MAB overflow discards the frame and sets SERRIF.
  // TX MAB underflow sets SERRIF/MODIF and enters Restricted Operation or Listen Only (CiCON.SERR2LOM).
  // Software must request Normal mode to resume TX. FIFO space and SPI CRC retries do not prevent this.
  // MCP251XFD errata: ordinary READ can silently return corrupt data. Retry READ_CRC on a mismatch.

  // ESP32 SPI DMA writes RX data in 4-byte units, even with polling transfers.
  // Align and pad the buffer so a 77-byte read cannot overwrite adjacent stack data.
  alignas(4) std::array<uint8_t, (MESSAGE_SIZE + 5 + 3) & ~size_t{3}> transfer{};
  for (uint8_t attempt = 0; attempt < 3; attempt++) {
    transfer.fill(0);
    std::copy(command.begin(), command.end(), transfer.begin());

    this->enable();
    this->transfer_array(transfer.data(), length + command.size() + 2);
    this->disable();

    uint16_t crc = crc16be(command.data(), command.size(), 0xFFFF, 0x8005);
    crc = crc16be(transfer.data() + command.size(), length, crc, 0x8005);
    const size_t crc_offset = command.size() + length;
    const uint16_t received_crc = encode_uint16(transfer[crc_offset], transfer[crc_offset + 1]);
    if (crc == received_crc) {
      if (length != 0)
        std::memcpy(data, transfer.data() + command.size(), length);
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
  this->fault_counters_.host_crc_failures++;
  const uint32_t now = App.get_loop_component_start_time();
  if (this->should_log_fault_(now)) {
    ESP_LOGW(TAG, "SPI read CRC mismatch after all retries");
  }
  return false;
}

bool MCP251XFD::read_register_(uint16_t address, uint32_t &value) {
  std::array<uint8_t, 4> data;
  if (!this->read_registers_(address, data.data(), data.size()))
    return false;
  value = encode_value_le<uint32_t>(data.data());
  return true;
}

bool MCP251XFD::should_log_fault_(uint32_t now) {
  if (this->fault_log_started_ && now - this->last_fault_log_time_ < FAULT_LOG_INTERVAL_MS)
    return false;
  this->last_fault_log_time_ = now;
  this->fault_log_started_ = true;
  return true;
}

void MCP251XFD::recover_mode_if_needed_(bool bus_off, uint32_t now) {
  uint8_t mode_status;
  if (!this->read_registers_(REG_CON + 2, &mode_status, 1))
    return;
  const uint8_t current_mode = (mode_status >> CON_MODE_SHIFT) & CON_MODE_MASK;
  const uint8_t target_mode = static_cast<uint8_t>(this->mcp_mode_);
  if (current_mode == target_mode) {
    if (this->mode_fault_active_ && this->mode_recovery_pending_) {
      ESP_LOGI(TAG, "CAN controller returned to configured mode %u", target_mode);
    }
    this->mode_fault_active_ = false;
    this->mode_recovery_pending_ = false;
    return;
  }

  if (!this->mode_fault_active_) {
    this->fault_counters_.unexpected_mode_changes++;
    this->mode_fault_active_ = true;
    ESP_LOGW(TAG, "Unexpected CAN controller mode %u, configured mode is %u", current_mode, target_mode);
  }
  if (bus_off || (this->mode_recovery_attempted_ && now - this->last_mode_recovery_time_ < MODE_RECOVERY_INTERVAL_MS))
    return;

  this->last_mode_recovery_time_ = now;
  this->mode_recovery_attempted_ = true;
  uint8_t control;
  if (!this->read_registers_(REG_CON + 3, &control, 1))
    return;
  this->fault_counters_.mode_recovery_requests++;
  this->write_byte_(REG_CON + 3, (control & ~CON_MODE_MASK) | target_mode);
  this->mode_recovery_pending_ = true;
}

void MCP251XFD::write_registers_(uint16_t address, const uint8_t *data, size_t length) {
  if (length > MESSAGE_SIZE) {
    this->status_set_warning(LOG_STR("SPI write length exceeds message buffer"));
    return;
  }

  std::array<uint8_t, MESSAGE_SIZE + 2> transfer{};
  transfer[0] = static_cast<uint8_t>(INSTRUCTION_WRITE | (address >> 8));
  transfer[1] = static_cast<uint8_t>(address);
  if (length != 0)
    std::memcpy(transfer.data() + 2, data, length);

  // Keep the CS-low window tight on every model. Bus ownership does not prevent task preemption.
  // A single buffered transfer reduces gaps but still needs prompt CS release after the last byte.
  this->enable();
  this->write_array(transfer.data(), length + 2);
  this->disable();
}

void MCP251XFD::write_register_(uint16_t address, uint32_t value) {
  const auto data = decode_value(byteswap(value));
  this->write_registers_(address, data.data(), data.size());
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

  std::array<uint8_t, MESSAGE_SIZE> message{};
  uint32_t id;
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
    std::memcpy(message.data() + 8, frame->data, length);

  this->write_registers_(address, message.data(), message.size());
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

  std::array<uint8_t, MESSAGE_SIZE> message;
  if (!this->read_registers_(address, message.data(), message.size()))
    return canbus::ERROR_FAIL;
  this->write_byte_(REG_RX_FIFO_CON + 1, FIFO_INCREMENT);

  const uint32_t id = encode_value_le<uint32_t>(message.data());
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
    std::memcpy(frame->data, message.data() + 8, length);
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

  const uint32_t now = App.get_loop_component_start_time();

  uint8_t events = 0;
  if (!this->read_register_(REG_TREC, this->last_trec_)) {
    if (this->mode_fault_active_)
      this->recover_mode_if_needed_((this->last_error_flags_ & ERROR_BUS_OFF) != 0, now);
    if (this->last_error_flags_ & ERROR_BUS_OFF)
      return canbus::CAN_EVENT_BUS_OFF;
    return static_cast<canbus::CanEventFlags>(0);
  }
  const uint8_t error_flags = this->last_trec_ >> 16;
  const bool was_passive = (this->last_error_flags_ & (ERROR_RX_PASSIVE | ERROR_TX_PASSIVE)) != 0;
  const bool is_passive = (error_flags & (ERROR_RX_PASSIVE | ERROR_TX_PASSIVE)) != 0;
  const bool was_bus_off = (this->last_error_flags_ & ERROR_BUS_OFF) != 0;
  const bool is_bus_off = (error_flags & ERROR_BUS_OFF) != 0;
  if (was_passive != is_passive) {
    if (is_passive) {
      events |= canbus::CAN_EVENT_PASSIVE;
      this->fault_counters_.error_passive_events++;
    } else {
      events |= canbus::CAN_EVENT_ACTIVE;
    }
  }
  if (is_bus_off) {
    events |= canbus::CAN_EVENT_BUS_OFF;
    if (!was_bus_off) {
      this->fault_counters_.bus_off_events++;
      if (this->should_log_fault_(now)) {
        ESP_LOGW(TAG, "CAN controller entered bus-off. Hardware will recover after bus activity subsides");
      }
    }
  }
  this->last_error_flags_ = error_flags;

  uint8_t fifo_status = 0;
  uint8_t tx_fifo_status = 0;
  bool tx_fifo_status_read = false;
  bool mode_check_needed = this->mode_fault_active_;
  bool fault_observed = false;
  bool rx_overrun_counted = false;
  if (this->read_registers_(REG_RX_FIFO_STA, &fifo_status, 1)) {
    if (fifo_status & FIFO_RX_OVERFLOW) {
      events |= canbus::CAN_EVENT_RX_QUEUE_FULL;
      this->fault_counters_.receive_overruns++;
      rx_overrun_counted = true;
      fault_observed = true;
      this->write_byte_(REG_RX_FIFO_STA, static_cast<uint8_t>(~FIFO_RX_OVERFLOW));
    }
  }
  std::array<uint8_t, 2> interrupt_flags{};
  if (this->read_registers_(REG_INT, interrupt_flags.data(), interrupt_flags.size())) {
    const uint8_t controller_flags = interrupt_flags[1];
    const uint8_t direct_flags = controller_flags & (INT_SYSTEM_ERROR | INT_CAN_ERROR | INT_INVALID_MESSAGE);
    if (controller_flags & INT_SYSTEM_ERROR) {
      this->fault_counters_.system_errors++;
      mode_check_needed = true;
      fault_observed = true;
    }
    if (controller_flags & INT_CAN_ERROR) {
      this->fault_counters_.can_errors++;
      fault_observed = true;
    }
    if (controller_flags & INT_INVALID_MESSAGE) {
      this->fault_counters_.invalid_messages++;
      fault_observed = true;
    }
    if (direct_flags != 0)
      this->write_byte_(REG_INT + 1, static_cast<uint8_t>(~direct_flags));

    if (interrupt_flags[0] & INT_MODE_CHANGE) {
      mode_check_needed = true;
      this->write_byte_(REG_INT, static_cast<uint8_t>(~INT_MODE_CHANGE));
    }

    if (controller_flags & INT_RX_OVERFLOW) {
      if (!rx_overrun_counted)
        this->fault_counters_.receive_overruns++;
      fault_observed = true;
    }
    if (controller_flags & INT_TX_ATTEMPTS_EXHAUSTED) {
      this->fault_counters_.transmit_attempts_exhausted++;
      fault_observed = true;
      tx_fifo_status_read = this->read_registers_(REG_TX_FIFO_STA, &tx_fifo_status, 1);
      if (tx_fifo_status_read && (tx_fifo_status & FIFO_TX_ATTEMPTS_EXHAUSTED))
        this->write_byte_(REG_TX_FIFO_STA, static_cast<uint8_t>(~FIFO_TX_ATTEMPTS_EXHAUSTED));
    }
    if (controller_flags & INT_SPI_CRC_ERROR) {
      uint8_t crc_status;
      if (this->read_registers_(REG_CRC + 2, &crc_status, 1)) {
        const uint8_t crc_flags = crc_status & (CRC_CRC_ERROR | CRC_FORMAT_ERROR);
        if (crc_flags & CRC_CRC_ERROR)
          this->fault_counters_.spi_crc_errors++;
        if (crc_flags & CRC_FORMAT_ERROR)
          this->fault_counters_.spi_format_errors++;
        if (crc_flags == 0)
          this->fault_counters_.spi_crc_errors++;
        if (crc_flags != 0)
          this->write_byte_(REG_CRC + 2, static_cast<uint8_t>(~crc_flags));
      } else {
        this->fault_counters_.spi_crc_errors++;
      }
      fault_observed = true;
    }
  }
  if (fault_observed && this->should_log_fault_(now)) {
    [[maybe_unused]] const uint8_t faults = interrupt_flags[1] | (rx_overrun_counted ? INT_RX_OVERFLOW : 0);
    ESP_LOGW(TAG, "CAN fault:%s%s%s%s%s%s RX errors=%u, TX errors=%u",
             faults & INT_INVALID_MESSAGE ? LOG_STR_LITERAL(" invalid frame;") : LOG_STR_LITERAL(""),
             faults & INT_CAN_ERROR ? LOG_STR_LITERAL(" CAN bus error;") : LOG_STR_LITERAL(""),
             faults & INT_RX_OVERFLOW ? LOG_STR_LITERAL(" RX overflow;") : LOG_STR_LITERAL(""),
             faults & INT_TX_ATTEMPTS_EXHAUSTED ? LOG_STR_LITERAL(" TX retry limit;") : LOG_STR_LITERAL(""),
             faults & INT_SPI_CRC_ERROR ? LOG_STR_LITERAL(" SPI CRC/format error;") : LOG_STR_LITERAL(""),
             faults & INT_SYSTEM_ERROR ? LOG_STR_LITERAL(" controller system error (possible RX loss or halted TX);")
                                       : LOG_STR_LITERAL(""),
             static_cast<uint8_t>(this->last_trec_), static_cast<uint8_t>(this->last_trec_ >> 8));
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
    // Read adjacent BDIAG0 and BDIAG1 together in one SPI transfer.
    std::array<uint8_t, 8> diag_reg{};
    const bool diag_read = this->read_registers_(REG_BDIAG0, diag_reg.data(), diag_reg.size());
    ESP_LOGV(TAG,
             "INT1=0x%02X, RX FIFO=0x%02X, TX FIFO=0x%02X (read=%u), TREC=0x%08" PRIX32 ", BDIAG0=0x%08" PRIX32
             ", BDIAG1=0x%08" PRIX32 " (history since reset, read=%u)",
             interrupt_flags[1], fifo_status, tx_fifo_status, tx_fifo_status_read, this->last_trec_,
             encode_value_le<uint32_t>(diag_reg.data()), encode_value_le<uint32_t>(diag_reg.data() + 4), diag_read);
#endif
  }

  if (mode_check_needed)
    this->recover_mode_if_needed_(is_bus_off, now);
  return static_cast<canbus::CanEventFlags>(events);
}

}  // namespace esphome::mcp251xfd
