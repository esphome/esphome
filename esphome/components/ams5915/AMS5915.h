/*
  AMS5915.h
  Brian R Taylor
  brian.taylor@bolderflight.com

  Copyright (c) 2017 Bolder Flight Systems

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#ifndef AMS5915_h
#define AMS5915_h

#include "Wire.h"
#include "Arduino.h"

class AMS5915{
  public:
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
    AMS5915(TwoWire &bus,uint8_t address,Transducer type);
    int begin();
    int readSensor();
    float getPressure_Pa();
    float getTemperature_C();
  private:
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
    // transducer type
    Transducer _type;
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
};

#endif
