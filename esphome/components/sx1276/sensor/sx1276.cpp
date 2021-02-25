#include "esphome/core/log.h"
#include "sx1276.h"

namespace esphome {
namespace sx1276 {

static const char *TAG = "sx1276.sensor";

void SX1276::setup() {
  ESP_LOGCONFIG(TAG, "Setting up SX1276...");
  this->spi_setup();

  this->rst_pin_->setup();
  this->di0_pin_->setup();
  this->cs_->setup();

  this->rst_pin_->digital_write(LOW);
  delay(20);
  this->rst_pin_->digital_write(HIGH);
  delay(50);

  this->cs_->digital_write(true);

  uint8_t version = readRegister(REG_VERSION);

  if (version != 0x12) {
    this->mark_failed();
    return;
  }

  // put in sleep mode
  SX1276::sleep();
  // set frequency
  SX1276::setFrequency();

  // set base addresses
  this->writeRegister(REG_FIFO_TX_BASE_ADDR, 0);
  this->writeRegister(REG_FIFO_RX_BASE_ADDR, 0);

  // set LNA boost
  this->writeRegister(REG_LNA, this->readRegister(REG_LNA) | 0x03);
  // set auto AGC
  this->writeRegister(REG_MODEM_CONFIG_3, 0x04);

  // set output power to 14 dBm

  this->setTxPower(14, RF_PACONFIG_PASELECT_PABOOST);
  //   else
  //     setTxPower(14, RF_PACONFIG_PASELECT_RFO);

  this->setSpreadingFactor(11);
  // put in standby mode
  this->setSignalBandwidth(125E3);
  // setCodingRate4(5);
  this->setSyncWord(0x34);
  this->disableCrc();
  this->enableCrc();
  this->idle();
}

void SX1276::update() {
  std::string to_write = "Hello";
  this->beginPacket();
  this->send_text_printf("Hello");
  this->write("%d", this->counter_++);
  this->endPacket();
  ESP_LOGD(TAG, "Updated");
}

void SX1276::send_text_printf(const char *format, ...) {
  va_list arg;
  va_start(arg, format);
  char buffer[256];
  int ret = vsnprintf(buffer, sizeof(buffer), format, arg);
  va_end(arg);
  if (ret > 0)
    this->write(buffer, ret);
}

void SX1276::dump_config() {
  LOG_SENSOR(TAG, "SX1276 Sensor", this);
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_PIN("  DI0 Pin: ", this->di0_pin_);
  LOG_PIN("  RST Pin: ", this->rst_pin_);
  ESP_LOGCONFIG(TAG, "  Band: %d MHz", this->band_);
  LOG_UPDATE_INTERVAL(this);
}

void SX1276::receive(int size) {
  if (size > 0) {
    implicitHeaderMode();
    writeRegister(REG_PAYLOAD_LENGTH, size & 0xff);
  } else {
    explicitHeaderMode();
  }

  writeRegister(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_RX_CONTINUOUS);
}

size_t SX1276::write(const char *buffer, int size) {
  int currentLength = readRegister(REG_PAYLOAD_LENGTH);
  // check size
  if ((currentLength + size) > MAX_PKT_LENGTH) {
    size = MAX_PKT_LENGTH - currentLength;
  }
  // write data
  for (size_t i = 0; i < size; i++) {
    writeRegister(REG_FIFO, buffer[i]);
  }
  // update length
  writeRegister(REG_PAYLOAD_LENGTH, currentLength + size);
  return size;
}

int SX1276::endPacket(bool async) {
  // put in TX mode
  writeRegister(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_TX);

  // wait for TX done
  while ((readRegister(REG_IRQ_FLAGS) & IRQ_TX_DONE_MASK) == 0) {
    yield();
  }
  // clear IRQ's
  writeRegister(REG_IRQ_FLAGS, IRQ_TX_DONE_MASK);

  return 1;
}

int SX1276::beginPacket(int implicitHeader) {
  // put in standby mode
  this->idle();
  if (implicitHeader) {
    this->implicitHeaderMode();
  } else {
    this->explicitHeaderMode();
  }
  // reset FIFO address and paload length
  this->writeRegister(REG_FIFO_ADDR_PTR, 0);
  this->writeRegister(REG_PAYLOAD_LENGTH, 0);
  return 1;
}

void SX1276::explicitHeaderMode() {
  _implicitHeaderMode = 0;
  this->writeRegister(REG_MODEM_CONFIG_1, readRegister(REG_MODEM_CONFIG_1) & 0xfe);
}

void SX1276::implicitHeaderMode() {
  _implicitHeaderMode = 1;
  this->writeRegister(REG_MODEM_CONFIG_1, readRegister(REG_MODEM_CONFIG_1) | 0x01);
}

void SX1276::sleep() { writeRegister(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_SLEEP); }

void SX1276::setFrequency() {
  long freq = this->band_ * 1000000;

  uint64_t frf = ((uint64_t) freq << 19) / 32000000;
  writeRegister(REG_FRF_MSB, (uint8_t)(frf >> 16));
  writeRegister(REG_FRF_MID, (uint8_t)(frf >> 8));
  writeRegister(REG_FRF_LSB, (uint8_t)(frf >> 0));
}

void SX1276::setSignalBandwidth(long sbw) {
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
  writeRegister(REG_MODEM_CONFIG_1, (readRegister(REG_MODEM_CONFIG_1) & 0x0f) | (bw << 4));
}

void SX1276::setSyncWord(int sw) { writeRegister(REG_SYNC_WORD, sw); }

void SX1276::enableCrc() { writeRegister(REG_MODEM_CONFIG_2, readRegister(REG_MODEM_CONFIG_2) | 0x04); }

void SX1276::disableCrc() { writeRegister(REG_MODEM_CONFIG_2, readRegister(REG_MODEM_CONFIG_2) & 0xfb); }

void SX1276::idle() { writeRegister(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_STDBY); }

void SX1276::setSpreadingFactor(int sf) {
  if (sf < 6) {
    sf = 6;
  } else if (sf > 12) {
    sf = 12;
  }
  if (sf == 6) {
    writeRegister(REG_DETECTION_OPTIMIZE, 0xc5);
    writeRegister(REG_DETECTION_THRESHOLD, 0x0c);
  } else {
    writeRegister(REG_DETECTION_OPTIMIZE, 0xc3);
    writeRegister(REG_DETECTION_THRESHOLD, 0x0a);
  }
  writeRegister(REG_MODEM_CONFIG_2, (readRegister(REG_MODEM_CONFIG_2) & 0x0f) | ((sf << 4) & 0xf0));
}

void SX1276::setTxPower(int8_t power, int8_t outputPin) {
  uint8_t paConfig = 0;
  uint8_t paDac = 0;

  paConfig = readRegister(REG_PA_CONFIG);
  paDac = readRegister(REG_PaDac);

  paConfig = (paConfig & RF_PACONFIG_PASELECT_MASK) | outputPin;
  paConfig = (paConfig & RF_PACONFIG_MAX_POWER_MASK) | 0x70;

  if ((paConfig & RF_PACONFIG_PASELECT_PABOOST) == RF_PACONFIG_PASELECT_PABOOST) {
    if (power > 17) {
      paDac = (paDac & RF_PADAC_20DBM_MASK) | RF_PADAC_20DBM_ON;
    } else {
      paDac = (paDac & RF_PADAC_20DBM_MASK) | RF_PADAC_20DBM_OFF;
    }
    if ((paDac & RF_PADAC_20DBM_ON) == RF_PADAC_20DBM_ON) {
      if (power < 5) {
        power = 5;
      }
      if (power > 20) {
        power = 20;
      }
      paConfig = (paConfig & RF_PACONFIG_OUTPUTPOWER_MASK) | (uint8_t)((uint16_t)(power - 5) & 0x0F);
    } else {
      if (power < 2) {
        power = 2;
      }
      if (power > 17) {
        power = 17;
      }
      paConfig = (paConfig & RF_PACONFIG_OUTPUTPOWER_MASK) | (uint8_t)((uint16_t)(power - 2) & 0x0F);
    }
  } else {
    if (power < -1) {
      power = -1;
    }
    if (power > 14) {
      power = 14;
    }
    paConfig = (paConfig & RF_PACONFIG_OUTPUTPOWER_MASK) | (uint8_t)((uint16_t)(power + 1) & 0x0F);
  }
  writeRegister(REG_PA_CONFIG, paConfig);
  writeRegister(REG_PaDac, paDac);
}

uint8_t SX1276::readRegister(uint8_t address) { return singleTransfer(address & 0x7f, 0x00); }

void SX1276::writeRegister(uint8_t address, uint8_t value) { singleTransfer(address | 0x80, value); }

uint8_t SX1276::singleTransfer(uint8_t address, uint8_t value) {
  uint8_t response;
  this->enable();
  this->transfer_byte(address);
  response = this->transfer_byte(value);
  this->disable();
  return response;
}

}  // namespace sx1276
}  // namespace esphome
