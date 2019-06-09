#include "atm90e32.h"
#include "esphome/core/log.h"

namespace esphome {
namespace atm90e32 {

static const char *TAG = "atm90e32";
#include "atm90e32_reg.h"

void ATM90E32Component::update() {

  if (this->read16_(ATM90E32_REGISTER_METEREN) != 1) {
    this->status_set_warning();
    return;
  }

  if (this->voltage_sensor_a_ != nullptr) {
    this->voltage_sensor_a_->publish_state(this->GetLineVoltageA());
  }
  if (this->voltage_sensor_b_ != nullptr) {
    this->voltage_sensor_b_->publish_state(this->GetLineVoltageB());
  }
  if (this->voltage_sensor_c_ != nullptr) {
    this->voltage_sensor_c_->publish_state(this->GetLineVoltageC());
  }
  if (this->current_sensor_a_ != nullptr) {
    this->current_sensor_a_->publish_state(this->GetLineCurrentA());
  }
  if (this->current_sensor_b_ != nullptr) {
    this->current_sensor_b_->publish_state(this->GetLineCurrentB());
  }
  if (this->current_sensor_c_ != nullptr) {
    this->current_sensor_c_->publish_state(this->GetLineCurrentC());
  }
  if (this->freq_sensor_ != nullptr) {
    this->freq_sensor_->publish_state(this->GetFrequency());
  }
  if (this->power_sensor_ != nullptr) {
    this->power_sensor_->publish_state(this->GetTotalActivePower());
  }
  this->status_clear_warning();
}

void ATM90E32Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ATM90E32Component...");
  this->spi_setup();

  uint16_t mmode0 = 0x185;
  if (line_freq_ == 60) {
    mmode0 |= 1 << 12;
  }

  this->write16_(ATM90E32_REGISTER_SOFTRESET, 0x789A);      // Perform soft reset
  this->write16_(ATM90E32_REGISTER_CFGREGACCEN, 0x55AA);    // enable register config access
  this->write16_(ATM90E32_REGISTER_METEREN, 0x0001);        // Enable Metering
  if (this->read16_(ATM90E32_REGISTER_LASTSPIDATA) != 0x0001) {
    this->mark_failed();
    return;
  }

  this->write16_(ATM90E32_REGISTER_ZXCONFIG, 0x0A55);       // ZX2, ZX1, ZX0 pin config
  this->write16_(ATM90E32_REGISTER_MMODE0, mmode0);         // Mode Config (frequency set in main program)
  this->write16_(ATM90E32_REGISTER_MMODE1, pga_gain_);      // PGA Gain Configuration for Current Channels - 0x002A (x4) // 0x0015 (x2) // 0x0000 (1x)
  this->write16_(ATM90E32_REGISTER_PSTARTTH, 0x0AFC);       // Active Startup Power Threshold - 50% of startup current = 0.9/0.00032 = 2812.5
  this->write16_(ATM90E32_REGISTER_QSTARTTH, 0x0AEC);       // Reactive Startup Power Threshold
  this->write16_(ATM90E32_REGISTER_PPHASETH, 0x00BC);       // Active Phase Threshold = 10% of startup current = 0.06/0.00032 = 187.5
  this->write16_(ATM90E32_REGISTER_UGAINA, volt_a_gain_);   // A Voltage rms gain
  this->write16_(ATM90E32_REGISTER_IGAINA, ct_a_gain_);     // A line current gain
  this->write16_(ATM90E32_REGISTER_UGAINB, volt_b_gain_);   // B Voltage rms gain
  this->write16_(ATM90E32_REGISTER_IGAINB, ct_b_gain_);     // B line current gain
  this->write16_(ATM90E32_REGISTER_UGAINC, volt_c_gain_);   // C Voltage rms gain
  this->write16_(ATM90E32_REGISTER_IGAINC, ct_c_gain_);     // C line current gain
  this->write16_(ATM90E32_REGISTER_CFGREGACCEN, 0x0000);    // end configuration
}

void ATM90E32Component::dump_config() {
  ESP_LOGCONFIG("", "ATM90E32:");
  LOG_PIN("  CS Pin: ", this->cs_);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with ATM90E32 failed!");
  }
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "VoltageA", this->voltage_sensor_a_)
  LOG_SENSOR("  ", "VoltageB", this->voltage_sensor_b_)
  LOG_SENSOR("  ", "CurrentA", this->current_sensor_a_)
  LOG_SENSOR("  ", "CurrentB", this->current_sensor_b_)
  LOG_SENSOR("  ", "CurrentC", this->current_sensor_c_)
  LOG_SENSOR("  ", "Frequency", this->freq_sensor_)
  LOG_SENSOR("  ", "Power", this->power_sensor_)
}
float ATM90E32Component::get_setup_priority() const { return setup_priority::DATA; }

