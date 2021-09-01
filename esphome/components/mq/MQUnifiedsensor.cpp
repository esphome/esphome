#include "MQUnifiedsensor.h"

MQUnifiedsensor::MQUnifiedsensor(String Placa, float Voltage_Resolution, int ADC_Bit_Resolution, int pin, String type) {
  this->_pin = pin;
  Placa.toCharArray(this->_placa, 20);
  type.toCharArray(this->_type, 6);
  // this->_type = type; //MQ-2, MQ-3 ... MQ-309A
  // this->_placa = Placa;
  this->_VOLT_RESOLUTION = Voltage_Resolution;
  this->_ADC_Bit_Resolution = ADC_Bit_Resolution;
}
void MQUnifiedsensor::init() { pinMode(_pin, INPUT); }
void MQUnifiedsensor::setA(float a) { this->_a = a; }
void MQUnifiedsensor::setB(float b) { this->_b = b; }
void MQUnifiedsensor::setR0(float R0) { this->_R0 = R0; }
void MQUnifiedsensor::setRL(float RL) { this->_RL = RL; }
void MQUnifiedsensor::setADC(int value) {
  this->_sensor_volt = (value) *_VOLT_RESOLUTION / ((pow(2, _ADC_Bit_Resolution)) - 1);
  this->_adc = value;
}
void MQUnifiedsensor::setVoltResolution(float voltage_resolution) { _VOLT_RESOLUTION = voltage_resolution; }
void MQUnifiedsensor::setRegressionMethod(int regressionMethod) {
  // this->_regressionMethod = regressionMethod;
  this->_regressionMethod = regressionMethod;
}
float MQUnifiedsensor::getR0() { return _R0; }
float MQUnifiedsensor::getRL() { return _RL; }
float MQUnifiedsensor::getVoltResolution() { return _VOLT_RESOLUTION; }
String MQUnifiedsensor::getRegressionMethod() {
  if (_regressionMethod == 1)
    return "Exponential";
  else
    return "Linear";
}
float MQUnifiedsensor::getA() { return _a; }
float MQUnifiedsensor::getB() { return _b; }
void MQUnifiedsensor::serialDebug(bool onSetup) {
  if (onSetup) {
    Serial.println();
    Serial.println("***************************************************************************************************"
                   "*********************************************");
    Serial.println("MQ sensor reading library for arduino");

    Serial.println(
        "Note: remember that all the parameters below can be modified during the program execution with the methods:");
    Serial.println("setR0, setRL, setA, setB where you will have to send as parameter the new value, example: "
                   "mySensor.setR0(20); //R0 = 20KΩ");

    Serial.println("Authors: Miguel A. Califa U - Yersson R. Carrillo A - Ghiordy F. Contreras C");
    Serial.println("Contributors: Andres A. Martinez - Juan A. Rodríguez - Mario A. Rodríguez O ");

    Serial.print("Sensor: ");
    Serial.println(_type);
    Serial.print("Supply voltage: ");
    Serial.print(_VOLT_RESOLUTION);
    Serial.println(" VDC");
    Serial.print("ADC Resolution: ");
    Serial.print(_ADC_Bit_Resolution);
    Serial.println(" Bits");
    Serial.print("R0: ");
    Serial.print(_R0);
    Serial.println(" KΩ");
    Serial.print("RL: ");
    Serial.print(_RL);
    Serial.println(" KΩ");

    Serial.print("Model: ");
    if (_regressionMethod == 1)
      Serial.println("Exponential");
    else
      Serial.println("Linear");
    Serial.print(_type);
    Serial.print(" -> a: ");
    Serial.print(_a);
    Serial.print(" | b: ");
    Serial.println(_b);

    Serial.print("Development board: ");
    Serial.println(_placa);
  } else {
    if (!_firstFlag) {
      Serial.print("| ********************************************************************");
      Serial.print(_type);
      Serial.println("*********************************************************************|");
      Serial.println("|ADC_In | Equation_V_ADC | Voltage_ADC |        Equation_RS        | Resistance_RS  |    "
                     "EQ_Ratio  | Ratio (RS/R0) | Equation_PPM |     PPM    |");
      _firstFlag = true;  // Headers are printed
    } else {
      Serial.print("|");
      Serial.print(_adc);
      Serial.print("| v = ADC*");
      Serial.print(_VOLT_RESOLUTION);
      Serial.print("/");
      Serial.print((pow(2, _ADC_Bit_Resolution)) - 1);
      Serial.print("  |    ");
      Serial.print(_sensor_volt);
      Serial.print("     | RS = ((");
      Serial.print(_VOLT_RESOLUTION);
      Serial.print("*RL)/Voltage) - RL|      ");
      Serial.print(_RS_Calc);
      Serial.print("     | Ratio = RS/R0|    ");
      Serial.print(_ratio);
      Serial.print("       |   ");
      if (_regressionMethod == 1)
        Serial.print("ratio*a + b");
      else
        Serial.print("pow(10, (log10(ratio)-b)/a)");
      Serial.print("  |   ");
      Serial.print(_PPM);
      Serial.println("  |");
    }
  }
}
void MQUnifiedsensor::update() { _sensor_volt = this->getVoltage(); }
float MQUnifiedsensor::validateEcuation(float ratioInput) {
  // Serial.print("Ratio input: "); Serial.println(ratioInput);
  // Serial.print("a: "); Serial.println(_a);
  // Serial.print("b: "); Serial.println(_b);
  // Usage of this function: Unit test on ALgorithmTester example;
  if (_regressionMethod == 1)
    _PPM = _a * pow(ratioInput, _b);
  else {
    // https://jayconsystems.com/blog/understanding-a-gas-sensor
    double ppm_log = (log10(ratioInput) - _b) / _a;  // Get ppm value in linear scale according to the the ratio value
    _PPM = pow(10, ppm_log);                         // Convert ppm value to log scale
  }
  // Serial.println("Regression Method: "); Serial.println(_regressionMethod);
  // Serial.println("Result: "); Serial.println(_PPM);
  return _PPM;
}
float MQUnifiedsensor::readSensor() {
  // More explained in: https://jayconsystems.com/blog/understanding-a-gas-sensor
  _RS_Calc = ((_VOLT_RESOLUTION * _RL) / _sensor_volt) - _RL;  // Get value of RS in a gas
  if (_RS_Calc < 0)
    _RS_Calc = 0;                 // No negative values accepted.
  _ratio = _RS_Calc / this->_R0;  // Get ratio RS_gas/RS_air
  if (_ratio <= 0)
    _ratio = 0;  // No negative values accepted or upper datasheet recomendation.
  if (_regressionMethod == 1)
    _PPM =
        _a * pow(_ratio, _b);  // <- Source excel analisis
                               // https://github.com/miguel5612/MQSensorsLib_Docs/tree/master/Internal_design_documents
  else {
    // https://jayconsystems.com/blog/understanding-a-gas-sensor <- Source of linear ecuation
    double ppm_log = (log10(_ratio) - _b) / _a;  // Get ppm value in linear scale according to the the ratio value
    _PPM = pow(10, ppm_log);                     // Convert ppm value to log scale
  }
  if (_PPM < 0)
    _PPM = 0;  // No negative values accepted or upper datasheet recomendation.
  // if(_PPM > 10000) _PPM = 99999999; //No negative values accepted or upper datasheet recomendation.
  return _PPM;
}
float MQUnifiedsensor::readSensorR0Rs() {
  // More explained in: https://jayconsystems.com/blog/understanding-a-gas-sensor
  _RS_Calc = ((_VOLT_RESOLUTION * _RL) / _sensor_volt) - _RL;  // Get value of RS in a gas
  if (_RS_Calc < 0)
    _RS_Calc = 0;                 // No negative values accepted.
  _ratio = this->_R0 / _RS_Calc;  // Get ratio RS_air/RS_gas <- INVERTED for MQ-131 issue 28
                                  // https://github.com/miguel5612/MQSensorsLib/issues/28
  if (_ratio <= 0)
    _ratio = 0;  // No negative values accepted or upper datasheet recomendation.
  if (_regressionMethod == 1)
    _PPM =
        _a * pow(_ratio, _b);  // <- Source excel analisis
                               // https://github.com/miguel5612/MQSensorsLib_Docs/tree/master/Internal_design_documents
  else {
    // https://jayconsystems.com/blog/understanding-a-gas-sensor <- Source of linear ecuation
    double ppm_log = (log10(_ratio) - _b) / _a;  // Get ppm value in linear scale according to the the ratio value
    _PPM = pow(10, ppm_log);                     // Convert ppm value to log scale
  }
  if (_PPM < 0)
    _PPM = 0;  // No negative values accepted or upper datasheet recomendation.
  // if(_PPM > 10000) _PPM = 99999999; //No negative values accepted or upper datasheet recomendation.
  return _PPM;
}
float MQUnifiedsensor::calibrate(float ratioInCleanAir) {
  // More explained in: https://jayconsystems.com/blog/understanding-a-gas-sensor
  /*
  V = I x R
  VRL = [VC / (RS + RL)] x RL
  VRL = (VC x RL) / (RS + RL)
  Así que ahora resolvemos para RS:
  VRL x (RS + RL) = VC x RL
  (VRL x RS) + (VRL x RL) = VC x RL
  (VRL x RS) = (VC x RL) - (VRL x RL)
  RS = [(VC x RL) - (VRL x RL)] / VRL
  RS = [(VC x RL) / VRL] - RL
  */
  float RS_air;                                              // Define variable for sensor resistance
  float R0;                                                  // Define variable for R0
  RS_air = ((_VOLT_RESOLUTION * _RL) / _sensor_volt) - _RL;  // Calculate RS in fresh air
  if (RS_air < 0)
    RS_air = 0;                   // No negative values accepted.
  R0 = RS_air / ratioInCleanAir;  // Calculate R0
  if (R0 < 0)
    R0 = 0;  // No negative values accepted.
  return R0;
}
float MQUnifiedsensor::getVoltage(int read) {
  float voltage;
  if (read) {
    float avg = 0.0;
    for (int i = 0; i < retries; i++) {
      _adc = analogRead(this->_pin);
      avg += _adc;
      delay(retry_interval);
    }
    voltage = (avg / retries) * _VOLT_RESOLUTION / ((pow(2, _ADC_Bit_Resolution)) - 1);
  } else {
    voltage = _sensor_volt;
  }
  return voltage;
}
float MQUnifiedsensor::stringTofloat(String &str) { return atof(str.c_str()); }
