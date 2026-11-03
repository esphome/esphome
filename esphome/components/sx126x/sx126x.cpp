#include "sx126x.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::sx126x {

ESPHOME_LOG_TAG(TAG, "sx126x");
static constexpr uint16_t RAMP[8] PROGMEM = {10, 20, 40, 80, 200, 800, 1700, 3400};
// Aligned 32 bit loads from flash are safe on ESP8266, so this table is read directly
static constexpr uint32_t BW_HZ[31] PROGMEM = {4800,   5800,   7300,   9700,   11700,  14600,  19500,  23400,
                                               29300,  39000,  46900,  58600,  78200,  93800,  117300, 156200,
                                               187200, 234300, 312000, 373600, 467000, 7810,   10420,  15630,
                                               20830,  31250,  41670,  62500,  125000, 250000, 500000};
static constexpr uint8_t BW_LORA[10] PROGMEM = {LORA_BW_7810,   LORA_BW_10420, LORA_BW_15630, LORA_BW_20830,
                                                LORA_BW_31250,  LORA_BW_41670, LORA_BW_62500, LORA_BW_125000,
                                                LORA_BW_250000, LORA_BW_500000};
static constexpr uint8_t BW_FSK[21] PROGMEM = {
    FSK_BW_4800,   FSK_BW_5800,   FSK_BW_7300,   FSK_BW_9700,   FSK_BW_11700,  FSK_BW_14600,  FSK_BW_19500,
    FSK_BW_23400,  FSK_BW_29300,  FSK_BW_39000,  FSK_BW_46900,  FSK_BW_58600,  FSK_BW_78200,  FSK_BW_93800,
    FSK_BW_117300, FSK_BW_156200, FSK_BW_187200, FSK_BW_234300, FSK_BW_312000, FSK_BW_373600, FSK_BW_467000};

static constexpr uint32_t RESET_DELAY_HIGH_US = 5000;
static constexpr uint32_t RESET_DELAY_LOW_US = 2000;
static constexpr uint32_t TRANSMIT_TIMEOUT_MS = 4000;

// OCP (Over Current Protection) values
static constexpr uint8_t OCP_80MA = 0x18;   // 80 mA max current
static constexpr uint8_t OCP_140MA = 0x38;  // 140 mA max current

// LoRa low data rate optimization threshold
static constexpr float LOW_DATA_RATE_OPTIMIZE_THRESHOLD = 16.38f;  // 16.38 ms

bool SX126x::read_fifo_(uint8_t offset, std::vector<uint8_t> &packet) {
  const uint8_t header[] = {RADIO_READ_BUFFER, offset, 0};
  return !this->is_failed() && this->command_result_(this->spi_device_.read(header, packet));
}

bool SX126x::write_fifo_(uint8_t offset, const uint8_t *data, size_t len) {
  const uint8_t header[] = {RADIO_WRITE_BUFFER, offset};
  return !this->is_failed() && this->command_result_(this->spi_device_.write(header, {data, len}));
}

bool SX126x::read_opcode_(uint8_t opcode, uint8_t *data, uint8_t size) {
  const uint8_t header[] = {opcode, 0};
  return !this->is_failed() && this->command_result_(this->spi_device_.read(header, {data, size}));
}

bool SX126x::write_opcode_(uint8_t opcode, uint8_t *data, uint8_t size) {
  return !this->is_failed() && this->command_result_(this->spi_device_.write({&opcode, 1}, {data, size}));
}

bool SX126x::read_register_(uint16_t reg, uint8_t *data, uint8_t size) {
  const uint8_t header[] = {RADIO_READ_REGISTER, static_cast<uint8_t>(reg >> 8), static_cast<uint8_t>(reg), 0};
  return !this->is_failed() && this->command_result_(this->spi_device_.read(header, {data, size}));
}

bool SX126x::write_register_(uint16_t reg, uint8_t *data, uint8_t size) {
  const uint8_t header[] = {RADIO_WRITE_REGISTER, static_cast<uint8_t>(reg >> 8), static_cast<uint8_t>(reg)};
  return !this->is_failed() && this->command_result_(this->spi_device_.write(header, {data, size}));
}

void IRAM_ATTR SX126x::gpio_intr(SX126x *arg) { arg->enable_loop_soon_any_context(); }

void SX126x::setup() {
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
  if (!this->configure_() && !this->is_failed()) {
    this->mark_failed(LOG_STR("Wait busy timeout"));
  }
}

void SX126x::configure() { this->configure_(); }

