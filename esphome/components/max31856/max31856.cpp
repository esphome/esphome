#include "max31856.h"

#include "esphome/core/log.h"
#include <cmath>

namespace esphome {
namespace max31856 {

static const char *const TAG = "max31856";

// Based on Adafruit's library: https://github.com/adafruit/Adafruit_MAX31856

void MAX31856Sensor::setup() {
  ESP_LOGCONFIG(TAG, "Setting up MAX31856Sensor '%s'...", this->name_.c_str());
  this->spi_setup();

  // assert on any fault
  ESP_LOGCONFIG(TAG, "Setting up assertion on all faults");
  this->write_register_(MAX31856_MASK_REG, 0x0);

  ESP_LOGCONFIG(TAG, "Setting up open circuit fault detection");
  this->write_register_(MAX31856_CR0_REG, MAX31856_CR0_OCFAULT01);

  this->set_thermocouple_type_();
  this->set_noise_filter_();

  ESP_LOGCONFIG(TAG, "Completed setting up MAX31856Sensor '%s'...", this->name_.c_str());
}

void MAX31856Sensor::dump_config() {
  LOG_SENSOR("", "MAX31856", this);
  LOG_PIN("  CS Pin: ", this->cs_);
  ESP_LOGCONFIG(TAG, "  Mains Filter: %s",
                (filter_ == FILTER_60HZ ? "60 Hz" : (filter_ == FILTER_50HZ ? "50 Hz" : "Unknown!")));
  if (this->thermocouple_type_ < 0 || this->thermocouple_type_ > 7) {
    ESP_LOGCONFIG(TAG, "  Thermocouple Type: Unknown");
  } else {
    ESP_LOGCONFIG(TAG, "  Thermocouple Type: %c", "BEJKNRST"[this->thermocouple_type_]);
  }

  LOG_UPDATE_INTERVAL(this);
}

void MAX31856Sensor::update() {
  ESP_LOGVV(TAG, "update");

  this->one_shot_temperature_();

  // Datasheet max conversion time for 1 shot is 155ms for 60Hz / 185ms for 50Hz
  auto f = std::bind(&MAX31856Sensor::read_thermocouple_temperature_, this);
  this->set_timeout("MAX31856Sensor::read_thermocouple_temperature_", filter_ == FILTER_60HZ ? 155 : 185, f);
}

void MAX31856Sensor::read_thermocouple_temperature_() {
  if (this->has_fault_()) {
    // Faults have been logged, clear it for next loop
    this->clear_fault_();
  } else {
    int32_t temp24 = this->read_register24_(MAX31856_LTCBH_REG);
    if (temp24 & 0x800000) {
      temp24 |= 0xFF000000;  // fix sign
    }

    temp24 >>= 5;  // bottom 5 bits are unused

    float temp_c = temp24;
    temp_c *= 0.0078125;

    ESP_LOGD(TAG, "Got thermocouple temperature: %.2f°C", temp_c);
    this->publish_state(temp_c);
  }
}

void MAX31856Sensor::one_shot_temperature_() {
  ESP_LOGVV(TAG, "one_shot_temperature_");
  this->write_register_(MAX31856_CJTO_REG, 0x0);

  uint8_t t = this->read_register_(MAX31856_CR0_REG);

  t &= ~MAX31856_CR0_AUTOCONVERT;  // turn off autoconversion mode
  t |= MAX31856_CR0_1SHOT;         // turn on one shot mode

  this->write_register_(MAX31856_CR0_REG, t);
}

bool MAX31856Sensor::has_fault_() {
  ESP_LOGVV(TAG, "read_fault_");
  uint8_t faults = this->read_register_(MAX31856_SR_REG);

  if (faults == 0) {
    ESP_LOGV(TAG, "status_set_warning");
    this->status_clear_warning();
    return false;
  }

  ESP_LOGV(TAG, "status_set_warning");
  this->status_set_warning();

  if ((faults & MAX31856_FAULT_CJRANGE) == MAX31856_FAULT_CJRANGE) {
    ESP_LOGW(TAG, "Cold Junction Out-of-Range: '%s'...", this->name_.c_str());
  }
  if ((faults & MAX31856_FAULT_TCRANGE) == MAX31856_FAULT_TCRANGE) {
    ESP_LOGW(TAG, "Thermocouple Out-of-Range: '%s'...", this->name_.c_str());
  }
  if ((faults & MAX31856_FAULT_CJHIGH) == MAX31856_FAULT_CJHIGH) {
    ESP_LOGW(TAG, "Cold-Junction High Fault: '%s'...", this->name_.c_str());
  }
  if ((faults & MAX31856_FAULT_CJLOW) == MAX31856_FAULT_CJLOW) {
    ESP_LOGW(TAG, "Cold-Junction Low Fault: '%s'...", this->name_.c_str());
  }
  if ((faults & MAX31856_FAULT_TCHIGH) == MAX31856_FAULT_TCHIGH) {
    ESP_LOGW(TAG, "Thermocouple Temperature High Fault: '%s'...", this->name_.c_str());
  }
  if ((faults & MAX31856_FAULT_TCLOW) == MAX31856_FAULT_TCLOW) {
    ESP_LOGW(TAG, "Thermocouple Temperature Low Fault: '%s'...", this->name_.c_str());
  }
  if ((faults & MAX31856_FAULT_OVUV) == MAX31856_FAULT_OVUV) {
    ESP_LOGW(TAG, "Overvoltage or Undervoltage Input Fault: '%s'...", this->name_.c_str());
  }
  if ((faults & MAX31856_FAULT_OPEN) == MAX31856_FAULT_OPEN) {
    ESP_LOGW(TAG, "Thermocouple Open-Circuit Fault (possibly not connected): '%s'...", this->name_.c_str());
  }

  return true;
}

void MAX31856Sensor::clear_fault_() {
  ESP_LOGV(TAG, "clear_fault_");
  uint8_t t = this->read_register_(MAX31856_CR0_REG);

  t |= MAX31856_CR0_FAULT;     // turn on fault interrupt mode
  t |= MAX31856_CR0_FAULTCLR;  // enable the fault status clear bit

  this->write_register_(MAX31856_CR0_REG, t);
}

void MAX31856Sensor::set_thermocouple_type_() {
  MAX31856ThermocoupleType type;
  if (this->thermocouple_type_ < 0 || this->thermocouple_type_ > 7) {
    type = MAX31856_TCTYPE_K;
  } else {
    type = this->thermocouple_type_;
  }
  ESP_LOGCONFIG(TAG, "set_thermocouple_type_: 0x%02X", type);
  uint8_t t = this->read_register_(MAX31856_CR1_REG);
  t &= 0xF0;  // mask off bottom 4 bits
  t |= (uint8_t) type & 0x0F;
  this->write_register_(MAX31856_CR1_REG, t);
}

void MAX31856Sensor::set_noise_filter_() {
  ESP_LOGCONFIG(TAG, "set_noise_filter_: 0x%02X", filter_);
  uint8_t t = this->read_register_(MAX31856_CR0_REG);
  if (filter_ == FILTER_50HZ) {
    t |= 0x01;
    ESP_LOGCONFIG(TAG, "set_noise_filter_: 50 Hz, t==0x%02X", t);
  } else {
    t &= 0xfe;
    ESP_LOGCONFIG(TAG, "set_noise_filter_: 60 Hz, t==0x%02X", t);
  }
  this->write_register_(MAX31856_CR0_REG, t);
}

void MAX31856Sensor::write_register_(uint8_t reg, uint8_t value) {
  ESP_LOGVV(TAG, "write_register_ raw reg=0x%02X value=0x%02X", reg, value);
  reg |= SPI_WRITE_M;
  ESP_LOGVV(TAG, "write_register_ masked reg=0x%02X value=0x%02X", reg, value);
  this->enable();
  ESP_LOGVV(TAG, "write_byte reg=0x%02X", reg);
  this->write_byte(reg);
  ESP_LOGVV(TAG, "write_byte value=0x%02X", value);
  this->write_byte(value);
  this->disable();
  ESP_LOGV(TAG, "write_register_ 0x%02X: 0x%02X", reg, value);
}

uint8_t MAX31856Sensor::read_register_(uint8_t reg) {
  ESP_LOGVV(TAG, "read_register_ 0x%02X", reg);
  this->enable();
  ESP_LOGVV(TAG, "write_byte reg=0x%02X", reg);
  this->write_byte(reg);
  const uint8_t value(this->read_byte());
  ESP_LOGVV(TAG, "read_byte value=0x%02X", value);
  this->disable();
  ESP_LOGV(TAG, "read_register_ reg=0x%02X: value=0x%02X", reg, value);
  return value;
}

uint32_t MAX31856Sensor::read_register24_(uint8_t reg) {
  ESP_LOGVV(TAG, "read_register_24_ 0x%02X", reg);
  this->enable();
  ESP_LOGVV(TAG, "write_byte reg=0x%02X", reg);
  this->write_byte(reg);
  const uint8_t msb(this->read_byte());
  ESP_LOGVV(TAG, "read_byte msb=0x%02X", msb);
  const uint8_t mid(this->read_byte());
  ESP_LOGVV(TAG, "read_byte mid=0x%02X", mid);
  const uint8_t lsb(this->read_byte());
  ESP_LOGVV(TAG, "read_byte lsb=0x%02X", lsb);
  this->disable();
  const uint32_t value((msb << 16) | (mid << 8) | lsb);
  ESP_LOGV(TAG, "read_register_24_ reg=0x%02X: value=0x%06" PRIX32, reg, value);
  return value;
}

float MAX31856Sensor::get_setup_priority() const { return setup_priority::DATA; }

}  // namespace max31856
}  // namespace esphome
