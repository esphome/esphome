#include "sx1231.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"
#include <cmath>

namespace esphome::sx1231 {

ESPHOME_LOG_TAG(TAG, "sx1231");
static constexpr uint32_t FXOSC = 32000000u;
static constexpr uint8_t CHIP_VERSION = 0x24;
static constexpr uint32_t TRANSMIT_TIMEOUT_MS = 4000;
static constexpr uint32_t MODE_TIMEOUT_MS = 50;
static constexpr uint32_t RECONFIGURE_INTERVAL_ID = 0;
static constexpr uint32_t RECONFIGURE_INTERVAL_MS = 1000;
static constexpr uint32_t RX_STALL_CHECK_INTERVAL_MS = 1000;
// In units of 16 bit times from signal detection: 510 byte times, more than a packet with a 435 byte preamble takes
static constexpr uint8_t RX_STALL_TIMEOUT = 0xFF;

// Indexed by SX1231Error
PROGMEM_STRING_TABLE(SX1231ErrorStrings, "none", "timeout", "invalid parameters", "radio not ready", "radio busy");

static const LogString *set_mode_failure() { return LOG_STR("Set mode failure"); }
static constexpr uint16_t RAMP[16] PROGMEM = {3400, 2000, 1000, 500, 250, 125, 100, 62, 50, 40, 31, 25, 20, 15, 12, 10};
// FSK width of a RegRxBw code: RxBwMant (16, 20 or 24) in bits 4:3 and RxBwExp in bits 2:0
static uint32_t bandwidth_hz(uint8_t rx_bw) { return FXOSC / ((16 + 4 * (rx_bw >> 3)) << ((rx_bw & 0x07) + 2)); }

static const LogString *shaping_name(uint8_t modulation, uint8_t shaping) {
  if (shaping == SHAPING_NONE) {
    return LOG_STR("NONE");
  }
  if (modulation == MOD_OOK) {
    return shaping == CUTOFF_BR_X_2 ? LOG_STR("CUTOFF_BR_X_2") : LOG_STR("CUTOFF_BR_X_1");
  }
  if (shaping == GAUSSIAN_BT_0_3) {
    return LOG_STR("GAUSSIAN_BT_0_3");
  }
  return shaping == GAUSSIAN_BT_0_5 ? LOG_STR("GAUSSIAN_BT_0_5") : LOG_STR("GAUSSIAN_BT_1_0");
}

uint8_t SX1231::read_register_(uint8_t reg) {
  this->enable();
  this->write_byte(reg & 0x7F);
  uint8_t value = this->read_byte();
  this->disable();
  return value;
}

void SX1231::write_register_(uint8_t reg, uint8_t value) {
  this->enable();
  this->write_byte(reg | 0x80);
  this->write_byte(value);
  this->disable();
}

void SX1231::read_fifo_(std::vector<uint8_t> &packet) {
  this->enable();
  this->write_byte(REG_FIFO & 0x7F);
  this->read_array(packet.data(), packet.size());
  this->disable();
}

// Burst write: the radio moves to the next register after each byte, except for REG_FIFO
void SX1231::write_registers_(uint8_t reg, const uint8_t *data, size_t len) {
  this->enable();
  this->write_byte(reg | 0x80);
  // Not write_array(): in a multi-byte write the FIFO sometimes stores a byte with its last bit cleared
  for (size_t i = 0; i < len; i++) {
    this->write_byte(data[i]);
  }
  this->disable();
}

void SX1231::write_register16_(uint8_t msb_reg, uint16_t value) {
  uint8_t bytes[2];
  put_be16(bytes, value);
  this->write_registers_(msb_reg, bytes, sizeof(bytes));
}

void SX1231::setup() {
  setup_output_pin(this->rst_pin_, false);
  this->spi_setup();

  const LogString *reason = this->configure_radio_();
  if (reason != nullptr) {
    this->mark_failed(reason);
    return;
  }
  // After the radio is known to work: core keeps a loop request from a failed component pending for good
  if (this->dio0_pin_ != nullptr) {
    this->dio0_pin_->setup();
    this->dio0_pin_->attach_interrupt(&SX1231::gpio_intr, this, gpio::INTERRUPT_RISING_EDGE);
  }
  this->state_ = RadioState::RADIO_STATE_IDLE;
  // Sized once so later packet reads do not reallocate
  this->packet_.reserve(SX1231_MAX_PACKET_SIZE);
  this->set_interval(RX_STALL_CHECK_INTERVAL_MS, [this]() { this->restart_stalled_rx_(); });
}

void SX1231::configure() {
  // Nothing to configure before setup() or after a failed setup
  if (!this->is_ready()) {
    return;
  }
  const LogString *reason = this->configure_radio_();
  if (reason != nullptr) {
    this->fail_(reason);
    return;
  }
  this->state_ = RadioState::RADIO_STATE_IDLE;
  if (this->reconfigure_pending_) {
    this->reconfigure_pending_ = false;
    this->cancel_interval(RECONFIGURE_INTERVAL_ID);
  }
  this->status_clear_warning();
  this->enable_loop();
}

void SX1231::fail_(const LogString *reason) {
  this->state_ = RadioState::RADIO_STATE_NEEDS_CONFIGURATION;
  // Nothing else configures the radio again
  if (!this->reconfigure_pending_) {
    this->reconfigure_pending_ = true;
    this->set_interval(RECONFIGURE_INTERVAL_ID, RECONFIGURE_INTERVAL_MS, [this]() { this->configure(); });
  }
  // Last: a log listener may call configure(), which cancels the retry
  this->status_set_warning(reason);
}

const LogString *SX1231::configure_radio_() {
  // Until this succeeds
  this->state_ = RadioState::RADIO_STATE_NEEDS_CONFIGURATION;
  // Codes from 0x18 are reserved, and OOK uses the next exponent down, which a code with exponent 0 does not have
  if (this->bandwidth_ >= 0x18 || (this->modulation_ == MOD_OOK && (this->bandwidth_ & 0x07) == 0)) {
    return LOG_STR("Invalid bandwidth");
  }
  const bool boost = this->pa_pin_ == PA_PIN_BOOST;
  const int8_t pa_power = clamp<int8_t>(this->pa_power_, boost ? -2 : -18, boost ? 20 : 13);
  if (pa_power != this->pa_power_) {
    ESP_LOGW(TAG, "PA power %d dBm is out of range for the PA pin, using %d dBm", this->pa_power_, pa_power);
    this->pa_power_ = pa_power;
  }
  const bool high_power = this->pa_power_ > 17;
  this->applied_ = {this->payload_length_, this->packet_mode_, high_power};

  // Reset is active high
  if (this->rst_pin_ != nullptr) {
    pulse_pin(this->rst_pin_, true, 1000);
    delayMicroseconds(10000);
  }

  const uint8_t version = this->read_register_(REG_VERSION);
  if (version != CHIP_VERSION) {
    ESP_LOGE(TAG, "Chip version 0x%02X, expected 0x%02X", version, CHIP_VERSION);
    return LOG_STR("Unsupported chip version");
  }

  if (!this->set_mode_(MODE_STDBY)) {
    return set_mode_failure();
  }

  uint8_t data_modul = this->modulation_ | this->shaping_;
  if (!this->packet_mode_) {
    data_modul |= this->bitsync_ ? CONTINUOUS_BIT_SYNC_ON : CONTINUOUS_BIT_SYNC_OFF;
  }
  this->write_register_(REG_DATA_MODUL, data_modul);
  uint16_t bitrate = (FXOSC + this->bitrate_ / 2) / this->bitrate_;  // rounded to nearest
  this->write_register16_(REG_BITRATE_MSB, bitrate);
  // Rounded to nearest, so that the lowest deviation allowed is not programmed as less
  uint16_t fdev = clamp_at_most((this->deviation_ * 4096 + 125000) / 250000, uint32_t{0x3FFF});
  this->write_register16_(REG_FDEV_MSB, fdev);

  uint64_t frf = ((uint64_t) this->frequency_ << 19) / FXOSC;
  uint8_t frf_bytes[3];
  put_be24(frf_bytes, static_cast<uint32_t>(frf));
  this->write_registers_(REG_FRF_MSB, frf_bytes, sizeof(frf_bytes));

  uint8_t pa_level = this->pa_pin_ | (this->pa_power_ + 18);
  if (this->pa_power_ > 13) {
    // Both BOOST amplifiers; the +20 dBm settings add another 3 dB
    pa_level = PA_PIN_BOOST | PA_BOOST_COMBINED | (this->pa_power_ + (high_power ? 11 : 14));
  }
  this->write_register_(REG_PA_LEVEL, pa_level);
  this->write_register_(REG_PA_RAMP, this->pa_ramp_);
  this->write_register_(REG_OCP, high_power ? OCP_OFF : OCP_ON_95MA);

  uint8_t rx_bw = this->bandwidth_;
  if (this->modulation_ == MOD_OOK) {
    // Each code is half as wide in OOK, so use the next exponent down
    rx_bw--;
  }
  this->write_register_(REG_LNA, LNA_ZIN_200);
  this->write_register_(REG_RX_BW, DCC_FREQ_4_PERCENT | rx_bw);
  this->write_register_(REG_RSSI_THRESH, std::abs(int(this->rx_floor_ * 2.0f)));
  this->write_register_(REG_RX_TIMEOUT2, RX_STALL_TIMEOUT);
  this->write_register_(REG_TEST_DAGC, DAGC_IMPROVED_MARGIN);
  this->write_register_(REG_DIO_MAPPING2, CLK_OUT_OFF);

  this->write_register16_(REG_PREAMBLE_SIZE_MSB, this->preamble_size_);
  if (!this->sync_value_.empty()) {
    uint8_t size = (this->sync_value_.size() - 1) << SYNC_SIZE_SHIFT;
    this->write_register_(REG_SYNC_CONFIG, SYNC_ON | size);
    this->write_registers_(REG_SYNC_VALUE1, this->sync_value_.data(), this->sync_value_.size());
  } else {
    this->write_register_(REG_SYNC_CONFIG, 0x00);
  }

  uint8_t crc_mode = this->crc_enable_ ? CRC_ON : CRC_OFF;
  if (this->payload_length_ > 0) {
    this->write_register_(REG_PAYLOAD_LENGTH, this->payload_length_);
    this->write_register_(REG_PACKET_CONFIG1, crc_mode | FIXED_LENGTH);
  } else {
    this->write_register_(REG_PAYLOAD_LENGTH, SX1231_MAX_PACKET_SIZE);
    this->write_register_(REG_PACKET_CONFIG1, crc_mode | VARIABLE_LENGTH);
  }
  this->write_register_(REG_FIFO_THRESH, TX_START_FIFO_NOT_EMPTY | FIFO_THRESHOLD_DEFAULT);
  this->write_register_(REG_PACKET_CONFIG2, AUTO_RX_RESTART_ON);
  // Writing this flag empties the FIFO
  this->write_register_(REG_IRQ_FLAGS2, FIFO_OVERRUN);

  if (!this->restore_()) {
    return set_mode_failure();
  }
  return nullptr;
}

size_t SX1231::get_max_packet_size() {
  if (this->applied_.payload_length > 0) {
    return this->applied_.payload_length;
  }
  return SX1231_MAX_PACKET_SIZE;
}

SX1231Error SX1231::transmit_packet(const uint8_t *data, size_t len) {
  SX1231Error result = this->start_transmit_(data, len);
  if (result == SX1231Error::SX1231_ERROR_NONE) {
    const uint32_t start = millis();
    // DIO0 is PacketSent while transmitting
    while (!this->dio0_pin_->digital_read()) {
      if (millis() - start >= TRANSMIT_TIMEOUT_MS) {
        result = SX1231Error::SX1231_ERROR_TIMEOUT;
        break;
      }
      App.feed_wdt();
    }
    if (!this->restore_()) {
      this->fail_(set_mode_failure());
    }
  }
  if (result != SX1231Error::SX1231_ERROR_NONE) {
    ESP_LOGW(TAG, "Packet send failed: %s", LOG_STR_ARG(SX1231ErrorStrings::get_log_str(result)));
  }
  return result;
}

SX1231Error SX1231::start_transmit_(const uint8_t *data, size_t len) {
  // Callers such as a template number's on_value can run before setup() or after a failed setup
  if (!this->is_ready() || this->state_ == RadioState::RADIO_STATE_NEEDS_CONFIGURATION) {
    return SX1231Error::SX1231_ERROR_NOT_READY;
  }
  if (!this->applied_.packet_mode || data == nullptr || len == 0 || len > this->get_max_packet_size() ||
      (this->applied_.payload_length > 0 && this->applied_.payload_length != len)) {
    return SX1231Error::SX1231_ERROR_INVALID_PARAMS;
  }
  if (this->dio0_pin_ == nullptr) {
    ESP_LOGE(TAG, "DIO0 pin not configured, cannot wait for transmit completion");
    return SX1231Error::SX1231_ERROR_INVALID_PARAMS;
  }

  // Checked in receive mode, as the chip clears this flag on leaving it; the packet stays in the FIFO for loop()
  if (this->packet_waiting_()) {
    return SX1231Error::SX1231_ERROR_BUSY;
  }
  if (!this->set_mode_(MODE_STDBY)) {
    this->fail_(set_mode_failure());
    return SX1231Error::SX1231_ERROR_NOT_READY;
  }
  // Drop any part of a packet that was still arriving
  this->write_register_(REG_IRQ_FLAGS2, FIFO_OVERRUN);
  if (this->applied_.payload_length == 0) {
    this->write_register_(REG_FIFO, len);
  }
  this->write_registers_(REG_FIFO, data, len);
  if (!this->set_mode_(MODE_TX)) {
    this->fail_(set_mode_failure());
    return SX1231Error::SX1231_ERROR_NOT_READY;
  }
  return SX1231Error::SX1231_ERROR_NONE;
}

bool SX1231::restore_() {
  // Via standby, which ends a send and clears PacketSent
  if (!this->set_mode_(MODE_STDBY)) {
    return false;
  }
  return this->set_mode_(this->mode_);
}

bool SX1231::is_receiving() {
  if (this->state_ != RadioState::RADIO_STATE_IDLE) {
    return false;
  }
  // Set from the sync word until the packet has been read, and cleared on leaving receive mode
  if ((this->read_register_(REG_IRQ_FLAGS1) & SYNC_ADDRESS_MATCH) != 0) {
    return true;
  }
  // Without a sync word, only a received packet that loop() has not read yet shows
  return this->packet_waiting_();
}

bool SX1231::packet_waiting_() {
  return this->applied_.packet_mode && (this->read_register_(REG_IRQ_FLAGS2) & PAYLOAD_READY) != 0;
}

void SX1231::call_listeners_(const std::vector<uint8_t> &packet, float rssi, float snr) {
  for (auto &listener : this->listeners_) {
    listener->on_packet(packet, rssi, snr);
  }
  this->packet_trigger_.trigger(packet, rssi, snr);
}

void SX1231::loop() {
  // DIO0's interrupt and configure() wake the loop
  this->disable_loop();
  if (this->state_ == RadioState::RADIO_STATE_IDLE && this->dio0_pin_ != nullptr && this->dio0_pin_->digital_read()) {
    this->receive_();
  }
}

void SX1231::receive_() {
  if (!this->packet_waiting_()) {
    return;
  }
  // Read first: emptying the FIFO restarts the receiver, which then measures again
  const float rssi = this->read_register_(REG_RSSI_VALUE) / -2.0f;
  uint8_t payload_length = this->applied_.payload_length;
  if (payload_length == 0) {
    payload_length = this->read_register_(REG_FIFO);
  }
  if (payload_length == 0 || payload_length > SX1231_MAX_PACKET_SIZE) {
    ESP_LOGW(TAG, "Dropping packet with invalid length %u", payload_length);
    // Empties the FIFO, which restarts the receiver
    this->write_register_(REG_IRQ_FLAGS2, FIFO_OVERRUN);
    return;
  }
  this->packet_.resize(payload_length);
  this->read_fifo_(this->packet_);
  this->call_listeners_(this->packet_, rssi, NAN);
}

// A signal above the RX floor that no sync word follows leaves the receiver waiting, with its gain set for that
// signal, so weaker packets are then missed. The chip flags this but does not recover from it.
void SX1231::restart_stalled_rx_() {
  if (this->state_ != RadioState::RADIO_STATE_IDLE || !this->applied_.packet_mode) {
    return;
  }
  const uint8_t flags = this->read_register_(REG_IRQ_FLAGS1);
  if ((flags & TIMEOUT) != 0 && (flags & SYNC_ADDRESS_MATCH) == 0) {
    ESP_LOGV(TAG, "Restarting stalled receiver");
    this->write_register_(REG_PACKET_CONFIG2, AUTO_RX_RESTART_ON | RESTART_RX);
  }
}

bool SX1231::set_mode_(uint8_t mode) {
  // DIO0 is PayloadReady while receiving and PacketSent while transmitting
  if (mode == MODE_RX) {
    this->write_register_(REG_DIO_MAPPING1, DIO0_MAPPING_01);
  } else if (mode == MODE_TX) {
    this->write_register_(REG_DIO_MAPPING1, DIO0_MAPPING_00);
  }
  // The +20 dBm settings must be off except while sending
  const bool high_power = mode == MODE_TX && this->applied_.high_power;
  this->write_register_(REG_TEST_PA1, high_power ? TEST_PA1_HIGH_POWER : TEST_PA1_NORMAL);
  this->write_register_(REG_TEST_PA2, high_power ? TEST_PA2_HIGH_POWER : TEST_PA2_NORMAL);
  uint32_t start = millis();
  this->write_register_(REG_OP_MODE, mode);
  while ((this->read_register_(REG_IRQ_FLAGS1) & MODE_READY) == 0) {
    if (millis() - start > MODE_TIMEOUT_MS) {
      return false;
    }
  }
  return true;
}

void SX1231::change_mode_(uint8_t mode) {
  if (this->state_ != RadioState::RADIO_STATE_IDLE) {
    ESP_LOGW(TAG, "Radio unavailable, mode change ignored");
    return;
  }
  this->mode_ = mode;
  if (!this->set_mode_(mode)) {
    this->fail_(set_mode_failure());
  }
}

void SX1231::set_mode_rx() { this->change_mode_(MODE_RX); }

void SX1231::set_mode_tx() { this->change_mode_(MODE_TX); }

void SX1231::set_mode_standby() { this->change_mode_(MODE_STDBY); }

void SX1231::set_mode_sleep() { this->change_mode_(MODE_SLEEP); }

void SX1231::dump_config() {
  ESP_LOGCONFIG(TAG, "SX1231:");
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_PIN("  RST Pin: ", this->rst_pin_);
  LOG_PIN("  DIO0 Pin: ", this->dio0_pin_);
  ESP_LOGCONFIG(TAG,
                "  Frequency: %" PRIu32 " Hz\n"
                "  Bandwidth: %" PRIu32 " Hz\n"
                "  PA Pin: %s\n"
                "  PA Power: %" PRId8 " dBm\n"
                "  PA Ramp: %" PRIu16 " us\n"
                "  Shaping: %s",
                this->frequency_, bandwidth_hz(this->bandwidth_),
                this->pa_pin_ == PA_PIN_BOOST ? LOG_STR_LITERAL("BOOST") : LOG_STR_LITERAL("RFO"), this->pa_power_,
                progmem_read_uint16(&RAMP[this->pa_ramp_]),
                LOG_STR_ARG(shaping_name(this->modulation_, this->shaping_)));
  if (this->modulation_ == MOD_FSK) {
    ESP_LOGCONFIG(TAG, "  Deviation: %" PRIu32 " Hz", this->deviation_);
  }
  ESP_LOGCONFIG(TAG,
                "  Modulation: %s\n"
                "  Bitrate: %" PRIu32 "b/s\n"
                "  Bitsync: %s\n"
                "  Rx Start: %s\n"
                "  Rx Floor: %.1f dBm\n"
                "  Packet Mode: %s",
                this->modulation_ == MOD_FSK ? LOG_STR_LITERAL("FSK") : LOG_STR_LITERAL("OOK"), this->bitrate_,
                TRUEFALSE(this->packet_mode_ || this->bitsync_), TRUEFALSE(this->rx_start_), this->rx_floor_,
                TRUEFALSE(this->packet_mode_));
  if (this->packet_mode_) {
    ESP_LOGCONFIG(TAG, "  CRC Enable: %s", TRUEFALSE(this->crc_enable_));
  }
  if (this->payload_length_ > 0) {
    ESP_LOGCONFIG(TAG, "  Payload Length: %" PRIu8, this->payload_length_);
  }
  if (!this->sync_value_.empty()) {
    char hex_buf[SX1231_MAX_SYNC_SIZE * 2 + 1];
    ESP_LOGCONFIG(TAG, "  Sync Value: 0x%s",
                  format_hex_to(hex_buf, this->sync_value_.data(), this->sync_value_.size()));
  }
  if (this->preamble_size_ > 0) {
    ESP_LOGCONFIG(TAG, "  Preamble Size: %" PRIu16, this->preamble_size_);
  }
}

}  // namespace esphome::sx1231