bool SX126x::configure_() {
  if (this->is_failed()) {
    return false;
  }
  const bool configured = this->operation_.configure([this]() { return this->apply_configuration_(); });
  if (configured) {
    this->enable_loop();
  }
  return configured;
}

bool SX126x::apply_configuration_() {
  uint8_t buf[8];
  // toggle chip reset
  this->rst_pin_->digital_write(true);
  delayMicroseconds(RESET_DELAY_HIGH_US);
  this->rst_pin_->digital_write(false);
  delayMicroseconds(RESET_DELAY_LOW_US);
  this->rst_pin_->digital_write(true);
  delayMicroseconds(RESET_DELAY_HIGH_US);

  // wakeup
  if (!this->read_opcode_(RADIO_GET_STATUS, nullptr, 0)) {
    return false;
  }

  // config tcxo
  if (this->tcxo_voltage_ != TCXO_CTRL_NONE) {
    uint32_t delay = this->tcxo_delay_ >> 6;
    buf[0] = this->tcxo_voltage_;
    buf[1] = (delay >> 16) & 0xFF;
    buf[2] = (delay >> 8) & 0xFF;
    buf[3] = (delay >> 0) & 0xFF;
    if (!this->write_opcode_(RADIO_SET_TCXOMODE, buf, 4)) {
      return false;
    }
    buf[0] = 0x7F;
    if (!this->write_opcode_(RADIO_CALIBRATE, buf, 1)) {
      return false;
    }
  }

  // clear errors
  buf[0] = 0x00;
  buf[1] = 0x00;
  if (!this->write_opcode_(RADIO_CLR_ERROR, buf, 2)) {
    return false;
  }

  // rf switch
  if (this->rf_switch_) {
    buf[0] = 0x01;
    if (!this->write_opcode_(RADIO_SET_RFSWITCHMODE, buf, 1)) {
      return false;
    }
  }

  // check silicon version to make sure hw is ok
  if (!this->read_register_(REG_VERSION_STRING, (uint8_t *) this->version_, sizeof(this->version_))) {
    return false;
  }
  this->version_[sizeof(this->version_) - 1] = '\0';
  if (strncmp(this->version_, "SX126", 5) != 0 && strncmp(this->version_, "LLCC68", 6) != 0) {
    this->mark_failed(LOG_STR("Unsupported chip version"));
    return false;
  }

  // setup packet type
  buf[0] = this->modulation_;
  if (!this->write_opcode_(RADIO_SET_PACKETTYPE, buf, 1)) {
    return false;
  }

  // calibrate image
  if (!this->run_image_cal_()) {
    return false;
  }

  // set frequency
  uint64_t freq = ((uint64_t) this->frequency_ << 25) / XTAL_FREQ;
  buf[0] = (uint8_t) ((freq >> 24) & 0xFF);
  buf[1] = (uint8_t) ((freq >> 16) & 0xFF);
  buf[2] = (uint8_t) ((freq >> 8) & 0xFF);
  buf[3] = (uint8_t) (freq & 0xFF);
  if (!this->write_opcode_(RADIO_SET_RFFREQUENCY, buf, 4)) {
    return false;
  }

  // configure pa
  int8_t pa_power = this->pa_power_;
  if (this->sx1261_) {
    // the following values were taken from section 13.1.14.1 table 13-21
    // in rev 2.1 of the datasheet
    if (pa_power == 15) {
      uint8_t cfg[4] = {0x06, 0x00, 0x01, 0x01};
      if (!this->write_opcode_(RADIO_SET_PACONFIG, cfg, 4)) {
        return false;
      }
    } else {
      uint8_t cfg[4] = {0x04, 0x00, 0x01, 0x01};
      if (!this->write_opcode_(RADIO_SET_PACONFIG, cfg, 4)) {
        return false;
      }
    }
    pa_power = std::max(pa_power, (int8_t) -3);
    pa_power = std::min(pa_power, (int8_t) 14);
    buf[0] = OCP_80MA;
    if (!this->write_register_(REG_OCP, buf, 1)) {
      return false;
    }
  } else {
    // the following values were taken from section 13.1.14.1 table 13-21
    // in rev 2.1 of the datasheet
    uint8_t cfg[4] = {0x04, 0x07, 0x00, 0x01};
    if (!this->write_opcode_(RADIO_SET_PACONFIG, cfg, 4)) {
      return false;
    }
    pa_power = std::max(pa_power, (int8_t) -3);
    pa_power = std::min(pa_power, (int8_t) 22);
    buf[0] = OCP_140MA;
    if (!this->write_register_(REG_OCP, buf, 1)) {
      return false;
    }
  }
  buf[0] = pa_power;
  buf[1] = this->pa_ramp_;
  if (!this->write_opcode_(RADIO_SET_TXPARAMS, buf, 2)) {
    return false;
  }

  // configure modem
  if (this->modulation_ == PACKET_TYPE_LORA) {
    // set modulation params
    float duration = 1000.0f * (1UL << this->spreading_factor_) / BW_HZ[this->bandwidth_];
    buf[0] = this->spreading_factor_;
    buf[1] = progmem_read_byte(&BW_LORA[this->bandwidth_ - SX126X_BW_7810]);
    buf[2] = this->coding_rate_;
    buf[3] = (duration > LOW_DATA_RATE_OPTIMIZE_THRESHOLD) ? 0x01 : 0x00;
    if (!this->write_opcode_(RADIO_SET_MODULATIONPARAMS, buf, 4)) {
      return false;
    }

    // set packet params and sync word
    if (!this->set_packet_params_(this->get_max_packet_size())) {
      return false;
    }
    if (this->sync_value_.size() == 2) {
      if (!this->write_register_(REG_LORA_SYNCWORD, this->sync_value_.data(), this->sync_value_.size())) {
        return false;
      }
    }
  } else {
    // set modulation params
    uint32_t bitrate = ((uint64_t) XTAL_FREQ * 32) / this->bitrate_;
    uint32_t fdev = ((uint64_t) this->deviation_ << 25) / XTAL_FREQ;
    buf[0] = (bitrate >> 16) & 0xFF;
    buf[1] = (bitrate >> 8) & 0xFF;
    buf[2] = (bitrate >> 0) & 0xFF;
    buf[3] = this->shaping_;
    buf[4] = progmem_read_byte(&BW_FSK[this->bandwidth_ - SX126X_BW_4800]);
    buf[5] = (fdev >> 16) & 0xFF;
    buf[6] = (fdev >> 8) & 0xFF;
    buf[7] = (fdev >> 0) & 0xFF;
    if (!this->write_opcode_(RADIO_SET_MODULATIONPARAMS, buf, 8)) {
      return false;
    }

    // set crc params
    if (this->crc_enable_) {
      buf[0] = this->crc_initial_ >> 8;
      buf[1] = this->crc_initial_ & 0xFF;
      if (!this->write_register_(REG_CRC_INITIAL, buf, 2)) {
        return false;
      }
      buf[0] = this->crc_polynomial_ >> 8;
      buf[1] = this->crc_polynomial_ & 0xFF;
      if (!this->write_register_(REG_CRC_POLYNOMIAL, buf, 2)) {
        return false;
      }
    }

    // set whitening params
    if (this->whitening_enable_) {
      // according to the datasheet, section 12 table 12-1 "The user should not
      // change the value of the 7 MSB of this register"
      if (!this->read_register_(REG_WHITENING_INITIAL, buf, 1)) {
        return false;
      }
      buf[0] = (buf[0] & 0xFE) | ((this->whitening_initial_ >> 8) & 0x01);
      buf[1] = this->whitening_initial_ & 0xFF;
      if (!this->write_register_(REG_WHITENING_INITIAL, buf, 2)) {
        return false;
      }
    }

    // set packet params and sync word
    if (!this->set_packet_params_(this->get_max_packet_size())) {
      return false;
    }
    if (!this->sync_value_.empty()) {
      if (!this->write_register_(REG_GFSK_SYNCWORD, this->sync_value_.data(), this->sync_value_.size())) {
        return false;
      }
    }
  }

  // switch to rx or sleep
  if (this->rx_start_) {
    if (!this->set_mode_rx_()) {
      return false;
    }
  } else {
    if (!this->set_mode_sleep_()) {
      return false;
    }
  }
  return true;
}

