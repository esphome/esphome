#include "mcp251xfd.h"

#include <algorithm>
#include <array>
#include <cinttypes>
#include <cstring>

#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"

namespace esphome::mcp251xfd {

ESPHOME_LOG_TAG(TAG, "mcp251xfd");

// Indexed by CanMode, which is the CON REQOP/OPMOD value.
PROGMEM_STRING_TABLE(CanModeStrings, "NORMAL", "SLEEP", "LOOPBACK", "LISTENONLY", "CONFIGURATION", "EXTERNAL LOOPBACK",
                     "CLASSIC", "UNKNOWN");

static const LogString *mode_str(uint8_t mode) { return CanModeStrings::get_log_str(mode, CanModeStrings::LAST_INDEX); }

bool MCP251XFD::setup_internal() {
  this->spi_setup();
  return this->configure_();
}

bool MCP251XFD::configure_() {
  // A 4 MHz oscillator starts without the PLL. Keep SPI below 0.85 * SYSCLK / 2 until it locks.
  const uint32_t operating_data_rate = this->data_rate_;
  const bool slow_start = this->mcp_clock_ == CanClock::CAN_CLOCK_4MHZ && operating_data_rate > 1000000;
  if (slow_start) {
    this->spi_teardown();
    this->set_data_rate(1000000);
    this->spi_setup();
  }
  const bool clock_ready = this->reset_() && this->configure_clock_();
  // Restore the rate even on failure, so a later reconfiguration starts from it
  if (slow_start) {
    this->spi_teardown();
    this->set_data_rate(operating_data_rate);
    this->spi_setup();
  }
  if (!clock_ready)
    return false;

  // Leave TEF, TXQ, automatic RTR responses and timestamps disabled. ESI follows the controller's error state.
  // Leave RTXAT clear for unlimited retransmissions, as in the MCP2515 driver.
  std::array<uint8_t, MAX_REGISTER_WRITE_SIZE> config_data;
  uint8_t *out =
      put_le32(config_data.data(), (static_cast<uint32_t>(CanMode::CAN_MODE_CONFIGURATION) << 24) | CON_ISO_CRC_ENABLE);
  out = put_le32(out, this->nominal_bit_timing_);
  out = put_le32(out, this->data_bit_timing_);
  put_le32(out, this->transmitter_delay_compensation_);
  this->write_registers_(REG_CON, config_data.data(), config_data.size());

  // NBTCFG, DBTCFG and TDC follow CON
  std::array<uint8_t, 12> timing_data;
  if (!this->read_registers_(REG_NBTCFG, timing_data.data(), timing_data.size()) ||
      std::memcmp(timing_data.data(), config_data.data() + 4, timing_data.size()) != 0) {
    ESP_LOGE(TAG, "Bit timing register verification failed");
    return false;
  }

  // Enable the local overflow summary for polling; only RX-not-empty is enabled on the INT pin.
  const uint32_t payload_size = this->classic_() ? FIFO_PAYLOAD_8 : FIFO_PAYLOAD_64;
  this->write_register_(REG_TX_FIFO_CON, payload_size | ((TX_FIFO_DEPTH - 1) << 24) | FIFO_TX_ENABLE);
  this->write_register_(REG_RX_FIFO_CON, payload_size | ((this->rx_fifo_depth_() - 1) << 24) | FIFO_RX_OVERFLOW_ENABLE |
                                             (this->interrupt_pin_ != nullptr ? FIFO_RX_NOT_EMPTY_ENABLE : 0));

  // Filter 0 accepts both standard and extended IDs and routes them to FIFO 2.
  const std::array<uint8_t, 8> zeros{};
  this->write_registers_(REG_FLTOBJ0, zeros.data(), zeros.size());
  this->write_byte_(REG_FLTCON, FILTER_ENABLE_RX_FIFO);

  if (!this->set_mode_(this->mcp_mode_) || !this->wait_for_register_(REG_TX_FIFO_CON + 1, FIFO_RESET, 0) ||
      !this->wait_for_register_(REG_RX_FIFO_CON + 1, FIFO_RESET, 0)) {
    // Back to Configuration, so a later mode check sees the reset and retries all of the setup
    this->set_mode_(CanMode::CAN_MODE_CONFIGURATION);
    return false;
  }

  if (this->interrupt_pin_ != nullptr) {
    this->interrupt_pin_->setup();
    this->write_byte_(REG_INT + 2, INT_RX_ENABLE);
  }

  ESP_LOGV(TAG, "Setup complete");
  return true;
}

void MCP251XFD::dump_config() {
  canbus::Canbus::dump_config();
  ESP_LOGCONFIG(TAG,
                "MCP251XFD (CAN FD):\n"
                "  Oscillator: %u MHz\n"
                "  Mode: %s\n"
                "  Nominal Bit Timing: 0x%08" PRIX32 "\n"
                "  Data Bit Timing: 0x%08" PRIX32 "\n"
                "  Transmitter Delay Compensation: 0x%08" PRIX32,
                static_cast<uint8_t>(this->mcp_clock_), LOG_STR_ARG(mode_str(static_cast<uint8_t>(this->mcp_mode_))),
                this->nominal_bit_timing_, this->data_bit_timing_, this->transmitter_delay_compensation_);
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
}

bool MCP251XFD::reset_() {
  // Configuration mode waits for the bus to go idle, so a reset does not cut a frame on the bus.
  if (!this->set_mode_(CanMode::CAN_MODE_CONFIGURATION, 250)) {
    ESP_LOGW(TAG, "Config mode not confirmed before reset");
  }

  const std::array<uint8_t, 2> command{INSTRUCTION_RESET, 0};
  this->enable();
  this->write_array(command);
  this->disable();
  delay(10);

  return this->set_mode_(CanMode::CAN_MODE_CONFIGURATION);
}

bool MCP251XFD::configure_clock_() {
  uint8_t ready = OSC_READY;
  if (this->mcp_clock_ == CanClock::CAN_CLOCK_4MHZ) {
    // Keep the reset CLKO divider; only PLLEN changes.
    this->write_byte_(REG_OSC, OSC_CLKODIV_10 | OSC_PLL_ENABLE);
    ready |= OSC_PLL_READY;
  }
  return this->wait_for_register_(REG_OSC + 1, ready, ready);
}

bool MCP251XFD::wait_for_register_(uint16_t address, uint8_t mask, uint8_t expected, uint16_t timeout_ms) {
  for (uint16_t attempt = 0; attempt < timeout_ms; attempt++) {
    uint8_t value;
    if (!this->read_registers_(address, &value, 1))
      return false;
    if ((value & mask) == expected)
      return true;
    delay(1);
  }
  ESP_LOGW(TAG, "Timeout waiting for register 0x%03X", address);
  return false;
}

bool MCP251XFD::set_mode_(CanMode mode, uint16_t timeout_ms) {
  // The rest of this byte is ABAT and TXBWS, which the driver leaves clear.
  this->write_byte_(REG_CON + 3, static_cast<uint8_t>(mode));
  return this->wait_for_register_(REG_CON + 2, CON_MODE_MASK << CON_MODE_SHIFT,
                                  static_cast<uint8_t>(mode) << CON_MODE_SHIFT, timeout_ms);
}

bool MCP251XFD::read_registers_(uint16_t address, uint8_t *data, size_t length) {
  std::array<uint8_t, 3> command;
  put_be16(command.data(), (INSTRUCTION_READ_CRC << 8) | address);
  command[2] = address >= RAM_START && address < RAM_START + RAM_SIZE ? length / 4 : length;

  // Errata DS80000792, section 1: SPI pauses during a RAM read can cost a frame (SERRIF), so each read is one
  // transfer. Plain READ can return corrupt data, so READ_CRC is retried on a mismatch.
  // ESP32 SPI DMA writes in 4-byte units, so the buffer is aligned and padded.
  alignas(4) std::array<uint8_t, (MAX_READ_SIZE + 5 + 3) & ~size_t{3}> transfer;
  for (uint8_t attempt = 0; attempt < 3; attempt++) {
    std::fill_n(std::copy(command.begin(), command.end(), transfer.begin()), length + 2, 0);

    this->enable();
    this->transfer_array(transfer.data(), length + command.size() + 2);
    this->disable();

    uint16_t crc = crc16be(command.data(), command.size(), 0xFFFF, 0x8005);
    crc = crc16be(transfer.data() + command.size(), length, crc, 0x8005);
    const size_t crc_offset = command.size() + length;
    const uint16_t received_crc = encode_uint16(transfer[crc_offset], transfer[crc_offset + 1]);
    if (crc == received_crc) {
      std::memcpy(data, transfer.data() + command.size(), length);
      // The only place the driver's own warning clears; the canbus base owns the bus-off one.
      if (this->comm_fault_active_) {
        this->comm_fault_active_ = false;
        if (!this->bus_off_)
          this->status_clear_warning();
      }
      return true;
    }
  }
  return this->set_comm_fault_(LOG_STR("SPI CRC mismatch"));
}

bool MCP251XFD::set_comm_fault_(const LogString *reason) {
  this->comm_fault_active_ = true;
  this->status_set_warning(reason);
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
  if (now - this->last_fault_log_time_ < FAULT_LOG_INTERVAL_MS)
    return false;
  this->last_fault_log_time_ = now;
  return true;
}

bool MCP251XFD::recover_mode_if_needed_(bool bus_off, bool retry_due) {
  // OPMOD is in byte 2 of CON and REQOP in byte 3. A refused request leaves REQOP pending, which stalls TX.
  std::array<uint8_t, 2> con;
  if (!this->read_registers_(REG_CON + 2, con.data(), con.size()))
    return false;
  const uint8_t current_mode = (con[0] >> CON_MODE_SHIFT) & CON_MODE_MASK;
  const uint8_t requested_mode = con[1] & CON_MODE_MASK;
  const uint8_t target_mode = static_cast<uint8_t>(this->mcp_mode_);
  if (current_mode == target_mode && requested_mode == target_mode) {
    if (this->mode_fault_active_) {
      this->mode_fault_active_ = false;
      ESP_LOGI(TAG, "Mode %s restored", LOG_STR_ARG(mode_str(target_mode)));
    }
    return true;
  }

  const bool first_attempt = !this->mode_fault_active_;
  if (first_attempt) {
    this->mode_fault_active_ = true;
    ESP_LOGW(TAG, "Unexpected mode %s, requested %s, expected %s", LOG_STR_ARG(mode_str(current_mode)),
             LOG_STR_ARG(mode_str(requested_mode)), LOG_STR_ARG(mode_str(target_mode)));
  } else if (!retry_due) {
    // The first fault is fixed at once; the check interval spaces out later attempts.
    return true;
  }
  // The driver never requests Configuration mode and leaves it there, so this is a chip reset, which cleared the bit
  // timing, FIFOs and filter. NBTCFG cannot show it: its reset value is also the common 500 kbps timing.
  constexpr uint8_t configuration = static_cast<uint8_t>(CanMode::CAN_MODE_CONFIGURATION);
  const bool chip_reset = current_mode == configuration && requested_mode == configuration;
  const uint32_t now = App.get_loop_component_start_time();
  // A reset chip reports bus-off (TXBO) until it is configured again, so only a running controller waits it out
  if (bus_off && !chip_reset) {
    // Retry as soon as the bus recovers rather than on the next interval
    this->last_mode_check_time_ = now - MODE_RECOVERY_INTERVAL_MS;
    return true;
  }
  this->last_mode_check_time_ = now;

  if (chip_reset) {
    ESP_LOGW(TAG, "Configuration lost, reconfiguring");
    this->configure_();
    return true;
  }
  // A direct request keeps the FIFOs, as after a TX MAB underflow (errata DS80000792). The controller refuses some
  // direct changes, such as LOOPBACK to CLASSIC, so a retry goes through Configuration, which resets the FIFOs.
  if (first_attempt || this->set_mode_(CanMode::CAN_MODE_CONFIGURATION))
    this->write_byte_(REG_CON + 3, target_mode);
  return true;
}

void MCP251XFD::write_transfer_(uint16_t address, uint8_t *transfer, size_t length) {
  put_be16(transfer, (INSTRUCTION_WRITE << 8) | address);
  // One buffered transfer keeps the CS-low window tight (errata DS80000792, section 1).
  this->enable();
  this->write_array(transfer, length + 2);
  this->disable();
}

void MCP251XFD::write_registers_(uint16_t address, const uint8_t *data, size_t length) {
  std::array<uint8_t, 2 + MAX_REGISTER_WRITE_SIZE> transfer;
  std::memcpy(transfer.data() + 2, data, length);
  this->write_transfer_(address, transfer.data(), length);
}

void MCP251XFD::write_register_(uint16_t address, uint32_t value) {
  std::array<uint8_t, 4> data;
  put_le32(data.data(), value);
  this->write_registers_(address, data.data(), data.size());
}

void MCP251XFD::write_byte_(uint16_t address, uint8_t value) { this->write_registers_(address, &value, 1); }

bool MCP251XFD::validate_fifo_address_(uint32_t offset, uint32_t start, uint32_t depth, uint16_t &address) {
  const uint32_t message_size = this->message_size_();
  if (offset < start || offset >= start + depth * message_size || (offset - start) % message_size != 0) {
    return this->set_comm_fault_(LOG_STR("Invalid FIFO address"));
  }
  address = RAM_START + offset;
  return true;
}

canbus::Error MCP251XFD::send_message(canbus::CanFrame *frame) {
  if (this->is_failed() || this->mcp_mode_ == CanMode::CAN_MODE_LISTENONLY)
    return canbus::ERROR_FAILTX;
  if (frame->can_data_length_code > MESSAGE_DLC_MASK)
    return canbus::ERROR_FAILTX;

  std::array<uint8_t, 8> fifo;
  if (!this->read_registers_(REG_TX_FIFO_STA, fifo.data(), fifo.size()))
    return canbus::ERROR_FAILTX;
  if ((fifo[0] & FIFO_NOT_FULL_NOT_EMPTY) == 0)
    return canbus::ERROR_ALLTXBUSY;

  uint16_t address;
  if (!this->validate_fifo_address_(encode_value_le<uint32_t>(fifo.data() + 4), 0, TX_FIFO_DEPTH, address))
    return canbus::ERROR_FAILTX;

  // The object follows the two write command bytes, so it goes out without a copy.
  std::array<uint8_t, 2 + MAX_MESSAGE_SIZE> transfer;
  const uint32_t id =
      frame->use_extended_id ? (frame->can_id >> 18) | ((frame->can_id & EXTENDED_ID_LOW_MASK) << 11) : frame->can_id;
  uint32_t flags = frame->can_data_length_code;
  assign_bits(flags, MESSAGE_IDE, frame->use_extended_id);
  assign_bits(flags, MESSAGE_RTR, frame->remote_transmission_request);
#ifdef USE_CANBUS_FD
  assign_bits(flags, MESSAGE_FDF, frame->fd);
  assign_bits(flags, MESSAGE_BRS, frame->bit_rate_switch);
#endif
  uint8_t *payload = put_le32(put_le32(transfer.data() + 2, id), flags);
  const size_t payload_length = frame->remote_transmission_request ? 0 : frame->payload_length();
  // RAM writes must contain whole words. Unused bytes in the last word are zero.
  const size_t padded_length = (payload_length + 3) & ~size_t{3};
  std::memcpy(payload, frame->data, payload_length);
  std::fill(payload + payload_length, payload + padded_length, 0);
  this->write_transfer_(address, transfer.data(), MESSAGE_HEADER_SIZE + padded_length);
  this->write_byte_(REG_TX_FIFO_CON + 1, FIFO_INCREMENT | FIFO_TX_REQUEST);
  return canbus::ERROR_OK;
}

canbus::Error MCP251XFD::read_message(canbus::CanFrame *frame) {
  // INT stays low until the RX FIFO is empty, so a high pin means there is nothing to read.
  if (this->interrupt_pin_ != nullptr && this->interrupt_pin_->digital_read())
    return canbus::ERROR_NOMSG;
  std::array<uint8_t, 8> fifo;
  if (!this->read_registers_(REG_RX_FIFO_STA, fifo.data(), fifo.size()))
    return canbus::ERROR_FAIL;
  if ((fifo[0] & FIFO_NOT_FULL_NOT_EMPTY) == 0)
    return canbus::ERROR_NOMSG;

  uint16_t address;
  if (!this->validate_fifo_address_(encode_value_le<uint32_t>(fifo.data() + 4), TX_FIFO_DEPTH * this->message_size_(),
                                    this->rx_fifo_depth_(), address))
    return canbus::ERROR_FAIL;

  // Most frames carry at most 8 bytes, so read the header and 8 data bytes first and read the rest of a longer
  // CAN FD payload straight into the frame, rather than always reading the whole 72-byte object.
  std::array<uint8_t, MESSAGE_HEADER_SIZE + canbus::CAN_MAX_DATA_LENGTH> message;
  if (!this->read_registers_(address, message.data(), message.size()))
    return canbus::ERROR_FAIL;
  const bool fd = (message[4] & MESSAGE_FDF) != 0;
#ifdef USE_CANBUS_FD
  // CAN FD lengths above 8 are 8 plus whole words, as RAM reads need.
  const uint8_t length = fd ? canbus::can_dlc_to_len(message[4] & MESSAGE_DLC_MASK) : 0;
  if (length > canbus::CAN_MAX_DATA_LENGTH &&
      !this->read_registers_(address + message.size(), frame->data + canbus::CAN_MAX_DATA_LENGTH,
                             length - canbus::CAN_MAX_DATA_LENGTH))
    return canbus::ERROR_FAIL;
#endif
  this->write_byte_(REG_RX_FIFO_CON + 1, FIFO_INCREMENT);
  if (fd && this->classic_())
    return canbus::ERROR_FAIL;

  const uint32_t id = encode_value_le<uint32_t>(message.data());
  frame->use_extended_id = (message[4] & MESSAGE_IDE) != 0;
  if (frame->use_extended_id) {
    frame->can_id = ((id & canbus::CAN_STANDARD_ID_MAX) << 18) | ((id >> 11) & EXTENDED_ID_LOW_MASK);
  } else {
    frame->can_id = id & canbus::CAN_STANDARD_ID_MAX;
  }
  // In a CAN FD frame the RTR position is the reserved RRS bit
  frame->remote_transmission_request = !fd && (message[4] & MESSAGE_RTR);
#ifdef USE_CANBUS_FD
  // The base clears the other CAN FD flags before each read
  frame->fd = fd;
  if (fd) {
    frame->bit_rate_switch = (message[4] & MESSAGE_BRS) != 0;
    frame->error_state_indicator = (message[5] & MESSAGE_ESI) != 0;
  }
#endif
  frame->can_data_length_code = message[4] & MESSAGE_DLC_MASK;
  const size_t payload_length = std::min<size_t>(frame->payload_length(), canbus::CAN_MAX_DATA_LENGTH);
  // A remote request carries no data, so zero its bytes rather than keep the previous frame's
  if (frame->remote_transmission_request) {
    std::memset(frame->data, 0, payload_length);
  } else {
    std::memcpy(frame->data, message.data() + MESSAGE_HEADER_SIZE, payload_length);
  }
  return canbus::ERROR_OK;
}

canbus::CanStatus MCP251XFD::get_status() {
  canbus::CanStatus status{};
  if (!this->is_failed())
    this->read_register_(REG_TREC, this->last_trec_);
  status.rx_error_counter = this->last_trec_;
  status.tx_error_counter = this->last_trec_ >> 8;
  status.bus_off = ((this->last_trec_ >> 16) & ERROR_BUS_OFF) != 0;
  status.rx_overrun_count = this->rx_overrun_count_;
  status.bus_error_count = this->bus_error_count_;
  return status;
}

canbus::CanEventFlags MCP251XFD::get_events() {
  const uint32_t now = App.get_loop_component_start_time();
  // A refused mode request sets no flag, so the mode is also checked on an interval.
  const bool mode_check_due = now - this->last_mode_check_time_ >= MODE_RECOVERY_INTERVAL_MS;

  std::array<uint8_t, 2> interrupt_flags;
  const bool comm_fault_persisted = this->comm_fault_active_;
  if (!this->read_registers_(REG_INT, interrupt_flags.data(), interrupt_flags.size())) {
    // A chip reset turns the PLL off, which leaves a 4 MHz SYSCLK too slow for the SPI rate, so every read fails;
    // a fault that outlived an earlier poll tells that apart from one burst of noise
    if (this->mcp_clock_ == CanClock::CAN_CLOCK_4MHZ && mode_check_due && comm_fault_persisted) {
      this->last_mode_check_time_ = now;
      ESP_LOGW(TAG, "SPI reads failing, reconfiguring");
      this->configure_();
    }
    return (this->last_error_flags_ & ERROR_BUS_OFF) != 0 ? canbus::CAN_EVENT_BUS_OFF : canbus::CanEventFlags{};
  }
  // Only the handled flags, so a stray latched one cannot force a TREC read on every poll
  const uint8_t controller_flags =
      interrupt_flags[1] & (INT_SYSTEM_ERROR | INT_CAN_ERROR | INT_INVALID_MESSAGE | INT_RX_OVERFLOW);
  // Keep polling TREC through error recovery.
  const bool trec_needed = controller_flags != 0 || this->last_error_flags_ != 0 || (this->last_trec_ >> 16) != 0;
  const bool trec_read = trec_needed && this->read_register_(REG_TREC, this->last_trec_);
  const uint8_t error_flags = trec_read ? this->last_trec_ >> 16 : this->last_error_flags_;
  uint8_t events = 0;
  const bool was_passive = (this->last_error_flags_ & (ERROR_RX_PASSIVE | ERROR_TX_PASSIVE)) != 0;
  const bool is_passive = (error_flags & (ERROR_RX_PASSIVE | ERROR_TX_PASSIVE)) != 0;
  const bool is_bus_off = (error_flags & ERROR_BUS_OFF) != 0;
  if (was_passive != is_passive)
    events |= is_passive ? canbus::CAN_EVENT_PASSIVE : canbus::CAN_EVENT_ACTIVE;
  if (is_bus_off)
    events |= canbus::CAN_EVENT_BUS_OFF;
  this->last_error_flags_ = error_flags;

  if (controller_flags & INT_RX_OVERFLOW) {
    uint8_t fifo_status;
    if (this->read_registers_(REG_RX_FIFO_STA, &fifo_status, 1) && (fifo_status & FIFO_RX_OVERFLOW))
      this->write_byte_(REG_RX_FIFO_STA, static_cast<uint8_t>(~FIFO_RX_OVERFLOW));
    events |= canbus::CAN_EVENT_RX_QUEUE_FULL;
    this->rx_overrun_count_++;
  }
  if (controller_flags & (INT_CAN_ERROR | INT_INVALID_MESSAGE))
    this->bus_error_count_++;

  uint8_t direct_flags = controller_flags & (INT_SYSTEM_ERROR | INT_CAN_ERROR | INT_INVALID_MESSAGE);
  // Leave CERRIF pending when its error-state read failed, so the next poll retries it.
  if (!trec_read)
    direct_flags &= ~INT_CAN_ERROR;
  uint8_t mode_flags = interrupt_flags[0] & INT_MODE_CHANGE;
  const bool mode_check_needed = mode_check_due || mode_flags != 0 || (controller_flags & INT_SYSTEM_ERROR);
  if (mode_check_needed &&
      ((trec_needed && !trec_read) || !this->recover_mode_if_needed_(is_bus_off, mode_check_due))) {
    mode_flags = 0;
    direct_flags &= ~INT_SYSTEM_ERROR;
  } else if (mode_check_due && !is_bus_off) {
    // While bus-off the check repeats each poll, so a retry follows as soon as the bus recovers.
    this->last_mode_check_time_ = now;
  }
  if (mode_flags != 0 || direct_flags != 0) {
    // Both bytes have write-zero-to-clear flags; ones preserve unrelated and newly latched flags.
    const std::array<uint8_t, 2> clear_flags{static_cast<uint8_t>(~mode_flags), static_cast<uint8_t>(~direct_flags)};
    this->write_registers_(REG_INT, clear_flags.data(), clear_flags.size());
  }
  if ((controller_flags & INT_SYSTEM_ERROR) && this->should_log_fault_(now)) {
    ESP_LOGW(TAG, "Controller system error, RX errors=%u, TX errors=%u", static_cast<uint8_t>(this->last_trec_),
             static_cast<uint8_t>(this->last_trec_ >> 8));
  }

  return static_cast<canbus::CanEventFlags>(events);
}

}  // namespace esphome::mcp251xfd
