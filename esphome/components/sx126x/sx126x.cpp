#include "sx126x.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::sx126x {

static const char *const TAG = "sx126x";
static const uint16_t RAMP[8] = {10, 20, 40, 80, 200, 800, 1700, 3400};
static const uint32_t BW_HZ[31] = {4800,  5800,  7300,  9700,   11700,  14600,  19500,  23400,  29300,  39000,  46900,
                                   58600, 78200, 93800, 117300, 156200, 187200, 234300, 312000, 373600, 467000, 7810,
                                   10420, 15630, 20830, 31250,  41670,  62500,  125000, 250000, 500000};
static const uint8_t BW_LORA[10] = {LORA_BW_7810,  LORA_BW_10420, LORA_BW_15630,  LORA_BW_20830,  LORA_BW_31250,
                                    LORA_BW_41670, LORA_BW_62500, LORA_BW_125000, LORA_BW_250000, LORA_BW_500000};
static const uint8_t BW_FSK[21] = {
    FSK_BW_4800,   FSK_BW_5800,   FSK_BW_7300,   FSK_BW_9700,   FSK_BW_11700,  FSK_BW_14600,  FSK_BW_19500,
    FSK_BW_23400,  FSK_BW_29300,  FSK_BW_39000,  FSK_BW_46900,  FSK_BW_58600,  FSK_BW_78200,  FSK_BW_93800,
    FSK_BW_117300, FSK_BW_156200, FSK_BW_187200, FSK_BW_234300, FSK_BW_312000, FSK_BW_373600, FSK_BW_467000};

static constexpr uint32_t RESET_DELAY_HIGH_US = 5000;
static constexpr uint32_t RESET_DELAY_LOW_US = 2000;
static constexpr uint32_t SWITCHING_DELAY_US = 1;
static constexpr uint32_t TRANSMIT_TIMEOUT_MS = 4000;
static constexpr uint32_t BUSY_TIMEOUT_MS = 20;

// OCP (Over Current Protection) values
static constexpr uint8_t OCP_80MA = 0x18;   // 80 mA max current
static constexpr uint8_t OCP_140MA = 0x38;  // 140 mA max current

// LoRa low data rate optimization threshold
static constexpr float LOW_DATA_RATE_OPTIMIZE_THRESHOLD = 16.38f;  // 16.38 ms

uint8_t SX126x::read_fifo_(uint8_t offset, std::vector<uint8_t> &packet) {
  this->enable();
  this->wait_busy_();
  this->transfer_byte(RADIO_READ_BUFFER);
  this->transfer_byte(offset);
  uint8_t status = this->transfer_byte(0x00);
  for (uint8_t &byte : packet) {
    byte = this->transfer_byte(0x00);
  }
  this->disable();
  return status;
}

void SX126x::write_fifo_(uint8_t offset, const std::vector<uint8_t> &packet) {
  this->enable();
  this->wait_busy_();
  this->transfer_byte(RADIO_WRITE_BUFFER);
  this->transfer_byte(offset);
  for (const uint8_t &byte : packet) {
    this->transfer_byte(byte);
  }
  this->disable();
  delayMicroseconds(SWITCHING_DELAY_US);
}

uint8_t SX126x::read_opcode_(uint8_t opcode, uint8_t *data, uint8_t size) {
  this->enable();
  this->wait_busy_();
  this->transfer_byte(opcode);
  uint8_t status = this->transfer_byte(0x00);
  for (int32_t i = 0; i < size; i++) {
    data[i] = this->transfer_byte(0x00);
  }
  this->disable();
  return status;
}

void SX126x::write_opcode_(uint8_t opcode, uint8_t *data, uint8_t size) {
  this->enable();
  this->wait_busy_();
  this->transfer_byte(opcode);
  for (int32_t i = 0; i < size; i++) {
    this->transfer_byte(data[i]);
  }
  this->disable();
  delayMicroseconds(SWITCHING_DELAY_US);
}

