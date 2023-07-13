#include "esphome/core/log.h"
#include "absolute_humidity.h"

namespace esphome {
namespace absolute_humidity {

static const char *const TAG = "absolute_humidity.sensor";

void AbsoluteHumidityComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up absolute humidity '%s'...", this->get_name().c_str());

  ESP_LOGD(TAG, "  Added callback for temperature '%s'", this->temperature_sensor_->get_name().c_str());
  this->temperature_sensor_->add_on_state_callback([this](float state) { this->temperature_callback_(state); });
  if (this->temperature_sensor_->has_state()) {
    this->temperature_callback_(this->temperature_sensor_->get_state());
  }

  ESP_LOGD(TAG, "  Added callback for relative humidity '%s'", this->humidity_sensor_->get_name().c_str());
  this->humidity_sensor_->add_on_state_callback([this](float state) { this->humidity_callback_(state); });
  if (this->humidity_sensor_->has_state()) {
    this->humidity_callback_(this->humidity_sensor_->get_state());
  }
}

void AbsoluteHumidityComponent::dump_config() {
  LOG_SENSOR("", "Absolute Humidity", this);

  switch (this->equation_) {
    case BUCK:
      ESP_LOGCONFIG(TAG, "Saturation Vapor Pressure Equation: Buck");
      break;
    case TETENS:
      ESP_LOGCONFIG(TAG, "Saturation Vapor Pressure Equation: Tetens");
      break;
    case WOBUS:
      ESP_LOGCONFIG(TAG, "Saturation Vapor Pressure Equation: Wobus");
      break;
    default:
      ESP_LOGE(TAG, "Invalid saturation vapor pressure equation selection!");
      break;
  }

  ESP_LOGCONFIG(TAG, "Sources");
  ESP_LOGCONFIG(TAG, "  Temperature: '%s'", this->temperature_sensor_->get_name().c_str());
  ESP_LOGCONFIG(TAG, "  Relative Humidity: '%s'", this->humidity_sensor_->get_name().c_str());
}

float AbsoluteHumidityComponent::get_setup_priority() const { return setup_priority::DATA; }

void AbsoluteHumidityComponent::loop() {
  if (!this->next_update_) {
    return;
  }
  this->next_update_ = false;

  // Ensure we have source data
  const bool no_temperature = std::isnan(this->temperature_);
  const bool no_humidity = std::isnan(this->humidity_);
  if (no_temperature || no_humidity) {
    if (no_temperature) {
      ESP_LOGW(TAG, "No valid state from temperature sensor!");
    }
    if (no_humidity) {
      ESP_LOGW(TAG, "No valid state from temperature sensor!");
    }
    ESP_LOGW(TAG, "Unable to calculate absolute humidity.");
    this->publish_state(NAN);
    this->status_set_warning();
    return;
  }

  // Convert to desired units
  const float temperature_c = this->temperature_;
  const float temperature_k = temperature_c + 273.15;
  const float hr = this->humidity_ / 100;

  // Calculate saturation vapor pressure
  float es;
  switch (this->equation_) {
    case BUCK:
      es = es_buck(temperature_c);
      break;
    case TETENS:
      es = es_tetens(temperature_c);
      break;
    case WOBUS:
      es = es_wobus(temperature_c);
      break;
    default:
      ESP_LOGE(TAG, "Invalid saturation vapor pressure equation selection!");
      this->publish_state(NAN);
      this->status_set_error();
      return;
  }
  ESP_LOGD(TAG, "Saturation vapor pressure %f kPa", es);

  // Calculate absolute humidity
  const float absolute_humidity = vapor_density(es, hr, temperature_k);

  // Publish absolute humidity
  ESP_LOGD(TAG, "Publishing absolute humidity %f g/m³", absolute_humidity);
  this->status_clear_warning();
  this->publish_state(absolute_humidity);
}

// Buck equation (https://en.wikipedia.org/wiki/Arden_Buck_equation)
// More accurate than Tetens in normal meteorologic conditions
float AbsoluteHumidityComponent::es_buck(float temperature_c) {
  float a, b, c, d;
  if (temperature_c >= 0) {
    a = 0.61121;
    b = 18.678;
    c = 234.5;
    d = 257.14;
  } else {
    a = 0.61115;
    b = 18.678;
    c = 233.7;
    d = 279.82;
  }
  return a * expf((b - (temperature_c / c)) * (temperature_c / (d + temperature_c)));
}

// Tetens equation (https://en.wikipedia.org/wiki/Tetens_equation)
float AbsoluteHumidityComponent::es_tetens(float temperature_c) {
  float a, b;
  if (temperature_c >= 0) {
    a = 17.27;
    b = 237.3;
  } else {
    a = 21.875;
    b = 265.5;
  }
  return 0.61078 * expf((a * temperature_c) / (temperature_c + b));
}

// Wobus equation
// https://wahiduddin.net/calc/density_altitude.htm
// https://wahiduddin.net/calc/density_algorithms.htm
// Calculate the saturation vapor pressure (kPa)
float AbsoluteHumidityComponent::es_wobus(float t) {
  // THIS FUNCTION RETURNS THE SATURATION VAPOR PRESSURE ESW (MILLIBARS)
  // OVER LIQUID WATER GIVEN THE TEMPERATURE T (CELSIUS). THE POLYNOMIAL
  // APPROXIMATION BELOW IS DUE TO HERMAN WOBUS, A MATHEMATICIAN WHO
  // WORKED AT THE NAVY WEATHER RESEARCH FACILITY, NORFOLK, VIRGINIA,
  // BUT WHO IS NOW RETIRED. THE COEFFICIENTS OF THE POLYNOMIAL WERE
  // CHOSEN TO FIT THE VALUES IN TABLE 94 ON PP. 351-353 OF THE SMITH-
  // SONIAN METEOROLOGICAL TABLES BY ROLAND LIST (6TH EDITION). THE
  // APPROXIMATION IS VALID FOR -50 < T < 100C.
  //
  //     Baker, Schlatter  17-MAY-1982     Original version.

  const float c0 = +0.99999683e00;
  const float c1 = -0.90826951e-02;
  const float c2 = +0.78736169e-04;
  const float c3 = -0.61117958e-06;
  const float c4 = +0.43884187e-08;
  const float c5 = -0.29883885e-10;
  const float c6 = +0.21874425e-12;
  const float c7 = -0.17892321e-14;
  const float c8 = +0.11112018e-16;
  const float c9 = -0.30994571e-19;
  const float p = c0 + t * (c1 + t * (c2 + t * (c3 + t * (c4 + t * (c5 + t * (c6 + t * (c7 + t * (c8 + t * (c9)))))))));
  return 0.61078 / pow(p, 8);
}

// From https://www.environmentalbiophysics.org/chalk-talk-how-to-calculate-absolute-humidity/
// H/T to https://esphome.io/cookbook/bme280_environment.html
// H/T to https://carnotcycle.wordpress.com/2012/08/04/how-to-convert-relative-humidity-to-absolute-humidity/
float AbsoluteHumidityComponent::vapor_density(float es, float hr, float ta) {
  // es = saturated vapor pressure (kPa)
  // hr = relative humidity [0-1]
  // ta = absolute temperature (K)

  const float ea = hr * es * 1000;   // vapor pressure of the air (Pa)
  const float mw = 18.01528;         // molar mass of water (g⋅mol⁻¹)
  const float r = 8.31446261815324;  // molar gas constant (J⋅K⁻¹)
  return (ea * mw) / (r * ta);
}

}  // namespace absolute_humidity
}  // namespace esphome
