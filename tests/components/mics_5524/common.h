#pragma once

#include <gtest/gtest.h>

#include <cmath>

#include "esphome/components/mics_5524/mics_math.h"

namespace esphome::mics_5524::testing {

using micsmath::air_value_from_voltage;
using micsmath::calibration_is_stable;
using micsmath::ppm_from_vendor_curve;
using micsmath::ratio_from_air_value;

// A fixture of the DFRobot table: an independent copy, so a change in `coefficients.py`
// cannot quietly change what these tests mean (the Python tests pin that table itself).
constexpr micsmath::VendorCurve CO{0.425f, 0.000405f, 1.0f, 1000.0f};
constexpr micsmath::VendorCurve H2{0.279f, 0.00026f, 1.0f, 1000.0f};
constexpr micsmath::VendorCurve NH3{0.8f, 0.0015f, 10.0f, 500.0f};
constexpr micsmath::VendorCurve C2H5OH{0.306f, 0.00057f, 10.0f, 500.0f};
constexpr micsmath::VendorCurve CH4{0.786f, 0.000023f, 1000.0f, 25000.0f};

}  // namespace esphome::mics_5524::testing
