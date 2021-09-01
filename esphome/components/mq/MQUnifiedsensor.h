#ifndef MQUnifiedsensor_H
#define MQUnifiedsensor_H

#include <Arduino.h>
#include <stdint.h>

/***********************Software Related Macros************************************/

#define ADC_RESOLUTION 10  // for 10bit analog to digital converter.
#define retries 2
#define retry_interval 20

class MQUnifiedsensor {
 public:
  MQUnifiedsensor(String Placa = "Arduino", float Voltage_Resolution = 5, int ADC_Bit_Resolution = 10, int pin = 1,
                  String type = "CUSTOM MQ");

  // Functions to set values
  void init();
  void update();
  void setR0(float R0 = 10);
  void setRL(float RL = 10);
  void setA(float a);
  void setB(float b);
  void setRegressionMethod(int regressionMethod);
  void setVoltResolution(float voltage_resolution = 5);
  void serialDebug(bool onSetup = false);  // Show on serial port information about sensor
  void setADC(int value);                  // For external ADC Usage

  // user functions
  float calibrate(float ratioInCleanAir);
  float readSensor();
  float readSensorR0Rs();
  float validateEcuation(float ratioInput = 0);

  // get function for info
  float getA();
  float getB();
  float getR0();
  float getRL();
  float getVoltResolution();
  String getRegressionMethod();
  float getVoltage(int read = true);

  float stringTofloat(String &str);

 private:
  /************************Private vars************************************/
  byte _pin;
  byte _firstFlag = false;
  byte _VOLT_RESOLUTION = 5.0;  // if 3.3v use 3.3
  byte _RL = 10;                // Value in KiloOhms
  byte _ADC_Bit_Resolution = 10;
  byte _regressionMethod = 1;  // 1 -> Exponential || 2 -> Linear

  float _adc, _a, _b, _sensor_volt;
  float _R0, RS_air, _ratio, _PPM, _RS_Calc;

  char _type[6];
  char _placa[20];
};

#endif  // MQUnifiedsensor_H