void SX126x::read_register_(uint16_t reg, uint8_t *data, uint8_t size) {
  this->enable();
  this->wait_busy_();
  this->write_byte(RADIO_READ_REGISTER);
  this->write_byte((reg >> 8) & 0xFF);
  this->write_byte((reg >> 0) & 0xFF);
  this->write_byte(0x00);
  for (int32_t i = 0; i < size; i++) {
    data[i] = this->transfer_byte(0x00);
  }
  this->disable();
}

void SX126x::write_register_(uint16_t reg, uint8_t *data, uint8_t size) {
  this->enable();
  this->wait_busy_();
  this->write_byte(RADIO_WRITE_REGISTER);
  this->write_byte((reg >> 8) & 0xFF);
  this->write_byte((reg >> 0) & 0xFF);
  for (int32_t i = 0; i < size; i++) {
    this->transfer_byte(data[i]);
  }
  this->disable();
  delayMicroseconds(SWITCHING_DELAY_US);
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
  if (this->pa_ctx_pin_) {
    this->pa_ctx_pin_->setup();
  }

  // start spi
  this->spi_setup();

  // configure rf
  this->configure();
}

void SX126x::configure() {
  uint8_t buf[8];

  // toggle chip reset
  this->rst_pin_->digital_write(true);
  delayMicroseconds(RESET_DELAY_HIGH_US);
  this->rst_pin_->digital_write(false);
  delayMicroseconds(RESET_DELAY_LOW_US);
  this->rst_pin_->digital_write(true);
  delayMicroseconds(RESET_DELAY_HIGH_US);

  // wakeup
  this->read_opcode_(RADIO_GET_STATUS, nullptr, 0);

  // config tcxo
  if (this->tcxo_voltage_ != TCXO_CTRL_NONE) {
    uint32_t delay = this->tcxo_delay_ >> 6;
    buf[0] = this->tcxo_voltage_;
    buf[1] = (delay >> 16) & 0xFF;
    buf[2] = (delay >> 8) & 0xFF;
    buf[3] = (delay >> 0) & 0xFF;
    this->write_opcode_(RADIO_SET_TCXOMODE, buf, 4);
    buf[0] = 0x7F;
    this->write_opcode_(RADIO_CALIBRATE, buf, 1);
  }

  // clear errors
  buf[0] = 0x00;
  buf[1] = 0x00;
  this->write_opcode_(RADIO_CLR_ERROR, buf, 2);

  // rf switch
  if (this->rf_switch_) {
    buf[0] = 0x01;
    this->write_opcode_(RADIO_SET_RFSWITCHMODE, buf, 1);
  }

  // check silicon version to make sure hw is ok
  this->read_register_(REG_VERSION_STRING, (uint8_t *) this->version_, sizeof(this->version_));
  this->version_[sizeof(this->version_) - 1] = '\0';
  if (strncmp(this->version_, "SX126", 5) != 0 && strncmp(this->version_, "LLCC68", 6) != 0) {
    this->mark_failed();
    return;
  }

  // setup packet type
  buf[0] = this->modulation_;
  this->write_opcode_(RADIO_SET_PACKETTYPE, buf, 1);

  // calibrate image
  this->run_image_cal();

  // set frequency
  uint64_t freq = ((uint64_t) this->frequency_ << 25) / XTAL_FREQ;
  buf[0] = (uint8_t) ((freq >> 24) & 0xFF);
  buf[1] = (uint8_t) ((freq >> 16) & 0xFF);
  buf[2] = (uint8_t) ((freq >> 8) & 0xFF);
  buf[3] = (uint8_t) (freq & 0xFF);
  this->write_opcode_(RADIO_SET_RFFREQUENCY, buf, 4);

  // configure pa
  int8_t pa_power = this->pa_power_;
  if (this->hw_version_ == "sx1261") {
    // the following values were taken from section 13.1.14.1 table 13-21
    // in rev 2.1 of the datasheet
    if (pa_power == 15) {
      uint8_t cfg[4] = {0x06, 0x00, 0x01, 0x01};
      this->write_opcode_(RADIO_SET_PACONFIG, cfg, 4);
    } else {
      uint8_t cfg[4] = {0x04, 0x00, 0x01, 0x01};
      this->write_opcode_(RADIO_SET_PACONFIG, cfg, 4);
    }
    pa_power = std::max(pa_power, (int8_t) -3);
    pa_power = std::min(pa_power, (int8_t) 14);
    buf[0] = OCP_80MA;
    this->write_register_(REG_OCP, buf, 1);
  } else {
    // the following values were taken from section 13.1.14.1 table 13-21
    // in rev 2.1 of the datasheet
    uint8_t cfg[4] = {0x04, 0x07, 0x00, 0x01};
    this->write_opcode_(RADIO_SET_PACONFIG, cfg, 4);
    pa_power = std::max(pa_power, (int8_t) -3);
    pa_power = std::min(pa_power, (int8_t) 22);
    buf[0] = OCP_140MA;
    this->write_register_(REG_OCP, buf, 1);
  }
  buf[0] = pa_power;
  buf[1] = this->pa_ramp_;
  this->write_opcode_(RADIO_SET_TXPARAMS, buf, 2);

  // configure modem
  if (this->modulation_ == PACKET_TYPE_LORA) {
    // set modulation params
    float duration = 1000.0f * std::pow(2, this->spreading_factor_) / BW_HZ[this->bandwidth_];
    buf[0] = this->spreading_factor_;
    buf[1] = BW_LORA[this->bandwidth_ - SX126X_BW_7810];
    buf[2] = this->coding_rate_;
    buf[3] = (duration > LOW_DATA_RATE_OPTIMIZE_THRESHOLD) ? 0x01 : 0x00;
    this->write_opcode_(RADIO_SET_MODULATIONPARAMS, buf, 4);

    // set packet params and sync word
    this->set_packet_params_(this->get_max_packet_size());
    if (this->sync_value_.size() == 2) {
      this->write_register_(REG_LORA_SYNCWORD, this->sync_value_.data(), this->sync_value_.size());
    }
  } else {
    // set modulation params
    uint32_t bitrate = ((uint64_t) XTAL_FREQ * 32) / this->bitrate_;
    uint32_t fdev = ((uint64_t) this->deviation_ << 25) / XTAL_FREQ;
    buf[0] = (bitrate >> 16) & 0xFF;
    buf[1] = (bitrate >> 8) & 0xFF;
    buf[2] = (bitrate >> 0) & 0xFF;
    buf[3] = this->shaping_;
    buf[4] = BW_FSK[this->bandwidth_ - SX126X_BW_4800];
    buf[5] = (fdev >> 16) & 0xFF;
    buf[6] = (fdev >> 8) & 0xFF;
    buf[7] = (fdev >> 0) & 0xFF;
    this->write_opcode_(RADIO_SET_MODULATIONPARAMS, buf, 8);

    // set crc params
    if (this->crc_enable_) {
      buf[0] = this->crc_initial_ >> 8;
      buf[1] = this->crc_initial_ & 0xFF;
      this->write_register_(REG_CRC_INITIAL, buf, 2);
      buf[0] = this->crc_polynomial_ >> 8;
      buf[1] = this->crc_polynomial_ & 0xFF;
      this->write_register_(REG_CRC_POLYNOMIAL, buf, 2);
    }

    // set packet params and sync word
    this->set_packet_params_(this->get_max_packet_size());
    if (!this->sync_value_.empty()) {
      this->write_register_(REG_GFSK_SYNCWORD, this->sync_value_.data(), this->sync_value_.size());
    }
  }

  // switch to rx or sleep
  if (this->rx_start_) {
    this->set_mode_rx();
  } else {
    this->set_mode_sleep();
  }
}