size_t SX126x::get_max_packet_size() {
  if (this->payload_length_ > 0) {
    return this->payload_length_;
  }
  return SX126X_MAX_PACKET_SIZE;
}

bool SX126x::set_packet_params_(uint8_t payload_length) {
  uint8_t buf[9];
  if (this->modulation_ == PACKET_TYPE_LORA) {
    buf[0] = (this->preamble_size_ >> 8) & 0xFF;
    buf[1] = (this->preamble_size_ >> 0) & 0xFF;
    buf[2] = (this->payload_length_ > 0) ? 0x01 : 0x00;
    buf[3] = payload_length;
    buf[4] = (this->crc_enable_) ? 0x01 : 0x00;
    buf[5] = 0x00;
    if (!this->write_opcode_(RADIO_SET_PACKETPARAMS, buf, 6)) {
      return false;
    }
  } else {
    uint16_t preamble_size = this->preamble_size_ * 8;
    buf[0] = (preamble_size >> 8) & 0xFF;
    buf[1] = (preamble_size >> 0) & 0xFF;
    buf[2] = (this->preamble_detect_ > 0) ? ((this->preamble_detect_ - 1) | 0x04) : 0x00;
    buf[3] = this->sync_value_.size() * 8;
    buf[4] = 0x00;
    buf[5] = (this->payload_length_ > 0) ? 0x00 : 0x01;
    buf[6] = payload_length;
    if (this->crc_enable_) {
      buf[7] = (this->crc_inverted_ ? 0x04 : 0x00) + (this->crc_size_ & 0x02);
    } else {
      buf[7] = 0x01;
    }
    buf[8] = (this->whitening_enable_) ? 0x01 : 0x00;
    if (!this->write_opcode_(RADIO_SET_PACKETPARAMS, buf, 9)) {
      return false;
    }
  }
  return true;
}

