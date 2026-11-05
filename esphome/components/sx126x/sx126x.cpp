#include "sx126x.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"

namespace esphome::sx126x {

ESPHOME_LOG_TAG(TAG, "sx126x");
static constexpr uint32_t RESET_DELAY_HIGH_US = 5000;
static constexpr uint32_t RESET_DELAY_LOW_US = 2000;
static constexpr uint32_t TRANSMIT_TIMEOUT_MS = 4000;
static constexpr uint32_t MAX_TRANSMIT_TIMEOUT_MS = 262143;  // 24-bit SetTx timeout in 1/64 ms steps
static constexpr uint32_t RECEIVE_RETRY_INTERVAL_MS = 200;

// Indexed by SX126xError.
PROGMEM_STRING_TABLE(SX126xErrorStrings, "none", "timeout", "invalid payload", "radio not ready", "bus timeout",
                     "radio busy", "cancelled");

void SX126x::setup() {
  this->packet_.reserve(SX126X_MAX_PACKET_SIZE);
  // setup pins
  this->busy_pin_->setup();
  this->rst_pin_->setup();
  this->dio1_pin_->setup();
  if (this->dio1_pin_->is_internal()) {
    static_cast<InternalGPIOPin *>(this->dio1_pin_)
        ->attach_interrupt(&SX126x::gpio_intr, this, gpio::INTERRUPT_RISING_EDGE);
  }

  // start spi
  this->spi_device_.setup();

  // configure rf
  this->configure();
  if (this->state_ != State::STATE_IDLE) {
    this->mark_failed(LOG_STR("Radio configuration failed"));
  }
}

void SX126x::commit_(State state) {
  this->state_ = state;
  this->busy_ = false;
  if (state != State::STATE_NEEDS_CONFIGURATION) {
    this->enable_loop();
  }
}

bool SX126x::report_(bool ok) { return this->status_comm_result(ok, LOG_STR("Wait busy timeout")); }

void SX126x::configure() {
  if (this->is_failed()) {
    return;
  }
  // A held packet may be discarded: explicit reconfiguration is the recovery path.
  if (this->busy_ || this->state_ == State::STATE_TRANSMITTING) {
    ESP_LOGW(TAG, "Radio operation in progress, configuration ignored");
    return;
  }
  this->busy_ = true;
  this->rx_retry_pending_ = false;
  const LogString *failure = this->apply_configuration_();
  this->commit_(failure == nullptr ? State::STATE_IDLE : State::STATE_NEEDS_CONFIGURATION);
  if (failure == nullptr) {
    this->status_clear_warning();
  } else {
    this->status_set_warning(failure);
  }
}

const LogString *SX126x::apply_configuration_() {
  if (!this->requested_profile_.sync_valid) {
    return LOG_STR("Invalid sync value");
  }
  this->rst_pin_->digital_write(true);
  delayMicroseconds(RESET_DELAY_HIGH_US);
  this->rst_pin_->digital_write(false);
  delayMicroseconds(RESET_DELAY_LOW_US);
  this->rst_pin_->digital_write(true);
  delayMicroseconds(RESET_DELAY_HIGH_US);
  this->spi_device_.begin();
  switch (this->commands_.configure(this->requested_profile_, this->version_)) {
    case ConfigurationResult::CONFIGURATION_RESULT_OK:
      this->active_profile_ = this->requested_profile_;
      return nullptr;
    case ConfigurationResult::CONFIGURATION_RESULT_UNSUPPORTED:
      return LOG_STR("Unsupported chip version");
    default:
      return LOG_STR("Wait busy timeout");
  }
}

void SX126x::set_mode_rx() { this->change_mode_(ModeCommand::MODE_COMMAND_RX); }
void SX126x::set_mode_tx() { this->change_mode_(ModeCommand::MODE_COMMAND_TX); }
void SX126x::set_mode_standby(SX126xStandbyMode mode) { this->change_mode_(ModeCommand::MODE_COMMAND_STANDBY, mode); }
void SX126x::set_mode_sleep(bool cold) {
  this->change_mode_(cold ? ModeCommand::MODE_COMMAND_COLD_SLEEP : ModeCommand::MODE_COMMAND_SLEEP);
}
void SX126x::run_image_cal() { this->change_mode_(ModeCommand::MODE_COMMAND_IMAGE_CAL); }

void SX126x::change_mode_(ModeCommand command, uint8_t arg) {
  if (this->busy_ || this->state_ != State::STATE_IDLE) {
    ESP_LOGW(TAG, "Radio unavailable, mode change ignored");
    return;
  }
  this->busy_ = true;
  this->spi_device_.begin();
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
      this->commands_.calibrate_image(this->active_profile_.frequency);
      break;
  }
  const bool ok = this->spi_device_.ok();
  // Cold sleep loses the configuration, and a failed command leaves the chip state unknown.
  this->commit_(ok && command != ModeCommand::MODE_COMMAND_COLD_SLEEP ? State::STATE_IDLE
                                                                      : State::STATE_NEEDS_CONFIGURATION);
  this->report_(ok);
}