size_t SX126x::get_max_packet_size() {
  if (this->payload_length_ > 0) {
    return this->payload_length_;
  }
  return 255;
}

void SX126x::set_packet_params_(uint8_t payload_length) {
  uint8_t buf[9];
  if (this->modulation_ == PACKET_TYPE_LORA) {
    buf[0] = (this->preamble_size_ >> 8) & 0xFF;
    buf[1] = (this->preamble_size_ >> 0) & 0xFF;
    buf[2] = (this->payload_length_ > 0) ? 0x01 : 0x00;
    buf[3] = payload_length;
    buf[4] = (this->crc_enable_) ? 0x01 : 0x00;
    buf[5] = 0x00;
    this->write_opcode_(RADIO_SET_PACKETPARAMS, buf, 6);
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
    buf[8] = 0x00;
    this->write_opcode_(RADIO_SET_PACKETPARAMS, buf, 9);
  }
}

SX126xError SX126x::transmit_packet(const std::vector<uint8_t> &packet) {
  if (this->payload_length_ > 0 && this->payload_length_ != packet.size()) {
    ESP_LOGE(TAG, "Packet size does not match config");
    return SX126xError::INVALID_PARAMS;
  }
  if (packet.empty() || packet.size() > this->get_max_packet_size()) {
    ESP_LOGE(TAG, "Packet size out of range");
    return SX126xError::INVALID_PARAMS;
  }

  // don't use this -- this would be a blocking loop for CAD -- using async_transmit_packet is a better choice
  // as the retry loop is at least asynchronous.  if scan_channel_clear() is returning false due to being in the
  // middle of receiving a packet, it could be tens-of-millieconds (or more) before the packet is fully received,
  // so blocking isn't playing very nice with the esphome scheduler
  //
  // use async if you want cad... so we don't hold the cpu
  // optional: CAD before transmit
  // if ( this->cad_enable_ && !skip_cad ){
  //   for ( int attempt = 0; attempt < 10; ++attempt ){
  //     if (this->scan_channel_clear()) {
  //       break;
  //     }
  //     if (attempt < 9){
  //       // random backoff 20-50ms
  //       uint32_t backoff = 50; // temporarily 20 ms static
  //       delay(backoff); // this is a blocking wait... we would have to change quite a bit to queue a packet to make
  //       this async
  //     } else {
  //       ESP_LOGW(TAG, "Channel busy after 5 CAD attempts, transmitting anyway...");
  //     }
  //   }
  // }

  SX126xError ret = SX126xError::NONE;
  this->set_mode_standby(STDBY_XOSC);
  if (this->payload_length_ == 0) {
    this->set_packet_params_(packet.size());
  }
  this->write_fifo_(0x00, packet);
  this->set_mode_tx();

  // wait until transmit completes, typically the delay will be less than 100 ms
  uint32_t start = millis();
  while (!this->dio1_pin_->digital_read()) {
    if (millis() - start > TRANSMIT_TIMEOUT_MS) {
      ESP_LOGE(TAG, "Transmit packet failure");
      ret = SX126xError::TIMEOUT;
      break;
    }
  }

  uint8_t buf[2];
  buf[0] = 0xFF;
  buf[1] = 0xFF;
  this->write_opcode_(RADIO_CLR_IRQSTATUS, buf, 2);
  if (this->payload_length_ == 0) {
    this->set_packet_params_(this->get_max_packet_size());
  }
  if (this->rx_start_) {
    this->set_mode_rx();
  } else {
    this->set_mode_sleep();
  }
  return ret;
}

