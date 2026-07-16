#include "cc1101.h"
#include "cc1101pa.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <cmath>
#include <utility>

namespace esphome::cc1101 {

static const char *const TAG = "cc1101";

static void split_float(float value, int mbits, uint8_t &e, uint32_t &m) {
  int e_tmp;
  float m_tmp = std::frexp(value, &e_tmp);
  if (e_tmp <= mbits) {
    e = 0;
    m = 0;
    return;
  }
  e = static_cast<uint8_t>(e_tmp - mbits - 1);
  m = static_cast<uint32_t>(((m_tmp * 2 - 1) * (1 << (mbits + 1))) + 1) >> 1;
  if (m == (1UL << mbits)) {
    e = e + 1;
    m = 0;
  }
}

CC1101Component::CC1101Component() {
  // Datasheet defaults
  memset(&this->state_, 0, sizeof(this->state_));
  this->state_.GDO2_CFG = 0x0D;  // Serial Data (for RX on GDO2)
  this->state_.GDO1_CFG = 0x2E;
  this->state_.GDO0_CFG = 0x0D;  // Serial Data (for RX on GDO0 / TX Input)
  this->state_.FIFO_THR = 7;
  this->state_.SYNC1 = 0xD3;
  this->state_.SYNC0 = 0x91;
  this->state_.PKTLEN = 0xFF;
  this->state_.APPEND_STATUS = 1;
  this->state_.LENGTH_CONFIG = 1;
  this->state_.CRC_EN = 1;
  this->state_.WHITE_DATA = 1;
  this->state_.FREQ_IF = 0x0F;
  this->state_.FREQ2 = 0x1E;
  this->state_.FREQ1 = 0xC4;
  this->state_.FREQ0 = 0xEC;
  this->state_.DRATE_E = 0x0C;
  this->state_.CHANBW_E = 0x02;
  this->state_.DRATE_M = 0x22;
  this->state_.SYNC_MODE = 2;
  this->state_.CHANSPC_E = 2;
  this->state_.NUM_PREAMBLE = 2;
  this->state_.CHANSPC_M = 0xF8;
  this->state_.DEVIATION_M = 7;
  this->state_.DEVIATION_E = 4;
  this->state_.RX_TIME = 7;
  this->state_.CCA_MODE = 3;
  this->state_.PO_TIMEOUT = 1;
  this->state_.FOC_LIMIT = 2;
  this->state_.FOC_POST_K = 1;
  this->state_.FOC_PRE_K = 2;
  this->state_.FOC_BS_CS_GATE = 1;
  this->state_.BS_POST_KP = 1;
  this->state_.BS_POST_KI = 1;
  this->state_.BS_PRE_KP = 2;
  this->state_.BS_PRE_KI = 1;
  this->state_.MAGN_TARGET = 3;
  this->state_.AGC_LNA_PRIORITY = 1;
  this->state_.FILTER_LENGTH = 1;
  this->state_.WAIT_TIME = 1;
  this->state_.HYST_LEVEL = 2;
  this->state_.WOREVT1 = 0x87;
  this->state_.WOREVT0 = 0x6B;
  this->state_.RC_CAL = 1;
  this->state_.EVENT1 = 7;
  this->state_.RC_PD = 1;
  this->state_.MIX_CURRENT = 2;
  this->state_.LODIV_BUF_CURRENT_RX = 1;
  this->state_.LNA2MIX_CURRENT = 1;
  this->state_.LNA_CURRENT = 1;
  this->state_.LODIV_BUF_CURRENT_TX = 1;
  this->state_.FSCAL3_LO = 9;
  this->state_.CHP_CURR_CAL_EN = 2;
  this->state_.FSCAL3_HI = 2;
  this->state_.FSCAL2 = 0x0A;
  this->state_.FSCAL1 = 0x20;
  this->state_.FSCAL0 = 0x0D;
  this->state_.RCCTRL1 = 0x41;
  this->state_.FSTEST = 0x59;
  this->state_.PTEST = 0x7F;
  this->state_.AGCTEST = 0x3F;
  this->state_.TEST2 = 0x88;
  this->state_.TEST1 = 0x31;
  this->state_.TEST0_LO = 1;
  this->state_.VCO_SEL_CAL_EN = 1;
  this->state_.TEST0_HI = 2;

  // PKTCTRL0
  this->state_.PKT_FORMAT = 3;
  this->state_.LENGTH_CONFIG = 2;
  this->state_.FS_AUTOCAL = 1;

  // CRITICAL: Initialize PA Table to avoid transmitting 0 power (Silence)
  memset(this->pa_table_, 0, sizeof(this->pa_table_));
}

void IRAM_ATTR CC1101Component::gpio_intr(CC1101Component *arg) { arg->enable_loop_soon_any_context(); }

void CC1101Component::setup() {
  this->spi_setup();

  if (this->gdo0_pin_ != nullptr) {
    this->gdo0_pin_->setup();
  }

  this->configure();
  if (this->is_failed()) {
    return;
  }

  // Defer pin mode setup until after all components have completed setup()
  // This handles the case where remote_transmitter runs after CC1101 and changes pin mode
  if (this->gdo0_pin_ != nullptr) {
    this->defer([this]() {
      this->gdo0_pin_->pin_mode(gpio::FLAG_INPUT);
      if (this->state_.PKT_FORMAT == static_cast<uint8_t>(PacketFormat::PACKET_FORMAT_FIFO)) {
        this->gdo0_pin_->attach_interrupt(&CC1101Component::gpio_intr, this, gpio::INTERRUPT_RISING_EDGE);
      }
    });
  }
}

void CC1101Component::configure() {
  // Manual reset sequence per CC1101 datasheet section 19.1.2
  this->cs_->digital_write(true);
  delayMicroseconds(1);
  this->cs_->digital_write(false);
  delayMicroseconds(1);
  this->cs_->digital_write(true);
  delayMicroseconds(41);
  this->cs_->digital_write(false);
  delay(5);

  this->strobe_(Command::RES);
  delay(5);

  this->read_(Register::PARTNUM);
  this->read_(Register::VERSION);
  this->chip_id_ = encode_uint16(this->state_.PARTNUM, this->state_.VERSION);
  ESP_LOGD(TAG, "CC1101 found! Chip ID: 0x%04X", this->chip_id_);
  if (this->state_.VERSION == 0 || this->state_.PARTNUM == 0xFF) {
    ESP_LOGE(TAG, "Failed to verify CC1101.");
    this->mark_failed();
    return;
  }

  this->initialized_ = true;

  for (uint8_t i = 0; i <= static_cast<uint8_t>(Register::TEST0); i++) {
    if (i == static_cast<uint8_t>(Register::FSTEST) || i == static_cast<uint8_t>(Register::AGCTEST)) {
      continue;
    }
    this->write_(static_cast<Register>(i));
  }
  this->set_output_power(this->output_power_requested_);

  if (!this->enter_rx_()) {
    this->mark_failed();
    return;
  }
}

void CC1101Component::call_listeners_(const std::vector<uint8_t> &packet, float freq_offset, float rssi, uint8_t lqi) {
  for (auto &listener : this->listeners_) {
    listener->on_packet(packet, freq_offset, rssi, lqi);
  }
  this->packet_trigger_.trigger(packet, freq_offset, rssi, lqi);
}

bool CC1101Component::is_long_packet_mode_() const {
  return this->packet_mode_ && (this->packet_length_ > 64 || this->packet_length_func_);
}

uint32_t CC1101Component::packet_idle_timeout_ms_() const {
  const float symbol_rate =
      (((256.0f + this->state_.DRATE_M) * (1 << this->state_.DRATE_E)) / (1 << 28)) * XTAL_FREQUENCY;
  const uint32_t timeout = static_cast<uint32_t>(std::ceil((8.0f * PACKET_IDLE_TIMEOUT_BYTES * 1000.0f) / symbol_rate));
  return timeout < PACKET_IDLE_TIMEOUT_MIN_MS ? PACKET_IDLE_TIMEOUT_MIN_MS : timeout;
}

void CC1101Component::reset_long_packet_rx_() {
  this->packet_.clear();
  this->packet_last_byte_ms_ = 0;
  this->packet_fixed_mode_armed_ = false;

  if (this->packet_length_func_) {
    this->packet_expected_length_ = 0;
    this->state_.LENGTH_CONFIG = static_cast<uint8_t>(LengthConfig::LENGTH_CONFIG_INFINITE);
    this->state_.PKTLEN = 0xFF;
  } else if (this->packet_length_ > 255) {
    this->packet_expected_length_ = this->packet_length_;
    this->state_.LENGTH_CONFIG = static_cast<uint8_t>(LengthConfig::LENGTH_CONFIG_INFINITE);
    this->state_.PKTLEN = static_cast<uint8_t>(this->packet_length_ % 256);
  } else {
    this->packet_expected_length_ = this->packet_length_;
    this->packet_fixed_mode_armed_ = true;
    this->state_.LENGTH_CONFIG = static_cast<uint8_t>(LengthConfig::LENGTH_CONFIG_FIXED);
    this->state_.PKTLEN = static_cast<uint8_t>(this->packet_length_);
  }

  if (this->initialized_) {
    this->write_(Register::PKTLEN);
    this->write_(Register::PKTCTRL0);
  }
}

void CC1101Component::restart_long_packet_rx_() {
  this->enter_idle_();
  this->strobe_(Command::FRX);
  this->reset_long_packet_rx_();
  this->enter_rx_();
  this->disable_loop();
}

void CC1101Component::maybe_switch_long_packet_to_fixed_() {
  if (this->packet_fixed_mode_armed_ || this->packet_expected_length_ == 0) {
    return;
  }
  const uint16_t received = this->packet_.size();
  const uint16_t remaining = this->packet_expected_length_ - received;
  if (remaining >= 256) {
    return;
  }

  // PKTLEN is intentionally not rewritten here: TI Design Note DN500 (SWRA109) section 2.3 sets PKTLEN
  // once to (length % 256) and keeps that same value across both the infinite and fixed phases, for both
  // TX and RX. Writing PKTLEN = remaining here instead would use an MCU-observed, FIFO-drain-timing-
  // dependent value in place of the datasheet's fixed target, misaligning the internal byte counter.
  this->state_.LENGTH_CONFIG = static_cast<uint8_t>(LengthConfig::LENGTH_CONFIG_FIXED);
  this->write_(Register::PKTCTRL0);
  this->packet_fixed_mode_armed_ = true;
  ESP_LOGVV(TAG, "Switched long packet RX to fixed mode: received=%u expected=%u pktlen=%u", received,
            this->packet_expected_length_, this->state_.PKTLEN);
}

void CC1101Component::finish_long_packet_() {
  this->packet_.resize(this->packet_expected_length_);
  this->read_(Register::FREQEST);
  this->read_(Register::RSSI);
  this->read_(Register::LQI);
  float freq_offset = static_cast<int8_t>(this->state_.FREQEST) * (XTAL_FREQUENCY / (1 << 14));
  float rssi = (this->state_.RSSI * RSSI_STEP) - RSSI_OFFSET;
  bool crc_ok = (this->state_.LQI & STATUS_CRC_OK_MASK) != 0;
  uint8_t lqi = this->state_.LQI & STATUS_LQI_MASK;
  if (this->state_.CRC_EN == 0 || crc_ok) {
    this->call_listeners_(this->packet_, freq_offset, rssi, lqi);
  }
  this->restart_long_packet_rx_();
}

void CC1101Component::handle_long_packet_(uint8_t rx_bytes) {
  if (this->packet_expected_length_ > 0 && this->packet_.size() >= this->packet_expected_length_) {
    this->finish_long_packet_();
    return;
  }

  uint16_t to_read = rx_bytes;
  if (this->packet_expected_length_ > 0) {
    const uint16_t remaining = this->packet_expected_length_ - this->packet_.size();
    if (to_read > remaining) {
      to_read = remaining;
    }
    if (to_read == rx_bytes && remaining > rx_bytes) {
      to_read--;
    }
  } else if (to_read > 0) {
    to_read--;
  }

  if (to_read == 0) {
    if (!this->packet_.empty() && millis() - this->packet_last_byte_ms_ > this->packet_idle_timeout_ms_()) {
      ESP_LOGW(TAG, "Packet receive timeout, discarding packet: received=%u expected=%u fifo=%u",
               static_cast<uint16_t>(this->packet_.size()), this->packet_expected_length_, rx_bytes);
      this->restart_long_packet_rx_();
    }
    return;
  }

  const size_t old_size = this->packet_.size();
  // Reserve the known target length once, so growth below stays within already-reserved capacity
  // instead of reallocating repeatedly. A no-op once capacity() already covers it (e.g. every call
  // after the first for a fixed packet_length, or every call after packet_length_lambda resolves it).
  if (this->packet_expected_length_ > 0 && this->packet_.capacity() < this->packet_expected_length_) {
    this->packet_.reserve(this->packet_expected_length_);
  }
  this->packet_.resize(old_size + to_read);
  this->read_(Register::FIFO, this->packet_.data() + old_size, to_read);
  this->packet_last_byte_ms_ = millis();
  ESP_LOGVV(TAG, "Drained RX FIFO: read=%u received=%u expected=%u fifo=%u", to_read,
            static_cast<uint16_t>(this->packet_.size()), this->packet_expected_length_, rx_bytes);

  if (this->packet_.size() > MAX_PACKET_LENGTH) {
    ESP_LOGW(TAG, "Packet length exceeds maximum, discarding packet: length=%u max=%u",
             static_cast<uint16_t>(this->packet_.size()), MAX_PACKET_LENGTH);
    this->restart_long_packet_rx_();
    return;
  }

  if (this->packet_expected_length_ == 0 && this->packet_length_func_) {
    const int32_t length = this->packet_length_func_(this->packet_);
    if (length < 0) {
      ESP_LOGVV(TAG, "Packet length lambda discarded packet: received=%u", static_cast<uint16_t>(this->packet_.size()));
      this->restart_long_packet_rx_();
      return;
    }
    if (length > MAX_PACKET_LENGTH) {
      ESP_LOGW(TAG, "Packet length exceeds maximum, discarding packet: length=%" PRId32 " max=%u", length,
               MAX_PACKET_LENGTH);
      this->restart_long_packet_rx_();
      return;
    }
    if (length > 0 && static_cast<size_t>(length) < this->packet_.size()) {
      ESP_LOGW(TAG, "Invalid computed packet length, discarding packet: length=%" PRId32 " received=%u", length,
               static_cast<uint16_t>(this->packet_.size()));
      this->restart_long_packet_rx_();
      return;
    }
    if (length > 0) {
      this->packet_expected_length_ = static_cast<uint16_t>(length);
      this->state_.PKTLEN = static_cast<uint8_t>(length <= 255 ? length : length % 256);
      this->write_(Register::PKTLEN);
      ESP_LOGVV(TAG, "Resolved packet length: received=%u expected=%u pktlen=%u",
                static_cast<uint16_t>(this->packet_.size()), this->packet_expected_length_, this->state_.PKTLEN);
    }
  }

  this->maybe_switch_long_packet_to_fixed_();

  if (this->packet_expected_length_ > 0 && this->packet_.size() >= this->packet_expected_length_) {
    this->finish_long_packet_();
  }
}

void CC1101Component::loop() {
  if (this->state_.PKT_FORMAT != static_cast<uint8_t>(PacketFormat::PACKET_FORMAT_FIFO) || this->gdo0_pin_ == nullptr ||
      (!this->is_long_packet_mode_() && !this->gdo0_pin_->digital_read())) {
    return;
  }

  if (!this->is_long_packet_mode_()) {
    this->disable_loop();
  }

  // Read state
  this->read_(Register::RXBYTES);
  uint8_t rx_bytes = this->state_.NUM_RXBYTES;
  bool overflow = this->state_.RXFIFO_OVERFLOW;
  if (this->is_long_packet_mode_()) {
    if (overflow) {
      ESP_LOGW(TAG, "RX FIFO overflow, discarding packet: received=%u expected=%u fifo=%u",
               static_cast<uint16_t>(this->packet_.size()), this->packet_expected_length_, rx_bytes);
      this->restart_long_packet_rx_();
      return;
    }
    if (rx_bytes == 0) {
      if (this->packet_.empty() && !this->gdo0_pin_->digital_read()) {
        this->disable_loop();
        return;
      }
      if (!this->packet_.empty() && millis() - this->packet_last_byte_ms_ > this->packet_idle_timeout_ms_()) {
        ESP_LOGW(TAG, "Packet receive timeout, discarding packet: received=%u expected=%u fifo=%u",
                 static_cast<uint16_t>(this->packet_.size()), this->packet_expected_length_, rx_bytes);
        this->restart_long_packet_rx_();
      }
      return;
    }
    this->handle_long_packet_(rx_bytes);
    return;
  }

  if (overflow || rx_bytes == 0) {
    ESP_LOGW(TAG, "RX FIFO overflow, flushing");
    this->enter_idle_();
    this->strobe_(Command::FRX);
    this->enter_rx_();
    return;
  }

  // Read packet
  uint8_t payload_length, expected_rx;
  if (this->state_.LENGTH_CONFIG == static_cast<uint8_t>(LengthConfig::LENGTH_CONFIG_VARIABLE)) {
    this->read_(Register::FIFO, &payload_length, 1);
    expected_rx = payload_length + 1;
  } else {
    payload_length = this->state_.PKTLEN;
    expected_rx = payload_length;
  }
  if (payload_length == 0 || payload_length > 64 || rx_bytes != expected_rx) {
    ESP_LOGW(TAG, "Invalid packet: rx_bytes %u, payload_length %u", rx_bytes, payload_length);
    this->enter_idle_();
    this->strobe_(Command::FRX);
    this->enter_rx_();
    return;
  }
  this->packet_.resize(payload_length);
  this->read_(Register::FIFO, this->packet_.data(), payload_length);

  // Read status from registers (more reliable than FIFO status bytes due to timing issues)
  this->read_(Register::FREQEST);
  this->read_(Register::RSSI);
  this->read_(Register::LQI);
  float freq_offset = static_cast<int8_t>(this->state_.FREQEST) * (XTAL_FREQUENCY / (1 << 14));
  float rssi = (this->state_.RSSI * RSSI_STEP) - RSSI_OFFSET;
  bool crc_ok = (this->state_.LQI & STATUS_CRC_OK_MASK) != 0;
  uint8_t lqi = this->state_.LQI & STATUS_LQI_MASK;
  if (this->state_.CRC_EN == 0 || crc_ok) {
    this->call_listeners_(this->packet_, freq_offset, rssi, lqi);
  }

  // Return to rx
  this->enter_idle_();
  this->strobe_(Command::FRX);
  this->enter_rx_();
}

void CC1101Component::dump_config() {
  static const char *const MODULATION_NAMES[] = {"2-FSK", "GFSK",   "UNUSED", "ASK/OOK",
                                                 "4-FSK", "UNUSED", "UNUSED", "MSK"};
  int32_t freq = static_cast<int32_t>(this->state_.FREQ2 << 16 | this->state_.FREQ1 << 8 | this->state_.FREQ0) *
                 XTAL_FREQUENCY / (1 << 16);
  float symbol_rate = (((256.0f + this->state_.DRATE_M) * (1 << this->state_.DRATE_E)) / (1 << 28)) * XTAL_FREQUENCY;
  float bw = XTAL_FREQUENCY / (8.0f * (4 + this->state_.CHANBW_M) * (1 << this->state_.CHANBW_E));
  ESP_LOGCONFIG(TAG,
                "CC1101:\n"
                "  Chip ID: 0x%04X\n"
                "  Frequency: %" PRId32 " Hz\n"
                "  Channel: %u\n"
                "  Modulation: %s\n"
                "  Symbol Rate: %.0f baud\n"
                "  Filter Bandwidth: %.1f Hz\n"
                "  Output Power: %.1f dBm",
                this->chip_id_, freq, this->state_.CHANNR, MODULATION_NAMES[this->state_.MOD_FORMAT & 0x07],
                symbol_rate, bw, this->output_power_effective_);
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_PIN("  GDO0 Pin: ", this->gdo0_pin_);

  if (!this->packet_mode_) {
    ESP_LOGCONFIG(TAG, "  Packet length: Async serial");
  } else if (this->packet_length_func_) {
    ESP_LOGCONFIG(TAG, "  Packet length: Dynamic lambda (max=%u), infinite-to-fixed", MAX_PACKET_LENGTH);
  } else if (this->packet_length_ == 0) {
    ESP_LOGCONFIG(TAG, "  Packet length: Variable, length byte after sync (max=%u)", this->state_.PKTLEN);
  } else if (this->packet_length_ <= 64) {
    ESP_LOGCONFIG(TAG, "  Packet length: Fixed, %u bytes", this->packet_length_);
  } else if (this->packet_length_ <= 255) {
    ESP_LOGCONFIG(TAG, "  Packet length: Long fixed, %u bytes (max=%u)", this->packet_length_, MAX_PACKET_LENGTH);
  } else {
    ESP_LOGCONFIG(TAG, "  Packet length: Long fixed, %u bytes (max=%u), infinite-to-fixed", this->packet_length_,
                  MAX_PACKET_LENGTH);
  }
}

void CC1101Component::begin_tx() {
  // Ensure Packet Format is 3 (Async Serial)
  this->write_(Register::PKTCTRL0, 0x32);
  ESP_LOGV(TAG, "Beginning TX sequence");
  if (this->gdo0_pin_ != nullptr) {
    this->gdo0_pin_->detach_interrupt();
    this->gdo0_pin_->pin_mode(gpio::FLAG_OUTPUT);
  }
  // Transition through IDLE to bypass CCA (Clear Channel Assessment) which can
  // block TX entry when strobing from RX, and to ensure FS_AUTOCAL calibration
  this->enter_idle_();
  if (!this->enter_tx_()) {
    ESP_LOGW(TAG, "Failed to enter TX state!");
  }
}

void CC1101Component::begin_rx() {
  ESP_LOGV(TAG, "Beginning RX sequence");
  if (this->gdo0_pin_ != nullptr) {
    this->gdo0_pin_->pin_mode(gpio::FLAG_INPUT);
  }
  // Transition through IDLE to ensure FS_AUTOCAL calibration occurs
  this->enter_idle_();
  if (!this->enter_rx_()) {
    ESP_LOGW(TAG, "Failed to enter RX state!");
  }
}

void CC1101Component::reset() {
  this->strobe_(Command::RES);
  this->configure();
}

void CC1101Component::set_idle() {
  ESP_LOGV(TAG, "Setting IDLE state");
  this->enter_idle_();
}

bool CC1101Component::wait_for_state_(State target_state, uint32_t timeout_ms) {
  uint32_t start = millis();
  while (millis() - start < timeout_ms) {
    this->read_(Register::MARCSTATE);
    State s = static_cast<State>(this->state_.MARC_STATE);
    if (s == target_state) {
      return true;
    }
    delayMicroseconds(100);
  }
  return false;
}

bool CC1101Component::enter_calibrated_(State target_state, Command cmd) {
  // The PLL must be recalibrated until PLL lock is achieved
  for (uint8_t retries = PLL_LOCK_RETRIES; retries > 0; retries--) {
    this->strobe_(cmd);
    if (!this->wait_for_state_(target_state)) {
      return false;
    }
    this->read_(Register::FSCAL1);
    if (this->state_.FSCAL1 != FSCAL1_PLL_NOT_LOCKED) {
      return true;
    }
    ESP_LOGW(TAG, "PLL lock failed, retrying calibration");
    this->enter_idle_();
  }
  ESP_LOGE(TAG, "PLL lock failed after retries");
  return false;
}

void CC1101Component::enter_idle_() {
  this->strobe_(Command::IDLE);
  this->wait_for_state_(State::IDLE);
}

bool CC1101Component::enter_rx_() { return this->enter_calibrated_(State::RX, Command::RX); }

bool CC1101Component::enter_tx_() { return this->enter_calibrated_(State::TX, Command::TX); }

uint8_t CC1101Component::strobe_(Command cmd) {
  uint8_t index = static_cast<uint8_t>(cmd);
  if (cmd < Command::RES || cmd > Command::NOP) {
    return 0xFF;
  }
  this->enable();
  uint8_t status_byte = this->transfer_byte(index);
  this->disable();
  return status_byte;
}

void CC1101Component::write_(Register reg) {
  uint8_t index = static_cast<uint8_t>(reg);
  this->enable();
  this->write_byte(index);
  this->write_array(&this->state_.regs()[index], 1);
  this->disable();
}

void CC1101Component::write_(Register reg, uint8_t value) {
  uint8_t index = static_cast<uint8_t>(reg);
  this->state_.regs()[index] = value;
  this->write_(reg);
}

void CC1101Component::write_(Register reg, const uint8_t *buffer, size_t length) {
  uint8_t index = static_cast<uint8_t>(reg);
  this->enable();
  this->write_byte(index | BUS_WRITE | BUS_BURST);
  this->write_array(buffer, length);
  this->disable();
}

void CC1101Component::read_(Register reg) {
  uint8_t index = static_cast<uint8_t>(reg);
  this->enable();
  this->write_byte(index | BUS_READ | BUS_BURST);
  this->state_.regs()[index] = this->transfer_byte(0);
  this->disable();
}

void CC1101Component::read_(Register reg, uint8_t *buffer, size_t length) {
  uint8_t index = static_cast<uint8_t>(reg);
  this->enable();
  this->write_byte(index | BUS_READ | BUS_BURST);
  this->read_array(buffer, length);
  this->disable();
}

CC1101Error CC1101Component::transmit_packet(const std::vector<uint8_t> &packet) {
  if (this->state_.PKT_FORMAT != static_cast<uint8_t>(PacketFormat::PACKET_FORMAT_FIFO)) {
    ESP_LOGW(TAG, "Cannot transmit packet: packet mode is not FIFO");
    return CC1101Error::PARAMS;
  }
  // Long packet TX needs chunked FIFO refills and is not implemented.
  if (this->state_.LENGTH_CONFIG == static_cast<uint8_t>(LengthConfig::LENGTH_CONFIG_INFINITE)) {
    ESP_LOGW(TAG, "Cannot transmit packet: long packet TX is not implemented for infinite packet mode");
    return CC1101Error::PARAMS;
  }
  if (this->state_.LENGTH_CONFIG == static_cast<uint8_t>(LengthConfig::LENGTH_CONFIG_VARIABLE) && packet.size() > 63) {
    ESP_LOGW(TAG,
             "Cannot transmit packet: long packet TX is not implemented, payload length %u exceeds variable-length TX "
             "payload limit 63",
             static_cast<unsigned>(packet.size()));
    return CC1101Error::PARAMS;
  }
  if (packet.size() > 64) {
    ESP_LOGW(TAG,
             "Cannot transmit packet: long packet TX is not implemented, payload length %u exceeds TX FIFO limit 64",
             static_cast<unsigned>(packet.size()));
    return CC1101Error::PARAMS;
  }

  // Write packet
  this->enter_idle_();
  this->strobe_(Command::FTX);
  if (this->state_.LENGTH_CONFIG == static_cast<uint8_t>(LengthConfig::LENGTH_CONFIG_VARIABLE)) {
    this->write_(Register::FIFO, static_cast<uint8_t>(packet.size()));
  }
  this->write_(Register::FIFO, packet.data(), packet.size());

  // Calibrate PLL
  if (!this->enter_calibrated_(State::FSTXON, Command::FSTXON)) {
    ESP_LOGW(TAG, "PLL lock failed during TX");
    this->enter_idle_();
    this->enter_rx_();
    return CC1101Error::PLL_LOCK;
  }

  // Transmit packet
  this->strobe_(Command::TX);
  if (!this->wait_for_state_(State::IDLE, 1000)) {
    ESP_LOGW(TAG, "TX timeout");
    this->enter_idle_();
    this->enter_rx_();
    return CC1101Error::TIMEOUT;
  }

  // Return to rx
  this->enter_rx_();
  return CC1101Error::NONE;
}

// Setters
void CC1101Component::set_output_power(float value) {
  this->output_power_requested_ = value;
  int32_t freq = static_cast<int32_t>(this->state_.FREQ2 << 16 | this->state_.FREQ1 << 8 | this->state_.FREQ0) *
                 XTAL_FREQUENCY / (1 << 16);
  uint8_t a = 0xC0;
  if (freq >= 300000000 && freq <= 348000000) {
    a = PowerTableItem::find(PA_TABLE_315, sizeof(PA_TABLE_315) / sizeof(PA_TABLE_315[0]), value);
  } else if (freq >= 378000000 && freq <= 464000000) {
    a = PowerTableItem::find(PA_TABLE_433, sizeof(PA_TABLE_433) / sizeof(PA_TABLE_433[0]), value);
  } else if (freq >= 779000000 && freq < 900000000) {
    a = PowerTableItem::find(PA_TABLE_868, sizeof(PA_TABLE_868) / sizeof(PA_TABLE_868[0]), value);
  } else if (freq >= 900000000 && freq <= 928000000) {
    a = PowerTableItem::find(PA_TABLE_915, sizeof(PA_TABLE_915) / sizeof(PA_TABLE_915[0]), value);
  }

  if (static_cast<Modulation>(this->state_.MOD_FORMAT) == Modulation::MODULATION_ASK_OOK) {
    this->pa_table_[0] = 0;
    this->pa_table_[1] = a;
  } else {
    this->pa_table_[0] = a;
    this->pa_table_[1] = 0;
  }
  this->output_power_effective_ = value;
  if (this->initialized_) {
    this->write_(Register::PATABLE, this->pa_table_, sizeof(this->pa_table_));
  }
}

void CC1101Component::set_rx_attenuation(RxAttenuation value) {
  this->state_.CLOSE_IN_RX = static_cast<uint8_t>(value);
  if (this->initialized_) {
    this->write_(Register::FIFOTHR);
  }
}

void CC1101Component::set_dc_blocking_filter(bool value) {
  this->state_.DEM_DCFILT_OFF = value ? 0 : 1;
  if (this->initialized_) {
    this->write_(Register::MDMCFG2);
  }
}

void CC1101Component::set_frequency(float value) {
  int32_t freq = static_cast<int32_t>(value * (1 << 16) / XTAL_FREQUENCY);
  this->state_.FREQ2 = static_cast<uint8_t>(freq >> 16);
  this->state_.FREQ1 = static_cast<uint8_t>(freq >> 8);
  this->state_.FREQ0 = static_cast<uint8_t>(freq);
  if (this->initialized_) {
    this->enter_idle_();
    this->write_(Register::FREQ2);
    this->write_(Register::FREQ1);
    this->write_(Register::FREQ0);
    this->enter_rx_();
  }
}

void CC1101Component::set_if_frequency(float value) {
  this->state_.FREQ_IF = value * (1 << 10) / XTAL_FREQUENCY;
  if (this->initialized_) {
    this->write_(Register::FSCTRL1);
  }
}

void CC1101Component::set_filter_bandwidth(float value) {
  uint8_t e;
  uint32_t m;
  split_float(XTAL_FREQUENCY / (value * 8), 2, e, m);
  this->state_.CHANBW_E = e;
  this->state_.CHANBW_M = static_cast<uint8_t>(m);
  if (this->initialized_) {
    this->write_(Register::MDMCFG4);
  }
}

void CC1101Component::set_channel(uint8_t value) {
  this->state_.CHANNR = value;
  if (this->initialized_) {
    this->enter_idle_();
    this->write_(Register::CHANNR);
    this->enter_rx_();
  }
}

void CC1101Component::set_channel_spacing(float value) {
  uint8_t e;
  uint32_t m;
  split_float(value * (1 << 18) / XTAL_FREQUENCY, 8, e, m);
  this->state_.CHANSPC_E = e;
  this->state_.CHANSPC_M = static_cast<uint8_t>(m);
  if (this->initialized_) {
    this->write_(Register::MDMCFG1);
    this->write_(Register::MDMCFG0);
  }
}

void CC1101Component::set_fsk_deviation(float value) {
  uint8_t e;
  uint32_t m;
  split_float(value * (1 << 17) / XTAL_FREQUENCY, 3, e, m);
  this->state_.DEVIATION_E = e;
  this->state_.DEVIATION_M = static_cast<uint8_t>(m);
  if (this->initialized_) {
    this->write_(Register::DEVIATN);
  }
}

void CC1101Component::set_msk_deviation(uint8_t value) {
  this->state_.DEVIATION_E = 0;
  this->state_.DEVIATION_M = value - 1;
  if (this->initialized_) {
    this->write_(Register::DEVIATN);
  }
}

void CC1101Component::set_symbol_rate(float value) {
  uint8_t e;
  uint32_t m;
  split_float(value * (1 << 28) / XTAL_FREQUENCY, 8, e, m);
  this->state_.DRATE_E = e;
  this->state_.DRATE_M = static_cast<uint8_t>(m);
  if (this->initialized_) {
    this->write_(Register::MDMCFG4);
    this->write_(Register::MDMCFG3);
  }
}

void CC1101Component::set_sync_mode(SyncMode value) {
  this->state_.SYNC_MODE = static_cast<uint8_t>(value);
  if (this->initialized_) {
    this->write_(Register::MDMCFG2);
  }
}

void CC1101Component::set_carrier_sense_above_threshold(bool value) {
  this->state_.CARRIER_SENSE_ABOVE_THRESHOLD = value ? 1 : 0;
  if (this->initialized_) {
    this->write_(Register::MDMCFG2);
  }
}

void CC1101Component::set_modulation_type(Modulation value) {
  this->state_.MOD_FORMAT = static_cast<uint8_t>(value);
  this->state_.PA_POWER = value == Modulation::MODULATION_ASK_OOK ? 1 : 0;
  if (this->initialized_) {
    this->enter_idle_();
    this->set_output_power(this->output_power_requested_);
    this->write_(Register::MDMCFG2);
    this->write_(Register::FREND0);
    this->enter_rx_();
  }
}

void CC1101Component::set_manchester(bool value) {
  this->state_.MANCHESTER_EN = value ? 1 : 0;
  if (this->initialized_) {
    this->write_(Register::MDMCFG2);
  }
}

void CC1101Component::set_num_preamble(uint8_t value) {
  this->state_.NUM_PREAMBLE = value;
  if (this->initialized_) {
    this->write_(Register::MDMCFG1);
  }
}

void CC1101Component::set_sync1(uint8_t value) {
  this->state_.SYNC1 = value;
  if (this->initialized_) {
    this->write_(Register::SYNC1);
  }
}

void CC1101Component::set_sync0(uint8_t value) {
  this->state_.SYNC0 = value;
  if (this->initialized_) {
    this->write_(Register::SYNC0);
  }
}

void CC1101Component::set_magn_target(MagnTarget value) {
  this->state_.MAGN_TARGET = static_cast<uint8_t>(value);
  if (this->initialized_) {
    this->write_(Register::AGCCTRL2);
  }
}

void CC1101Component::set_max_lna_gain(MaxLnaGain value) {
  this->state_.MAX_LNA_GAIN = static_cast<uint8_t>(value);
  if (this->initialized_) {
    this->write_(Register::AGCCTRL2);
  }
}

void CC1101Component::set_max_dvga_gain(MaxDvgaGain value) {
  this->state_.MAX_DVGA_GAIN = static_cast<uint8_t>(value);
  if (this->initialized_) {
    this->write_(Register::AGCCTRL2);
  }
}

void CC1101Component::set_carrier_sense_abs_thr(int8_t value) {
  this->state_.CARRIER_SENSE_ABS_THR = static_cast<uint8_t>(value & 0b1111);
  if (this->initialized_) {
    this->write_(Register::AGCCTRL1);
  }
}

void CC1101Component::set_carrier_sense_rel_thr(CarrierSenseRelThr value) {
  this->state_.CARRIER_SENSE_REL_THR = static_cast<uint8_t>(value);
  if (this->initialized_) {
    this->write_(Register::AGCCTRL1);
  }
}

void CC1101Component::set_lna_priority(bool value) {
  this->state_.AGC_LNA_PRIORITY = value ? 1 : 0;
  if (this->initialized_) {
    this->write_(Register::AGCCTRL1);
  }
}

void CC1101Component::set_filter_length_fsk_msk(FilterLengthFskMsk value) {
  this->state_.FILTER_LENGTH = static_cast<uint8_t>(value);
  if (this->initialized_) {
    this->write_(Register::AGCCTRL0);
  }
}

void CC1101Component::set_filter_length_ask_ook(FilterLengthAskOok value) {
  this->state_.FILTER_LENGTH = static_cast<uint8_t>(value);
  if (this->initialized_) {
    this->write_(Register::AGCCTRL0);
  }
}

void CC1101Component::set_freeze(Freeze value) {
  this->state_.AGC_FREEZE = static_cast<uint8_t>(value);
  if (this->initialized_) {
    this->write_(Register::AGCCTRL0);
  }
}

void CC1101Component::set_wait_time(WaitTime value) {
  this->state_.WAIT_TIME = static_cast<uint8_t>(value);
  if (this->initialized_) {
    this->write_(Register::AGCCTRL0);
  }
}

void CC1101Component::set_hyst_level(HystLevel value) {
  this->state_.HYST_LEVEL = static_cast<uint8_t>(value);
  if (this->initialized_) {
    this->write_(Register::AGCCTRL0);
  }
}

void CC1101Component::set_foc_bs_cs_gate(bool value) {
  this->state_.FOC_BS_CS_GATE = value ? 1 : 0;
  if (this->initialized_) {
    this->write_(Register::FOCCFG);
  }
}

void CC1101Component::set_foc_limit(FocLimit value) {
  this->state_.FOC_LIMIT = static_cast<uint8_t>(value);
  if (this->initialized_) {
    this->write_(Register::FOCCFG);
  }
}

void CC1101Component::set_foc_pre_k(FocPreK value) {
  this->state_.FOC_PRE_K = static_cast<uint8_t>(value);
  if (this->initialized_) {
    this->write_(Register::FOCCFG);
  }
}

void CC1101Component::set_foc_post_k(FocPostK value) {
  this->state_.FOC_POST_K = static_cast<uint8_t>(value);
  if (this->initialized_) {
    this->write_(Register::FOCCFG);
  }
}

void CC1101Component::set_bs_limit(BsLimit value) {
  this->state_.BS_LIMIT = static_cast<uint8_t>(value);
  if (this->initialized_) {
    this->write_(Register::BSCFG);
  }
}

void CC1101Component::set_bs_pre_ki(BsPreKi value) {
  this->state_.BS_PRE_KI = static_cast<uint8_t>(value);
  if (this->initialized_) {
    this->write_(Register::BSCFG);
  }
}

void CC1101Component::set_bs_pre_kp(BsPreKp value) {
  this->state_.BS_PRE_KP = static_cast<uint8_t>(value);
  if (this->initialized_) {
    this->write_(Register::BSCFG);
  }
}

void CC1101Component::set_bs_post_ki(BsPostKi value) {
  this->state_.BS_POST_KI = static_cast<uint8_t>(value);
  if (this->initialized_) {
    this->write_(Register::BSCFG);
  }
}

void CC1101Component::set_bs_post_kp(BsPostKp value) {
  this->state_.BS_POST_KP = static_cast<uint8_t>(value);
  if (this->initialized_) {
    this->write_(Register::BSCFG);
  }
}

void CC1101Component::configure_packet_mode_() {
  if (!this->packet_mode_) {
    this->state_.PKT_FORMAT = static_cast<uint8_t>(PacketFormat::PACKET_FORMAT_ASYNC_SERIAL);
    this->state_.GDO0_CFG = 0x0D;
  } else {
    this->state_.PKT_FORMAT = static_cast<uint8_t>(PacketFormat::PACKET_FORMAT_FIFO);
    this->state_.APPEND_STATUS = 0;
    if (this->is_long_packet_mode_()) {
      // Threshold-only signal reasserts after each partial FIFO drain during long packets.
      this->state_.GDO0_CFG = 0x00;
      this->state_.FIFO_THR = 7;
      this->reset_long_packet_rx_();
    } else {
      this->state_.GDO0_CFG = 0x01;
      this->state_.FIFO_THR = 15;
      if (this->packet_length_ == 0) {
        this->state_.LENGTH_CONFIG = static_cast<uint8_t>(LengthConfig::LENGTH_CONFIG_VARIABLE);
      } else {
        this->state_.LENGTH_CONFIG = static_cast<uint8_t>(LengthConfig::LENGTH_CONFIG_FIXED);
        this->state_.PKTLEN = static_cast<uint8_t>(this->packet_length_);
      }
    }
  }

  if (this->initialized_) {
    this->write_(Register::PKTCTRL0);
    this->write_(Register::PKTCTRL1);
    this->write_(Register::PKTLEN);
    this->write_(Register::IOCFG0);
    this->write_(Register::FIFOTHR);
  }
}

void CC1101Component::set_packet_mode(bool value) {
  this->packet_mode_ = value;
  this->configure_packet_mode_();
  if (this->initialized_) {
    if (this->gdo0_pin_ != nullptr) {
      if (value) {
        this->gdo0_pin_->attach_interrupt(&CC1101Component::gpio_intr, this, gpio::INTERRUPT_RISING_EDGE);
      } else {
        this->gdo0_pin_->detach_interrupt();
      }
    }
  }
}

void CC1101Component::set_packet_length(uint16_t value) {
  this->packet_length_ = value;
  this->configure_packet_mode_();
}

void CC1101Component::set_packet_length_lambda(packet_length_func_t func) {
  this->packet_length_func_ = std::move(func);
  this->configure_packet_mode_();
}

void CC1101Component::set_crc_enable(bool value) {
  this->state_.CRC_EN = value ? 1 : 0;
  if (this->initialized_) {
    this->write_(Register::PKTCTRL0);
  }
}

void CC1101Component::set_whitening(bool value) {
  this->state_.WHITE_DATA = value ? 1 : 0;
  if (this->initialized_) {
    this->write_(Register::PKTCTRL0);
  }
}

}  // namespace esphome::cc1101
