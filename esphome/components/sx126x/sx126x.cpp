#include "sx126x.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"

namespace esphome::sx126x {

ESPHOME_LOG_TAG(TAG, "sx126x");
static constexpr uint32_t TRANSMIT_TIMEOUT_MS = 4000;
static constexpr uint32_t MAX_TRANSMIT_TIMEOUT_MS = 262143;  // 24-bit SetTx timeout in 1/64 ms steps
static constexpr uint32_t RECONFIGURE_INTERVAL_ID = 0;
static constexpr uint32_t RECONFIGURE_INTERVAL_MS = 1000;

// Indexed by SX126xError.
PROGMEM_STRING_TABLE(SX126xErrorStrings, "none", "timeout", "invalid payload", "radio not ready", "bus timeout",
                     "radio busy", "cancelled");

void SX126x::setup() {
  // Sized once so later packet reads never reallocate; read_packet() sets the length.
  this->packet_.resize(SX126X_MAX_PACKET_SIZE);
  // setup pins
  GPIOPin *dio1 = this->radio_.get_dio1_pin();
  this->radio_.bus().get_busy_pin()->setup();
  this->radio_.get_rst_pin()->setup();
  dio1->setup();
  if (dio1->is_internal()) {
    static_cast<InternalGPIOPin *>(dio1)->attach_interrupt(&SX126x::gpio_intr, this, gpio::INTERRUPT_RISING_EDGE);
  }

  // start spi
  this->radio_.bus().setup();

  // configure rf; a failure is reported through mark_failed() only, with its own reason
  const RadioEffects effects = this->radio_.configure(this->requested_profile_, this->version_);
  if (effects.warning != nullptr) {
    this->mark_failed(effects.warning);
    return;
  }
  this->apply_(effects);
}

void SX126x::apply_(const RadioEffects &effects) {
  // User code may only run once the radio has committed. A rejection during another
  // operation's command sequence carries no status or callbacks.
  ESPHOME_DEBUG_ASSERT(!this->radio_.busy() || (!effects.report && effects.listener == nullptr && !effects.received));
  if (effects.sleep_loop) {
    this->disable_loop();
  } else if (this->radio_.state() != RadioState::RADIO_STATE_NEEDS_CONFIGURATION) {
    this->enable_loop();
  }
  this->watch_configuration_();
  // The packet buffer is borrowed from the status report until listeners return;
  // loop() will not refill it meanwhile.
  if (effects.received) {
    this->rx_dispatching_ = true;
  }
  if (effects.report) {
    this->status_comm_result(effects.warning == nullptr, effects.warning);
  }
  if (effects.listener != nullptr) {
    effects.listener->on_transmit_complete(effects.result);
  }
  if (effects.received) {
    for (auto *listener : this->listeners_) {
      listener->on_packet(this->packet_, effects.rssi, effects.snr);
    }
    this->packet_trigger_.trigger(this->packet_, effects.rssi, effects.snr);
    this->rx_dispatching_ = false;
  }
}

void SX126x::configure() {
  // Before setup() the bus is not set up, and after a failed setup the radio stays unused
  if (!this->is_ready()) {
    return;
  }
  if (!this->radio_.can_configure()) {
    ESP_LOGW(TAG, "Radio operation in progress, configuration ignored");
    return;
  }
  this->cold_sleep_ = false;
  this->apply_(this->radio_.configure(this->requested_profile_, this->version_));
}

// A failed command leaves the radio needing configuration, which nothing else would do, so a node
// that only uses YAML would stay deaf. Cold sleep is asked for, so it waits for a mode command.
void SX126x::watch_configuration_() {
  const bool needed = this->radio_.state() == RadioState::RADIO_STATE_NEEDS_CONFIGURATION && !this->cold_sleep_;
  if (needed == this->reconfigure_pending_) {
    return;
  }
  this->reconfigure_pending_ = needed;
  if (needed) {
    this->set_interval(RECONFIGURE_INTERVAL_ID, RECONFIGURE_INTERVAL_MS, [this]() { this->configure(); });
  } else {
    this->cancel_interval(RECONFIGURE_INTERVAL_ID);
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
  const bool sleep = command == ModeCommand::MODE_COMMAND_SLEEP || command == ModeCommand::MODE_COMMAND_COLD_SLEEP;
  if (this->cold_sleep_) {
    if (sleep) {
      return;
    }
    // Cold sleep lost the configuration, so waking needs it applied again first
    this->configure();
  }
  if (!this->radio_.can_change_mode()) {
    ESP_LOGW(TAG, "Radio unavailable, mode change ignored");
    return;
  }
  this->cold_sleep_ = command == ModeCommand::MODE_COMMAND_COLD_SLEEP;
  this->apply_(this->radio_.change_mode(command, arg));
}

size_t SX126x::get_max_packet_size() { return this->radio_.max_packet_size(); }

SX126xError SX126x::transmit_packet(const uint8_t *data, size_t len) {
  SX126xError result = this->start_transmit_(nullptr, data, len, TRANSMIT_TIMEOUT_MS);
  if (result == SX126xError::SX126X_ERROR_NONE) {
    // Wait for this send's own completion. A send without a listener cannot be cancelled,
    // and no user code runs between these polls, so the send cannot be replaced.
    result = SX126xError::SX126X_ERROR_NOT_READY;
    while (this->radio_.is_transmitting()) {
      const RadioEffects effects = this->radio_.poll_transmit(millis());
      if (effects.finished) {
        result = effects.result;
        this->apply_(effects);
        break;
      }
      App.feed_wdt();
    }
  }
  if (result != SX126xError::SX126X_ERROR_NONE) {
    ESP_LOGW(
        TAG, "Packet send failed: %s",
        LOG_STR_ARG(SX126xErrorStrings::get_log_str(static_cast<uint8_t>(result), SX126xErrorStrings::LAST_INDEX)));
  }
  return result;
}

SX126xError SX126x::start_transmit_async(SX126xTransmitListener *listener, const uint8_t *data, size_t len,
                                         uint32_t timeout_ms) {
  if (listener == nullptr || timeout_ms == 0 || timeout_ms > MAX_TRANSMIT_TIMEOUT_MS) {
    return SX126xError::SX126X_ERROR_INVALID_PARAMS;
  }
  return this->start_transmit_(listener, data, len, timeout_ms);
}

SX126xError SX126x::start_transmit_(SX126xTransmitListener *listener, const uint8_t *data, size_t len,
                                    uint32_t timeout_ms) {
  if (!this->is_ready()) {
    return SX126xError::SX126X_ERROR_NOT_READY;
  }
  const RadioEffects effects = this->radio_.start_transmit(listener, data, len, timeout_ms);
  this->apply_(effects);
  return effects.result;
}

bool SX126x::cancel_transmit_async(SX126xTransmitListener *listener) {
  const RadioEffects effects = this->radio_.cancel_transmit(listener);
  if (!effects.finished) {
    return false;
  }
  this->apply_(effects);
  return true;
}

void SX126x::loop() {
  if (this->rx_dispatching_ || this->is_failed()) {
    return;
  }
  this->apply_(this->radio_.poll(App.get_loop_component_start_time(), this->packet_));
}

void SX126x::dump_config() {
  ESP_LOGCONFIG(TAG, "SX126x:\n  Ready for operation: %s",
                YESNO(!this->is_failed() && this->radio_.state() == RadioState::RADIO_STATE_IDLE));
  LOG_PIN("  CS Pin: ", this->radio_.bus().get_cs_pin());
  LOG_PIN("  BUSY Pin: ", this->radio_.bus().get_busy_pin());
  LOG_PIN("  RST Pin: ", this->radio_.get_rst_pin());
  LOG_PIN("  DIO1 Pin: ", this->radio_.get_dio1_pin());
  ESP_LOGCONFIG(TAG,
                "  HW Version: %15s\n"
                "  Requested configuration:\n"
                "  Frequency: %" PRIu32 " Hz\n"
                "  Bandwidth: %" PRIu32 " Hz\n"
                "  PA Power: %" PRId8 " dBm\n"
                "  PA Ramp: %" PRIu16 " us\n"
                "  Payload Length: %" PRIu8 "\n"
                "  CRC Enable: %s\n"
                "  Rx Start: %s",
                this->version_, this->requested_profile_.frequency,
                SX126xCommands::bandwidth_hz(this->requested_profile_.bandwidth), this->requested_profile_.pa_power,
                SX126xCommands::ramp_us(this->requested_profile_.pa_ramp), this->requested_profile_.payload_length,
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