// validates and enqueues a packet to transmit, assigning the packet cad timeout
// and kicking off the async transmit process after a 1ms delay (immediately) subject
// to device scheduling
SX126xError SX126x::async_transmit_packet(const std::vector<uint8_t> &packet, optional<uint32_t> cad_timeout) {
  if (this->payload_length_ > 0 && this->payload_length_ != packet.size()) {
    ESP_LOGE(TAG, "Packet size does not match config");
    return SX126xError::INVALID_PARAMS;
  }
  if (packet.empty() || packet.size() > this->get_max_packet_size()) {
    ESP_LOGE(TAG, "Packet size out of range");
    return SX126xError::INVALID_PARAMS;
  }

  // wrap the packet and the cad_timeout value, preferring the local value over the global config
  uint32_t cadto = cad_timeout.value_or(this->cad_timeout_);
  AsyncPacket ap = AsyncPacket(packet, cadto);

  ESP_LOGD(TAG, "async_transmit_packet: packet queued size=%d, cad_timeout=%lu", packet.size(), ap.cad_timeout);
  this->async_tx_queue_.push(ap);

  // schedule the next send loop only 1ms out
  this->set_timeout("maybe_transmit_queued_packet", 1, [this]() { this->maybe_transmit_queued_packet(); });

  return SX126xError::NONE;
}

