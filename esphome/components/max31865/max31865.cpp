#include "max31865.h"
#include "esphome/core/log.h"

// NOTES: Currently configured for 3-wire PT1000 with Vbias always on

#define MAX31865_WRITE  (0x80)
#define MAX31865_CFG    (0x00)
#define MAX31865_RTD_H  (0x01)
#define MAX31865_RTD_L  (0x02)
#define MAX31865_FLT    (0x07)

#define MAX31865_CFG_6050 (0x01)  // Filter (0 - 60 Hz / 1 - 50Hz)
#define MAX31865_CFG_FLTC (0x02)  // 1 - Clear Fault (Auto-Clear)
#define MAX31865_CFG_WIRE (0x10)  // Wires (0 - 2 or 4 Wire / 1 - 3 Wire)
#define MAX31865_CFG_MODE (0x40)  // 1 - Auto conversion
#define MAX31865_CFG_BIAS (0x80)  // 1 - Vbias Enabled

#define MAX31865_RTD_L_FLT (0x01)  // Indicates Fault

#define MAX31865_FLT_OVUV    (0x04)
#define MAX31865_FLT_RTNO    (0x08)
#define MAX31865_FLT_RFNO    (0x10)
#define MAX31865_FLT_RFNS    (0x20)
#define MAX31865_FLT_TSHL    (0x40)
#define MAX31865_FLT_TSHH    (0x80)

#define RTD_A 3.9083e-3
#define RTD_B -5.775e-7

namespace esphome {
namespace max31865 {

static const char *TAG = "max31865";

float  MAX31865Sensor::temperature(unsigned short adc, float RTDnominal, float refResistor) {
  // https://github.com/adafruit/Adafruit_MAX31865/blob/master/Adafruit_MAX31865.cpp
  // http://www.analog.com/media/en/technical-documentation/application-notes/AN709_0.pdf

  float Z1, Z2, Z3, Z4, Rt, temp;

  Rt = adc;
  Rt /= 32768;
  Rt *= refResistor;

  ESP_LOGD(TAG, "'%s': Resistance: %.3f", this->name_.c_str(), Rt);

  Z1 = -RTD_A;
  Z2 = RTD_A * RTD_A - (4 * RTD_B);
  Z3 = (4 * RTD_B) / RTDnominal;
  Z4 = 2 * RTD_B;

  temp = Z2 + (Z3 * Rt);
  temp = (sqrt(temp) + Z1) / Z4;

  if (temp >= 0) return temp;

  // ugh.
  Rt /= RTDnominal;
  Rt *= 100;      // normalize to 100 ohm

  float rpoly = Rt;

  temp = -242.02;
  temp += 2.2228 * rpoly;
  rpoly *= Rt;  // square
  temp += 2.5859e-3 * rpoly;
  rpoly *= Rt;  // ^3
  temp -= 4.8260e-6 * rpoly;
  rpoly *= Rt;  // ^4
  temp -= 2.8183e-8 * rpoly;
  rpoly *= Rt;  // ^5
  temp += 1.5243e-10 * rpoly;

  return temp;
}

void MAX31865Sensor::write_config_() {
  this->enable();
  delay(1);
  this->write_byte(MAX31865_CFG|MAX31865_WRITE);
  this->write_byte(MAX31865_CFG_FLTC|MAX31865_CFG_WIRE|MAX31865_CFG_MODE|MAX31865_CFG_BIAS); //TODO: Allow user to specify configuration
  this->disable();
}

void MAX31865Sensor::update() {
  // Conversion time typ: 52ms (60Hz mode), max: 220ms
  auto f = std::bind(&MAX31865Sensor::read_data_, this);
  this->set_timeout("value", 220, f);
}

void MAX31865Sensor::setup() {
  ESP_LOGCONFIG(TAG, "Setting up MAX31865Sensor '%s'...", this->name_.c_str());
  this->spi_setup();

  //Configuire MAX31865 device
  this->write_config_();
  //TODO: Run fault-detection cycle
}

void MAX31865Sensor::dump_config() {
  LOG_SENSOR("", "MAX31865", this);
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_UPDATE_INTERVAL(this);
}

float MAX31865Sensor::get_setup_priority() const { return setup_priority::DATA; }

void MAX31865Sensor::read_data_() {
  this->enable();
  delay(1);
  this->write_byte(MAX31865_RTD_H);
  uint8_t data[2];
  this->read_array(data, 2);
  this->disable();

  uint16_t val = (data[1] | (data[0] << 8)) >> 1;

  ESP_LOGD(TAG, "'%s': Read ADC of 0x%04X", this->name_.c_str(), val);

  if((data[1] & MAX31865_RTD_L_FLT) == MAX31865_RTD_L_FLT) {
    this->enable();
    delay(1);
    this->write_byte(MAX31865_FLT);
    data[0] = this->read_byte();
    this->disable();

    this->write_config_();  // This is to clear fault status bits
  }
  else {
    data[0] = 0;
  }

  if ((data[0] & MAX31865_FLT_OVUV) != 0) {
    ESP_LOGW(TAG, "Got Overvoltage/undervoltage fault from MAX31865Sensor (0x%04X) (0x%04X)", val, data[0]);
    this->status_set_warning();
    return;
  }
  if ((data[0] & MAX31865_FLT_RTNO) != 0) {
    ESP_LOGW(TAG, "Got RTDIN- < 0.85 x VBIAS (FORCE- open) fault from MAX31865Sensor (0x%04X) (0x%04X)", val, data[0]);
    this->status_set_warning();
    return;
  }
  if ((data[0] & MAX31865_FLT_RFNO) != 0) {
    ESP_LOGW(TAG, "Got REFIN- < 0.85 x VBIAS(FORCE- open) fault from MAX31865Sensor (0x%04X) (0x%04X)", val, data[0]);
    this->status_set_warning();
    return;
  }
  if ((data[0] & MAX31865_FLT_RFNS) != 0) {
    ESP_LOGW(TAG, "Got REFIN- > 0.85 x VBIAS fault from MAX31865Sensor (0x%04X) (0x%04X)", val, data[0]);
    this->status_set_warning();
    return;
  }

  //TODO: Support parameterization
  // The value of the Rref resistor. Use 430.0 for PT100 and 4300.0 for PT1000
  #define RREF 4300.0
  // The 'nominal' 0-degrees-C resistance of the sensor 100.0 for PT100, 1000.0 for PT1000
  #define RNOMINAL 1000.0

  float temperature =  this->temperature(val, RNOMINAL, RREF);
  ESP_LOGD(TAG, "'%s': Got temperature=%.1f°C", this->name_.c_str(), temperature);
  this->publish_state(temperature);
  this->status_clear_warning();
}

}  // namespace max31865
}  // namespace esphome
