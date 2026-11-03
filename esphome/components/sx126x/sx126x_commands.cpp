#include "sx126x_commands.h"
#include "esphome/core/hal.h"

namespace esphome::sx126x {
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

// OCP (Over Current Protection) values
static constexpr uint8_t OCP_80MA = 0x18;   // 80 mA max current
static constexpr uint8_t OCP_140MA = 0x38;  // 140 mA max current

// LoRa low data rate optimization threshold
static constexpr float LOW_DATA_RATE_OPTIMIZE_THRESHOLD = 16.38f;  // 16.38 ms

bool Sx126xCommands::read_fifo_(uint8_t offset, std::vector<uint8_t> &packet) {
  const uint8_t header[] = {RADIO_READ_BUFFER, offset, 0};
  return this->spi_device_.read(header, packet);
}

bool Sx126xCommands::write_fifo_(uint8_t offset, const uint8_t *data, size_t len) {
  const uint8_t header[] = {RADIO_WRITE_BUFFER, offset};
  return this->spi_device_.write(header, {data, len});
}

bool Sx126xCommands::read_opcode_(uint8_t opcode, uint8_t *data, uint8_t size) {
  const uint8_t header[] = {opcode, 0};
  return this->spi_device_.read(header, {data, size});
}

bool Sx126xCommands::write_opcode_(uint8_t opcode, const uint8_t *data, uint8_t size) {
  return this->spi_device_.write({&opcode, 1}, {data, size});
}

bool Sx126xCommands::read_register_(uint16_t reg, uint8_t *data, uint8_t size) {
  const uint8_t header[] = {RADIO_READ_REGISTER, static_cast<uint8_t>(reg >> 8), static_cast<uint8_t>(reg), 0};
  return this->spi_device_.read(header, {data, size});
}

bool Sx126xCommands::write_register_(uint16_t reg, const uint8_t *data, uint8_t size) {
  const uint8_t header[] = {RADIO_WRITE_REGISTER, static_cast<uint8_t>(reg >> 8), static_cast<uint8_t>(reg)};
  return this->spi_device_.write(header, {data, size});
}

ConfigurationResult Sx126xCommands::configure(const RadioProfile &profile, std::span<char, 16> version) {
  uint8_t buf[8];
  // wakeup
  if (!this->read_opcode_(RADIO_GET_STATUS, nullptr, 0)) {
    return ConfigurationResult::CONFIGURATION_RESULT_BUSY_TIMEOUT;
  }

  // config tcxo
  if (profile.tcxo_voltage != TCXO_CTRL_NONE) {
    uint32_t delay = profile.tcxo_delay >> 6;
    buf[0] = profile.tcxo_voltage;
    buf[1] = (delay >> 16) & 0xFF;
    buf[2] = (delay >> 8) & 0xFF;
    buf[3] = (delay >> 0) & 0xFF;
    if (!this->write_opcode_(RADIO_SET_TCXOMODE, buf, 4)) {
      return ConfigurationResult::CONFIGURATION_RESULT_BUSY_TIMEOUT;
    }
    buf[0] = 0x7F;
    if (!this->write_opcode_(RADIO_CALIBRATE, buf, 1)) {
      return ConfigurationResult::CONFIGURATION_RESULT_BUSY_TIMEOUT;
    }
  }

  // clear errors
  buf[0] = 0x00;
  buf[1] = 0x00;
  if (!this->write_opcode_(RADIO_CLR_ERROR, buf, 2)) {
    return ConfigurationResult::CONFIGURATION_RESULT_BUSY_TIMEOUT;
  }

  // rf switch
  if (profile.rf_switch) {
    buf[0] = 0x01;
    if (!this->write_opcode_(RADIO_SET_RFSWITCHMODE, buf, 1)) {
      return ConfigurationResult::CONFIGURATION_RESULT_BUSY_TIMEOUT;
    }
  }

  // check silicon version to make sure hw is ok
  if (!this->read_register_(REG_VERSION_STRING, reinterpret_cast<uint8_t *>(version.data()), version.size())) {
    return ConfigurationResult::CONFIGURATION_RESULT_BUSY_TIMEOUT;
  }
  version[version.size() - 1] = '\0';
  if (strncmp(version.data(), "SX126", 5) != 0 && strncmp(version.data(), "LLCC68", 6) != 0) {
    return ConfigurationResult::CONFIGURATION_RESULT_UNSUPPORTED;
  }

  // setup packet type
  buf[0] = profile.modulation;
  if (!this->write_opcode_(RADIO_SET_PACKETTYPE, buf, 1)) {
    return ConfigurationResult::CONFIGURATION_RESULT_BUSY_TIMEOUT;
  }

  // calibrate image
  if (!this->calibrate_image(profile.frequency)) {
    return ConfigurationResult::CONFIGURATION_RESULT_BUSY_TIMEOUT;
  }

  // set frequency
  uint64_t freq = ((uint64_t) profile.frequency << 25) / XTAL_FREQ;
  buf[0] = (uint8_t) ((freq >> 24) & 0xFF);
  buf[1] = (uint8_t) ((freq >> 16) & 0xFF);
  buf[2] = (uint8_t) ((freq >> 8) & 0xFF);
  buf[3] = (uint8_t) (freq & 0xFF);
  if (!this->write_opcode_(RADIO_SET_RFFREQUENCY, buf, 4)) {
    return ConfigurationResult::CONFIGURATION_RESULT_BUSY_TIMEOUT;
  }

  // configure pa
  int8_t pa_power = profile.pa_power;
  if (profile.sx1261) {
    // the following values were taken from section 13.1.14.1 table 13-21
    // in rev 2.1 of the datasheet
    if (pa_power == 15) {
      uint8_t cfg[4] = {0x06, 0x00, 0x01, 0x01};
      if (!this->write_opcode_(RADIO_SET_PACONFIG, cfg, 4)) {
        return ConfigurationResult::CONFIGURATION_RESULT_BUSY_TIMEOUT;
      }
    } else {
      uint8_t cfg[4] = {0x04, 0x00, 0x01, 0x01};
      if (!this->write_opcode_(RADIO_SET_PACONFIG, cfg, 4)) {
        return ConfigurationResult::CONFIGURATION_RESULT_BUSY_TIMEOUT;
      }
    }
    pa_power = std::max(pa_power, (int8_t) -3);
    pa_power = std::min(pa_power, (int8_t) 14);
    buf[0] = OCP_80MA;
    if (!this->write_register_(REG_OCP, buf, 1)) {
      return ConfigurationResult::CONFIGURATION_RESULT_BUSY_TIMEOUT;
    }
  } else {
    // the following values were taken from section 13.1.14.1 table 13-21
    // in rev 2.1 of the datasheet
    uint8_t cfg[4] = {0x04, 0x07, 0x00, 0x01};
    if (!this->write_opcode_(RADIO_SET_PACONFIG, cfg, 4)) {
      return ConfigurationResult::CONFIGURATION_RESULT_BUSY_TIMEOUT;
    }
    pa_power = std::max(pa_power, (int8_t) -3);
    pa_power = std::min(pa_power, (int8_t) 22);
    buf[0] = OCP_140MA;
    if (!this->write_register_(REG_OCP, buf, 1)) {
      return ConfigurationResult::CONFIGURATION_RESULT_BUSY_TIMEOUT;
    }
  }
  buf[0] = pa_power;
  buf[1] = profile.pa_ramp;
  if (!this->write_opcode_(RADIO_SET_TXPARAMS, buf, 2)) {
    return ConfigurationResult::CONFIGURATION_RESULT_BUSY_TIMEOUT;
  }

  // configure modem
  if (profile.modulation == PACKET_TYPE_LORA) {
    // set modulation params
    float duration = 1000.0f * (1UL << profile.spreading_factor) / BW_HZ[profile.bandwidth];
    buf[0] = profile.spreading_factor;
    buf[1] = progmem_read_byte(&BW_LORA[profile.bandwidth - SX126X_BW_7810]);
    buf[2] = profile.coding_rate;
    buf[3] = (duration > LOW_DATA_RATE_OPTIMIZE_THRESHOLD) ? 0x01 : 0x00;
    if (!this->write_opcode_(RADIO_SET_MODULATIONPARAMS, buf, 4)) {
      return ConfigurationResult::CONFIGURATION_RESULT_BUSY_TIMEOUT;
    }

    // set packet params and sync word
    if (!this->set_packet_params(profile, profile.max_packet_size())) {
      return ConfigurationResult::CONFIGURATION_RESULT_BUSY_TIMEOUT;
    }
    if (profile.sync_size == 2) {
      if (!this->write_register_(REG_LORA_SYNCWORD, profile.sync_value.data(), profile.sync_size)) {
        return ConfigurationResult::CONFIGURATION_RESULT_BUSY_TIMEOUT;
      }
    }
  } else {
    // set modulation params
    uint32_t bitrate = ((uint64_t) XTAL_FREQ * 32) / profile.bitrate;
    uint32_t fdev = ((uint64_t) profile.deviation << 25) / XTAL_FREQ;
    buf[0] = (bitrate >> 16) & 0xFF;
    buf[1] = (bitrate >> 8) & 0xFF;
    buf[2] = (bitrate >> 0) & 0xFF;
    buf[3] = profile.shaping;
    buf[4] = progmem_read_byte(&BW_FSK[profile.bandwidth - SX126X_BW_4800]);
    buf[5] = (fdev >> 16) & 0xFF;
    buf[6] = (fdev >> 8) & 0xFF;
    buf[7] = (fdev >> 0) & 0xFF;
    if (!this->write_opcode_(RADIO_SET_MODULATIONPARAMS, buf, 8)) {
      return ConfigurationResult::CONFIGURATION_RESULT_BUSY_TIMEOUT;
    }

    // set crc params
    if (profile.crc_enable) {
      buf[0] = profile.crc_initial >> 8;
      buf[1] = profile.crc_initial & 0xFF;
      if (!this->write_register_(REG_CRC_INITIAL, buf, 2)) {
        return ConfigurationResult::CONFIGURATION_RESULT_BUSY_TIMEOUT;
      }
      buf[0] = profile.crc_polynomial >> 8;
      buf[1] = profile.crc_polynomial & 0xFF;
      if (!this->write_register_(REG_CRC_POLYNOMIAL, buf, 2)) {
        return ConfigurationResult::CONFIGURATION_RESULT_BUSY_TIMEOUT;
      }
    }

    // set whitening params
    if (profile.whitening_enable) {
      // according to the datasheet, section 12 table 12-1 "The user should not
      // change the value of the 7 MSB of this register"
      if (!this->read_register_(REG_WHITENING_INITIAL, buf, 1)) {
        return ConfigurationResult::CONFIGURATION_RESULT_BUSY_TIMEOUT;
      }
      buf[0] = (buf[0] & 0xFE) | ((profile.whitening_initial >> 8) & 0x01);
      buf[1] = profile.whitening_initial & 0xFF;
      if (!this->write_register_(REG_WHITENING_INITIAL, buf, 2)) {
        return ConfigurationResult::CONFIGURATION_RESULT_BUSY_TIMEOUT;
      }
    }

    // set packet params and sync word
    if (!this->set_packet_params(profile, profile.max_packet_size())) {
      return ConfigurationResult::CONFIGURATION_RESULT_BUSY_TIMEOUT;
    }
    if (profile.sync_size != 0) {
      if (!this->write_register_(REG_GFSK_SYNCWORD, profile.sync_value.data(), profile.sync_size)) {
        return ConfigurationResult::CONFIGURATION_RESULT_BUSY_TIMEOUT;
      }
    }
  }

  // switch to rx or sleep
  if (profile.rx_start) {
    if (!this->receive()) {
      return ConfigurationResult::CONFIGURATION_RESULT_BUSY_TIMEOUT;
    }
  } else {
    if (!this->sleep()) {
      return ConfigurationResult::CONFIGURATION_RESULT_BUSY_TIMEOUT;
    }
  }
  return ConfigurationResult::CONFIGURATION_RESULT_OK;
}

bool Sx126xCommands::set_packet_params(const RadioProfile &profile, uint8_t payload_length) {
  uint8_t buf[9];
  if (profile.modulation == PACKET_TYPE_LORA) {
    buf[0] = (profile.preamble_size >> 8) & 0xFF;
    buf[1] = (profile.preamble_size >> 0) & 0xFF;
    buf[2] = (profile.payload_length > 0) ? 0x01 : 0x00;
    buf[3] = payload_length;
    buf[4] = (profile.crc_enable) ? 0x01 : 0x00;
    buf[5] = 0x00;
    if (!this->write_opcode_(RADIO_SET_PACKETPARAMS, buf, 6)) {
      return false;
    }
  } else {
    uint16_t preamble_size = profile.preamble_size * 8;
    buf[0] = (preamble_size >> 8) & 0xFF;
    buf[1] = (preamble_size >> 0) & 0xFF;
    buf[2] = (profile.preamble_detect > 0) ? ((profile.preamble_detect - 1) | 0x04) : 0x00;
    buf[3] = profile.sync_size * 8;
    buf[4] = 0x00;
    buf[5] = (profile.payload_length > 0) ? 0x00 : 0x01;
    buf[6] = payload_length;
    if (profile.crc_enable) {
      buf[7] = (profile.crc_inverted ? 0x04 : 0x00) + (profile.crc_size & 0x02);
    } else {
      buf[7] = 0x01;
    }
    buf[8] = (profile.whitening_enable) ? 0x01 : 0x00;
    if (!this->write_opcode_(RADIO_SET_PACKETPARAMS, buf, 9)) {
      return false;
    }
  }
  return true;
}

bool Sx126xCommands::calibrate_image(uint32_t frequency) {
  // the following values were taken from section 9.2.1 table 9-2
  // in rev 2.1 of the datasheet
  uint8_t buf[2] = {0, 0};
  if (frequency > 900000000) {
    buf[0] = 0xE1;
    buf[1] = 0xE9;
  } else if (frequency > 850000000) {
    buf[0] = 0xD7;
    buf[1] = 0xDB;
  } else if (frequency > 770000000) {
    buf[0] = 0xC1;
    buf[1] = 0xC5;
  } else if (frequency > 460000000) {
    buf[0] = 0x75;
    buf[1] = 0x81;
  } else if (frequency > 425000000) {
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

bool Sx126xCommands::receive() {
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

bool Sx126xCommands::transmit(uint32_t timeout_ms) {
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

  const uint32_t ticks = timeout_ms * 64;
  buf[0] = ticks >> 16;
  buf[1] = ticks >> 8;
  buf[2] = ticks;
  return this->write_opcode_(RADIO_SET_TX, buf, 3);
}

bool Sx126xCommands::sleep(bool cold) {
  // 0x04 = warm start (config retained), 0x00 = cold start (config lost, lowest power)
  uint8_t buf[1];
  buf[0] = cold ? 0x00 : 0x04;
  return this->write_opcode_(RADIO_SET_SLEEP, buf, 1);
}

bool Sx126xCommands::standby(SX126xStandbyMode mode) {
  uint8_t buf[1];
  buf[0] = mode;
  return this->write_opcode_(RADIO_SET_STANDBY, buf, 1);
}

uint32_t Sx126xCommands::bandwidth_hz(SX126xBw bw) { return BW_HZ[bw]; }
uint16_t Sx126xCommands::ramp_us(uint8_t ramp) { return progmem_read_uint16(&RAMP[ramp]); }
bool Sx126xCommands::clear_irqs(uint16_t mask) {
  uint8_t data[] = {static_cast<uint8_t>(mask >> 8), static_cast<uint8_t>(mask)};
  return this->write_opcode_(RADIO_CLR_IRQSTATUS, data, 2);
}
bool Sx126xCommands::read_irq(uint16_t &status) {
  uint8_t data[2];
  if (!this->read_opcode_(RADIO_GET_IRQSTATUS, data, 2)) {
    return false;
  }
  status = (uint16_t(data[0]) << 8) | data[1];
  return true;
}
bool Sx126xCommands::write_fifo(const uint8_t *data, size_t size) { return this->write_fifo_(0, data, size); }
bool Sx126xCommands::read_packet(const RadioProfile &profile, std::vector<uint8_t> &packet, float &rssi, float &snr) {
  uint8_t data[3];
  if (!this->read_opcode_(RADIO_GET_PACKETSTATUS, data, 3)) {
    return false;
  }
  rssi = (profile.modulation == PACKET_TYPE_LORA ? data[0] : data[2]) / -2.0f;
  snr = profile.modulation == PACKET_TYPE_LORA ? static_cast<int8_t>(data[1]) / 4.0f : 0;
  if (!this->read_opcode_(RADIO_GET_RXBUFFERSTATUS, data, 2)) {
    return false;
  }
  packet.resize(data[0]);
  return this->read_fifo_(data[1], packet);
}

}  // namespace esphome::sx126x
