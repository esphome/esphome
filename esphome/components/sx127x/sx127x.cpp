#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "sx127x.h"

namespace esphome {
namespace sx127x {

static const char *TAG = "sx127x";

void ICACHE_RAM_ATTR LoraComponentStore::gpio_intr(LoraComponentStore *arg) {
  arg->last_interrupt = micros();
  arg->found_packet = false;
  arg->sx127x->handle_di0();
}

void SX127X::setup() {
  this->is_setup_ = false;
  ESP_LOGCONFIG(TAG, "Setting up SX127X...");
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
  SX127X::sleep();
  // set frequency
  SX127X::set_frequency();

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

  this->store_.sx127x = this;
  this->store_.receive_buffer = &this->receive_buffer_;
  this->di0_pin_->attach_interrupt(LoraComponentStore::gpio_intr, &this->store_, CHANGE);

  ESP_LOGD(TAG, "Done attaching interrupt");
  this->is_setup_ = true;
  this->receive();  // go back into receive mode
}

void SX127X::send_printf(const char *format, ...) {
  if (!this->is_setup_)
    return;

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

void SX127X::dump_config() {
  ESP_LOGCONFIG(TAG, "SX127X");
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_PIN("  DI0 Pin: ", this->di0_pin_);
  LOG_PIN("  RST Pin: ", this->rst_pin_);
  ESP_LOGCONFIG(TAG, "  Band: %d MHz", this->band_);
  ESP_LOGCONFIG(TAG, "  Sync Word: 0x%2X", this->sync_word_);
}

uint32_t oldvalue = 0;
void SX127X::loop() {
  if (oldvalue != this->store_.last_interrupt) {
    ESP_LOGD(TAG, "Loop %zu found packet %s receive_buffer_ %zu packetLength %d todelete %d badcrc %s",
             this->store_.last_interrupt, YESNO(this->store_.found_packet), this->receive_buffer_.length(),
             this->store_.packetLength, this->store_.todelete, YESNO(this->store_.badcrc));
    oldvalue = this->store_.last_interrupt;
  }
  this->parse_buffer();

  while (!this->lora_packets_.empty()) {
    auto lora_packet = this->lora_packets_.front();
    this->process_lora_packet(lora_packet);
    this->lora_packets_.pop_front();
    delete lora_packet;
  }

  this->receive();  // go back into receive mode
}

void SX127X::parse_buffer() {
  std::ptrdiff_t const match_count(
      std::distance(std::sregex_iterator(this->receive_buffer_.begin(), this->receive_buffer_.end(),
                                         this->lora_regex_group_end_delimiter_),
                    std::sregex_iterator()));

  if (match_count == 0)
    return;

  this->store_.found_packet = true;

  int todelete = 0;
  std::vector<std::string> c(std::sregex_token_iterator(this->receive_buffer_.begin(), this->receive_buffer_.end(),
                                                        this->lora_regex_group_end_delimiter_, -1),
                             {});

  ESP_LOGD(TAG, "receive_buffer_ %s %d", this->receive_buffer_.c_str(), this->receive_buffer_.length());
  for (int i = 0; i < match_count; ++i) {
    std::vector<std::string> data(std::sregex_token_iterator(c[i].begin(), c[i].end(), this->lora_regex_delimiter_, -1),
                                  {});

    lora::LoraPacket *lora_packet = new lora::LoraPacket();

    for (int index = 0; index < data.size(); ++index) {
      ESP_LOGD(TAG, "data %d %s", index, data[index].c_str());
    }

    todelete += c[i].length() + 1;

    size_t found = data[0].find(this->lora_group_start_delimiter_);

    if (data.size() != 5 || found != 0) {
      ESP_LOGE(TAG, "Bad packet %d %s %d", data.size(), data[0].c_str(), c[i].length());
      this->store_.todelete = todelete;
      this->receive_buffer_.erase(0, todelete);
      return;
    }

    auto to_test = c[i].substr(0, c[i].find_last_of(this->lora_delimiter_) + 1);
    auto test = fnv1_hash(to_test);

    ESP_LOGD(TAG, "CRC %s %d %d %s", to_test.c_str(), to_test.length(), test, data[4].c_str());
    if (to_string(test) != data[4]) {
      ESP_LOGE(TAG, "Bad CRC %d %s %d", data.size(), data[0].c_str(), c[i].length());
      this->store_.todelete = todelete;
      this->receive_buffer_.erase(0, todelete);
      return;
    }

    lora_packet->appname = data[0].substr(1, std::string::npos);
    lora_packet->component_type = strtol(data[1].c_str(), nullptr, 10);
    lora_packet->component_name = data[2];

    if (lora_packet->component_type == 3) {
      lora_packet->state_str = data[3];
    } else {
      auto state = parse_float(data[3]);
      if (!state.has_value()) {
        state = parse_float(data[3].substr(0, data[3].size() - 1));
        if (!state.has_value()) {
          ESP_LOGE(TAG, "Can't convert '%s' to float! %s", data[3].c_str(),
                   data[3].substr(0, data[3].size() - 1).c_str());
          ESP_LOGD(TAG, "Deleting %d", todelete);
          this->store_.todelete = todelete;
          this->receive_buffer_.erase(0, todelete);
          return;
        }
      }
      lora_packet->state = *state;
    }

    lora_packet->rssi = this->packet_rssi();
    lora_packet->snr = this->packet_snr();
    this->lora_packets_.push_back(lora_packet);
  }

  ESP_LOGD(TAG, "Deleting %d", todelete);
  this->store_.todelete = todelete;
  this->receive_buffer_.erase(0, todelete);
}

int SX127X::read() {
  if (!this->available()) {
    return -1;
  }
  this->packet_index_++;
  return this->read_register(REG_FIFO);
}

void SX127X::handle_di0() {
  this->store_.badcrc = false;
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
        this->receive_buffer_ += (char) this->read();

    // reset FIFO address
    this->write_register(REG_FIFO_ADDR_PTR, 0);
  } else {
    // Bad CRC, flush the fifo buffer
    this->store_.badcrc = true;
    while (this->available())
      this->read();
  }
}

int SX127X::parse_packet(int size) {
  this->store_.badcrc = false;
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

  if ((irq_flags & IRQ_RX_DONE_MASK) == 0) {
    if ((irq_flags & IRQ_PAYLOAD_CRC_ERROR_MASK) == 0) {
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
    } else {
      // Bad CRC, flush the fifo buffer
      this->store_.badcrc = true;
      while (this->available())
        this->read();
      return 0;
    }
  } else if (this->read_register(REG_OP_MODE) != (MODE_LONG_RANGE_MODE | MODE_RX_SINGLE)) {
    // not currently in RX mode

    // reset FIFO address
    this->write_register(REG_FIFO_ADDR_PTR, 0);

    // put in single RX mode
    this->write_register(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_RX_SINGLE);
  }
  return packet_length;
}

float SX127X::packet_snr() { return ((int8_t) this->read_register(REG_PKT_SNR_VALUE)) * 0.25; }

int SX127X::packet_rssi() {
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

int SX127X::available() { return (this->read_register(REG_RX_NB_BYTES) - this->packet_index_); }

void SX127X::receive(int size) {
  if (size > 0) {
    implicit_header_mode();
    write_register(REG_PAYLOAD_LENGTH, size & 0xff);
  } else {
    explicit_header_mode();
  }

  write_register(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_RX_CONTINUOUS);
}

size_t SX127X::write(const char *buffer, int size) {
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

int SX127X::end_packet(bool async) {
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

int SX127X::begin_packet(int implicit_header) {
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

void SX127X::explicit_header_mode() {
  this->implicit_header_mode_ = 0;
  this->write_register(REG_MODEM_CONFIG_1, read_register(REG_MODEM_CONFIG_1) & 0xfe);
}

void SX127X::implicit_header_mode() {
  this->implicit_header_mode_ = 1;
  this->write_register(REG_MODEM_CONFIG_1, read_register(REG_MODEM_CONFIG_1) | 0x01);
}

void SX127X::sleep() { write_register(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_SLEEP); }

void SX127X::set_frequency() {
  this->frequency_ = this->band_ * 1000000;

  uint64_t frf = ((uint64_t) this->frequency_ << 19) / 32000000;
  write_register(REG_FRF_MSB, (uint8_t)(frf >> 16));
  write_register(REG_FRF_MID, (uint8_t)(frf >> 8));
  write_register(REG_FRF_LSB, (uint8_t)(frf >> 0));
}

void SX127X::set_signal_bandwidth(double sbw) {
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

void SX127X::set_sync_word(int sw) { write_register(REG_SYNC_WORD, sw); }

void SX127X::enable_crc() { write_register(REG_MODEM_CONFIG_2, read_register(REG_MODEM_CONFIG_2) | 0x04); }

void SX127X::disable_crc() { write_register(REG_MODEM_CONFIG_2, read_register(REG_MODEM_CONFIG_2) & 0xfb); }

void SX127X::idle() { write_register(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_STDBY); }

void SX127X::set_spreading_factor(int sf) {
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

void SX127X::set_tx_power(int8_t power, int8_t output_pin) {
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

uint8_t SX127X::read_register(uint8_t address) { return single_transfer(address & 0x7f, 0x00); }

void SX127X::write_register(uint8_t address, uint8_t value) { single_transfer(address | 0x80, value); }

uint8_t SX127X::single_transfer(uint8_t address, uint8_t value) {
  uint8_t response;
  this->enable();
  this->transfer_byte(address);
  response = this->transfer_byte(value);
  this->disable();
  return response;
}

}  // namespace sx127x
}  // namespace esphome
