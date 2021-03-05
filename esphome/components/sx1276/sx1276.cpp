#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "sx1276.h"

namespace esphome {
namespace sx1276 {

static const char *TAG = "sx1276";

void ICACHE_RAM_ATTR LoraComponentStore::gpio_intr(LoraComponentStore *arg) {
  arg->last_interrupt = micros();
  arg->found_packet = false;
  arg->sx1276->handle_di0();
}

void SX1276::setup() {
  ESP_LOGCONFIG(TAG, "Setting up SX1276...");
  this->spi_setup();

  this->rst_pin_->setup();
  this->di0_pin_->setup();

  this->cs_->setup();

  this->rst_pin_->digital_write(LOW);
  delay(20);
  this->rst_pin_->digital_write(HIGH);
  delay(50);  // NOLINT

  this->cs_->digital_write(true);

  uint8_t version = read_register(REG_VERSION);

  if (version != 0x12) {
    this->mark_failed();
    return;
  }

  // put in sleep mode
  SX1276::sleep();
  // set frequency
  SX1276::set_frequency();

  // set base addresses
  this->write_register(REG_FIFO_TX_BASE_ADDR, 0);
  this->write_register(REG_FIFO_RX_BASE_ADDR, 0);

  // set LNA boost
  this->write_register(REG_LNA, this->read_register(REG_LNA) | 0x03);
  // set auto AGC
  this->write_register(REG_MODEM_CONFIG_3, 0x04);

  // set output power to 14 dBm

  this->set_tx_power(14, RF_PACONFIG_PASELECT_PABOOST);
  //   else
  //     set_tx_power(14, RF_PACONFIG_PASELECT_RFO);

  this->set_spreading_factor(11);
  // put in standby mode
  this->set_signal_bandwidth(125E3);
  // setCodingRate4(5);

  this->set_sync_word(this->sync_word_);

  this->disable_crc();
  this->enable_crc();
  ESP_LOGD(TAG, "Idle");
  this->idle();

  ESP_LOGD(TAG, "Attaching interrupt");

  this->store_.sx1276 = this;
  this->store_.receive_buffer = &this->receive_buffer_;
  this->di0_pin_->attach_interrupt(LoraComponentStore::gpio_intr, &this->store_, CHANGE);

  ESP_LOGD(TAG, "Done attaching interrupt");
  this->receive();  // go back into receive mode
}

void SX1276::send_printf(const char *format, ...) {
  va_list arg;
  va_start(arg, format);
  char buffer[256];
  int ret = vsnprintf(buffer, sizeof(buffer), format, arg);
  va_end(arg);
  ESP_LOGD(TAG, "Sending %s %d", buffer, ret);
  if (ret > 0) {
    this->begin_packet();
    this->write(buffer, ret);
    this->end_packet();
  }
  this->receive();  // go back into receive mode
}

void SX1276::dump_config() {
  ESP_LOGCONFIG(TAG, "SX1276");
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_PIN("  DI0 Pin: ", this->di0_pin_);
  LOG_PIN("  RST Pin: ", this->rst_pin_);
  ESP_LOGCONFIG(TAG, "  Band: %d MHz", this->band_);
  ESP_LOGCONFIG(TAG, "  Sync Word: 0x%2X", this->sync_word_);
}

uint32_t oldvalue = 0;
void SX1276::loop() {
  if (oldvalue != this->store_.last_interrupt) {
    ESP_LOGD(TAG, "Loop %zu found packet %s receive_buffer_ %zu packetLength %d todelete %d",
             this->store_.last_interrupt, YESNO(this->store_.found_packet), this->receive_buffer_.length(),
             this->store_.packetLength, this->store_.todelete);
    oldvalue = this->store_.last_interrupt;
  }
  this->parse_buffer();

  while (!this->lora_packets_.empty()) {
    auto lora_packet = this->lora_packets_.front();
    this->process_lora_packet(lora_packet);
    this->lora_packets_.pop_front();
  }

  this->receive();  // go back into receive mode
}

// void SX1276::loop2() {

// }

void SX1276::parse_buffer() {
  lora::LoraPacket lora_packet;

  std::ptrdiff_t const match_count(std::distance(
      std::sregex_iterator(this->receive_buffer_.begin(), this->receive_buffer_.end(), this->lora_regex_delimiter_),
      std::sregex_iterator()));

  if (match_count < 4)
    return;

  this->store_.found_packet = true;
  int loop = match_count / 4;

  // JUST GET THE 4 |

  int todelete = 0;
  std::vector<std::string> c(std::sregex_token_iterator(this->receive_buffer_.begin(), this->receive_buffer_.end(),
                                                        this->lora_regex_delimiter_, -1),
                             {});

  ESP_LOGD(TAG, "receive_buffer_ %s", this->receive_buffer_.c_str());
  for (int i = 0; i < loop; ++i) {
    int offset = i * 4;

    todelete += c[0 + offset].length() + 1 + c[1 + offset].length() + 1 + c[2 + offset].length() + 1 +
                c[3 + offset].length() + 1;

    lora_packet.appname = c[0 + offset];
    lora_packet.component_type = strtol(c[1 + offset].c_str(), nullptr, 10);
    lora_packet.component_name = c[2 + offset];

    // if (lora_packet.component_name == "switch_binary_sensor_1") {
    //   ESP_LOGD(TAG, "Can't convert '%s' to float!", c[3 + offset].c_str());
    // }
    //    auto state = parse_float(c[3 + offset)].substr(0, c[3 + offset].size() - 1));
    auto state = parse_float(c[3 + offset]);
    if (!state.has_value()) {
      state = parse_float(c[3 + offset].substr(0, c[3 + offset].size() - 1));
      if (!state.has_value()) {
        ESP_LOGE(TAG, "Can't convert '%s' to float!", c[3 + offset].c_str());
        return;
      }
    }

    lora_packet.state = *state;

    lora_packet.rssi = this->packet_rssi();
    lora_packet.snr = this->packet_snr();
    this->lora_packets_.push_back(lora_packet);
  }
  ESP_LOGD(TAG, "Deleting %d", todelete);
  this->store_.todelete = todelete;

  this->receive_buffer_.erase(0, todelete);
}  // namespace sx1276

int SX1276::read() {
  if (!this->available()) {
    return -1;
  }
  this->packet_index_++;
  return this->read_register(REG_FIFO);
}

void SX1276::handle_di0() {
  int irqFlags = this->read_register(REG_IRQ_FLAGS);
  // clear IRQ's
  this->write_register(REG_IRQ_FLAGS, irqFlags);
  if ((irqFlags & IRQ_PAYLOAD_CRC_ERROR_MASK) == 0) {
    // received a packet
    this->packet_index_ = 0;
    // read packet length
    int packetLength =
        this->implicit_header_mode_ ? this->read_register(REG_PAYLOAD_LENGTH) : this->read_register(REG_RX_NB_BYTES);
    // set FIFO address to current RX address
    this->store_.packetLength = packetLength;

    this->write_register(REG_FIFO_ADDR_PTR, this->read_register(REG_FIFO_RX_CURRENT_ADDR));
    // this->read_available(packetLength);

    if (packetLength != 0)
      while (this->available())
        this->receive_buffer_ += this->read();

    // reset FIFO address
    this->write_register(REG_FIFO_ADDR_PTR, 0);
  }
}

int SX1276::parse_packet(int size) {
  int packet_length = 0;
  int irq_flags = this->read_register(REG_IRQ_FLAGS);

  if (size > 0) {
    this->implicit_header_mode();

    this->write_register(REG_PAYLOAD_LENGTH, size & 0xff);
  } else {
    this->explicit_header_mode();
  }

  // clear IRQ's
  this->write_register(REG_IRQ_FLAGS, irq_flags);

  if ((irq_flags & IRQ_RX_DONE_MASK) && (irq_flags & IRQ_PAYLOAD_CRC_ERROR_MASK) == 0) {
    // received a packet
    this->packet_index_ = 0;

    // read packet length
    if (this->implicit_header_mode_) {
      packet_length = this->read_register(REG_PAYLOAD_LENGTH);
    } else {
      packet_length = this->read_register(REG_RX_NB_BYTES);
    }

    // set FIFO address to current RX address
    this->write_register(REG_FIFO_ADDR_PTR, this->read_register(REG_FIFO_RX_CURRENT_ADDR));

    // put in standby mode
    idle();
  } else if (this->read_register(REG_OP_MODE) != (MODE_LONG_RANGE_MODE | MODE_RX_SINGLE)) {
    // not currently in RX mode

    // reset FIFO address
    this->write_register(REG_FIFO_ADDR_PTR, 0);

    // put in single RX mode
    this->write_register(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_RX_SINGLE);
  }

  return packet_length;
}

float SX1276::packet_snr() { return ((int8_t) this->read_register(REG_PKT_SNR_VALUE)) * 0.25; }

int SX1276::packet_rssi() {
  int8_t snr = 0;
  int8_t snr_value = this->read_register(0x19);
  int16_t rssi = this->read_register(REG_PKT_RSSI_VALUE);

  if (snr_value & 0x80)  // The SNR sign bit is 1
  {
    // Invert and divide by 4
    snr = ((~snr_value + 1) & 0xFF) >> 2;
    snr = -snr;
  } else {
    // Divide by 4
    snr = (snr_value & 0xFF) >> 2;
  }
  if (snr < 0) {
    rssi = rssi - (this->frequency_ < 525E6 ? 164 : 157) + (rssi >> 4) + snr;
  } else {
    rssi = rssi - (this->frequency_ < 525E6 ? 164 : 157) + (rssi >> 4);
  }

  return (rssi);
}

int SX1276::available() { return (this->read_register(REG_RX_NB_BYTES) - this->packet_index_); }

void SX1276::receive(int size) {
  if (size > 0) {
    implicit_header_mode();
    write_register(REG_PAYLOAD_LENGTH, size & 0xff);
  } else {
    explicit_header_mode();
  }

  write_register(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_RX_CONTINUOUS);
}

size_t SX1276::write(const char *buffer, int size) {
  int current_length = read_register(REG_PAYLOAD_LENGTH);
  // check size
  if ((current_length + size) > MAX_PKT_LENGTH) {
    size = MAX_PKT_LENGTH - current_length;
  }
  // write data
  for (size_t i = 0; i < size; i++) {
    write_register(REG_FIFO, buffer[i]);
  }
  // update length
  write_register(REG_PAYLOAD_LENGTH, current_length + size);
  return size;
}

int SX1276::end_packet(bool async) {
  // put in TX mode
  write_register(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_TX);

  // wait for TX done
  while ((read_register(REG_IRQ_FLAGS) & IRQ_TX_DONE_MASK) == 0) {
    yield();
  }
  // clear IRQ's
  write_register(REG_IRQ_FLAGS, IRQ_TX_DONE_MASK);

  return 1;
}

int SX1276::begin_packet(int implicit_header) {
  // put in standby mode
  this->idle();
  if (implicit_header) {
    this->implicit_header_mode();
  } else {
    this->explicit_header_mode();
  }
  // reset FIFO address and paload length
  this->write_register(REG_FIFO_ADDR_PTR, 0);
  this->write_register(REG_PAYLOAD_LENGTH, 0);
  return 1;
}

void SX1276::explicit_header_mode() {
  implicit_header_mode_ = 0;
  this->write_register(REG_MODEM_CONFIG_1, read_register(REG_MODEM_CONFIG_1) & 0xfe);
}

void SX1276::implicit_header_mode() {
  implicit_header_mode_ = 1;
  this->write_register(REG_MODEM_CONFIG_1, read_register(REG_MODEM_CONFIG_1) | 0x01);
}

void SX1276::sleep() { write_register(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_SLEEP); }

void SX1276::set_frequency() {
  this->frequency_ = this->band_ * 1000000;

  uint64_t frf = ((uint64_t) this->frequency_ << 19) / 32000000;
  write_register(REG_FRF_MSB, (uint8_t)(frf >> 16));
  write_register(REG_FRF_MID, (uint8_t)(frf >> 8));
  write_register(REG_FRF_LSB, (uint8_t)(frf >> 0));
}

void SX1276::set_signal_bandwidth(double sbw) {
  int bw;

  if (sbw <= 7.8E3) {
    bw = 0;
  } else if (sbw <= 10.4E3) {
    bw = 1;
  } else if (sbw <= 15.6E3) {
    bw = 2;
  } else if (sbw <= 20.8E3) {
    bw = 3;
  } else if (sbw <= 31.25E3) {
    bw = 4;
  } else if (sbw <= 41.7E3) {
    bw = 5;
  } else if (sbw <= 62.5E3) {
    bw = 6;
  } else if (sbw <= 125E3) {
    bw = 7;
  } else if (sbw <= 250E3) {
    bw = 8;
  } else /*if (sbw <= 250E3)*/ {
    bw = 9;
  }
  write_register(REG_MODEM_CONFIG_1, (read_register(REG_MODEM_CONFIG_1) & 0x0f) | (bw << 4));
}

void SX1276::set_sync_word(int sw) { write_register(REG_SYNC_WORD, sw); }

void SX1276::enable_crc() { write_register(REG_MODEM_CONFIG_2, read_register(REG_MODEM_CONFIG_2) | 0x04); }

void SX1276::disable_crc() { write_register(REG_MODEM_CONFIG_2, read_register(REG_MODEM_CONFIG_2) & 0xfb); }

void SX1276::idle() { write_register(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_STDBY); }

void SX1276::set_spreading_factor(int sf) {
  if (sf < 6) {
    sf = 6;
  } else if (sf > 12) {
    sf = 12;
  }
  if (sf == 6) {
    write_register(REG_DETECTION_OPTIMIZE, 0xc5);
    write_register(REG_DETECTION_THRESHOLD, 0x0c);
  } else {
    write_register(REG_DETECTION_OPTIMIZE, 0xc3);
    write_register(REG_DETECTION_THRESHOLD, 0x0a);
  }
  write_register(REG_MODEM_CONFIG_2, (read_register(REG_MODEM_CONFIG_2) & 0x0f) | ((sf << 4) & 0xf0));
}

void SX1276::set_tx_power(int8_t power, int8_t output_pin) {
  uint8_t pa_config = 0;
  uint8_t pa_dac = 0;

  pa_config = read_register(REG_PA_CONFIG);
  pa_dac = read_register(REG_PA_DAC);

  pa_config = (pa_config & RF_PACONFIG_PASELECT_MASK) | output_pin;
  pa_config = (pa_config & RF_PACONFIG_MAX_POWER_MASK) | 0x70;

  if ((pa_config & RF_PACONFIG_PASELECT_PABOOST) == RF_PACONFIG_PASELECT_PABOOST) {
    if (power > 17) {
      pa_dac = (pa_dac & RF_PADAC_20DBM_MASK) | RF_PADAC_20DBM_ON;
    } else {
      pa_dac = (pa_dac & RF_PADAC_20DBM_MASK) | RF_PADAC_20DBM_OFF;
    }
    if ((pa_dac & RF_PADAC_20DBM_ON) == RF_PADAC_20DBM_ON) {
      if (power < 5) {
        power = 5;
      }
      if (power > 20) {
        power = 20;
      }
      pa_config = (pa_config & RF_PACONFIG_OUTPUTPOWER_MASK) | (uint8_t)((uint16_t)(power - 5) & 0x0F);
    } else {
      if (power < 2) {
        power = 2;
      }
      if (power > 17) {
        power = 17;
      }
      pa_config = (pa_config & RF_PACONFIG_OUTPUTPOWER_MASK) | (uint8_t)((uint16_t)(power - 2) & 0x0F);
    }
  } else {
    if (power < -1) {
      power = -1;
    }
    if (power > 14) {
      power = 14;
    }
    pa_config = (pa_config & RF_PACONFIG_OUTPUTPOWER_MASK) | (uint8_t)((uint16_t)(power + 1) & 0x0F);
  }
  write_register(REG_PA_CONFIG, pa_config);
  write_register(REG_PA_DAC, pa_dac);
}

uint8_t SX1276::read_register(uint8_t address) { return single_transfer(address & 0x7f, 0x00); }

void SX1276::write_register(uint8_t address, uint8_t value) { single_transfer(address | 0x80, value); }

uint8_t SX1276::single_transfer(uint8_t address, uint8_t value) {
  uint8_t response;
  this->enable();
  this->transfer_byte(address);
  response = this->transfer_byte(value);
  this->disable();
  return response;
}

}  // namespace sx1276
}  // namespace esphome