uint16_t ATM90E32Component::read16_(uint16_t a_register) {
  uint8_t addrh = (1 << 7) | ((a_register >> 8) & 0x03);
  uint8_t addrl = (a_register & 0xFF);
  uint8_t data[2];
  uint16_t output;

  this->enable();
  delayMicroseconds(10);
  this->write_byte(addrh);
  this->write_byte(addrl);
  delayMicroseconds(4);
  this->read_array(data, 2);
  this->disable();

  output = (uint16_t(data[0] & 0xFF) << 8) | (data[1] & 0xFF);
  ESP_LOGVV(TAG, "read16_ 0x%04X output 0x%04X", a_register, output);
  return output;
}

uint32_t ATM90E32Component::read32_(uint16_t addr_h, uint16_t addr_l) {
  uint32_t val_h = this->read16_(addr_h);
  uint32_t val_l = this->read16_(addr_l);

  return (val_h << 16) | val_l;
}

void ATM90E32Component::write16_(uint16_t a_register, uint16_t val) {
  uint8_t addrh = (a_register >> 8) & 0x03;
  uint8_t addrl = (a_register & 0xFF);

  ESP_LOGVV(TAG, "write16_ 0x%04X val 0x%04X", a_register, val);
  this->enable();
  delayMicroseconds(10);
  this->write_byte(addrh);
  this->write_byte(addrl);
  delayMicroseconds(4);
  this->write_byte((val >> 8) & 0xff);
  this->write_byte(val & 0xFF);
  this->disable();
}

float ATM90E32Component::GetLineVoltageA() {
  uint16_t voltage = this->read16_(ATM90E32_REGISTER_URMSA);
  return (float)voltage / 100;
}
float ATM90E32Component::GetLineVoltageB() {
  uint16_t voltage = this->read16_(ATM90E32_REGISTER_URMSB);
  return (float)voltage / 100;
}
float ATM90E32Component::GetLineVoltageC() {
  uint16_t voltage = this->read16_(ATM90E32_REGISTER_URMSC);
  return (float)voltage / 100;
}
float ATM90E32Component::GetLineCurrentA() {
  uint16_t current = this->read16_(ATM90E32_REGISTER_IRMSA);
  return (float)current / 1000;
}
float ATM90E32Component::GetLineCurrentB() {
  uint16_t current = this->read16_(ATM90E32_REGISTER_IRMSB);
  return (float)current / 1000;
}
float ATM90E32Component::GetLineCurrentC() {
  uint16_t current = this->read16_(ATM90E32_REGISTER_IRMSC);
  return (float)current / 1000;
}
float ATM90E32Component::GetTotalActivePower() {
   int val = this->read32_(ATM90E32_REGISTER_PMEANT, ATM90E32_REGISTER_PMEANTLSB);
   return (float)val * 0.00032;
}
float ATM90E32Component::GetFrequency() {
  uint16_t freq = this->read16_(ATM90E32_REGISTER_FREQ);
  return (float)freq / 100;
}
}  // namespace atm90e32
}  // namespace esphome
