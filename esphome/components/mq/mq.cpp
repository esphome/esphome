#include "esphome/core/log.h"
#include "mq.h"

#include <cmath>

namespace esphome {
namespace mq {

static const float VOLTAGE_RESOLUTION = 3.3;
static const int ADC_BIT_RESOLUTION_ESP8266 = 10;
static const int ADC_BIT_RESOLUTION_ESP32 = 12;
static const int RETRY_INTERVAL = 20;
static const int RETRIES = 2;
static const int REGRESSION_METHOD = 1;
static const char *const TAG = "mq";
static const std::map<MQModel, std::vector<MQGasSensor>> GAS_PARAMETERS_DEFINITION = {
    {MQ_MODEL_2,
     {
         {.gas_type = MQ_GAS_TYPE_H2, .a = 987.99, .b = -2.162},
         {.gas_type = MQ_GAS_TYPE_LPG, .a = 574.25, .b = -2.222},
         {.gas_type = MQ_GAS_TYPE_CO, .a = 36974.0, .b = -3.109},
         {.gas_type = MQ_GAS_TYPE_ALCOHOL, .a = 3616.1, .b = -2.675},
         {.gas_type = MQ_GAS_TYPE_PROPANE, .a = 658.71, .b = -2.168},
     }},
    {MQ_MODEL_3,
     {
         {.gas_type = MQ_GAS_TYPE_LPG, .a = 44771, .b = -3.245},
         {.gas_type = MQ_GAS_TYPE_CH4, .a = 2 * 10 ^ 31, .b = 19.01},
         {.gas_type = MQ_GAS_TYPE_CO, .a = 521853, .b = -3.821},
         {.gas_type = MQ_GAS_TYPE_ALCOHOL, .a = 0.3934, .b = -1.504},
         {.gas_type = MQ_GAS_TYPE_BENZENE, .a = 4.8387, .b = -2.68},
         {.gas_type = MQ_GAS_TYPE_HEXANE, .a = 7585.3, .b = -2.849},
     }},
    {MQ_MODEL_4,
     {
         {.gas_type = MQ_GAS_TYPE_LPG, .a = 3811.9, .b = -3.113},
         {.gas_type = MQ_GAS_TYPE_CH4, .a = 1012.7, .b = -2.786},
         {.gas_type = MQ_GAS_TYPE_CO, .a = 200000000000000.0, .b = -19.05},
         {.gas_type = MQ_GAS_TYPE_ALCOHOL, .a = 60000000000.0, .b = -14.01},
         {.gas_type = MQ_GAS_TYPE_SMOKE, .a = 30000000.0, .b = -8.308},
     }},

    {MQ_MODEL_5,
     {
         {.gas_type = MQ_GAS_TYPE_H2, .a = 1163.8, .b = -3.874},
         {.gas_type = MQ_GAS_TYPE_LPG, .a = 80.897, .b = -2.431},
         {.gas_type = MQ_GAS_TYPE_CH4, .a = 177.65, .b = -2.56},
         {.gas_type = MQ_GAS_TYPE_CO, .a = 491204, .b = -5.826},
         {.gas_type = MQ_GAS_TYPE_ALCOHOL, .a = 97124, .b = -4.918},
     }},
    {MQ_MODEL_6,
     {
         {.gas_type = MQ_GAS_TYPE_H2, .a = 88158, .b = -3.597},
         {.gas_type = MQ_GAS_TYPE_LPG, .a = 1009.2, .b = -2.35},
         {.gas_type = MQ_GAS_TYPE_CH4, .a = 2127.2, .b = -2.526},
         {.gas_type = MQ_GAS_TYPE_CO, .a = 1000000000000000.0, .b = -13.5},
         {.gas_type = MQ_GAS_TYPE_ALCOHOL, .a = 50000000.0, .b = -6.017},
     }},
    {MQ_MODEL_7,
     {
         {.gas_type = MQ_GAS_TYPE_H2, .a = 69.014, .b = -1.374},
         {.gas_type = MQ_GAS_TYPE_LPG, .a = 700000000.0, .b = -7.703},
         {.gas_type = MQ_GAS_TYPE_CH4, .a = 60000000000000.0, .b = -10.54},
         {.gas_type = MQ_GAS_TYPE_CO, .a = 99.042, .b = -1.518},
         {.gas_type = MQ_GAS_TYPE_ALCOHOL, .a = 40000000000000000.0, .b = -12.35},
     }},
    {MQ_MODEL_8,
     {
         {.gas_type = MQ_GAS_TYPE_H2, .a = 976.97, .b = -0.688},
         {.gas_type = MQ_GAS_TYPE_LPG, .a = 10000000.0, .b = -3.123},
         {.gas_type = MQ_GAS_TYPE_CH4, .a = 80000000000000.0, .b = -6.666},
         {.gas_type = MQ_GAS_TYPE_CO, .a = 2000000000000000000.0, .b = -8.074},
         {.gas_type = MQ_GAS_TYPE_ALCOHOL, .a = 76101.0, .b = -1.86},
     }},
    {MQ_MODEL_9,
     {
         {.gas_type = MQ_GAS_TYPE_LPG, .a = 1000.5, .b = -2.186},
         {.gas_type = MQ_GAS_TYPE_CH4, .a = 4269.6, .b = -2.648},
         {.gas_type = MQ_GAS_TYPE_CO, .a = 599.65, .b = -2.244},
     }},
    {MQ_MODEL_135,
     {
         {.gas_type = MQ_GAS_TYPE_CO, .a = 605.18, .b = -3.937},
         {.gas_type = MQ_GAS_TYPE_ALCOHOL, .a = 77.255, .b = -3.18},
         {.gas_type = MQ_GAS_TYPE_CO2, .a = 110.47, .b = -2.862},
         {.gas_type = MQ_GAS_TYPE_TOLUENO, .a = 44.947, .b = -3.445},
         {.gas_type = MQ_GAS_TYPE_NH4, .a = 102.2, .b = -2.473},
         {.gas_type = MQ_GAS_TYPE_ACETONA, .a = 34.668, .b = -3.369},
     }},
};

// Public

MQSensor::MQSensor(GPIOPin *pin, MQModel model, bool is_esp8266, float rl) {
  this->pin_ = pin;
  this->is_esp8266_ = is_esp8266;
  this->rl_ = rl;
  this->model_parameters_ = {.model = model, .gas_sensors = GAS_PARAMETERS_DEFINITION.at(model)};
  this->adc_bit_resolution_ = this->is_esp8266_ ? ADC_BIT_RESOLUTION_ESP8266 : ADC_BIT_RESOLUTION_ESP32;
}

void MQSensor::add_sensor(sensor::Sensor *sensor, MQGasType gas_type) {
  std::vector<MQGasSensor>::iterator it =
      std::find_if(this->model_parameters_.gas_sensors.begin(), this->model_parameters_.gas_sensors.end(),
                   [gas_type](const MQGasSensor &e) { return e.gas_type == gas_type; });
  it->sensor = sensor;
}

void MQSensor::set_r0(float r0) { this->r0_ = r0; }

// Component overrides

float MQSensor::get_setup_priority() const { return setup_priority::DATA; }

void MQSensor::setup() {
  ESP_LOGCONFIG(TAG, "Setting up %s...", this->model_parameters_.model_name().c_str());

  if (this->r0_ == 0.0) {
    ESP_LOGCONFIG(TAG, "Calibrating please wait.");
    float clean_air_ratio = this->model_parameters_.get_ratio_in_clean_air();
    float calc_r0 = 0;
    for (int i = 1; i <= 100; i++) {
      this->update_sensor_voltage();
      calc_r0 += this->calibrate(clean_air_ratio);
    }
    this->r0_ = calc_r0 / 100;
    ESP_LOGCONFIG(TAG, "Calibrated R0 = %.2f kΩ", this->r0_);
  }

  if (isinf(this->r0_)) {
    ESP_LOGW(TAG, "Warning: Conection issue founded, R0 is infite (Open circuit detected) please check your "
                  "wiring and supply");
    mark_failed();
  }
  if (this->r0_ == 0) {
    ESP_LOGW(TAG, "Warning: Conection issue founded, R0 is zero (Analog pin with short circuit to ground) please "
                  "check your wiring and supply");
    mark_failed();
  }
}

void MQSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "%s", this->model_parameters_.model_name().c_str());
  ESP_LOGCONFIG(TAG, "  R0: %.2f kΩ", this->r0_);
  ESP_LOGCONFIG(TAG, "  RL: %.2f kΩ", this->rl_);
  LOG_PIN("  Pin: ", this->pin_);
  LOG_UPDATE_INTERVAL(this);

