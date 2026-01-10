#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif

namespace esphome {
namespace apds9930 {

// Register addresses
static const uint8_t APDS9930_ENABLE = 0x00;
static const uint8_t APDS9930_ATIME = 0x01;
static const uint8_t APDS9930_PTIME = 0x02;
static const uint8_t APDS9930_WTIME = 0x03;
static const uint8_t APDS9930_PERS = 0x0C;
static const uint8_t APDS9930_CONFIG = 0x0D;
static const uint8_t APDS9930_PPULSE = 0x0E;
static const uint8_t APDS9930_CONTROL = 0x0F;
static const uint8_t APDS9930_ID = 0x12;
static const uint8_t APDS9930_STATUS = 0x13;
static const uint8_t APDS9930_CH0DATAL = 0x14;
static const uint8_t APDS9930_CH0DATAH = 0x15;
static const uint8_t APDS9930_CH1DATAL = 0x16;
static const uint8_t APDS9930_CH1DATAH = 0x17;
static const uint8_t APDS9930_PDATAL = 0x18;
static const uint8_t APDS9930_PDATAH = 0x19;
static const uint8_t APDS9930_POFFSET = 0x1E;

// Command byte
static const uint8_t APDS9930_CMD = 0x80;
static const uint8_t APDS9930_CMD_AUTO_INCREMENT = 0xA0;

// Device IDs
static const uint8_t APDS9930_ID_1 = 0x12;
static const uint8_t APDS9930_ID_2 = 0x39;

// Enable register bits
static const uint8_t APDS9930_PON = 0x01;   // Power on
static const uint8_t APDS9930_AEN = 0x02;   // Ambient light enable
static const uint8_t APDS9930_PEN = 0x04;   // Proximity enable
static const uint8_t APDS9930_WEN = 0x08;   // Wait enable

// Status register bits
static const uint8_t APDS9930_AVALID = 0x01;  // Ambient light data valid
static const uint8_t APDS9930_PVALID = 0x02;  // Proximity data valid

// Default values
static const uint8_t APDS9930_DEFAULT_ATIME = 0xED;    // 103ms integration time
static const uint8_t APDS9930_DEFAULT_WTIME = 0xFF;    // 27ms wait time
static const uint8_t APDS9930_DEFAULT_PTIME = 0xFF;    // 2.73ms proximity time
static const uint8_t APDS9930_DEFAULT_PPULSE = 0x08;   // 8 proximity pulses
static const uint8_t APDS9930_DEFAULT_CONFIG = 0x00;
static const uint8_t APDS9930_DEFAULT_POFFSET = 0x00;

// Lux calculation coefficients
static const float APDS9930_DF = 52.0f;
static const float APDS9930_GA = 0.49f;
static const float APDS9930_ALS_B = 1.862f;
static const float APDS9930_ALS_C = 0.746f;
static const float APDS9930_ALS_D = 1.291f;

class APDS9930 : public PollingComponent, public i2c::I2CDevice {
#ifdef USE_SENSOR
  SUB_SENSOR(ambient_light)
  SUB_SENSOR(proximity)
#endif

 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void update() override;

  void set_led_drive(uint8_t level) { this->led_drive_ = level; }
  void set_proximity_gain(uint8_t gain) { this->proximity_gain_ = gain; }
  void set_ambient_gain(uint8_t gain) { this->ambient_gain_ = gain; }
  void set_proximity_diode(uint8_t diode) { this->proximity_diode_ = diode; }

 protected:
  bool is_ambient_enabled_() const;
  bool is_proximity_enabled_() const;
  void read_ambient_data_(uint8_t status);
  void read_proximity_data_(uint8_t status);
  float calculate_lux_(uint16_t ch0, uint16_t ch1);

  uint8_t led_drive_;
  uint8_t proximity_gain_;
  uint8_t ambient_gain_;
  uint8_t proximity_diode_;
  uint8_t atime_{APDS9930_DEFAULT_ATIME};

  enum ErrorCode {
    NONE = 0,
    COMMUNICATION_FAILED,
    WRONG_ID,
  } error_code_{NONE};
};

}  // namespace apds9930
}  // namespace esphome