size_t SX126x::get_max_packet_size() { return this->active_profile_.max_packet_size(); }

SX126xError SX126x::transmit_packet(const uint8_t *data, size_t len) {
  SX126xError result = this->start_transmit_(nullptr, data, len, TRANSMIT_TIMEOUT_MS);
  if (result == SX126xError::NONE) {
    // Wait for this send's own completion. A send without a listener cannot be cancelled,
    // and only its own poll can end it, so a later send started by a callback is not awaited.
    result = SX126xError::SX126X_ERROR_NOT_READY;
    while (this->state_ == State::STATE_TRANSMITTING && !this->poll_transmit_(millis(), &result)) {
      App.feed_wdt();
    }
  }
  if (result != SX126xError::NONE) {
    ESP_LOGW(
        TAG, "Packet send failed: %s",
        LOG_STR_ARG(SX126xErrorStrings::get_log_str(static_cast<uint8_t>(result), SX126xErrorStrings::LAST_INDEX)));
  }
  return result;
}

SX126xError SX126x::start_transmit_async(SX126xTransmitListener *listener, const uint8_t *data, size_t len,
                                         uint32_t timeout_ms) {
  if (listener == nullptr || timeout_ms == 0 || timeout_ms > MAX_TRANSMIT_TIMEOUT_MS) {
    return SX126xError::INVALID_PARAMS;
  }
  return this->start_transmit_(listener, data, len, timeout_ms);
}

bool SX126x::cancel_transmit_async(SX126xTransmitListener *listener) {
  if (this->busy_ || this->state_ != State::STATE_TRANSMITTING || listener == nullptr ||
      listener != this->tx_.listener) {
    return false;
  }
  this->finish_transmit_(SX126xError::SX126X_ERROR_CANCELLED, nullptr);
  return true;
}

SX126xError SX126x::start_transmit_(SX126xTransmitListener *listener, const uint8_t *data, size_t len,
                                    uint32_t timeout_ms) {
  if (!this->is_ready() || this->state_ == State::STATE_NEEDS_CONFIGURATION) {
    return SX126xError::SX126X_ERROR_NOT_READY;
  }
  if (this->busy_ || this->state_ != State::STATE_IDLE) {
    return SX126xError::SX126X_ERROR_BUSY;
  }
  const uint32_t fixed_length = this->active_profile_.payload_length;
  if (data == nullptr || len == 0 || len > this->get_max_packet_size() || (fixed_length != 0 && len != fixed_length)) {
    return SX126xError::INVALID_PARAMS;
  }
  this->busy_ = true;
  this->spi_device_.begin();
  this->commands_.standby(STDBY_XOSC);
  // RX cannot advance in standby. Keep a packet that completed before standby intact for
  // loop(), before clearing IRQs or overwriting the shared FIFO. A failed read returns 0.
  if (this->commands_.read_irq() & IRQ_RX_DONE) {
    this->commit_(State::STATE_HOLDING_RX);
    return SX126xError::SX126X_ERROR_BUSY;
  }
  this->commands_.clear_irqs();
  if (fixed_length == 0) {
    this->commands_.set_packet_params(this->active_profile_, len);
  }
  this->commands_.write_fifo(data, len);
  this->commands_.transmit(timeout_ms);
  if (!this->spi_device_.ok()) {
    const bool restored = this->restore_();
    this->commit_(restored ? State::STATE_IDLE : State::STATE_NEEDS_CONFIGURATION);
    this->report_(restored);  // Last step: logging may run user code.
    return SX126xError::SX126X_ERROR_BUSY_TIMEOUT;
  }
  this->tx_ = {listener, millis(), timeout_ms};
  this->commit_(State::STATE_TRANSMITTING);
  // No status report on success, so no user code runs before the caller has the result.
  // The completion reports status instead.
  return SX126xError::NONE;
}