// transmit a packet if there is one to send
// will first invoke channel activity detection if configured, maintain and check per-packet timeout values
// and then transmit the packet if the air is clear or the cad_timeout has been exceeded
void SX126x::maybe_transmit_queued_packet() {
  if (this->async_tx_queue_.empty()) {
    return;
  }
  ESP_LOGD(TAG, "TX packet - queue_size=%d", this->async_tx_queue_.size());

  AsyncPacket &ap = this->async_tx_queue_.front();  // peek at the front item in the queue to send it

  // if there is a cad_timeout assigned and the start_time is still 0, then assign the current time
  // to the async packet, to 'start the clock' on cad_timeout for this packet
  if (ap.cad_timeout && ap.start_time == 0) {
    ap.start_time = millis();
  }

  bool cad = true;
  if (ap.cad_timeout == 0) {
    // cad_timeout is disabled, skip this (leave cad = true, process it right away)
  } else if (millis() - ap.start_time >= ap.cad_timeout) {
    // the timeout has been exceeded, leave cad = true to transmit it anyway.
    ESP_LOGW(TAG, "CAD timeout, transmitting anyway.");
  } else {
    // perform channel activity detection and record result
    uint32_t start = millis();
    cad = this->scan_channel_clear();
    ESP_LOGD(TAG, "TX CAD duration=%lu ms, result=%s", millis() - start, cad ? "clear" : "busy");
  }

  if (cad) {  // cad == true means that the channel is clear

    ESP_LOGV(TAG, "transmitting queued packet size=%d", ap.packet.size());

    SX126xError result = this->transmit_packet(ap.packet);

    if (result == SX126xError::NONE) {  // we already validated the size before enqueue, so only a tx timeout can fail
                                        // here - which may be hardware error
      ESP_LOGD(TAG, "TX packet size=%d", ap.packet.size());
    } else {
      ESP_LOGW(TAG, "TX ERROR TRANSMITTING PACKET, PACKET DROPPED size=%s", ap.packet.size());
    }

    // the only error types are essentially fatal for the packet, so pop it from the queue no matter what so we don't
    // get stuck on a poison packet
    this->async_tx_queue_.pop();

    // if there are still items in the queue, try to send the next 100ms later.
    if (!this->async_tx_queue_.empty()) {
      ESP_LOGD(TAG, "TX more packets in queue, setting timeout to send the next in 100ms");
      this->set_timeout("maybe_transmit_queued_packet", 100, [this]() { this->maybe_transmit_queued_packet(); });
    }

  } else {  // cad == false -- the channel is busy
    ESP_LOGD(TAG, "CAD channel not clear , deferring tx for 20ms");
    this->set_timeout("maybe_transmit_queued_packet", 20, [this]() { this->maybe_transmit_queued_packet(); });
  }
}

