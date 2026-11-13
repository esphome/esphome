#include "max31855_linearize.h"

#ifdef USE_MAX31855_LINEARIZE

#include "esphome/core/hal.h"

#include <cmath>
#include <cstddef>

namespace esphome::max31855 {

// NIST ITS-90 type K polynomials (NIST Monograph 175). E in mV, T in °C.
// E(T), -270 to 0 °C.
static const float K_E_NEG[] PROGMEM = {0.0f,
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
static const float K_E_POS[] PROGMEM = {
    -0.176004136860E-01f, 0.389212049750E-01f, 0.185587700320E-04f,  -0.994575928740E-07f, 0.318409457190E-09f,
    -0.560728448890E-12f, 0.560750590590E-15f, -0.320207200030E-18f, 0.971511471520E-22f,  -0.121047212750E-25f};
static constexpr float K_E_A0 = 0.118597600000E+00f;
static constexpr float K_E_A1 = -0.118343200000E-03f;
static constexpr float K_E_A2 = 0.126968600000E+03f;
// Inverse T(E): -5.891 to 0 mV, 0 to 20.644 mV, 20.644 to 54.886 mV.
static const float K_T_NEG[] PROGMEM = {0.0f,
                                        2.5173462E+01f,
                                        -1.1662878f,
                                        -1.0833638f,
                                        -8.9773540E-01f,
                                        -3.7342377E-01f,
                                        -8.6632643E-02f,
                                        -1.0450598E-02f,
                                        -5.1920577E-04f};
static const float K_T_LOW[] PROGMEM = {0.0f,           2.508355E+01f, 7.860106E-02f,  -2.503131E-01f, 8.315270E-02f,
                                        -1.228034E-02f, 9.804036E-04f, -4.413030E-05f, 1.057734E-06f,  -1.052755E-08f};
static const float K_T_HIGH[] PROGMEM = {-1.318058E+02f, 4.830222E+01f, -1.646031f,    5.464731E-02f,
                                         -9.650715E-04f, 8.802193E-06f, -3.110810E-08f};

template<size_t N> static float poly(const float (&c)[N], float x) {
  float r = 0.0f;
  for (size_t i = N; i-- > 0;) {
    float ci;
    progmem_memcpy(&ci, &c[i], sizeof(ci));  // the tables are in flash on ESP8266
    r = r * x + ci;
  }
  return r;
}

float k_type_mv(float t) {
  if (t < 0.0f)
    return poly(K_E_NEG, t);
  const float d = t - K_E_A2;
  return poly(K_E_POS, t) + K_E_A0 * expf(K_E_A1 * d * d);
}

float k_type_celsius(float mv) {
  if (!(mv >= K_TYPE_MV_MIN && mv <= K_TYPE_MV_MAX))
    return NAN;
  if (mv < 0.0f)
    return poly(K_T_NEG, mv);
  if (mv < 20.644f)
    return poly(K_T_LOW, mv);
  return poly(K_T_HIGH, mv);
}

float k_type_linearize(float t_chip, float t_cj) {
  const float v_tc_mv = 0.041276f * (t_chip - t_cj);
  return k_type_celsius(v_tc_mv + k_type_mv(t_cj));
}

}  // namespace esphome::max31855

#endif  // USE_MAX31855_LINEARIZE