bool SX126x::poll_transmit_(uint32_t now, SX126xError *result) {
  if (this->busy_ || this->state_ != State::STATE_TRANSMITTING) {
    return false;
  }
  bool done = false;
  SX126xError outcome = SX126xError::NONE;
  if (this->dio1_pin_->digital_read()) {
    this->busy_ = true;
    this->spi_device_.begin();
    const uint16_t status = this->commands_.read_irq();
    this->commands_.clear_irqs(status);
    this->busy_ = false;
    if (!this->spi_device_.ok()) {
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
  if (done) {
    this->finish_transmit_(outcome, result);
  }
  return done;
}

void SX126x::finish_transmit_(SX126xError result, SX126xError *out) {
  this->busy_ = true;
  const bool restored = this->restore_();
  this->commit_(restored ? State::STATE_IDLE : State::STATE_NEEDS_CONFIGURATION);
  if (!restored) {
    result = SX126xError::SX126X_ERROR_BUSY_TIMEOUT;
  }
  auto *listener = std::exchange(this->tx_.listener, nullptr);
  if (out != nullptr) {
    *out = result;
  }
  // User code may run from here on: logging first, then the completion.
  this->report_(restored);
  if (listener != nullptr) {
    listener->on_transmit_complete(result);
  }
}

bool SX126x::restore_() {
  this->spi_device_.begin();
  this->commands_.standby(STDBY_XOSC);
  this->commands_.clear_irqs();
  if (this->active_profile_.payload_length == 0) {
    this->commands_.set_packet_params(this->active_profile_, this->get_max_packet_size());
  }
  if (this->active_profile_.rx_start) {
    this->commands_.receive();
  } else {
    this->commands_.sleep();
  }
  return this->spi_device_.ok();
}

void SX126x::loop() {
  if (this->busy_ || this->rx_dispatching_ || this->is_failed()) {
    return;
  }
  const uint32_t now = App.get_loop_component_start_time();
  if (this->state_ == State::STATE_TRANSMITTING) {
    this->poll_transmit_(now, nullptr);
    return;
  }
  if (this->state_ == State::STATE_NEEDS_CONFIGURATION) {
    this->disable_loop();
    return;
  }
  if (this->rx_retry_pending_ && now - this->rx_retry_started_ms_ < RECEIVE_RETRY_INTERVAL_MS) {
    return;
  }
  const bool holding = this->state_ == State::STATE_HOLDING_RX;
  if (!holding && !this->dio1_pin_->digital_read()) {
    if (this->dio1_pin_->is_internal()) {
      this->disable_loop();
    }
    return;
  }
  this->receive_(holding, now);
}

void SX126x::receive_(bool holding, uint32_t now) {
  this->busy_ = true;
  this->spi_device_.begin();
  const uint16_t status = this->commands_.read_irq();
  if (!holding) {
    this->commands_.clear_irqs(status);  // A held packet keeps its IRQ until restoration.
  }
  const bool received = (status & IRQ_RX_DONE) && !(status & IRQ_CRC_ERROR);
  float rssi = 0, snr = 0;
  if (received) {
    this->commands_.read_packet(this->active_profile_, this->packet_, rssi, snr);
  }
  const bool read = this->spi_device_.ok();
  // Keep a failed packet's IRQ/FIFO, but do not spend a BUSY timeout on every loop.
  this->rx_retry_pending_ = !read;
  this->rx_retry_started_ms_ = now;
  State next = this->state_;
  if (read && holding) {
    next = this->restore_() ? State::STATE_IDLE : State::STATE_NEEDS_CONFIGURATION;
  }
  this->commit_(next);
  // The packet buffer is borrowed from here until listeners return; loop() will not refill it.
  this->rx_dispatching_ = true;
  this->report_(read && next != State::STATE_NEEDS_CONFIGURATION);
  if (read && received) {
    for (auto *listener : this->listeners_) {
      listener->on_packet(this->packet_, rssi, snr);
    }
    this->packet_trigger_.trigger(this->packet_, rssi, snr);
  }
  this->rx_dispatching_ = false;
}

void SX126x::dump_config() {
  ESP_LOGCONFIG(TAG, "SX126x:\n  Ready for operation: %s",
                YESNO(!this->is_failed() && this->state_ == State::STATE_IDLE));
  LOG_PIN("  CS Pin: ", this->cs_pin_);
  LOG_PIN("  BUSY Pin: ", this->busy_pin_);
  LOG_PIN("  RST Pin: ", this->rst_pin_);
  LOG_PIN("  DIO1 Pin: ", this->dio1_pin_);
  ESP_LOGCONFIG(TAG,
                "  HW Version: %15s\n"
                "  Requested configuration:\n"
                "  Frequency: %" PRIu32 " Hz\n"
                "  Bandwidth: %" PRIu32 " Hz\n"
                "  PA Power: %" PRId8 " dBm\n"
                "  PA Ramp: %" PRIu16 " us\n"
                "  Payload Length: %" PRIu32 "\n"
                "  CRC Enable: %s\n"
                "  Rx Start: %s",
                this->version_, this->requested_profile_.frequency,
                Sx126xCommands::bandwidth_hz(this->requested_profile_.bandwidth), this->requested_profile_.pa_power,
                Sx126xCommands::ramp_us(this->requested_profile_.pa_ramp), this->requested_profile_.payload_length,
                TRUEFALSE(this->requested_profile_.crc_enable), TRUEFALSE(this->requested_profile_.rx_start));
  if (this->requested_profile_.modulation == PACKET_TYPE_GFSK) {
    const char *shaping = "NONE";
    if (this->requested_profile_.shaping == GAUSSIAN_BT_0_3) {
      shaping = "GAUSSIAN_BT_0_3";
    } else if (this->requested_profile_.shaping == GAUSSIAN_BT_0_5) {
      shaping = "GAUSSIAN_BT_0_5";
    } else if (this->requested_profile_.shaping == GAUSSIAN_BT_0_7) {
      shaping = "GAUSSIAN_BT_0_7";
    } else if (this->requested_profile_.shaping == GAUSSIAN_BT_1_0) {
      shaping = "GAUSSIAN_BT_1_0";
    }
    ESP_LOGCONFIG(TAG,
                  "  Modulation: FSK\n"
                  "  Deviation: %" PRIu32 " Hz\n"
                  "  Shaping: %s\n"
                  "  Preamble Size: %" PRIu16 "\n"
                  "  Preamble Detect: %" PRIu16 "\n"
                  "  Bitrate: %" PRIu32 "b/s",
                  this->requested_profile_.deviation, shaping, this->requested_profile_.preamble_size,
                  this->requested_profile_.preamble_detect, this->requested_profile_.bitrate);
  } else if (this->requested_profile_.modulation == PACKET_TYPE_LORA) {
    const char *cr = "4/8";
    if (this->requested_profile_.coding_rate == LORA_CR_4_5) {
      cr = "4/5";
    } else if (this->requested_profile_.coding_rate == LORA_CR_4_6) {
      cr = "4/6";
    } else if (this->requested_profile_.coding_rate == LORA_CR_4_7) {
      cr = "4/7";
    }
    ESP_LOGCONFIG(TAG,
                  "  Modulation: LORA\n"
                  "  Spreading Factor: %" PRIu8 "\n"
                  "  Coding Rate: %s\n"
                  "  Preamble Size: %" PRIu16,
                  this->requested_profile_.spreading_factor, cr, this->requested_profile_.preamble_size);
  }
  if (this->requested_profile_.sync_size != 0) {
    char hex_buf[17];  // 8 bytes max = 16 hex chars + null
    ESP_LOGCONFIG(
        TAG, "  Sync Value: 0x%s",
        format_hex_to(hex_buf, this->requested_profile_.sync_value.data(), this->requested_profile_.sync_size));
  }
  ESP_LOGCONFIG(TAG, "  Whitening Enable: %s", TRUEFALSE(this->requested_profile_.whitening_enable));
  if (this->requested_profile_.whitening_enable) {
    ESP_LOGCONFIG(TAG, "  Whitening Initial: 0x%03x", this->requested_profile_.whitening_initial);
  }
}

}  // namespace esphome::sx126x
