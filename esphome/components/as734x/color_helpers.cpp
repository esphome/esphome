#include "color_helpers.h"
#include <cmath>

namespace esphome::as734x {

inline float clamp(float val, float min_val, float max_val) { return std::fmax(min_val, std::fmin(max_val, val)); }

// Gamma correction for sRGB
float gamma_correct(float channel) {
  if (channel <= 0.0031308) {
    return 12.92 * channel;
  } else {
    return 1.055 * std::pow(channel, 1.0 / 2.4) - 0.055;
  }
}

void tristimulus_to_hex(float x, float y, float z, uint8_t &ro, uint8_t &go, uint8_t &bo) {
  // Convert XYZ to linear RGB
  float r_lin = 3.2406 * x - 1.5372 * y - 0.4986 * z;
  float g_lin = -0.9689 * x + 1.8758 * y + 0.0415 * z;
  float b_lin = 0.0557 * x - 0.2040 * y + 1.0570 * z;

  // Apply gamma correction
  float r = gamma_correct(r_lin);
  float g = gamma_correct(g_lin);
  float b = gamma_correct(b_lin);

  // Clamp and convert to 0-255 range
  ro = static_cast<uint8_t>(clamp(r, 0.0, 1.0) * 255.0);
  go = static_cast<uint8_t>(clamp(g, 0.0, 1.0) * 255.0);
  bo = static_cast<uint8_t>(clamp(b, 0.0, 1.0) * 255.0);
}

void tristimulus_to_cct(float x, float y, float z, float &cct) {
  constexpr float epsilon = 0.001;
  float xyz_sum = x + y + z;
  float chroma_x{0}, chroma_y{0} /*, chroma_z{0}*/;  // chromaticity coordinates
  if (std::fabs(xyz_sum) < epsilon) {
    cct = 0;
  } else {
    chroma_x = x / xyz_sum;
    chroma_y = y / xyz_sum;
    // chroma_z = z / xyz_sum;

    float n = (chroma_x - 0.3320) / (0.1858 - chroma_y);

    // cct = 437 * pow(n, 3) + 3601 * pow(n, 2) + 6861 * n + 5517;
    cct = ((437.0f * n + 3601.0f) * n + 6861.0f) * n + 5517.0f;
  }
}

// void duv(){
//  float uprime;
// float vprime;

// uprime = 4 * X / (X + 15 * Y + 3 * Z);
// vprime = 9 * Y / (X + 15 * Y + 3 * Z);

// {
//   double u = (4 * x) / (-2 * x + 12 * y + 3);
//   double v = (6 * y) / (-2 * x + 12 * y + 3);

//   const double k6 = -0.00616793;
//   const double k5 = 0.0893944;
//   const double k4 = -0.5179722;
//   const double k3 = 1.5317403;
//   const double k2 = -2.4243787;
//   const double k1 = 1.925865;
//   const double k0 = -0.471106;

//   double Lfp = sqrt(pow((u - 0.292), 2) + pow((v - 0.24), 2));
//   double a = acos((u - 0.292) / Lfp);
//   double Lbb = k6 * pow(a, 6) + k5 * pow(a, 5) + k4 * pow(a, 4) + k3 * pow(a, 3) + k2 * pow(a, 2) + k1 * a + k0;

//   duv = static_cast<float>(Lfp - Lbb);
// }

//}

}  // namespace esphome::as734x
