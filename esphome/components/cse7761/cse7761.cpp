#include "cse7761.h"

#include "esphome/core/log.h"

namespace esphome {
namespace cse7761 {

static const char *const TAG = "cse7761";

/*********************************************************************************************\
 * CSE7761 - Energy  (Sonoff Dual R3 Pow v1.x)
 *
 * Based on Tasmota source code
 * See https://github.com/arendst/Tasmota/discussions/10793
 * https://github.com/arendst/Tasmota/blob/development/tasmota/xnrg_19_cse7761.ino
\*********************************************************************************************/

static const int CSE7761_UREF = 42563;  // RmsUc
static const int CSE7761_IREF = 52241;  // RmsIAC
static const int CSE7761_PREF = 44513;  // PowerPAC

static const uint8_t CSE7761_REG_SYSCON = 0x00;     // (2) System Control Register (0x0A04)
static const uint8_t CSE7761_REG_EMUCON = 0x01;     // (2) Metering control register (0x0000)
static const uint8_t CSE7761_REG_EMUCON2 = 0x13;    // (2) Metering control register 2 (0x0001)
static const uint8_t CSE7761_REG_PULSE1SEL = 0x1D;  // (2) Pin function output select register (0x3210)

static const uint8_t CSE7761_REG_RMSIA = 0x24;      // (3) The effective value of channel A current (0x000000)
static const uint8_t CSE7761_REG_RMSIB = 0x25;      // (3) The effective value of channel B current (0x000000)
static const uint8_t CSE7761_REG_RMSU = 0x26;       // (3) Voltage RMS (0x000000)
static const uint8_t CSE7761_REG_POWERPA = 0x2C;    // (4) Channel A active power, update rate 27.2Hz (0x00000000)
static const uint8_t CSE7761_REG_POWERPB = 0x2D;    // (4) Channel B active power, update rate 27.2Hz (0x00000000)
static const uint8_t CSE7761_REG_SYSSTATUS = 0x43;  // (1) System status register

static const uint8_t CSE7761_REG_COEFFCHKSUM = 0x6F;  // (2) Coefficient checksum
static const uint8_t CSE7761_REG_RMSIAC = 0x70;       // (2) Channel A effective current conversion coefficient

static const uint8_t CSE7761_SPECIAL_COMMAND = 0xEA;   // Start special command
static const uint8_t CSE7761_CMD_RESET = 0x96;         // Reset command, after receiving the command, the chip resets
static const uint8_t CSE7761_CMD_CLOSE_WRITE = 0xDC;   // Close write operation
static const uint8_t CSE7761_CMD_ENABLE_WRITE = 0xE5;  // Enable write operation

enum CSE7761 { RMS_IAC, RMS_IBC, RMS_UC, POWER_PAC, POWER_PBC, POWER_SC, ENERGY_AC, ENERGY_BC };

void CSE7761Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up CSE7761...");
  this->write_(CSE7761_SPECIAL_COMMAND, CSE7761_CMD_RESET);
  uint16_t syscon = this->read_(0x00, 2);  // Default 0x0A04
  if ((0x0A04 == syscon) && this->chip_init_()) {
    this->write_(CSE7761_SPECIAL_COMMAND, CSE7761_CMD_CLOSE_WRITE);
    ESP_LOGD(TAG, "CSE7761 found");
    this->data_.ready = true;
  } else {
    this->mark_failed();
  }
}

void CSE7761Component::dump_config() {
  ESP_LOGCONFIG(TAG, "CSE7761:");
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with CSE7761 failed!");
  }
  LOG_UPDATE_INTERVAL(this);
  this->check_uart_settings(38400, 1, uart::UART_CONFIG_PARITY_EVEN, 8);
}

float CSE7761Component::get_setup_priority() const { return setup_priority::DATA; }

void CSE7761Component::update() {
  if (this->data_.ready) {
    this->get_data_();
  }
}

void CSE7761Component::write_(uint8_t reg, uint16_t data) {
  uint8_t buffer[5];

  buffer[0] = 0xA5;
  buffer[1] = reg;
  uint32_t len = 2;
  if (data) {
    if (data < 0xFF) {
      buffer[2] = data & 0xFF;
      len = 3;
    } else {
      buffer[2] = (data >> 8) & 0xFF;
      buffer[3] = data & 0xFF;
      len = 4;
    }
    uint8_t crc = 0;
    for (uint32_t i = 0; i < len; i++) {
      crc += buffer[i];
    }
    buffer[len] = ~crc;
    len++;
  }

  this->write_array(buffer, len);
}

bool CSE7761Component::read_once_(uint8_t reg, uint8_t size, uint32_t *value) {
  while (this->available()) {
    this->read();
  }

  this->write_(reg, 0);

  uint8_t buffer[8] = {0};
  uint32_t rcvd = 0;

  for (uint32_t i = 0; i <= size; i++) {
    int value = this->read();
    if (value > -1 && rcvd < sizeof(buffer) - 1) {
      buffer[rcvd++] = value;
    }
  }

  if (!rcvd) {
    ESP_LOGD(TAG, "Received 0 bytes for register %hhu", reg);
    return false;
  }

  rcvd--;
  uint32_t result = 0;
  // CRC check
  uint8_t crc = 0xA5 + reg;
  for (uint32_t i = 0; i < rcvd; i++) {
    result = (result << 8) | buffer[i];
    crc += buffer[i];
  }
  crc = ~crc;
  if (crc != buffer[rcvd]) {
    return false;
  }

  *value = result;
  return true;
}

