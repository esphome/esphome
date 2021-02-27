#include "esphome/core/log.h"
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
  this->set_sync_word(0x34);
  this->disable_crc();
  this->enable_crc();
  this->idle();
}

// void SX1276::update() {
//   std::string to_write = "Hello";
//   this->begin_packet();
//   this->send_text_printf("Hello");
//   this->write("%d", this->counter_++);
//   this->end_packet();
//   ESP_LOGD(TAG, "Updated");
// }

void SX1276::send_printf(const char *format, ...) {
  va_list arg;
  va_start(arg, format);
  char buffer[256];
  int ret = vsnprintf(buffer, sizeof(buffer), format, arg);
  va_end(arg);
  if (ret > 0)
    this->write(buffer, ret);
}

void SX1276::dump_config() {
  ESP_LOGCONFIG(TAG, "SX1276");
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_PIN("  DI0 Pin: ", this->di0_pin_);
  LOG_PIN("  RST Pin: ", this->rst_pin_);
  ESP_LOGCONFIG(TAG, "  Band: %d MHz", this->band_);
}

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
  long freq = this->band_ * 1000000;

  uint64_t frf = ((uint64_t) freq << 19) / 32000000;
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
