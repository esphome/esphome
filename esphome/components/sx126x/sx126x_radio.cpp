#include "sx126x_radio.h"
#include "esphome/core/log.h"
#include <utility>

namespace esphome::sx126x {

// No ESP_LOG calls in this file: log listeners are user code, see sx126x_radio.h.

static constexpr uint32_t RESET_DELAY_HIGH_US = 5000;
static constexpr uint32_t RESET_DELAY_LOW_US = 2000;
static constexpr uint32_t RECEIVE_RETRY_INTERVAL_MS = 200;

RadioEffects Sx126xRadio::comm_report(bool ok) {
  RadioEffects effects;
  effects.report = true;
  effects.warning = ok ? nullptr : LOG_STR("Wait busy timeout");
  return effects;
}

RadioEffects Sx126xRadio::configure(const RadioProfile &requested, std::span<char, 16> version) {
  ESPHOME_DEBUG_ASSERT(this->can_configure());
  this->busy_ = true;
  this->rx_retry_pending_ = false;
  this->pending_irq_ = 0;
  RadioEffects effects;
  effects.report = true;
  effects.warning = this->apply_configuration_(requested, version);
  this->commit_(effects.warning == nullptr ? RadioState::RADIO_STATE_IDLE
                                           : RadioState::RADIO_STATE_NEEDS_CONFIGURATION);
  return effects;
}

const LogString *Sx126xRadio::apply_configuration_(const RadioProfile &requested, std::span<char, 16> version) {
  if (!requested.sync_valid) {
    return LOG_STR("Invalid sync value");
  }
  this->rst_pin_->digital_write(true);
  delayMicroseconds(RESET_DELAY_HIGH_US);
  this->rst_pin_->digital_write(false);
  delayMicroseconds(RESET_DELAY_LOW_US);
  this->rst_pin_->digital_write(true);
  delayMicroseconds(RESET_DELAY_HIGH_US);
  this->bus().begin();
  AppliedProfile applied;
  switch (this->commands_.configure(requested, version, applied)) {
    case ConfigurationResult::CONFIGURATION_RESULT_OK:
      this->applied_ = applied;
      return nullptr;
    case ConfigurationResult::CONFIGURATION_RESULT_UNSUPPORTED:
      return LOG_STR("Unsupported chip version");
    default:
      return LOG_STR("Wait busy timeout");
  }
}

RadioEffects Sx126xRadio::change_mode(ModeCommand command, uint8_t arg) {
  ESPHOME_DEBUG_ASSERT(this->can_change_mode());
  this->busy_ = true;
  this->bus().begin();
  switch (command) {
    case ModeCommand::MODE_COMMAND_RX:
      this->commands_.receive();
      break;
    case ModeCommand::MODE_COMMAND_TX:
      this->commands_.transmit();
      break;
    case ModeCommand::MODE_COMMAND_STANDBY:
      this->commands_.standby(static_cast<SX126xStandbyMode>(arg));
      break;
    case ModeCommand::MODE_COMMAND_SLEEP:
    case ModeCommand::MODE_COMMAND_COLD_SLEEP:
      this->commands_.sleep(command == ModeCommand::MODE_COMMAND_COLD_SLEEP);
      break;
    case ModeCommand::MODE_COMMAND_IMAGE_CAL:
      this->commands_.calibrate_image(this->applied_.frequency);
      break;
  }
  const bool ok = this->bus().ok();
  // Cold sleep loses the configuration, and a failed command leaves the chip state unknown.
  this->commit_(ok && command != ModeCommand::MODE_COMMAND_COLD_SLEEP ? RadioState::RADIO_STATE_IDLE
                                                                      : RadioState::RADIO_STATE_NEEDS_CONFIGURATION);
  return comm_report(ok);
}

RadioEffects Sx126xRadio::start_transmit(SX126xTransmitListener *listener, const uint8_t *data, size_t len,
                                         uint32_t timeout_ms) {
  RadioEffects effects;
  if (this->state_ == RadioState::RADIO_STATE_NEEDS_CONFIGURATION) {
    effects.result = SX126xError::SX126X_ERROR_NOT_READY;
    return effects;
  }
  if (this->busy_ || this->state_ != RadioState::RADIO_STATE_IDLE) {
    effects.result = SX126xError::SX126X_ERROR_BUSY;
    return effects;
  }
  const uint8_t fixed_length = this->applied_.payload_length;
  if (data == nullptr || len == 0 || len > this->max_packet_size() || (fixed_length != 0 && len != fixed_length)) {
    effects.result = SX126xError::INVALID_PARAMS;
    return effects;
  }
  this->busy_ = true;
  this->bus().begin();
  this->commands_.standby(STDBY_XOSC);
  // RX cannot advance in standby. Keep a packet that completed before standby intact for
  // poll(), before clearing IRQs or overwriting the shared FIFO. A failed read returns 0.
  // A packet that failed its CRC is not delivered, so it does not hold up the send.
  const uint16_t irq = this->commands_.read_irq() | this->pending_irq_;
  if ((irq & IRQ_RX_DONE) && !(irq & IRQ_CRC_ERROR)) {
    this->commit_(RadioState::RADIO_STATE_HOLDING_RX);
    effects.result = SX126xError::SX126X_ERROR_BUSY;
    return effects;
  }
  this->commands_.clear_irqs();
  this->pending_irq_ = 0;
  if (fixed_length == 0) {
    this->commands_.set_packet_length(this->applied_, len);
  }
  this->commands_.write_fifo(data, len);
  this->commands_.transmit(timeout_ms);
  if (!this->bus().ok()) {
    const bool restored = this->restore_();
    this->commit_(restored ? RadioState::RADIO_STATE_IDLE : RadioState::RADIO_STATE_NEEDS_CONFIGURATION);
    effects = comm_report(restored);
    effects.result = SX126xError::SX126X_ERROR_BUSY_TIMEOUT;
    return effects;
  }
  this->tx_ = {listener, millis(), timeout_ms};
  this->commit_(RadioState::RADIO_STATE_TRANSMITTING);
  // No status report on success, so no user code runs before the caller has the result.
  // The completion reports status instead.
  return effects;
}

RadioEffects Sx126xRadio::poll_transmit(uint32_t now) {
  if (this->busy_ || !this->is_transmitting()) {
    return {};
  }
  bool done = false;
  SX126xError outcome = SX126xError::NONE;
  if (this->dio1_pin_->digital_read()) {
    this->busy_ = true;
    this->bus().begin();
    const uint16_t status = this->commands_.read_irq();
    this->commands_.clear_irqs(status);
    this->busy_ = false;
    if (!this->bus().ok()) {
      outcome = SX126xError::SX126X_ERROR_BUSY_TIMEOUT;
      done = true;
    } else if (status & IRQ_RX_TX_TIMEOUT) {
      outcome = SX126xError::TIMEOUT;
      done = true;
    } else if (status & IRQ_TX_DONE) {
      done = true;
    }
  }
  // A cached loop tick can precede admission; signed subtraction also handles wraparound.
  if (!done && static_cast<int32_t>(now - this->tx_.started_ms) >= static_cast<int32_t>(this->tx_.timeout_ms)) {
    outcome = SX126xError::TIMEOUT;
    done = true;
  }
  return done ? this->finish_transmit_(outcome) : RadioEffects{};
}

RadioEffects Sx126xRadio::cancel_transmit(SX126xTransmitListener *listener) {
  if (this->busy_ || !this->is_transmitting() || listener == nullptr || listener != this->tx_.listener) {
    return {};
  }
  return this->finish_transmit_(SX126xError::SX126X_ERROR_CANCELLED);
}

// The only place a send ends.
RadioEffects Sx126xRadio::finish_transmit_(SX126xError result) {
  this->busy_ = true;
  const bool restored = this->restore_();
  this->commit_(restored ? RadioState::RADIO_STATE_IDLE : RadioState::RADIO_STATE_NEEDS_CONFIGURATION);
  RadioEffects effects = comm_report(restored);
  effects.result = restored ? result : SX126xError::SX126X_ERROR_BUSY_TIMEOUT;
  effects.listener = std::exchange(this->tx_.listener, nullptr);
  effects.finished = true;
  return effects;
}

bool Sx126xRadio::restore_() {
  this->bus().begin();
  this->commands_.standby(STDBY_XOSC);
  this->commands_.clear_irqs();
  if (this->applied_.payload_length == 0) {
    this->commands_.set_packet_length(this->applied_, this->max_packet_size());
  }
  if (this->applied_.rx_start) {
    this->commands_.receive();
  } else {
    this->commands_.sleep();
  }
  return this->bus().ok();
}

RadioEffects Sx126xRadio::poll(uint32_t now, std::vector<uint8_t> &packet) {
  if (this->busy_) {
    return {};
  }
  if (this->is_transmitting()) {
    return this->poll_transmit(now);
  }
  RadioEffects effects;
  if (this->state_ == RadioState::RADIO_STATE_NEEDS_CONFIGURATION) {
    effects.sleep_loop = true;  // Polling cannot recover the radio; configure() can.
    return effects;
  }
  if (this->rx_retry_pending_ && now - this->rx_retry_started_ms_ < RECEIVE_RETRY_INTERVAL_MS) {
    return effects;
  }
  const bool holding = this->state_ == RadioState::RADIO_STATE_HOLDING_RX;
  if (!holding && this->pending_irq_ == 0 && !this->dio1_pin_->digital_read()) {
    effects.sleep_loop = this->dio1_pin_->is_internal();  // Its interrupt wakes the loop.
    return effects;
  }
  return this->receive_(holding, now, packet);
}

RadioEffects Sx126xRadio::receive_(bool holding, uint32_t now, std::vector<uint8_t> &packet) {
  this->busy_ = true;
  this->bus().begin();
  // A packet whose read failed has had its flags cleared, so they are kept until it is read.
  const uint16_t status = this->commands_.read_irq() | this->pending_irq_;
  const bool received = (status & IRQ_RX_DONE) && !(status & IRQ_CRC_ERROR);
  if (!holding) {
    // Cleared before the read, so a packet that completes during the read raises DIO1 again.
    // A held packet keeps its IRQ until restoration.
    this->commands_.clear_irqs(status);
  }
  float rssi = 0, snr = 0;
  if (received) {
    this->commands_.read_packet(this->applied_.modulation, packet, rssi, snr);
  }
  const bool read = this->bus().ok();
  this->pending_irq_ = read ? 0 : status;
  // Do not spend a BUSY timeout on every loop while a read keeps failing.
  this->rx_retry_pending_ = !read;
  this->rx_retry_started_ms_ = now;
  RadioState next = this->state_;
  if (read && holding) {
    next = this->restore_() ? RadioState::RADIO_STATE_IDLE : RadioState::RADIO_STATE_NEEDS_CONFIGURATION;
  }
  this->commit_(next);
  RadioEffects effects = comm_report(read && next != RadioState::RADIO_STATE_NEEDS_CONFIGURATION);
  effects.received = read && received;
  effects.rssi = rssi;
  effects.snr = snr;
  return effects;
}

}  // namespace esphome::sx126x