uint32_t CSE7761Component::read_(uint8_t reg, uint8_t size) {
  bool result = false;  // Start loop
  uint8_t retry = 3;    // Retry up to three times
  uint32_t value = 0;   // Default no value
  while (!result && retry > 0) {
    retry--;
    if (this->read_once_(reg, size, &value))
      return value;
  }
  ESP_LOGE(TAG, "Reading register %hhu failed!", reg);
  return value;
}

uint32_t CSE7761Component::coefficient_by_unit_(uint32_t unit) {
  switch (unit) {
    case RMS_UC:
      return 0x400000 * 100 / this->data_.coefficient[RMS_UC];
    case RMS_IAC:
      return (0x800000 * 100 / this->data_.coefficient[RMS_IAC]) * 10;  // Stay within 32 bits
    case POWER_PAC:
      return 0x80000000 / this->data_.coefficient[POWER_PAC];
  }
  return 0;
}

bool CSE7761Component::chip_init_() {
  uint16_t calc_chksum = 0xFFFF;
  for (uint32_t i = 0; i < 8; i++) {
    this->data_.coefficient[i] = this->read_(CSE7761_REG_RMSIAC + i, 2);
    calc_chksum += this->data_.coefficient[i];
  }
  calc_chksum = ~calc_chksum;
  uint16_t coeff_chksum = this->read_(CSE7761_REG_COEFFCHKSUM, 2);
  if ((calc_chksum != coeff_chksum) || (!calc_chksum)) {
    ESP_LOGD(TAG, "Default calibration");
    this->data_.coefficient[RMS_IAC] = CSE7761_IREF;
    this->data_.coefficient[RMS_UC] = CSE7761_UREF;
    this->data_.coefficient[POWER_PAC] = CSE7761_PREF;
  }

  this->write_(CSE7761_SPECIAL_COMMAND, CSE7761_CMD_ENABLE_WRITE);

  uint8_t sys_status = this->read_(CSE7761_REG_SYSSTATUS, 1);
  if (sys_status & 0x10) {  // Write enable to protected registers (WREN)
    this->write_(CSE7761_REG_SYSCON | 0x80, 0xFF04);
    this->write_(CSE7761_REG_EMUCON | 0x80, 0x1183);
    this->write_(CSE7761_REG_EMUCON2 | 0x80, 0x0FC1);
    this->write_(CSE7761_REG_PULSE1SEL | 0x80, 0x3290);
  } else {
    ESP_LOGD(TAG, "Write failed at chip_init");
    return false;
  }
  return true;
}

void CSE7761Component::get_data_() {
  // The effective value of current and voltage Rms is a 24-bit signed number,
  // the highest bit is 0 for valid data,
  //   and when the highest bit is 1, the reading will be processed as zero
  // The active power parameter PowerA/B is in two’s complement format, 32-bit
  // data, the highest bit is Sign bit.
  uint32_t value = this->read_(CSE7761_REG_RMSU, 3);
  this->data_.voltage_rms = (value >= 0x800000) ? 0 : value;

  value = this->read_(CSE7761_REG_RMSIA, 3);
  this->data_.current_rms[0] = ((value >= 0x800000) || (value < 1600)) ? 0 : value;  // No load threshold of 10mA
  value = this->read_(CSE7761_REG_POWERPA, 4);
  this->data_.active_power[0] = (0 == this->data_.current_rms[0]) ? 0 : ((uint32_t) abs((int) value));

  value = this->read_(CSE7761_REG_RMSIB, 3);
  this->data_.current_rms[1] = ((value >= 0x800000) || (value < 1600)) ? 0 : value;  // No load threshold of 10mA
  value = this->read_(CSE7761_REG_POWERPB, 4);
  this->data_.active_power[1] = (0 == this->data_.current_rms[1]) ? 0 : ((uint32_t) abs((int) value));

  // convert values and publish to sensors

  float voltage = (float) this->data_.voltage_rms / this->coefficient_by_unit_(RMS_UC);
  if (this->voltage_sensor_ != nullptr) {
    this->voltage_sensor_->publish_state(voltage);
  }

  for (uint8_t channel = 0; channel < 2; channel++) {
    // Active power = PowerPA * PowerPAC * 1000 / 0x80000000
    float active_power = (float) this->data_.active_power[channel] / this->coefficient_by_unit_(POWER_PAC);  // W
    float amps = (float) this->data_.current_rms[channel] / this->coefficient_by_unit_(RMS_IAC);             // A
    ESP_LOGD(TAG, "Channel %d power %f W, current %f A", channel + 1, active_power, amps);
    if (channel == 0) {
      if (this->power_sensor_1_ != nullptr) {
        this->power_sensor_1_->publish_state(active_power);
      }
      if (this->current_sensor_1_ != nullptr) {
        this->current_sensor_1_->publish_state(amps);
      }
    } else if (channel == 1) {
      if (this->power_sensor_2_ != nullptr) {
        this->power_sensor_2_->publish_state(active_power);
      }
      if (this->current_sensor_2_ != nullptr) {
        this->current_sensor_2_->publish_state(amps);
      }
    }
  }
}

}  // namespace cse7761
}  // namespace esphome