  for (auto &it : this->model_parameters_.gas_sensors) {
    LOG_SENSOR("  ", it.gas_name().c_str(), it.sensor);
  }
}

void MQSensor::loop() {}

void MQSensor::update() {
  this->update_sensor_voltage();

  ESP_LOGD(TAG, "Update %s", this->model_parameters_.model_name().c_str());

  for (auto &it : this->model_parameters_.gas_sensors) {
    if (it.sensor == nullptr) {
      continue;
    }
    this->set_a(it.a);
    this->set_b(it.b);
    float gas_value = this->read_sensor();

    ESP_LOGD(TAG, "%s: %.2f", it.gas_name().c_str(), gas_value);

    it.sensor->publish_state(gas_value);
  }
}

// Private

void MQSensor::update_sensor_voltage() {
  float avg = 0.0;
  for (int i = 0; i < RETRIES; i++) {
    avg += analogRead(this->pin_->get_pin());  // NOLINT
    delay(RETRY_INTERVAL);
  }
  this->sensor_volt_ = (avg / RETRIES) * VOLTAGE_RESOLUTION / ((pow(2, this->adc_bit_resolution_)) - 1);
}

float MQSensor::read_sensor() {
  // More explained in: https://jayconsystems.com/blog/understanding-a-gas-sensor
  // Get value of RS in a gas
  float rs_calc = ((VOLTAGE_RESOLUTION * rl_) / this->sensor_volt_) - this->rl_;
  if (rs_calc < 0) {
    rs_calc = 0;
  }

  // Get ratio RS_gas/RS_air
  float ratio = rs_calc / this->r0_;

  if (ratio <= 0) {
    ratio = 0;
  }

  float ppm = 0.0;
  if (REGRESSION_METHOD == 1) {
    // Source excel analisis
    // https://github.com/miguel5612/MQSensorsLib_Docs/tree/master/Internal_design_documents
    ppm = this->a_ * std::pow(ratio, this->b_);
  } else {
    // Source of linear ecuation
    // https://jayconsystems.com/blog/understanding-a-gas-sensor
    // Get ppm value in linear scale according to the the ratio value
    double ppm_log = (std::log10(ratio) - this->b_) / this->a_;
    // Convert ppm value to log scale
    ppm = pow(10, ppm_log);
  }

  if (ppm < 0) {
    ppm = 0;
  }

  return ppm;
}

float MQSensor::calibrate(float ratio_in_clean_air) {
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
  // Define variable for sensor resistance
  float rs_air;
  // Define variable for R0
  float r0;
  // Calculate RS in fresh air
  rs_air = ((VOLTAGE_RESOLUTION * this->rl_) / this->sensor_volt_) - this->rl_;
  if (rs_air < 0)
    rs_air = 0;                      // No negative values accepted.
  r0 = rs_air / ratio_in_clean_air;  // Calculate R0
  if (r0 < 0) {
    r0 = 0;
  }

  return r0;
}

void MQSensor::set_a(float a) { this->a_ = a; }
void MQSensor::set_b(float b) { this->b_ = b; }

}  // namespace mq
}  // namespace esphome