void SX126x::call_listeners_(const std::vector<uint8_t> &packet, float rssi, float snr) {
  for (auto &listener : this->listeners_) {
    listener->on_packet(packet, rssi, snr);
  }
  this->packet_trigger_.trigger(packet, rssi, snr);
}

void SX126x::loop() {
  if (this->dio1_pin_->is_internal()) {
    this->disable_loop();
  }
  if (!this->dio1_pin_->digital_read()) {
    return;
  }

  uint16_t status;
  uint8_t buf[3];
  uint8_t rssi;
  int8_t snr;
  this->read_opcode_(RADIO_GET_IRQSTATUS, buf, 2);
  this->write_opcode_(RADIO_CLR_IRQSTATUS, buf, 2);
  status = (buf[0] << 8) | buf[1];
  if ((status & IRQ_RX_DONE) == IRQ_RX_DONE) {
    if ((status & IRQ_CRC_ERROR) != IRQ_CRC_ERROR) {
      this->read_opcode_(RADIO_GET_PACKETSTATUS, buf, 3);
      if (this->modulation_ == PACKET_TYPE_LORA) {
        rssi = buf[0];
        snr = buf[1];
      } else {
        rssi = buf[2];
        snr = 0;
      }
      this->read_opcode_(RADIO_GET_RXBUFFERSTATUS, buf, 2);
      this->packet_.resize(buf[0]);
      this->read_fifo_(buf[1], this->packet_);
      this->call_listeners_(this->packet_, (float) rssi / -2.0f, (float) snr / 4.0f);
    }
  }
}

void SX126x::run_image_cal() {
  // the following values were taken from section 9.2.1 table 9-2
  // in rev 2.1 of the datasheet
  uint8_t buf[2] = {0, 0};
  if (this->frequency_ > 900000000) {
    buf[0] = 0xE1;
    buf[1] = 0xE9;
  } else if (this->frequency_ > 850000000) {
    buf[0] = 0xD7;
    buf[1] = 0xD8;
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
    this->write_opcode_(RADIO_CALIBRATEIMAGE, buf, 2);
  }
}

void SX126x::set_mode_rx() {
  uint8_t buf[8];

  if (this->pa_ctx_pin_) {
    if (this->pa_ctx_pin_->digital_read()) {
      ESP_LOGV(TAG, "setting pa_ctx_pin %d LOW for RX", static_cast<InternalGPIOPin *>(this->pa_ctx_pin_)->get_pin());
      this->pa_ctx_pin_->digital_write(false);
      delay(1);  // let pa switch stabilize
    }
  }

  // configure irq params
  uint16_t irq = IRQ_RX_DONE | IRQ_RX_TX_TIMEOUT | IRQ_CRC_ERROR | IRQ_PREAMBLE_DETECTED;
  buf[0] = (irq >> 8) & 0xFF;
  buf[1] = (irq >> 0) & 0xFF;
  buf[2] = (irq >> 8) & 0xFF;
  buf[3] = (irq >> 0) & 0xFF;
  buf[4] = (IRQ_RADIO_NONE >> 8) & 0xFF;
  buf[5] = (IRQ_RADIO_NONE >> 0) & 0xFF;
  buf[6] = (IRQ_RADIO_NONE >> 8) & 0xFF;
  buf[7] = (IRQ_RADIO_NONE >> 0) & 0xFF;
  this->write_opcode_(RADIO_SET_DIOIRQPARAMS, buf, 8);

  // set timeout to 0
  buf[0] = 0x00;
  this->write_opcode_(RADIO_SET_LORASYMBTIMEOUT, buf, 1);

  // switch to continuous mode rx
  buf[0] = 0xFF;
  buf[1] = 0xFF;
  buf[2] = 0xFF;
  this->write_opcode_(RADIO_SET_RX, buf, 3);
}

