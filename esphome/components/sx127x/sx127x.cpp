#include "sx127x.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"
#include <cmath>
#include <utility>

namespace esphome::sx127x {

ESPHOME_LOG_TAG(TAG, "sx127x");
static const uint32_t FXOSC = 32000000u;
static constexpr uint32_t TRANSMIT_TIMEOUT_MS = 4000;
static constexpr uint32_t MAX_TRANSMIT_TIMEOUT_MS = 262143;  // the same range as SX126x
static constexpr uint32_t MODE_TIMEOUT_MS = 20;
static constexpr uint32_t RECONFIGURE_INTERVAL_ID = 0;
static constexpr uint32_t RECONFIGURE_INTERVAL_MS = 1000;

// Indexed by SX127xError
PROGMEM_STRING_TABLE(SX127xErrorStrings, "none", "timeout", "invalid parameters", "radio not ready", "radio busy",
                     "cancelled");

static const LogString *set_mode_failure() { return LOG_STR("Set mode failure"); }
static constexpr uint16_t RAMP[16] PROGMEM = {3400, 2000, 1000, 500, 250, 125, 100, 62, 50, 40, 31, 25, 20, 15, 12, 10};
// Aligned 32 bit loads from flash are safe on ESP8266, so this table is read directly
static constexpr uint32_t BW_HZ[22] PROGMEM = {2604,   3125,   3906,   5208,   6250,   7812,  10416, 12500,
                                               15625,  20833,  25000,  31250,  41666,  50000, 62500, 83333,
                                               100000, 125000, 166666, 200000, 250000, 500000};
static constexpr uint8_t BW_LORA[22] PROGMEM = {
    BW_7_8,  BW_7_8,  BW_7_8,  BW_7_8,  BW_7_8,   BW_7_8,   BW_10_4,  BW_15_6,  BW_15_6,  BW_20_8,  BW_31_3,
    BW_31_3, BW_41_7, BW_62_5, BW_62_5, BW_125_0, BW_125_0, BW_125_0, BW_250_0, BW_250_0, BW_250_0, BW_500_0};
static constexpr uint8_t BW_FSK_OOK[22] PROGMEM = {
    RX_BW_2_6,   RX_BW_3_1,   RX_BW_3_9,   RX_BW_5_2,   RX_BW_6_3,   RX_BW_7_8,  RX_BW_10_4, RX_BW_12_5,
    RX_BW_15_6,  RX_BW_20_8,  RX_BW_25_0,  RX_BW_31_3,  RX_BW_41_7,  RX_BW_50_0, RX_BW_62_5, RX_BW_83_3,
    RX_BW_100_0, RX_BW_125_0, RX_BW_166_7, RX_BW_200_0, RX_BW_250_0, RX_BW_250_0};

static const int32_t RSSI_OFFSET_HF = 157;
static const int32_t RSSI_OFFSET_LF = 164;

uint8_t SX127x::read_register_(uint8_t reg) {
  this->enable();
  this->write_byte(reg & 0x7F);
  uint8_t value = this->read_byte();
  this->disable();
  return value;
}

void SX127x::write_register_(uint8_t reg, uint8_t value) {
  this->enable();
  this->write_byte(reg | 0x80);
  this->write_byte(value);
  this->disable();
}

void SX127x::read_fifo_(std::vector<uint8_t> &packet) {
  this->enable();
  this->write_byte(REG_FIFO & 0x7F);
  for (auto &byte : packet) {
    byte = this->transfer_byte(0x00);
  }
  this->disable();
}

// Burst write: the radio moves to the next register after each byte, except for REG_FIFO
void SX127x::write_registers_(uint8_t reg, const uint8_t *data, size_t len) {
  this->enable();
  this->write_byte(reg | 0x80);
  this->write_array(data, len);
  this->disable();
}

void SX127x::write_register16_(uint8_t msb_reg, uint16_t value) {
  uint8_t bytes[2];
  put_be16(bytes, value);
  this->write_registers_(msb_reg, bytes, sizeof(bytes));
}

void SX127x::setup() {
  // setup reset
  this->rst_pin_->setup();

  // setup dio0
  if (this->dio0_pin_ != nullptr) {
    this->dio0_pin_->setup();
    if (this->dio0_pin_->is_internal()) {
      static_cast<InternalGPIOPin *>(this->dio0_pin_)
          ->attach_interrupt(&SX127x::gpio_intr, this, gpio::INTERRUPT_RISING_EDGE);
    }
  }

  // start spi
  this->spi_setup();

  // configure rf
  const LogString *reason = this->configure_radio_();
  if (reason != nullptr) {
    this->mark_failed(reason);
    return;
  }
  this->state_ = RadioState::RADIO_STATE_IDLE;
  // Sized once so later packet reads do not reallocate
  this->packet_.reserve(this->get_max_packet_size());
}

void SX127x::configure() {
  // Nothing to configure before setup() or after a failed setup
  if (!this->is_ready()) {
    return;
  }
  if (this->is_transmitting()) {
    ESP_LOGW(TAG, "Radio operation in progress, configuration ignored");
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

void SX127x::fail_(const LogString *reason) {
  this->state_ = RadioState::RADIO_STATE_NEEDS_CONFIGURATION;
  // Nothing else configures the radio again
  if (!this->reconfigure_pending_) {
    this->reconfigure_pending_ = true;
    this->set_interval(RECONFIGURE_INTERVAL_ID, RECONFIGURE_INTERVAL_MS, [this]() { this->configure(); });
  }
  // Last: a log listener may call configure(), which cancels the retry
  this->status_set_warning(reason);
}

const LogString *SX127x::configure_radio_() {
  // Until this succeeds; a held packet is discarded
  this->state_ = RadioState::RADIO_STATE_NEEDS_CONFIGURATION;
  this->applied_ = {this->frequency_, this->payload_length_, this->modulation_, this->packet_mode_, this->rx_start_};

  // toggle chip reset
  this->rst_pin_->digital_write(false);
  delayMicroseconds(1000);
  this->rst_pin_->digital_write(true);
  delayMicroseconds(10000);

  // check silicon version to make sure hw is ok
  if (this->read_register_(REG_VERSION) != 0x12) {
    return LOG_STR("Unsupported chip version");
  }

  // enter sleep mode
  if (!this->set_mode_(MOD_FSK, MODE_SLEEP)) {
    return set_mode_failure();
  }

  // set freq
  uint64_t frf = ((uint64_t) this->frequency_ << 19) / FXOSC;
  uint8_t frf_bytes[3];
  put_be24(frf_bytes, static_cast<uint32_t>(frf));
  this->write_registers_(REG_FRF_MSB, frf_bytes, sizeof(frf_bytes));

  // enter standby mode
  if (!this->set_mode_(MOD_FSK, MODE_STDBY)) {
    return set_mode_failure();
  }

  // run image cal
  const LogString *reason = this->image_cal_();
  if (reason != nullptr) {
    return reason;
  }

  // go back to sleep
  if (!this->set_mode_(this->modulation_, MODE_SLEEP)) {
    return set_mode_failure();
  }

  // config pa
  if (this->pa_pin_ == PA_PIN_BOOST) {
    this->pa_power_ = clamp(this->pa_power_, (uint8_t) 2, (uint8_t) 17);
    this->write_register_(REG_PA_CONFIG, (this->pa_power_ - 2) | this->pa_pin_ | PA_MAX_POWER);
  } else {
    this->pa_power_ = clamp_at_most(this->pa_power_, (uint8_t) 14);
    this->write_register_(REG_PA_CONFIG, (this->pa_power_ - 0) | this->pa_pin_ | PA_MAX_POWER);
  }
  if (this->modulation_ != MOD_LORA) {
    this->write_register_(REG_PA_RAMP, this->pa_ramp_ | this->shaping_);
  } else {
    this->write_register_(REG_PA_RAMP, this->pa_ramp_);
  }

  // configure modem
  if (this->modulation_ != MOD_LORA) {
    this->configure_fsk_ook_();
  } else {
    this->configure_lora_();
  }

  // switch to rx or sleep
  if (!this->restore_()) {
    return set_mode_failure();
  }
  return nullptr;
}

void SX127x::configure_fsk_ook_() {
  // set the channel bw
  this->write_register_(REG_RX_BW, progmem_read_byte(&BW_FSK_OOK[this->bandwidth_]));

  // set fdev
  uint16_t fdev = clamp_at_most((this->deviation_ * 4096) / 250000, uint32_t{0x3FFF});
  this->write_register16_(REG_FDEV_MSB, fdev);

  // set bitrate
  uint16_t bitrate = (FXOSC + this->bitrate_ / 2) / this->bitrate_;  // round up
  this->write_register16_(REG_BITRATE_MSB, bitrate);

  // configure rx and afc
  this->write_register_(REG_AFC_BW, progmem_read_byte(&BW_FSK_OOK[this->afc_bandwidth_]));
  uint8_t trigger = (this->preamble_detect_ > 0) ? TRIGGER_PREAMBLE : TRIGGER_RSSI;
  this->write_register_(REG_AFC_FEI, AFC_AUTO_CLEAR_ON);
  if (this->modulation_ == MOD_FSK) {
    this->write_register_(REG_RX_CONFIG, AFC_AUTO_ON | AGC_AUTO_ON | trigger);
  } else {
    this->write_register_(REG_RX_CONFIG, AGC_AUTO_ON | trigger);
  }

  // configure packet mode
  if (this->packet_mode_) {
    uint8_t crc_mode = (this->crc_enable_) ? CRC_ON : CRC_OFF;
    this->write_register_(REG_FIFO_THRESH, TX_START_FIFO_EMPTY);
    if (this->payload_length_ > 0) {
      this->write_register_(REG_PAYLOAD_LENGTH_LSB, this->payload_length_);
      this->write_register_(REG_PACKET_CONFIG_1, crc_mode | FIXED_LENGTH);
    } else {
      this->write_register_(REG_PAYLOAD_LENGTH_LSB, this->get_max_packet_size() - 1);
      this->write_register_(REG_PACKET_CONFIG_1, crc_mode | VARIABLE_LENGTH);
    }
    this->write_register_(REG_PACKET_CONFIG_2, PACKET_MODE);
  } else {
    this->write_register_(REG_PACKET_CONFIG_2, CONTINUOUS_MODE);
  }
  this->write_register_(REG_DIO_MAPPING1, DIO0_MAPPING_00);

  // config bit synchronizer
  uint8_t polarity = (this->preamble_polarity_ == 0xAA) ? PREAMBLE_AA : PREAMBLE_55;
  if (!this->sync_value_.empty()) {
    uint8_t size = this->sync_value_.size() - 1;
    this->write_register_(REG_SYNC_CONFIG, AUTO_RESTART_PLL_LOCK | polarity | SYNC_ON | size);
    for (uint32_t i = 0; i < this->sync_value_.size(); i++) {
      this->write_register_(REG_SYNC_VALUE1 + i, this->sync_value_[i]);
    }
  } else {
    this->write_register_(REG_SYNC_CONFIG, AUTO_RESTART_PLL_LOCK | polarity);
  }

  // config preamble detector
  if (this->preamble_detect_ > 0) {
    uint8_t size = (this->preamble_detect_ - 1) << PREAMBLE_DETECTOR_SIZE_SHIFT;
    uint8_t tol = this->preamble_errors_ << PREAMBLE_DETECTOR_TOL_SHIFT;
    this->write_register_(REG_PREAMBLE_DETECT, PREAMBLE_DETECTOR_ON | size | tol);
  } else {
    this->write_register_(REG_PREAMBLE_DETECT, PREAMBLE_DETECTOR_OFF);
  }
  this->write_register16_(REG_PREAMBLE_SIZE_MSB, this->preamble_size_);

  // config sync generation and setup ook threshold
  uint8_t bitsync = this->bitsync_ ? BIT_SYNC_ON : BIT_SYNC_OFF;
  this->write_register_(REG_OOK_PEAK, bitsync | OOK_THRESH_STEP_0_5 | OOK_THRESH_PEAK);
  this->write_register_(REG_OOK_AVG, OOK_AVG_RESERVED | OOK_THRESH_DEC_1_8);

  // set rx floor
  this->write_register_(REG_OOK_FIX, 256 + int(this->rx_floor_ * 2.0f));
  this->write_register_(REG_RSSI_THRESH, std::abs(int(this->rx_floor_ * 2.0f)));
}

void SX127x::configure_lora_() {
  // config modem
  uint8_t header_mode = this->payload_length_ > 0 ? IMPLICIT_HEADER : EXPLICIT_HEADER;
  uint8_t crc_mode = (this->crc_enable_) ? RX_PAYLOAD_CRC_ON : RX_PAYLOAD_CRC_OFF;
  uint8_t spreading_factor = this->spreading_factor_ << SPREADING_FACTOR_SHIFT;
  this->write_register_(REG_MODEM_CONFIG1,
                        progmem_read_byte(&BW_LORA[this->bandwidth_]) | this->coding_rate_ | header_mode);
  this->write_register_(REG_MODEM_CONFIG2, spreading_factor | crc_mode);

  // config fifo and payload length
  this->write_register_(REG_FIFO_TX_BASE_ADDR, 0x00);
  this->write_register_(REG_FIFO_RX_BASE_ADDR, 0x00);
  this->write_register_(REG_PAYLOAD_LENGTH, std::max(this->payload_length_, (uint32_t) 1));

  // config preamble
  if (this->preamble_size_ >= 6) {
    this->write_register16_(REG_PREAMBLE_LEN_MSB, this->preamble_size_);
  }

  // optimize detection
  float duration = 1000.0f * (1UL << this->spreading_factor_) / BW_HZ[this->bandwidth_];
  if (duration > 16) {
    this->write_register_(REG_MODEM_CONFIG3, MODEM_AGC_AUTO_ON | LOW_DATA_RATE_OPTIMIZE_ON);
  } else {
    this->write_register_(REG_MODEM_CONFIG3, MODEM_AGC_AUTO_ON);
  }
  if (this->spreading_factor_ == 6) {
    this->write_register_(REG_DETECT_OPTIMIZE, 0xC5);
    this->write_register_(REG_DETECT_THRESHOLD, 0x0C);
  } else {
    this->write_register_(REG_DETECT_OPTIMIZE, 0xC3);
    this->write_register_(REG_DETECT_THRESHOLD, 0x0A);
  }

  // config sync word
  if (!this->sync_value_.empty()) {
    this->write_register_(REG_SYNC_WORD, this->sync_value_[0]);
  }
}

size_t SX127x::get_max_packet_size() {
  if (this->applied_.payload_length > 0) {
    return this->applied_.payload_length;
  }
  if (this->applied_.modulation == MOD_LORA) {
    return SX127X_MAX_PACKET_SIZE;
  }
  return 64;
}

SX127xError SX127x::transmit_packet(const uint8_t *data, size_t len) {
  SX127xError result = this->start_transmit_(nullptr, data, len, TRANSMIT_TIMEOUT_MS);
  if (result == SX127xError::SX127X_ERROR_NONE) {
    // Without a listener this send cannot be cancelled or replaced
    result = SX127xError::SX127X_ERROR_NOT_READY;
    while (this->is_transmitting() && !this->poll_transmit_(millis(), result)) {
      App.feed_wdt();
    }
  }
  if (result != SX127xError::SX127X_ERROR_NONE) {
    ESP_LOGW(
        TAG, "Packet send failed: %s",
        LOG_STR_ARG(SX127xErrorStrings::get_log_str(static_cast<uint8_t>(result), SX127xErrorStrings::LAST_INDEX)));
  }
  return result;
}

SX127xError SX127x::start_transmit_async(SX127xTransmitListener *listener, const uint8_t *data, size_t len,
                                         uint32_t timeout_ms) {
  if (listener == nullptr || timeout_ms == 0 || timeout_ms > MAX_TRANSMIT_TIMEOUT_MS) {
    return SX127xError::SX127X_ERROR_INVALID_PARAMS;
  }
  return this->start_transmit_(listener, data, len, timeout_ms);
}

SX127xError SX127x::start_transmit_(SX127xTransmitListener *listener, const uint8_t *data, size_t len,
                                    uint32_t timeout_ms) {
  // Callers such as a template number's on_value can run before setup() or after a failed setup
  if (!this->is_ready() || this->state_ == RadioState::RADIO_STATE_NEEDS_CONFIGURATION) {
    return SX127xError::SX127X_ERROR_NOT_READY;
  }
  if (this->state_ != RadioState::RADIO_STATE_IDLE) {
    return SX127xError::SX127X_ERROR_BUSY;
  }
  if (data == nullptr || len == 0 || len > this->get_max_packet_size() ||
      (this->applied_.payload_length > 0 && this->applied_.payload_length != len)) {
    return SX127xError::SX127X_ERROR_INVALID_PARAMS;
  }
  if (this->dio0_pin_ == nullptr) {
    ESP_LOGE(TAG, "DIO0 pin not configured, cannot wait for transmit completion");
    return SX127xError::SX127X_ERROR_INVALID_PARAMS;
  }

  if (!this->set_mode_(this->applied_.modulation, MODE_STDBY)) {
    this->fail_(set_mode_failure());
    return SX127xError::SX127X_ERROR_NOT_READY;
  }
  // A packet received before standby stays in the FIFO for loop()
  if (this->packet_waiting_()) {
    this->state_ = RadioState::RADIO_STATE_HOLDING_RX;
    this->enable_loop();
    return SX127xError::SX127X_ERROR_BUSY;
  }
  if (this->applied_.modulation == MOD_LORA) {
    if (this->applied_.payload_length == 0) {
      this->write_register_(REG_PAYLOAD_LENGTH, len);
    }
    this->write_register_(REG_IRQ_FLAGS, 0xFF);
    this->write_register_(REG_FIFO_ADDR_PTR, 0);
  } else if (this->applied_.payload_length == 0) {
    this->write_register_(REG_FIFO, len);
  }
  this->write_registers_(REG_FIFO, data, len);
  if (!this->enter_tx_()) {
    this->fail_(set_mode_failure());
    return SX127xError::SX127X_ERROR_NOT_READY;
  }
  this->tx_ = {listener, millis(), timeout_ms};
  this->state_ = RadioState::RADIO_STATE_TRANSMITTING;
  this->enable_loop();
  return SX127xError::SX127X_ERROR_NONE;
}

bool SX127x::poll_transmit_(uint32_t now, SX127xError &result) {
  if (!this->is_transmitting()) {
    return false;
  }
  // DIO0 is TX_DONE in LoRa, and PacketSent in FSK and OOK while transmitting
  if (this->dio0_pin_->digital_read()) {
    result = SX127xError::SX127X_ERROR_NONE;
  } else if (static_cast<int32_t>(now - this->tx_.started_ms) >= static_cast<int32_t>(this->tx_.timeout_ms)) {
    // Signed, as the cached loop time can precede the start
    result = SX127xError::SX127X_ERROR_TIMEOUT;
  } else {
    return false;
  }
  result = this->finish_transmit_(result);
  return true;
}

bool SX127x::cancel_transmit_async(SX127xTransmitListener *listener) {
  if (!this->is_transmitting() || listener == nullptr || listener != this->tx_.listener) {
    return false;
  }
  this->finish_transmit_(SX127xError::SX127X_ERROR_CANCELLED);
  return true;
}

// The only place a send ends; state is committed before the listener runs
SX127xError SX127x::finish_transmit_(SX127xError result) {
  SX127xTransmitListener *listener = std::exchange(this->tx_.listener, nullptr);
  if (this->restore_()) {
    this->state_ = RadioState::RADIO_STATE_IDLE;
  } else {
    this->fail_(set_mode_failure());
  }
  if (listener != nullptr) {
    listener->on_transmit_complete(result);
  }
  return result;
}

bool SX127x::restore_() {
  // Via standby: the chip ignores RX while a packet is still going out
  if (!this->set_mode_(this->applied_.modulation, MODE_STDBY)) {
    return false;
  }
  if (this->applied_.modulation == MOD_LORA) {
    this->write_register_(REG_IRQ_FLAGS, 0xFF);
  }
  return this->applied_.rx_start ? this->enter_rx_() : this->set_mode_(this->applied_.modulation, MODE_SLEEP);
}

bool SX127x::packet_waiting_() {
  if (this->applied_.modulation == MOD_LORA) {
    const uint8_t flags = this->read_register_(REG_IRQ_FLAGS);
    return (flags & RX_DONE) != 0 && (flags & PAYLOAD_CRC_ERROR) == 0;
  }
  return this->applied_.packet_mode && (this->read_register_(REG_IRQ_FLAGS2) & PAYLOAD_READY) != 0;
}

void SX127x::call_listeners_(const std::vector<uint8_t> &packet, float rssi, float snr) {
  for (auto &listener : this->listeners_) {
    listener->on_packet(packet, rssi, snr);
  }
  this->packet_trigger_.trigger(packet, rssi, snr);
}

void SX127x::loop() {
  switch (this->state_) {
    case RadioState::RADIO_STATE_TRANSMITTING: {
      SX127xError result;
      this->poll_transmit_(App.get_loop_component_start_time(), result);
      return;
    }
    case RadioState::RADIO_STATE_HOLDING_RX:
      this->receive_();
      return;
    case RadioState::RADIO_STATE_IDLE:
      if (this->dio0_pin_ == nullptr) {
        this->disable_loop();
        return;
      }
      // DIO0's interrupt wakes the loop
      if (this->dio0_pin_->is_internal()) {
        this->disable_loop();
      }
      if (this->dio0_pin_->digital_read()) {
        this->receive_();
      }
      return;
    default:
      // configure() wakes the loop
      this->disable_loop();
      return;
  }
}

void SX127x::receive_() {
  const bool holding = this->state_ == RadioState::RADIO_STATE_HOLDING_RX;
  bool received = false;
  float rssi = 0.0f;
  float snr = NAN;  // FSK and OOK have no SNR
  if (this->applied_.modulation == MOD_LORA) {
    uint8_t status = this->read_register_(REG_IRQ_FLAGS);
    this->write_register_(REG_IRQ_FLAGS, 0xFF);
    if ((status & RX_DONE) != 0 && (status & PAYLOAD_CRC_ERROR) == 0) {
      uint8_t bytes = this->read_register_(REG_NB_RX_BYTES);
      uint8_t addr = this->read_register_(REG_FIFO_RX_CURR_ADDR);
      uint8_t raw_rssi = this->read_register_(REG_PKT_RSSI_VALUE);
      int8_t raw_snr = (int8_t) this->read_register_(REG_PKT_SNR_VALUE);
      this->packet_.resize(bytes);
      this->write_register_(REG_FIFO_ADDR_PTR, addr);
      this->read_fifo_(this->packet_);
      rssi = (float) raw_rssi - (this->applied_.frequency > 700000000 ? RSSI_OFFSET_HF : RSSI_OFFSET_LF);
      snr = (float) raw_snr / 4;
      received = true;
    }
  } else if (this->applied_.packet_mode && (this->read_register_(REG_IRQ_FLAGS2) & PAYLOAD_READY) != 0) {
    uint8_t payload_length = this->applied_.payload_length;
    if (payload_length == 0) {
      payload_length = this->read_register_(REG_FIFO);
    }
    this->packet_.resize(payload_length);
    this->read_fifo_(this->packet_);
    received = true;
  }
  if (holding) {
    if (this->restore_()) {
      this->state_ = RadioState::RADIO_STATE_IDLE;
    } else {
      this->fail_(set_mode_failure());
    }
  }
  if (received) {
    this->call_listeners_(this->packet_, rssi, snr);
  }
}

const LogString *SX127x::image_cal_() {
  if (this->applied_.modulation == MOD_LORA) {
    if (!this->set_mode_(MOD_FSK, MODE_SLEEP) || !this->set_mode_(MOD_FSK, MODE_STDBY)) {
      return set_mode_failure();
    }
  }
  if (this->auto_cal_) {
    this->write_register_(REG_IMAGE_CAL, IMAGE_CAL_START | AUTO_IMAGE_CAL_ON | TEMP_THRESHOLD_10C);
  } else {
    this->write_register_(REG_IMAGE_CAL, IMAGE_CAL_START);
  }
  uint32_t start = millis();
  while (this->read_register_(REG_IMAGE_CAL) & IMAGE_CAL_RUNNING) {
    if (millis() - start > 20) {
      return LOG_STR("Image cal failure");
    }
  }
  if (this->applied_.modulation == MOD_LORA) {
    if (!this->set_mode_(this->applied_.modulation, MODE_SLEEP) ||
        !this->set_mode_(this->applied_.modulation, MODE_STDBY)) {
      return set_mode_failure();
    }
  }
  return nullptr;
}

void SX127x::run_image_cal() {
  if (!this->can_change_mode_()) {
    return;
  }
  const LogString *reason = this->image_cal_();
  if (reason != nullptr) {
    this->fail_(reason);
  }
}

bool SX127x::set_mode_(uint8_t modulation, uint8_t mode) {
  uint32_t start = millis();
  this->write_register_(REG_OP_MODE, modulation | mode);
  while (true) {
    uint8_t curr = this->read_register_(REG_OP_MODE) & MODE_MASK;
    if ((curr == mode) || (mode == MODE_RX && curr == MODE_RX_FS)) {
      if (mode == MODE_SLEEP) {
        this->write_register_(REG_OP_MODE, modulation | mode);
      }
      return true;
    }
    if (millis() - start > MODE_TIMEOUT_MS) {
      return false;
    }
  }
}

bool SX127x::enter_rx_() {
  if (this->applied_.modulation == MOD_LORA) {
    this->write_register_(REG_IRQ_FLAGS_MASK, 0x00);
    this->write_register_(REG_DIO_MAPPING1, DIO0_MAPPING_00);
  }
  return this->set_mode_(this->applied_.modulation, MODE_RX);
}

bool SX127x::enter_tx_() {
  if (this->applied_.modulation == MOD_LORA) {
    this->write_register_(REG_IRQ_FLAGS_MASK, 0x00);
    this->write_register_(REG_DIO_MAPPING1, DIO0_MAPPING_01);
  }
  return this->set_mode_(this->applied_.modulation, MODE_TX);
}

bool SX127x::can_change_mode_() {
  if (this->state_ == RadioState::RADIO_STATE_IDLE) {
    return true;
  }
  ESP_LOGW(TAG, "Radio unavailable, mode change ignored");
  return false;
}

void SX127x::change_mode_(uint8_t mode) {
  if (!this->can_change_mode_()) {
    return;
  }
  bool ok;
  if (mode == MODE_RX) {
    ok = this->enter_rx_();
  } else if (mode == MODE_TX) {
    ok = this->enter_tx_();
  } else {
    ok = this->set_mode_(this->applied_.modulation, mode);
  }
  if (!ok) {
    this->fail_(set_mode_failure());
  }
}

void SX127x::set_mode_rx() { this->change_mode_(MODE_RX); }

void SX127x::set_mode_tx() { this->change_mode_(MODE_TX); }

void SX127x::set_mode_standby() { this->change_mode_(MODE_STDBY); }

void SX127x::set_mode_sleep() { this->change_mode_(MODE_SLEEP); }

void SX127x::dump_config() {
  ESP_LOGCONFIG(TAG, "SX127x:");
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_PIN("  RST Pin: ", this->rst_pin_);
  LOG_PIN("  DIO0 Pin: ", this->dio0_pin_);
  const char *pa_pin = "RFO";
  if (this->pa_pin_ == PA_PIN_BOOST) {
    pa_pin = "BOOST";
  }
  ESP_LOGCONFIG(TAG,
                "  Auto Cal: %s\n"
                "  Frequency: %" PRIu32 " Hz\n"
                "  Bandwidth: %" PRIu32 " Hz\n"
                "  PA Pin: %s\n"
                "  PA Power: %" PRIu8 " dBm\n"
                "  PA Ramp: %" PRIu16 " us",
                TRUEFALSE(this->auto_cal_), this->frequency_, BW_HZ[this->bandwidth_], pa_pin, this->pa_power_,
                progmem_read_uint16(&RAMP[this->pa_ramp_]));
  if (this->modulation_ == MOD_FSK) {
    ESP_LOGCONFIG(TAG,
                  "  Deviation: %" PRIu32 " Hz\n"
                  "  AFC Bandwidth: %" PRIu32 " Hz",
                  this->deviation_, BW_HZ[this->afc_bandwidth_]);
  }
  if (this->modulation_ == MOD_LORA) {
    const char *cr = "4/8";
    if (this->coding_rate_ == CODING_RATE_4_5) {
      cr = "4/5";
    } else if (this->coding_rate_ == CODING_RATE_4_6) {
      cr = "4/6";
    } else if (this->coding_rate_ == CODING_RATE_4_7) {
      cr = "4/7";
    }
    ESP_LOGCONFIG(TAG,
                  "  Modulation: LORA\n"
                  "  Preamble Size: %" PRIu16 "\n"
                  "  Spreading Factor: %" PRIu8 "\n"
                  "  Coding Rate: %s\n"
                  "  CRC Enable: %s",
                  this->preamble_size_, this->spreading_factor_, cr, TRUEFALSE(this->crc_enable_));
    if (this->payload_length_ > 0) {
      ESP_LOGCONFIG(TAG, "  Payload Length: %" PRIu32, this->payload_length_);
    }
    if (!this->sync_value_.empty()) {
      ESP_LOGCONFIG(TAG, "  Sync Value: 0x%02x", this->sync_value_[0]);
    }
  } else {
    const char *shaping = "NONE";
    if (this->modulation_ == MOD_FSK) {
      if (this->shaping_ == GAUSSIAN_BT_0_3) {
        shaping = "GAUSSIAN_BT_0_3";
      } else if (this->shaping_ == GAUSSIAN_BT_0_5) {
        shaping = "GAUSSIAN_BT_0_5";
      } else if (this->shaping_ == GAUSSIAN_BT_1_0) {
        shaping = "GAUSSIAN_BT_1_0";
      }
    } else {
      if (this->shaping_ == CUTOFF_BR_X_2) {
        shaping = "CUTOFF_BR_X_2";
      } else if (this->shaping_ == CUTOFF_BR_X_1) {
        shaping = "CUTOFF_BR_X_1";
      }
    }
    ESP_LOGCONFIG(TAG,
                  "  Shaping: %s\n"
                  "  Modulation: %s\n"
                  "  Bitrate: %" PRIu32 "b/s\n"
                  "  Bitsync: %s\n"
                  "  Rx Start: %s\n"
                  "  Rx Floor: %.1f dBm\n"
                  "  Packet Mode: %s",
                  shaping, this->modulation_ == MOD_FSK ? LOG_STR_LITERAL("FSK") : LOG_STR_LITERAL("OOK"),
                  this->bitrate_, TRUEFALSE(this->bitsync_), TRUEFALSE(this->rx_start_), this->rx_floor_,
                  TRUEFALSE(this->packet_mode_));
    if (this->packet_mode_) {
      ESP_LOGCONFIG(TAG, "  CRC Enable: %s", TRUEFALSE(this->crc_enable_));
    }
    if (this->payload_length_ > 0) {
      ESP_LOGCONFIG(TAG, "  Payload Length: %" PRIu32, this->payload_length_);
    }
    if (!this->sync_value_.empty()) {
      char hex_buf[17];  // 8 bytes max = 16 hex chars + null
      ESP_LOGCONFIG(TAG, "  Sync Value: 0x%s",
                    format_hex_to(hex_buf, this->sync_value_.data(), this->sync_value_.size()));
    }
    if (this->preamble_size_ > 0 || this->preamble_detect_ > 0) {
      ESP_LOGCONFIG(TAG,
                    "  Preamble Polarity: 0x%X\n"
                    "  Preamble Size: %" PRIu16 "\n"
                    "  Preamble Detect: %" PRIu8 "\n"
                    "  Preamble Errors: %" PRIu8,
                    this->preamble_polarity_, this->preamble_size_, this->preamble_detect_, this->preamble_errors_);
    }
  }
}

}  // namespace esphome::sx127x
