#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include "Wire.h"
#include "Arduino.h"

namespace esphome {
namespace ams5915 {
  enum Transducer
  {
      AMS5915_0005_D,
      AMS5915_0010_D,
      AMS5915_0005_D_B,
      AMS5915_0010_D_B,
      AMS5915_0020_D,
      AMS5915_0050_D,
      AMS5915_0100_D,
      AMS5915_0020_D_B,
      AMS5915_0050_D_B,
      AMS5915_0100_D_B,
      AMS5915_0200_D,
      AMS5915_0350_D,
      AMS5915_1000_D,
      AMS5915_2000_D,
      AMS5915_4000_D,
      AMS5915_7000_D,
      AMS5915_10000_D,
      AMS5915_0200_D_B,
      AMS5915_0350_D_B,
      AMS5915_1000_D_B,
      AMS5915_1000_A,
      AMS5915_1200_B
  };
class Ams5915 : public PollingComponent, public sensor::Sensor, public i2c::I2CDevice {
 public:
  Ams5915(): PollingComponent(5000) {}
  void dump_config() override;
  void setup() override;
  void update() override;
  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { temperature_sensor_ = temperature_sensor; }
  void set_pressure_sensor(sensor::Sensor *pressure_sensor) { pressure_sensor_ = pressure_sensor; }
  void set_transducer_type(Transducer type); 
  
  int begin();
  int readSensor();
  float getPressure_Pa();
  float getTemperature_C();
  // transducer type
    Transducer _type;
  protected:
  // struct to hold sensor data
    struct Data {
      float Pressure_Pa;
      float Temp_C;
    };
    Data _data;
    // I2C bus
    TwoWire *_bus;
    // sensor address
    uint8_t _address;

    // buffer for I2C data
    uint8_t _buffer[4];
    // number of bytes received from I2C
    size_t _numBytes;
    // maximum number of attempts to talk to sensor
    const size_t _maxAttempts = 10;
    // track success of reading from sensor
    int _status;
    // pressure digital output, counts
    uint16_t _pressureCounts;      
    // temperature digital output, counts
    uint16_t _temperatureCounts;   
    // min and max pressure, millibar
    int _pMin;
    int _pMax;
    // i2c bus frequency
    const uint32_t _i2cRate = 400000;
    // conversion millibar to PA
    const float _mBar2Pa = 100.0f; 
    // digital output at minimum pressure
    const int _digOutPmin = 1638;   
    // digital output at maximum pressure
    const int _digOutPmax = 14745;  
    // min and max pressures, millibar
    const int AMS5915_0005_D_P_MIN = 0;
    const int AMS5915_0005_D_P_MAX = 5;
    const int AMS5915_0010_D_P_MIN = 0;
    const int AMS5915_0010_D_P_MAX = 10;
    const int AMS5915_0005_D_B_P_MIN = -5;
    const int AMS5915_0005_D_B_P_MAX = 5;
    const int AMS5915_0010_D_B_P_MIN = -10;
    const int AMS5915_0010_D_B_P_MAX = 10;
    const int AMS5915_0020_D_P_MIN = 0;
    const int AMS5915_0020_D_P_MAX = 20;
    const int AMS5915_0050_D_P_MIN = 0;
    const int AMS5915_0050_D_P_MAX = 50;
    const int AMS5915_0100_D_P_MIN = 0;
    const int AMS5915_0100_D_P_MAX = 100;
    const int AMS5915_0020_D_B_P_MIN = -20;
    const int AMS5915_0020_D_B_P_MAX = 20;
    const int AMS5915_0050_D_B_P_MIN = -50;
    const int AMS5915_0050_D_B_P_MAX = 50;
    const int AMS5915_0100_D_B_P_MIN = -100;
    const int AMS5915_0100_D_B_P_MAX = 100;
    const int AMS5915_0200_D_P_MIN = 0;
    const int AMS5915_0200_D_P_MAX = 200;
    const int AMS5915_0350_D_P_MIN = 0;
    const int AMS5915_0350_D_P_MAX = 350;
    const int AMS5915_1000_D_P_MIN = 0;
    const int AMS5915_1000_D_P_MAX = 1000;
    const int AMS5915_2000_D_P_MIN = 0;
    const int AMS5915_2000_D_P_MAX = 2000;
    const int AMS5915_4000_D_P_MIN = 0;
    const int AMS5915_4000_D_P_MAX = 4000;
    const int AMS5915_7000_D_P_MIN = 0;
    const int AMS5915_7000_D_P_MAX = 7000;
    const int AMS5915_10000_D_P_MIN = 0;
    const int AMS5915_10000_D_P_MAX = 10000;
    const int AMS5915_0200_D_B_P_MIN = -200;
    const int AMS5915_0200_D_B_P_MAX = 200;
    const int AMS5915_0350_D_B_P_MIN = -350;
    const int AMS5915_0350_D_B_P_MAX = 350;
    const int AMS5915_1000_D_B_P_MIN = -1000;
    const int AMS5915_1000_D_B_P_MAX = 1000;
    const int AMS5915_1000_A_P_MIN = 0;
    const int AMS5915_1000_A_P_MAX = 1000;
    const int AMS5915_1200_B_P_MIN = 700;
    const int AMS5915_1200_B_P_MAX = 1200;
    void getTransducer();
    int readBytes(uint16_t* pressureCounts, uint16_t* temperatureCounts);
    sensor::Sensor *temperature_sensor_{nullptr};
    sensor::Sensor *pressure_sensor_{nullptr};
};

}  // namespace ams5915
}  // namespace esphome