SX126xError SX126x::transmit_packet(const uint8_t *data, size_t len) {
  // Callers such as a template number's on_value can run before setup() or after a failed setup
  if (!this->is_ready()) {
    ESP_LOGW(TAG, "Radio not ready, packet dropped");
    return SX126xError::SX126X_ERROR_NOT_READY;
  }
  const auto admission = this->operation_.transmit_admission();
  if (admission != SX126xError::NONE) {
    return admission;
  }
  if (this->payload_length_ > 0 && this->payload_length_ != len) {
    ESP_LOGE(TAG, "Packet size does not match config");
    return SX126xError::INVALID_PARAMS;
  }
  if (data == nullptr || len == 0 || len > this->get_max_packet_size()) {
    ESP_LOGE(TAG, "Packet size out of range");
    return SX126xError::INVALID_PARAMS;
  }

  const auto result = this->operation_.transmit([this, data, len]() { return this->prepare_transmit_(data, len); },
                                                [this]() { return this->wait_for_transmit_(); },
                                                [this]() { return this->restore_after_transmit_(); });
  if (this->operation_.is_ready()) {
    this->enable_loop();
  }
  return result;
}

bool SX126x::prepare_transmit_(const uint8_t *data, size_t len) {
  uint8_t clear[2] = {0xFF, 0xFF};
  return this->set_mode_standby_(STDBY_XOSC) && (this->payload_length_ != 0 || this->set_packet_params_(len)) &&
         this->write_fifo_(0x00, data, len) && this->write_opcode_(RADIO_CLR_IRQSTATUS, clear, 2) &&
         this->set_mode_tx_();
}

SX126xError SX126x::wait_for_transmit_() {
  const uint32_t start = millis();
  while (!this->dio1_pin_->digital_read()) {
    App.feed_wdt();
    if (millis() - start > TRANSMIT_TIMEOUT_MS) {
      ESP_LOGE(TAG, "Transmit packet failure");
      return SX126xError::TIMEOUT;
    }
  }
  return SX126xError::NONE;
}

bool SX126x::restore_after_transmit_() {
  uint8_t clear[2] = {0xFF, 0xFF};
  return this->set_mode_standby_(STDBY_XOSC) && this->write_opcode_(RADIO_CLR_IRQSTATUS, clear, 2) &&
         (this->payload_length_ != 0 || this->set_packet_params_(this->get_max_packet_size())) &&
         (this->rx_start_ ? this->set_mode_rx_() : this->set_mode_sleep_());
}

void SX126x::call_listeners_(const std::vector<uint8_t> &packet, float rssi, float snr) {
  for (auto &listener : this->listeners_) {
    listener->on_packet(packet, rssi, snr);
  }
  this->packet_trigger_.trigger(packet, rssi, snr);
}