void SX126x::set_mode_tx() {
  uint8_t buf[8];

  if (this->pa_ctx_pin_) {
    if (!this->pa_ctx_pin_->digital_read()) {
      ESP_LOGV(TAG, "setting pa_ctx_pin %d HIGH for TX", static_cast<InternalGPIOPin *>(this->pa_ctx_pin_)->get_pin());
      this->pa_ctx_pin_->digital_write(true);
      delay(2);  // let pa stabilize
    }
  }

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
  this->write_opcode_(RADIO_SET_DIOIRQPARAMS, buf, 8);

  // switch to single mode tx
  buf[0] = 0x00;
  buf[1] = 0x00;
  buf[2] = 0x00;
  this->write_opcode_(RADIO_SET_TX, buf, 3);
}

void SX126x::set_mode_sleep(bool cold) {
  // 0x04 = warm start (config retained), 0x00 = cold start (config lost, lowest power)
  uint8_t buf[1];
  buf[0] = cold ? 0x00 : 0x04;
  this->write_opcode_(RADIO_SET_SLEEP, buf, 1);

  if (this->pa_ctx_pin_) {
    ESP_LOGV(TAG, "setting pa_ctx_pin %d LOW for SLEEP", static_cast<InternalGPIOPin *>(this->pa_ctx_pin_)->get_pin());
    this->pa_ctx_pin_->digital_write(false);
    // no delay, we're going to sleep anyway
  }
}

void SX126x::set_mode_standby(SX126xStandbyMode mode) {
  uint8_t buf[1];
  buf[0] = mode;
  this->write_opcode_(RADIO_SET_STANDBY, buf, 1);

  if (this->pa_ctx_pin_) {
    this->pa_ctx_pin_->digital_write(false);
    // no delay, we're going to standby anyway
  }
}

void SX126x::wait_busy_() {
  // wait if the device is busy, the maximum delay is only be a few ms
  // with most commands taking only a few us
  uint32_t start = millis();
  while (this->busy_pin_->digital_read()) {
    if (millis() - start > BUSY_TIMEOUT_MS) {
      ESP_LOGE(TAG, "Wait busy timeout");
      this->mark_failed();
      break;
    }
  }
}

void SX126x::dump_config() {
  ESP_LOGCONFIG(TAG, "SX126x:");
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_PIN("  BUSY Pin: ", this->busy_pin_);
  LOG_PIN("  RST Pin: ", this->rst_pin_);
  LOG_PIN("  DIO1 Pin: ", this->dio1_pin_);
  if (this->pa_ctx_pin_) {
    LOG_PIN("  PA_CTX Pin: ", this->pa_ctx_pin_);
  }
  ESP_LOGCONFIG(TAG,
                "  HW Version: %15s\n"
                "  Frequency: %" PRIu32 " Hz\n"
                "  Bandwidth: %" PRIu32 " Hz\n"
                "  PA Power: %" PRId8 " dBm\n"
                "  PA Ramp: %" PRIu16 " us\n"
                "  Payload Length: %" PRIu32 "\n"
                "  CRC Enable: %s\n"
                "  Rx Start: %s",
                this->version_, this->frequency_, BW_HZ[this->bandwidth_], this->pa_power_, RAMP[this->pa_ramp_],
                this->payload_length_, TRUEFALSE(this->crc_enable_), TRUEFALSE(this->rx_start_));
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
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Configuring SX126x failed");
  }
}

