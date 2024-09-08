#include "bl0910.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "constant.h"
#include <cinttypes>

namespace esphome {
namespace bl0910 {

static const char *const TAG = "bl0910";

static uint8_t checksum_calc(uint8_t *data) {
  uint8_t checksum = 0;
  for (int i = 0; i < 5; i++) {
    checksum += data[i];
  }
  checksum &= 0xFF;
  checksum ^= 0xFF;

  return checksum;
}

void BL0910::write_register(uint8_t addr, uint8_t data_h, uint8_t data_m, uint8_t data_l) {
  uint8_t packet[6] = {0};
  packet[0] = BL0910_WRITE_COMMAND;
  packet[1] = addr;
  packet[2] = data_h;
  packet[3] = data_m;
  packet[4] = data_l;
  packet[5] = checksum_calc(packet);
  this->enable();
  this->transfer_array(packet, sizeof(packet) / sizeof(packet[0]));
  this->disable();
}

int32_t BL0910::read_register(uint8_t addr) {
  int32_t data = 0;
  uint8_t packet[6] = {0};

  packet[0] = BL0910_READ_COMMAND;
  packet[1] = addr;
  this->enable();
  this->transfer_array(packet, sizeof(packet) / sizeof(packet[0]));
  this->disable();

  // Verify checksum
  uint8_t checksum = BL0910_READ_COMMAND + addr;
  for (int i = 2; i < 5; i++) {
    checksum += packet[i];
  }
  checksum &= 0xFF;
  checksum ^= 0xFF;
  if (checksum != packet[5])  // checksum is byte 6
  {
    ESP_LOGE(TAG, "Checksum error calculated: %x != read: %x", checksum, packet[5]);
    return 0xFF000000;
  }

  return (((int8_t) packet[2]) << 16) | (packet[3] << 8) | packet[4];
}

float BL0910::getVoltage(uint8_t channel) {
  return ((float) read_register(BL0910_REG_RMS[channel])) / this->voltage_reference[channel];
}

float BL0910::getCurrent(uint8_t channel) {
  return ((float) read_register(BL0910_REG_RMS[channel])) / this->current_reference[channel];
}

float BL0910::getPower(uint8_t channel) {
  return ((float) read_register(BL0910_REG_WATT[channel])) / this->power_reference[channel];
}

float BL0910::getEnergy(uint8_t channel) {
  return ((float) read_register(BL0910_REG_CF_CNT[channel])) / this->energy_reference[channel];
}

float BL0910::getFreq(void) {
  const float freq = (float) read_register(BL0910_REG_PERIOD);
  return 10000000.0 / freq;
}

float BL0910::getTemperature(void) {
  const float temp = (float) read_register(BL0910_REG_TPS1);
  return (temp - 64.0) * 12.5 / 59.0 - 40.0;
}

float BL0910::getPowerFactor(uint8_t channel, float freq) {
  const float angle = (float) read_register(BL0910_REG_ANGLE[channel]);
  return (360.0f * angle * freq) / 500000.0f;
}

void BL0910::loop() {}

void BL0910::update() {
  static int i = 0;
  static float freq = 50.0;
  if (i < NUM_CHANNELS) {
    if (voltage_sensor[i])
      voltage_sensor[i]->publish_state(getVoltage(i));
    if (current_sensor[i])
      current_sensor[i]->publish_state(getCurrent(i));
    if (power_sensor[i])
      power_sensor[i]->publish_state(getPower(i));
    if (energy_sensor[i])
      energy_sensor[i]->publish_state(getEnergy(i));
    if (power_factor_sensor[i])
      power_factor_sensor[i]->publish_state(getPowerFactor(i, freq));
    i++;
  } else {
    freq = getFreq();
    if (frequency_sensor)
      frequency_sensor->publish_state(freq);
    if (temperature_sensor)
      temperature_sensor->publish_state(getTemperature());
    i = 0;
  }
}

void BL0910::setup() {
  this->spi_setup();
  this->write_register(BL0910_REG_SOFT_RESET, 0x5A, 0x5A, 0x5A);  // Reset to default
  this->write_register(BL0910_REG_USR_WRPROT, 0x00, 0x55, 0x55);  // Enable User Operation Write
  this->write_register(BL0910_REG_MODE, 0x00, 0x00, 0x00);
  this->write_register(BL0910_REG_TPS_CTRL, 0x00, 0x07, 0xFF);
  this->write_register(BL0910_REG_FAST_RMS_CTRL, 0x40, 0xFF, 0xFF);
  this->write_register(BL0910_REG_GAIN1, 0x00, 0x00, 0x00);
}

void BL0910::dump_config() {  // NOLINT(readability-function-cognitive-complexity)
  ESP_LOGCONFIG(TAG, "BL0910:");
  for (int i = 0; i < NUM_CHANNELS; i++) {
    if (voltage_sensor[i]) {
      char buf[20];
      snprintf(buf, sizeof(buf), "Voltage %d", i + 1);
      LOG_SENSOR("", buf, voltage_sensor[i]);
    }

    if (current_sensor[i]) {
      char buf[20];
      snprintf(buf, sizeof(buf), "Current %d", i + 1);
      LOG_SENSOR("", buf, current_sensor[i]);
    }
    if (power_sensor[i]) {
      char buf[20];
      snprintf(buf, sizeof(buf), "Power %d", i + 1);
      LOG_SENSOR("", buf, power_sensor[i]);
    }
    if (energy_sensor[i]) {
      char buf[20];
      snprintf(buf, sizeof(buf), "Energy %d", i + 1);
      LOG_SENSOR("", buf, energy_sensor[i]);
    }
    if (power_factor_sensor[i]) {
      char buf[20];
      snprintf(buf, sizeof(buf), "Power Factor %d", i + 1);
      LOG_SENSOR("", buf, power_factor_sensor[i]);
    }
  }
  if (this->frequency_sensor) {
    LOG_SENSOR("", "Frequency", this->frequency_sensor);
  }
  if (this->temperature_sensor) {
    LOG_SENSOR("", "Temperature", this->temperature_sensor);
  }
}

}  // namespace bl0910
}  // namespace esphome
