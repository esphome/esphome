/*
  AMS5915.cpp
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

#include "Arduino.h"
#include "AMS5915.h"

/* constructor, I2C bus, sensor address, and transducer type */
AMS5915::AMS5915(TwoWire &bus,uint8_t address,Transducer type){
  // I2C bus
  _bus = &bus; 
  // I2C address
  _address = address; 
  // transducer type
  _type = type; 
}

/* starts the I2C communication and sets the pressure and temperature ranges using getTransducer */
int AMS5915::begin(){
  // starting the I2C bus
	_bus->begin();
  // setting the I2C clock
  _bus->setClock(_i2cRate);
	// setting the min and max pressure based on the chip
	getTransducer();
  // checking to see if we can talk with the sensor
  for (size_t i=0; i < _maxAttempts; i++) {
    _status = readBytes(&_pressureCounts,&_temperatureCounts);
    if (_status > 0) {break;}
    delay(10);
  }
  return _status;
}

/* reads data from the sensor */
int AMS5915::readSensor(){
  // get pressure and temperature counts off transducer
  _status = readBytes(&_pressureCounts,&_temperatureCounts);
  // convert counts to pressure, PA
  _data.Pressure_Pa = (((float)(_pressureCounts - _digOutPmin))/(((float)(_digOutPmax - _digOutPmin))/((float)(_pMax - _pMin)))+(float)_pMin)*_mBar2Pa;
  // convert counts to temperature, C
  _data.Temp_C = (float)((_temperatureCounts*200))/2048.0f-50.0f;
  return _status;
}

/* returns the pressure value, PA */
float AMS5915::getPressure_Pa(){
  return _data.Pressure_Pa;
}

/* returns the temperature value, C */
float AMS5915::getTemperature_C(){
  return _data.Temp_C;
}

/* sets the pressure range based on the chip */
void AMS5915::getTransducer(){
  // setting the min and max pressures based on which transducer it is
  switch(_type) {
    case AMS5915_0005_D:
      _pMin = AMS5915_0005_D_P_MIN;
      _pMax = AMS5915_0005_D_P_MAX;
      break;
    case AMS5915_0010_D:
      _pMin = AMS5915_0010_D_P_MIN;
      _pMax = AMS5915_0010_D_P_MAX;
      break;
    case AMS5915_0005_D_B:
      _pMin = AMS5915_0005_D_B_P_MIN;
      _pMax = AMS5915_0005_D_B_P_MAX;
      break;
    case AMS5915_0010_D_B:
      _pMin = AMS5915_0010_D_B_P_MIN;
      _pMax = AMS5915_0010_D_B_P_MAX;
      break;
    case AMS5915_0020_D:
      _pMin = AMS5915_0020_D_P_MIN;
      _pMax = AMS5915_0020_D_P_MAX;
      break;
    case AMS5915_0050_D:
      _pMin = AMS5915_0050_D_P_MIN;
      _pMax = AMS5915_0050_D_P_MAX;
      break;
    case AMS5915_0100_D:
      _pMin = AMS5915_0100_D_P_MIN;
      _pMax = AMS5915_0100_D_P_MAX;
      break;
    case AMS5915_0020_D_B:
      _pMin = AMS5915_0020_D_B_P_MIN;
      _pMax = AMS5915_0020_D_B_P_MAX;
      break;
    case AMS5915_0050_D_B:
      _pMin = AMS5915_0050_D_B_P_MIN;
      _pMax = AMS5915_0050_D_B_P_MAX;
      break;
    case AMS5915_0100_D_B:
      _pMin = AMS5915_0100_D_B_P_MIN;
      _pMax = AMS5915_0100_D_B_P_MAX;
      break;
    case AMS5915_0200_D:
      _pMin = AMS5915_0200_D_P_MIN;
      _pMax = AMS5915_0200_D_P_MAX;
      break;
    case AMS5915_0350_D:
      _pMin = AMS5915_0350_D_P_MIN;
      _pMax = AMS5915_0350_D_P_MAX;
      break;
    case AMS5915_1000_D:
      _pMin = AMS5915_1000_D_P_MIN;
      _pMax = AMS5915_1000_D_P_MAX;
      break;
    case AMS5915_2000_D:
      _pMin = AMS5915_2000_D_P_MIN;
      _pMax = AMS5915_2000_D_P_MAX;
      break;
    case AMS5915_4000_D:
      _pMin = AMS5915_4000_D_P_MIN;
      _pMax = AMS5915_4000_D_P_MAX;
      break;
    case AMS5915_7000_D:
      _pMin = AMS5915_7000_D_P_MIN;
      _pMax = AMS5915_7000_D_P_MAX;
      break;
    case AMS5915_10000_D:
      _pMin = AMS5915_10000_D_P_MIN;
      _pMax = AMS5915_10000_D_P_MAX;
      break;
    case AMS5915_0200_D_B:
      _pMin = AMS5915_0200_D_B_P_MIN;
      _pMax = AMS5915_0200_D_B_P_MAX;
      break;
    case AMS5915_0350_D_B:
      _pMin = AMS5915_0350_D_B_P_MIN;
      _pMax = AMS5915_0350_D_B_P_MAX;
      break;
    case AMS5915_1000_D_B:
      _pMin = AMS5915_1000_D_B_P_MIN;
      _pMax = AMS5915_1000_D_B_P_MAX;
      break;
    case AMS5915_1000_A:
      _pMin = AMS5915_1000_A_P_MIN;
      _pMax = AMS5915_1000_A_P_MAX;
      break;
    case AMS5915_1200_B:
      _pMin = AMS5915_1200_B_P_MIN;
      _pMax = AMS5915_1200_B_P_MAX;
      break;
  }
}

/* reads pressure and temperature and returns values in counts */
int AMS5915::readBytes(uint16_t* pressureCounts,uint16_t* temperatureCounts){
  // read from sensor
  _numBytes = _bus->requestFrom(_address,sizeof(_buffer));
  // put the data in buffer
  if (_numBytes == sizeof(_buffer)) {
    _buffer[0] = _bus->read(); 
    _buffer[1] = _bus->read();
    _buffer[2] = _bus->read();
    _buffer[3] = _bus->read();
    // assemble into a uint16_t
    *pressureCounts = (((uint16_t) (_buffer[0]&0x3F)) <<8) + (((uint16_t) _buffer[1]));
    *temperatureCounts = (((uint16_t) (_buffer[2])) <<3) + (((uint16_t) _buffer[3]&0xE0)>>5);
    _status = 1;
  } else {
    _status = -1;
  }
  return _status;
}