// invoke the sx126x channel air detection mode to determine if the channel is free to transmit
// this is an efficient, hardware-enabled 'listen-before-talk' implementation.
// before invoking the hardware CAD mode, we first check to see if we're in the middle of receiving a
// LORA packet -- based on the radio registers indicating that a preamble packet has been received,
// but the packet is not yet 'done'.
bool SX126x::scan_channel_clear() {
  ESP_LOGV(TAG, "CAD detection started");

  // check IRQ register directly to determine if radio is mid-packet
  {
    uint8_t irq_raw[2] = {0, 0};
    this->read_opcode_(RADIO_GET_IRQSTATUS, irq_raw, 2);
    uint16_t flags = ((uint16_t) irq_raw[0] << 8) | irq_raw[1];
    if ((flags & IRQ_PREAMBLE_DETECTED) && !(flags & IRQ_RX_DONE)) {
      ESP_LOGD(TAG, "CAD skipped: preamble in register (0x%04X)", flags);
      return false;  // report busy - caller should retry
    }
  }

  // 1. standby
  this->set_mode_standby(STDBY_XOSC);

  // 2. Configure IRQ for CAD
  uint16_t irq_mask = IRQ_CAD_DONE | IRQ_CAD_ACTIVITY_DETECTED;

  uint8_t irq_buf[8] = {
      (uint8_t) ((irq_mask >> 8) & 0xFF),  // global IRQ mask high — flags still set in register
      (uint8_t) (irq_mask & 0xFF),         // global IRQ mask low
      0x00,
      0x00,  // DIO1: assert nothing
      0x00,
      0x00,  // DIO2: assert nothing — won't disturb RF switch
      0x00,
      0x00  // DIO3: assert nothing
  };
  this->write_opcode_(RADIO_SET_DIOIRQPARAMS, irq_buf, 8);

  // 3. Clear stale IRQs
  uint8_t clr[2] = {0xFF, 0xFF};
  this->write_opcode_(RADIO_CLR_IRQSTATUS, clr, 2);

  // 4. Set CAD parameters - AN1200.48 recommended values
  static const uint8_t CAD_DET_PEAK[6] = {22, 22, 24, 25, 26, 30};  // SF7..SF12
  uint8_t sf = this->spreading_factor_;
  uint8_t det_peak = (sf >= 7 && sf <= 12) ? CAD_DET_PEAK[sf - 7] : 22;

  uint8_t cad_params[7] = {
      0x01,  // cadSymbolNum: 2 symbols (CAD_ON_2_SYMB)
      det_peak,
      10,                   // cadDetMin
      0x00,                 // cadExitMode, STDBY_RC after CAD done
      0x00,     0x00, 0x00  // cadTimeout: 0
  };
  this->write_opcode_(RADIO_SET_CADPARAMS, cad_params, 7);

  // 5. Start CAD
  this->write_opcode_(RADIO_SET_CAD, nullptr, 0);

  // 6. Poll IRQ register (not DIO1) to avoid loop() race
  uint16_t irq_flags = 0;
  uint32_t start = millis();
  while (!(irq_flags & (IRQ_CAD_DONE | IRQ_CAD_ACTIVITY_DETECTED))) {
    ESP_LOGV(TAG, "CAD detect loop");
    if (millis() - start > 100) {  // 100ms timeout
      ESP_LOGW(TAG, "CAD timeout, returning busy");
      this->set_mode_rx();
      return false;  // cad detected busy airtime
    }
    delayMicroseconds(200);
    uint8_t raw[2] = {0, 0};
    this->read_opcode_(RADIO_GET_IRQSTATUS, raw, 2);
    irq_flags = ((uint16_t) raw[0] << 8) | raw[1];
  }

  // 7. Clear CAD IRQ flags
  uint8_t clr2[2] = {(uint8_t) ((irq_flags >> 8) & 0xFF), (uint8_t) (irq_flags & 0xFF)};
  this->write_opcode_(RADIO_CLR_IRQSTATUS, clr2, 2);

  // 8. Restore RX
  this->set_mode_rx();  // also resets radio_state_ to RX_LISTENING

  // 9. Result
  bool clear = !(irq_flags & IRQ_CAD_ACTIVITY_DETECTED);
  ESP_LOGV(TAG, "CAD result: %s (IRQ=0x%04X, SF%u)", clear ? "clear" : "busy", irq_flags, sf);

  return clear;  // ? CadResult::CLEAR : CadResult::BUSY;
}

}  // namespace esphome::sx126x