void SX126x::loop() {
  if (this->is_failed() || !this->operation_.is_ready()) {
    return;
  }
  if (!this->dio1_pin_->digital_read()) {
    if (this->dio1_pin_->is_internal()) {
      this->disable_loop();
    }
    return;
  }

  uint16_t status;
  uint8_t buf[3];
  uint8_t rssi;
  int8_t snr;
  if (!this->read_opcode_(RADIO_GET_IRQSTATUS, buf, 2)) {
    return;
  }
  if (!this->write_opcode_(RADIO_CLR_IRQSTATUS, buf, 2)) {
    return;
  }
  status = (buf[0] << 8) | buf[1];
  if ((status & IRQ_RX_DONE) == IRQ_RX_DONE) {
    if ((status & IRQ_CRC_ERROR) != IRQ_CRC_ERROR) {
      if (!this->read_opcode_(RADIO_GET_PACKETSTATUS, buf, 3)) {
        return;
      }
      if (this->modulation_ == PACKET_TYPE_LORA) {
        rssi = buf[0];
        snr = buf[1];
      } else {
        rssi = buf[2];
        snr = 0;
      }
      if (!this->read_opcode_(RADIO_GET_RXBUFFERSTATUS, buf, 2)) {
        return;
      }
      this->packet_.resize(buf[0]);
      if (!this->read_fifo_(buf[1], this->packet_)) {
        return;
      }
      this->call_listeners_(this->packet_, (float) rssi / -2.0f, (float) snr / 4.0f);
    }
  }
}

bool SX126x::run_image_cal_() {
  // the following values were taken from section 9.2.1 table 9-2
  // in rev 2.1 of the datasheet
  uint8_t buf[2] = {0, 0};
  if (this->frequency_ > 900000000) {
    buf[0] = 0xE1;
    buf[1] = 0xE9;
  } else if (this->frequency_ > 850000000) {
    buf[0] = 0xD7;
    buf[1] = 0xDB;
  } else if (this->frequency_ > 770000000) {
    buf[0] = 0xC1;
    buf[1] = 0xC5;
  } else if (this->frequency_ > 460000000) {
    buf[0] = 0x75;
    buf[1] = 0x81;
  } else if (this->frequency_ > 425000000) {
    buf[0] = 0x6B;
    buf[1] = 0x6F;
  }
  if (buf[0] > 0 && buf[1] > 0) {
    if (!this->write_opcode_(RADIO_CALIBRATEIMAGE, buf, 2)) {
      return false;
    }
  }
  return true;
}

bool SX126x::set_mode_rx_() {
  uint8_t buf[8];

  // configure irq params
  uint16_t irq = IRQ_RX_DONE | IRQ_RX_TX_TIMEOUT | IRQ_CRC_ERROR;
  buf[0] = (irq >> 8) & 0xFF;
  buf[1] = (irq >> 0) & 0xFF;
  buf[2] = (irq >> 8) & 0xFF;
  buf[3] = (irq >> 0) & 0xFF;
  buf[4] = (IRQ_RADIO_NONE >> 8) & 0xFF;
  buf[5] = (IRQ_RADIO_NONE >> 0) & 0xFF;
  buf[6] = (IRQ_RADIO_NONE >> 8) & 0xFF;
  buf[7] = (IRQ_RADIO_NONE >> 0) & 0xFF;
  if (!this->write_opcode_(RADIO_SET_DIOIRQPARAMS, buf, 8)) {
    return false;
  }

  // set timeout to 0
  buf[0] = 0x00;
  if (!this->write_opcode_(RADIO_SET_LORASYMBTIMEOUT, buf, 1)) {
    return false;
  }

  // switch to continuous mode rx
  buf[0] = 0xFF;
  buf[1] = 0xFF;
  buf[2] = 0xFF;
  return this->write_opcode_(RADIO_SET_RX, buf, 3);
}

bool SX126x::set_mode_tx_() {
  uint8_t buf[8];

  // configure irq params
  uint16_t irq = IRQ_TX_DONE | IRQ_RX_TX_TIMEOUT;
  buf[0] = (irq >> 8) & 0xFF;
  buf[1] = (irq >> 0) & 0xFF;
  buf[2] = (irq >> 8) & 0xFF;
  buf[3] = (irq >> 0) & 0xFF;
  buf[4] = (IRQ_RADIO_NONE >> 8) & 0xFF;
  buf[5] = (IRQ_RADIO_NONE >> 0) & 0xFF;
  buf[6] = (IRQ_RADIO_NONE >> 8) & 0xFF;
  buf[7] = (IRQ_RADIO_NONE >> 0) & 0xFF;
  if (!this->write_opcode_(RADIO_SET_DIOIRQPARAMS, buf, 8)) {
    return false;
  }

  // switch to single mode tx
  buf[0] = 0x00;
  buf[1] = 0x00;
  buf[2] = 0x00;
  return this->write_opcode_(RADIO_SET_TX, buf, 3);
}

