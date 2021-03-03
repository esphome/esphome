#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "sx1276.h"

namespace esphome {
namespace sx1276 {

static const char *TAG = "sx1276";

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
  this->idle();
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
}

void SX1276::dump_config() {
  ESP_LOGCONFIG(TAG, "SX1276");
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_PIN("  DI0 Pin: ", this->di0_pin_);
  LOG_PIN("  RST Pin: ", this->rst_pin_);
  ESP_LOGCONFIG(TAG, "  Band: %d MHz", this->band_);
  ESP_LOGCONFIG(TAG, "  Sync Word: 0x%2X", this->sync_word_);
}

void SX1276::loop() {
  int packet_size = this->parse_packet();

  if (packet_size) {
    lora::LoraPacket lora_packet;
    std::string string_to_split;

    bool full_packet = false;

    while (this->available()) {
      string_to_split += static_cast<char>(this->read());

      std::ptrdiff_t const match_count(std::distance(
          std::sregex_iterator(string_to_split.begin(), string_to_split.end(), this->lora_regex_delimiter_),
          std::sregex_iterator()));

      if (match_count == 4) {
        full_packet = true;
        break;
      }
    }

    if (full_packet) {
      if (this->available())
        ESP_LOGE(TAG, "full packet and more to available");

      ESP_LOGD(TAG, "stringToSplit %s", string_to_split.c_str());

      std::vector<std::string> c(
          std::sregex_token_iterator(string_to_split.begin(), string_to_split.end(), this->lora_regex_delimiter_, -1),
          {});

      lora_packet.appname = c[0];
      lora_packet.component_type = strtol(c[1].c_str(), nullptr, 10);
      lora_packet.component_name = c[2];

      auto state = parse_float(c[3]);
      if (!state.has_value()) {
        ESP_LOGE(TAG, "Can't convert '%s' to float!", c[4].c_str());
        return;
      }
      lora_packet.state = *state;

      lora_packet.rssi = this->packet_rssi();

      LOG_LORA_PACKET(lora_packet) this->process_lora_packet(lora_packet);
    } else {
      ESP_LOGE(TAG, "Received bad packet");
      return;
    }
  }
}  // namespace sx1276

int SX1276::read() {
  if (!this->available()) {
    return -1;
  }
  this->packet_index_++;
  return this->read_register(REG_FIFO);
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
