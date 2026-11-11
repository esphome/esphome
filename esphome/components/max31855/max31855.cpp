#include "max31855.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <cmath>

namespace esphome::max31855 {

ESPHOME_LOG_TAG(TAG, "max31855");

static constexpr uint32_t READ_TIMEOUT_ID = 0;

// NIST ITS-90 type K polynomials (NIST Monograph 175). E in mV, T in °C.
// E(T), -270 to 0 °C.
static constexpr float K_E_NEG[] = {0.0f,
                                    0.394501280250E-01f,
                                    0.236223735980E-04f,
                                    -0.328589067840E-06f,
                                    -0.499048287770E-08f,
                                    -0.675090591730E-10f,
                                    -0.574103274280E-12f,
                                    -0.310888728940E-14f,
                                    -0.104516093650E-16f,
                                    -0.198892668780E-19f,
                                    -0.163226974860E-22f};
// E(T), 0 to 1372 °C, plus the exponential term a0 * exp(a1 * (T - a2)^2).
static constexpr float K_E_POS[] = {
    -0.176004136860E-01f, 0.389212049750E-01f, 0.185587700320E-04f,  -0.994575928740E-07f, 0.318409457190E-09f,
    -0.560728448890E-12f, 0.560750590590E-15f, -0.320207200030E-18f, 0.971511471520E-22f,  -0.121047212750E-25f};
static constexpr float K_E_A0 = 0.118597600000E+00f;
static constexpr float K_E_A1 = -0.118343200000E-03f;
static constexpr float K_E_A2 = 0.126968600000E+03f;
// Inverse T(E): -5.891 to 0 mV, 0 to 20.644 mV, 20.644 to 54.886 mV.
static constexpr float K_T_NEG[] = {0.0f,
                                    2.5173462E+01f,
                                    -1.1662878f,
                                    -1.0833638f,
                                    -8.9773540E-01f,
                                    -3.7342377E-01f,
                                    -8.6632643E-02f,
                                    -1.0450598E-02f,
                                    -5.1920577E-04f};
static constexpr float K_T_LOW[] = {0.0f,           2.508355E+01f, 7.860106E-02f,  -2.503131E-01f, 8.315270E-02f,
                                    -1.228034E-02f, 9.804036E-04f, -4.413030E-05f, 1.057734E-06f,  -1.052755E-08f};
static constexpr float K_T_HIGH[] = {-1.318058E+02f, 4.830222E+01f, -1.646031f,    5.464731E-02f,
                                     -9.650715E-04f, 8.802193E-06f, -3.110810E-08f};

template<size_t N> static float poly(const float (&c)[N], float x) {
  float r = 0.0f;
  for (size_t i = N; i-- > 0;)
    r = r * x + c[i];
  return r;
}

// Type K thermoelectric voltage in mV for a temperature in °C.
static float k_type_mv(float t) {
  if (t < 0.0f)
    return poly(K_E_NEG, t);
  const float d = t - K_E_A2;
  return poly(K_E_POS, t) + K_E_A0 * expf(K_E_A1 * d * d);
}

// Type K temperature in °C for a thermoelectric voltage in mV.
static float k_type_celsius(float mv) {
  if (mv < 0.0f)
    return poly(K_T_NEG, mv);
  if (mv < 20.644f)
    return poly(K_T_LOW, mv);
  return poly(K_T_HIGH, mv);
}

// The MAX31855 reports t_chip = t_cj + V_tc / 41.276 uV/°C, a straight-line
// approximation of the type K curve. Recover V_tc, add the NIST voltage of
// the cold junction, and convert the sum back with the NIST inverse.
static float k_type_linearize(float t_chip, float t_cj) {
  const float v_tc_mv = 0.041276f * (t_chip - t_cj);
  return k_type_celsius(v_tc_mv + k_type_mv(t_cj));
}

void MAX31855Sensor::update() {
  this->enable();
  delay(1);
  // conversion initiated by rising edge
  this->disable();

  // Conversion time typ: 170ms, max: 220ms
  this->set_timeout(READ_TIMEOUT_ID, 220, [this]() { this->read_data_(); });
}

void MAX31855Sensor::setup() { this->spi_setup(); }
void MAX31855Sensor::dump_config() {
  ESP_LOGCONFIG(TAG, "MAX31855:");
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Thermocouple", this);
  ESP_LOGCONFIG(TAG, "  Type K linearization: %s", YESNO(this->linearize_));
  if (this->reference_sensor_) {
    LOG_SENSOR("  ", "Reference", this->reference_sensor_);
  } else {
    ESP_LOGCONFIG(TAG, "  Reference temperature disabled.");
  }
}
void MAX31855Sensor::read_data_() {
  this->enable();
  delay(1);
  uint8_t data[4];
  this->read_array(data, 4);
  this->disable();

  const uint32_t mem = encode_uint32(data[0], data[1], data[2], data[3]);

  // Verify we got data
  if (mem != 0xFFFFFFFF) {
    this->status_clear_error();
  } else {
    ESP_LOGE(TAG, "No data received from MAX31855 (0x%08" PRIX32 "). Check wiring!", mem);
    this->publish_state(NAN);
    PUBLISH_STATE(this->reference_sensor_, NAN);
    this->status_set_error();
    return;
  }

  // Internal reference temperature always works
  int16_t ref_val = (mem & 0x0000FFF0) >> 4;
  if (ref_val & 0x0800) {
    ref_val |= 0xF000;  // Pad out 2's complement
  }
  const float t_ref = float(ref_val) * 0.0625f;
  if (this->reference_sensor_) {
    ESP_LOGD(TAG, "Got reference temperature: %.4f°C", t_ref);
    this->reference_sensor_->publish_state(t_ref);
  }

  // Check thermocouple faults
  if (mem & 0x00000001) {
    ESP_LOGW(TAG, "Thermocouple open circuit (not connected) fault from MAX31855 (0x%08" PRIX32 ")", mem);
    this->publish_state(NAN);
    this->status_set_warning();
    return;
  }
  if (mem & 0x00000002) {
    ESP_LOGW(TAG, "Thermocouple short circuit to ground fault from MAX31855 (0x%08" PRIX32 ")", mem);
    this->publish_state(NAN);
    this->status_set_warning();
    return;
  }
  if (mem & 0x00000004) {
    ESP_LOGW(TAG, "Thermocouple short circuit to VCC fault from MAX31855 (0x%08" PRIX32 ")", mem);
    this->publish_state(NAN);
    this->status_set_warning();
    return;
  }
  if (mem & 0x00010000) {
    ESP_LOGW(TAG, "Got faulty reading from MAX31855 (0x%08" PRIX32 ")", mem);
    this->publish_state(NAN);
    this->status_set_warning();
    return;
  }

  // Decode thermocouple temperature
  int16_t val = (mem & 0xFFFC0000) >> 18;
  if (val & 0x2000) {
    val |= 0xC000;  // Pad out 2's complement
  }
  float t_sense = float(val) * 0.25f;
  if (this->linearize_) {
    const float t_chip = t_sense;
    t_sense = k_type_linearize(t_chip, t_ref);
    ESP_LOGD(TAG, "Got thermocouple temperature: %.2f°C (chip reported %.2f°C)", t_sense, t_chip);
  } else {
    ESP_LOGD(TAG, "Got thermocouple temperature: %.2f°C", t_sense);
  }
  this->publish_state(t_sense);
  this->status_clear_warning();
}

}  // namespace esphome::max31855