bool SX126x::set_mode_sleep_(bool cold) {
  // 0x04 = warm start (config retained), 0x00 = cold start (config lost, lowest power)
  uint8_t buf[1];
  buf[0] = cold ? 0x00 : 0x04;
  return this->write_opcode_(RADIO_SET_SLEEP, buf, 1);
}

bool SX126x::set_mode_standby_(SX126xStandbyMode mode) {
  uint8_t buf[1];
  buf[0] = mode;
  return this->write_opcode_(RADIO_SET_STANDBY, buf, 1);
}

bool SX126x::command_result_(bool success) {
  if (success) {
    this->status_clear_warning();
  } else {
    this->status_set_warning(LOG_STR("Wait busy timeout"));
  }
  return success;
}

void SX126x::dump_config() {
  ESP_LOGCONFIG(TAG, "SX126x:");
  LOG_PIN("  CS Pin: ", this->cs_pin_);
  LOG_PIN("  BUSY Pin: ", this->busy_pin_);
  LOG_PIN("  RST Pin: ", this->rst_pin_);
  LOG_PIN("  DIO1 Pin: ", this->dio1_pin_);
  ESP_LOGCONFIG(TAG,
                "  HW Version: %15s\n"
                "  Frequency: %" PRIu32 " Hz\n"
                "  Bandwidth: %" PRIu32 " Hz\n"
                "  PA Power: %" PRId8 " dBm\n"
                "  PA Ramp: %" PRIu16 " us\n"
                "  Payload Length: %" PRIu32 "\n"
                "  CRC Enable: %s\n"
                "  Rx Start: %s",
                this->version_, this->frequency_, BW_HZ[this->bandwidth_], this->pa_power_,
                progmem_read_uint16(&RAMP[this->pa_ramp_]), this->payload_length_, TRUEFALSE(this->crc_enable_),
                TRUEFALSE(this->rx_start_));
  if (this->modulation_ == PACKET_TYPE_GFSK) {
    const char *shaping = "NONE";
    if (this->shaping_ == GAUSSIAN_BT_0_3) {
      shaping = "GAUSSIAN_BT_0_3";
    } else if (this->shaping_ == GAUSSIAN_BT_0_5) {
      shaping = "GAUSSIAN_BT_0_5";
    } else if (this->shaping_ == GAUSSIAN_BT_0_7) {
      shaping = "GAUSSIAN_BT_0_7";
    } else if (this->shaping_ == GAUSSIAN_BT_1_0) {
      shaping = "GAUSSIAN_BT_1_0";
    }
    ESP_LOGCONFIG(TAG,
                  "  Modulation: FSK\n"
                  "  Deviation: %" PRIu32 " Hz\n"
                  "  Shaping: %s\n"
                  "  Preamble Size: %" PRIu16 "\n"
                  "  Preamble Detect: %" PRIu16 "\n"
                  "  Bitrate: %" PRIu32 "b/s",
                  this->deviation_, shaping, this->preamble_size_, this->preamble_detect_, this->bitrate_);
  } else if (this->modulation_ == PACKET_TYPE_LORA) {
    const char *cr = "4/8";
    if (this->coding_rate_ == LORA_CR_4_5) {
      cr = "4/5";
    } else if (this->coding_rate_ == LORA_CR_4_6) {
      cr = "4/6";
    } else if (this->coding_rate_ == LORA_CR_4_7) {
      cr = "4/7";
    }
    ESP_LOGCONFIG(TAG,
                  "  Modulation: LORA\n"
                  "  Spreading Factor: %" PRIu8 "\n"
                  "  Coding Rate: %s\n"
                  "  Preamble Size: %" PRIu16,
                  this->spreading_factor_, cr, this->preamble_size_);
  }
  if (!this->sync_value_.empty()) {
    char hex_buf[17];  // 8 bytes max = 16 hex chars + null
    ESP_LOGCONFIG(TAG, "  Sync Value: 0x%s",
                  format_hex_to(hex_buf, this->sync_value_.data(), this->sync_value_.size()));
  }
  ESP_LOGCONFIG(TAG, "  Whitening Enable: %s", TRUEFALSE(this->whitening_enable_));
  if (this->whitening_enable_) {
    ESP_LOGCONFIG(TAG, "  Whitening Initial: 0x%03x", this->whitening_initial_);
  }
}

}  // namespace esphome::sx126x
