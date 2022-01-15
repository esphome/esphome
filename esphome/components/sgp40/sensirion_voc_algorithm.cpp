
#include "sensirion_voc_algorithm.h"

namespace esphome {
namespace sgp40 {

/* The VOC code were originally created by
 *  https://github.com/Sensirion/embedded-sgp
 * The fixed point arithmetic parts of this code were originally created by
 * https://github.com/PetteriAimonen/libfixmath
 */

/*!< the maximum value of fix16_t */
#define FIX16_MAXIMUM 0x7FFFFFFF
/*!< the minimum value of fix16_t */
static const uint32_t FIX16_MINIMUM = 0x80000000;
/*!< the value used to indicate overflows when FIXMATH_NO_OVERFLOW is not
 * specified */
static const uint32_t FIX16_OVERFLOW = 0x80000000;
/*!< fix16_t value of 1 */
const uint32_t FIX16_ONE = 0x00010000;

inline fix16_t fix16_from_int(int32_t a) { return a * FIX16_ONE; }

inline int32_t fix16_cast_to_int(fix16_t a) { return (a >> 16); }

/*! Multiplies the two given fix16_t's and returns the result. */
static fix16_t fix16_mul(fix16_t in_arg0, fix16_t in_arg1);

/*! Divides the first given fix16_t by the second and returns the result. */
static fix16_t fix16_div(fix16_t a, fix16_t b);

/*! Returns the square root of the given fix16_t. */
static fix16_t fix16_sqrt(fix16_t in_value);

/*! Returns the exponent (e^) of the given fix16_t. */
static fix16_t fix16_exp(fix16_t in_value);

static fix16_t fix16_mul(fix16_t in_arg0, fix16_t in_arg1) {
  // Each argument is divided to 16-bit parts.
  //                    AB
  //            *     CD
  // -----------
  //                    BD    16 * 16 -> 32 bit products
  //                 CB
  //                 AD
  //                AC
  //             |----| 64 bit product
  int32_t a = (in_arg0 >> 16), c = (in_arg1 >> 16);
  uint32_t b = (in_arg0 & 0xFFFF), d = (in_arg1 & 0xFFFF);

  int32_t ac = a * c;
  int32_t ad_cb = a * d + c * b;
  uint32_t bd = b * d;

  int32_t product_hi = ac + (ad_cb >> 16);  // NOLINT

  // Handle carry from lower 32 bits to upper part of result.
  uint32_t ad_cb_temp = ad_cb << 16;  // NOLINT
  uint32_t product_lo = bd + ad_cb_temp;
  if (product_lo < bd)
    product_hi++;

#ifndef FIXMATH_NO_OVERFLOW
  // The upper 17 bits should all be the same (the sign).
  if (product_hi >> 31 != product_hi >> 15)
    return FIX16_OVERFLOW;
#endif

#ifdef FIXMATH_NO_ROUNDING
  return (product_hi << 16) | (product_lo >> 16);
#else
  // Subtracting 0x8000 (= 0.5) and then using signed right shift
  // achieves proper rounding to result-1, except in the corner
  // case of negative numbers and lowest word = 0x8000.
  // To handle that, we also have to subtract 1 for negative numbers.
  uint32_t product_lo_tmp = product_lo;
  product_lo -= 0x8000;
  product_lo -= (uint32_t) product_hi >> 31;
  if (product_lo > product_lo_tmp)
    product_hi--;

  // Discard the lowest 16 bits. Note that this is not exactly the same
  // as dividing by 0x10000. For example if product = -1, result will
  // also be -1 and not 0. This is compensated by adding +1 to the result
  // and compensating this in turn in the rounding above.
  fix16_t result = (product_hi << 16) | (product_lo >> 16);  // NOLINT
  result += 1;
  return result;
#endif
}

static fix16_t fix16_div(fix16_t a, fix16_t b) {
  // This uses the basic binary restoring division algorithm.
  // It appears to be faster to do the whole division manually than
  // trying to compose a 64-bit divide out of 32-bit divisions on
  // platforms without hardware divide.

  if (b == 0)
    return FIX16_MINIMUM;

  uint32_t remainder = (a >= 0) ? a : (-a);
  uint32_t divider = (b >= 0) ? b : (-b);

  uint32_t quotient = 0;
  uint32_t bit = 0x10000;

  /* The algorithm requires D >= R */
  while (divider < remainder) {
    divider <<= 1;
    bit <<= 1;
  }

#ifndef FIXMATH_NO_OVERFLOW
  if (!bit)
    return FIX16_OVERFLOW;
#endif

  if (divider & 0x80000000) {
    // Perform one step manually to avoid overflows later.
    // We know that divider's bottom bit is 0 here.
    if (remainder >= divider) {
      quotient |= bit;
      remainder -= divider;
    }
    divider >>= 1;
    bit >>= 1;
  }

  /* Main division loop */
  while (bit && remainder) {
    if (remainder >= divider) {
      quotient |= bit;
      remainder -= divider;
    }

    remainder <<= 1;
    bit >>= 1;
  }

#ifndef FIXMATH_NO_ROUNDING
  if (remainder >= divider) {
    quotient++;
  }
#endif

  fix16_t result = quotient;

  /* Figure out the sign of result */
  if ((a ^ b) & 0x80000000) {
#ifndef FIXMATH_NO_OVERFLOW
    if (result == FIX16_MINIMUM)  // NOLINT(clang-diagnostic-sign-compare)
      return FIX16_OVERFLOW;
#endif

    result = -result;
  }

  return result;
}

static fix16_t fix16_sqrt(fix16_t in_value) {
  // It is assumed that x is not negative

  uint32_t num = in_value;
  uint32_t result = 0;
  uint32_t bit;
  uint8_t n;

  bit = (uint32_t) 1 << 30;
  while (bit > num)
    bit >>= 2;

  // The main part is executed twice, in order to avoid
  // using 64 bit values in computations.
  for (n = 0; n < 2; n++) {
    // First we get the top 24 bits of the answer.
    while (bit) {
      if (num >= result + bit) {
        num -= result + bit;
        result = (result >> 1) + bit;
      } else {
        result = (result >> 1);
      }
      bit >>= 2;
    }

    if (n == 0) {
      // Then process it again to get the lowest 8 bits.
      if (num > 65535) {
        // The remainder 'num' is too large to be shifted left
        // by 16, so we have to add 1 to result manually and
        // adjust 'num' accordingly.
        // num = a - (result + 0.5)^2
        //     = num + result^2 - (result + 0.5)^2
        //     = num - result - 0.5
        num -= result;
        num = (num << 16) - 0x8000;
        result = (result << 16) + 0x8000;
      } else {
        num <<= 16;
        result <<= 16;
      }

      bit = 1 << 14;
    }
  }

#ifndef FIXMATH_NO_ROUNDING
  // Finally, if next bit would have been 1, round the result upwards.
  if (num > result) {
    result++;
  }
#endif

  return (fix16_t) result;
}

static fix16_t fix16_exp(fix16_t in_value) {
  // Function to approximate exp(); optimized more for code size than speed

  // exp(x) for x = +/- {1, 1/8, 1/64, 1/512}
  fix16_t x = in_value;
  static const uint8_t NUM_EXP_VALUES = 4;
  static const fix16_t EXP_POS_VALUES[4] = {F16(2.7182818), F16(1.1331485), F16(1.0157477), F16(1.0019550)};
  static const fix16_t EXP_NEG_VALUES[4] = {F16(0.3678794), F16(0.8824969), F16(0.9844964), F16(0.9980488)};
  const fix16_t *exp_values;

  fix16_t res, arg;
  uint16_t i;

  if (x >= F16(10.3972))
    return FIX16_MAXIMUM;
  if (x <= F16(-11.7835))
    return 0;

  if (x < 0) {
    x = -x;
    exp_values = EXP_NEG_VALUES;
  } else {
    exp_values = EXP_POS_VALUES;
  }

  res = FIX16_ONE;
  arg = FIX16_ONE;
  for (i = 0; i < NUM_EXP_VALUES; i++) {
    while (x >= arg) {
      res = fix16_mul(res, exp_values[i]);
      x -= arg;
    }
    arg >>= 3;
  }
  return res;
}

static void voc_algorithm_init_instances(VocAlgorithmParams *params);
static void voc_algorithm_mean_variance_estimator_init(VocAlgorithmParams *params);
static void voc_algorithm_mean_variance_estimator_init_instances(VocAlgorithmParams *params);
static void voc_algorithm_mean_variance_estimator_set_parameters(VocAlgorithmParams *params, fix16_t std_initial,
                                                                 fix16_t tau_mean_variance_hours,
                                                                 fix16_t gating_max_duration_minutes);
static void voc_algorithm_mean_variance_estimator_set_states(VocAlgorithmParams *params, fix16_t mean, fix16_t std,
                                                             fix16_t uptime_gamma);
static fix16_t voc_algorithm_mean_variance_estimator_get_std(VocAlgorithmParams *params);
static fix16_t voc_algorithm_mean_variance_estimator_get_mean(VocAlgorithmParams *params);
static void voc_algorithm_mean_variance_estimator_calculate_gamma(VocAlgorithmParams *params,
                                                                  fix16_t voc_index_from_prior);
static void voc_algorithm_mean_variance_estimator_process(VocAlgorithmParams *params, fix16_t sraw,
                                                          fix16_t voc_index_from_prior);
static void voc_algorithm_mean_variance_estimator_sigmoid_init(VocAlgorithmParams *params);
static void voc_algorithm_mean_variance_estimator_sigmoid_set_parameters(VocAlgorithmParams *params, fix16_t l,
                                                                         fix16_t x0, fix16_t k);
static fix16_t voc_algorithm_mean_variance_estimator_sigmoid_process(VocAlgorithmParams *params, fix16_t sample);
static void voc_algorithm_mox_model_init(VocAlgorithmParams *params);
static void voc_algorithm_mox_model_set_parameters(VocAlgorithmParams *params, fix16_t sraw_std, fix16_t sraw_mean);
static fix16_t voc_algorithm_mox_model_process(VocAlgorithmParams *params, fix16_t sraw);
static void voc_algorithm_sigmoid_scaled_init(VocAlgorithmParams *params);
static void voc_algorithm_sigmoid_scaled_set_parameters(VocAlgorithmParams *params, fix16_t offset);
static fix16_t voc_algorithm_sigmoid_scaled_process(VocAlgorithmParams *params, fix16_t sample);
static void voc_algorithm_adaptive_lowpass_init(VocAlgorithmParams *params);
static void voc_algorithm_adaptive_lowpass_set_parameters(VocAlgorithmParams *params);
static fix16_t voc_algorithm_adaptive_lowpass_process(VocAlgorithmParams *params, fix16_t sample);

void voc_algorithm_init(VocAlgorithmParams *params) {
  params->mVoc_Index_Offset = F16(VOC_ALGORITHM_VOC_INDEX_OFFSET_DEFAULT);
  params->mTau_Mean_Variance_Hours = F16(VOC_ALGORITHM_TAU_MEAN_VARIANCE_HOURS);
  params->mGating_Max_Duration_Minutes = F16(VOC_ALGORITHM_GATING_MAX_DURATION_MINUTES);
  params->mSraw_Std_Initial = F16(VOC_ALGORITHM_SRAW_STD_INITIAL);
  params->mUptime = F16(0.);
  params->mSraw = F16(0.);
  params->mVoc_Index = 0;
  voc_algorithm_init_instances(params);
}

static void voc_algorithm_init_instances(VocAlgorithmParams *params) {
  voc_algorithm_mean_variance_estimator_init(params);
  voc_algorithm_mean_variance_estimator_set_parameters(
      params, params->mSraw_Std_Initial, params->mTau_Mean_Variance_Hours, params->mGating_Max_Duration_Minutes);
  voc_algorithm_mox_model_init(params);
  voc_algorithm_mox_model_set_parameters(params, voc_algorithm_mean_variance_estimator_get_std(params),
                                         voc_algorithm_mean_variance_estimator_get_mean(params));
  voc_algorithm_sigmoid_scaled_init(params);
  voc_algorithm_sigmoid_scaled_set_parameters(params, params->mVoc_Index_Offset);
  voc_algorithm_adaptive_lowpass_init(params);
  voc_algorithm_adaptive_lowpass_set_parameters(params);
}

void voc_algorithm_get_states(VocAlgorithmParams *params, int32_t *state0, int32_t *state1) {
  *state0 = voc_algorithm_mean_variance_estimator_get_mean(params);
  *state1 = voc_algorithm_mean_variance_estimator_get_std(params);
}

void voc_algorithm_set_states(VocAlgorithmParams *params, int32_t state0, int32_t state1) {
  voc_algorithm_mean_variance_estimator_set_states(params, state0, state1, F16(VOC_ALGORITHM_PERSISTENCE_UPTIME_GAMMA));
  params->mSraw = state0;
}

void voc_algorithm_set_tuning_parameters(VocAlgorithmParams *params, int32_t voc_index_offset,
                                         int32_t learning_time_hours, int32_t gating_max_duration_minutes,
                                         int32_t std_initial) {
  params->mVoc_Index_Offset = (fix16_from_int(voc_index_offset));
  params->mTau_Mean_Variance_Hours = (fix16_from_int(learning_time_hours));
  params->mGating_Max_Duration_Minutes = (fix16_from_int(gating_max_duration_minutes));
  params->mSraw_Std_Initial = (fix16_from_int(std_initial));
  voc_algorithm_init_instances(params);
}

void voc_algorithm_process(VocAlgorithmParams *params, int32_t sraw, int32_t *voc_index) {
  if ((params->mUptime <= F16(VOC_ALGORITHM_INITIAL_BLACKOUT))) {
    params->mUptime = (params->mUptime + F16(VOC_ALGORITHM_SAMPLING_INTERVAL));
  } else {
    if (((sraw > 0) && (sraw < 65000))) {
      if ((sraw < 20001)) {
        sraw = 20001;
      } else if ((sraw > 52767)) {
        sraw = 52767;
      }
      params->mSraw = (fix16_from_int((sraw - 20000)));
    }
    params->mVoc_Index = voc_algorithm_mox_model_process(params, params->mSraw);
    params->mVoc_Index = voc_algorithm_sigmoid_scaled_process(params, params->mVoc_Index);
    params->mVoc_Index = voc_algorithm_adaptive_lowpass_process(params, params->mVoc_Index);
    if ((params->mVoc_Index < F16(0.5))) {
      params->mVoc_Index = F16(0.5);
    }
    if ((params->mSraw > F16(0.))) {
      voc_algorithm_mean_variance_estimator_process(params, params->mSraw, params->mVoc_Index);
      voc_algorithm_mox_model_set_parameters(params, voc_algorithm_mean_variance_estimator_get_std(params),
                                             voc_algorithm_mean_variance_estimator_get_mean(params));
    }
  }
  *voc_index = (fix16_cast_to_int((params->mVoc_Index + F16(0.5))));
}

static void voc_algorithm_mean_variance_estimator_init(VocAlgorithmParams *params) {
  voc_algorithm_mean_variance_estimator_set_parameters(params, F16(0.), F16(0.), F16(0.));
  voc_algorithm_mean_variance_estimator_init_instances(params);
}

static void voc_algorithm_mean_variance_estimator_init_instances(VocAlgorithmParams *params) {
  voc_algorithm_mean_variance_estimator_sigmoid_init(params);
}

static void voc_algorithm_mean_variance_estimator_set_parameters(VocAlgorithmParams *params, fix16_t std_initial,
                                                                 fix16_t tau_mean_variance_hours,
                                                                 fix16_t gating_max_duration_minutes) {
  params->m_Mean_Variance_Estimator_Gating_Max_Duration_Minutes = gating_max_duration_minutes;
  params->m_Mean_Variance_Estimator_Initialized = false;
  params->m_Mean_Variance_Estimator_Mean = F16(0.);
  params->m_Mean_Variance_Estimator_Sraw_Offset = F16(0.);
  params->m_Mean_Variance_Estimator_Std = std_initial;
  params->m_Mean_Variance_Estimator_Gamma =
      (fix16_div(F16((VOC_ALGORITHM_MEAN_VARIANCE_ESTIMATOR_GAMMA_SCALING * (VOC_ALGORITHM_SAMPLING_INTERVAL / 3600.))),
                 (tau_mean_variance_hours + F16((VOC_ALGORITHM_SAMPLING_INTERVAL / 3600.)))));
  params->m_Mean_Variance_Estimator_Gamma_Initial_Mean =
      F16(((VOC_ALGORITHM_MEAN_VARIANCE_ESTIMATOR_GAMMA_SCALING * VOC_ALGORITHM_SAMPLING_INTERVAL) /
           (VOC_ALGORITHM_TAU_INITIAL_MEAN + VOC_ALGORITHM_SAMPLING_INTERVAL)));
  params->m_Mean_Variance_Estimator_Gamma_Initial_Variance =
      F16(((VOC_ALGORITHM_MEAN_VARIANCE_ESTIMATOR_GAMMA_SCALING * VOC_ALGORITHM_SAMPLING_INTERVAL) /
           (VOC_ALGORITHM_TAU_INITIAL_VARIANCE + VOC_ALGORITHM_SAMPLING_INTERVAL)));
  params->m_Mean_Variance_Estimator_Gamma_Mean = F16(0.);
  params->m_Mean_Variance_Estimator_Gamma_Variance = F16(0.);
  params->m_Mean_Variance_Estimator_Uptime_Gamma = F16(0.);
  params->m_Mean_Variance_Estimator_Uptime_Gating = F16(0.);
  params->m_Mean_Variance_Estimator_Gating_Duration_Minutes = F16(0.);
}

static void voc_algorithm_mean_variance_estimator_set_states(VocAlgorithmParams *params, fix16_t mean, fix16_t std,
                                                             fix16_t uptime_gamma) {
  params->m_Mean_Variance_Estimator_Mean = mean;
  params->m_Mean_Variance_Estimator_Std = std;
  params->m_Mean_Variance_Estimator_Uptime_Gamma = uptime_gamma;
  params->m_Mean_Variance_Estimator_Initialized = true;
}

static fix16_t voc_algorithm_mean_variance_estimator_get_std(VocAlgorithmParams *params) {
  return params->m_Mean_Variance_Estimator_Std;
}

static fix16_t voc_algorithm_mean_variance_estimator_get_mean(VocAlgorithmParams *params) {
  return (params->m_Mean_Variance_Estimator_Mean + params->m_Mean_Variance_Estimator_Sraw_Offset);
}

static void voc_algorithm_mean_variance_estimator_calculate_gamma(VocAlgorithmParams *params,
                                                                  fix16_t voc_index_from_prior) {
  fix16_t uptime_limit;
  fix16_t sigmoid_gamma_mean;
  fix16_t gamma_mean;
  fix16_t gating_threshold_mean;
  fix16_t sigmoid_gating_mean;
  fix16_t sigmoid_gamma_variance;
  fix16_t gamma_variance;
  fix16_t gating_threshold_variance;
  fix16_t sigmoid_gating_variance;

  uptime_limit = F16((VOC_ALGORITHM_MEAN_VARIANCE_ESTIMATOR_FI_X16_MAX - VOC_ALGORITHM_SAMPLING_INTERVAL));
  if ((params->m_Mean_Variance_Estimator_Uptime_Gamma < uptime_limit)) {
    params->m_Mean_Variance_Estimator_Uptime_Gamma =
        (params->m_Mean_Variance_Estimator_Uptime_Gamma + F16(VOC_ALGORITHM_SAMPLING_INTERVAL));
  }
  if ((params->m_Mean_Variance_Estimator_Uptime_Gating < uptime_limit)) {
    params->m_Mean_Variance_Estimator_Uptime_Gating =
        (params->m_Mean_Variance_Estimator_Uptime_Gating + F16(VOC_ALGORITHM_SAMPLING_INTERVAL));
  }
  voc_algorithm_mean_variance_estimator_sigmoid_set_parameters(params, F16(1.), F16(VOC_ALGORITHM_INIT_DURATION_MEAN),
                                                               F16(VOC_ALGORITHM_INIT_TRANSITION_MEAN));
  sigmoid_gamma_mean =
      voc_algorithm_mean_variance_estimator_sigmoid_process(params, params->m_Mean_Variance_Estimator_Uptime_Gamma);
  gamma_mean =
      (params->m_Mean_Variance_Estimator_Gamma +
       (fix16_mul((params->m_Mean_Variance_Estimator_Gamma_Initial_Mean - params->m_Mean_Variance_Estimator_Gamma),
                  sigmoid_gamma_mean)));
  gating_threshold_mean = (F16(VOC_ALGORITHM_GATING_THRESHOLD) +
                           (fix16_mul(F16((VOC_ALGORITHM_GATING_THRESHOLD_INITIAL - VOC_ALGORITHM_GATING_THRESHOLD)),
                                      voc_algorithm_mean_variance_estimator_sigmoid_process(
                                          params, params->m_Mean_Variance_Estimator_Uptime_Gating))));
  voc_algorithm_mean_variance_estimator_sigmoid_set_parameters(params, F16(1.), gating_threshold_mean,
                                                               F16(VOC_ALGORITHM_GATING_THRESHOLD_TRANSITION));
  sigmoid_gating_mean = voc_algorithm_mean_variance_estimator_sigmoid_process(params, voc_index_from_prior);
  params->m_Mean_Variance_Estimator_Gamma_Mean = (fix16_mul(sigmoid_gating_mean, gamma_mean));
  voc_algorithm_mean_variance_estimator_sigmoid_set_parameters(
      params, F16(1.), F16(VOC_ALGORITHM_INIT_DURATION_VARIANCE), F16(VOC_ALGORITHM_INIT_TRANSITION_VARIANCE));
  sigmoid_gamma_variance =
      voc_algorithm_mean_variance_estimator_sigmoid_process(params, params->m_Mean_Variance_Estimator_Uptime_Gamma);
  gamma_variance =
      (params->m_Mean_Variance_Estimator_Gamma +
       (fix16_mul((params->m_Mean_Variance_Estimator_Gamma_Initial_Variance - params->m_Mean_Variance_Estimator_Gamma),
                  (sigmoid_gamma_variance - sigmoid_gamma_mean))));
  gating_threshold_variance =
      (F16(VOC_ALGORITHM_GATING_THRESHOLD) +
       (fix16_mul(F16((VOC_ALGORITHM_GATING_THRESHOLD_INITIAL - VOC_ALGORITHM_GATING_THRESHOLD)),
                  voc_algorithm_mean_variance_estimator_sigmoid_process(
                      params, params->m_Mean_Variance_Estimator_Uptime_Gating))));
  voc_algorithm_mean_variance_estimator_sigmoid_set_parameters(params, F16(1.), gating_threshold_variance,
                                                               F16(VOC_ALGORITHM_GATING_THRESHOLD_TRANSITION));
  sigmoid_gating_variance = voc_algorithm_mean_variance_estimator_sigmoid_process(params, voc_index_from_prior);
  params->m_Mean_Variance_Estimator_Gamma_Variance = (fix16_mul(sigmoid_gating_variance, gamma_variance));
  params->m_Mean_Variance_Estimator_Gating_Duration_Minutes =
      (params->m_Mean_Variance_Estimator_Gating_Duration_Minutes +
       (fix16_mul(F16((VOC_ALGORITHM_SAMPLING_INTERVAL / 60.)),
                  ((fix16_mul((F16(1.) - sigmoid_gating_mean), F16((1. + VOC_ALGORITHM_GATING_MAX_RATIO)))) -
                   F16(VOC_ALGORITHM_GATING_MAX_RATIO)))));
  if ((params->m_Mean_Variance_Estimator_Gating_Duration_Minutes < F16(0.))) {
    params->m_Mean_Variance_Estimator_Gating_Duration_Minutes = F16(0.);
  }
  if ((params->m_Mean_Variance_Estimator_Gating_Duration_Minutes >
       params->m_Mean_Variance_Estimator_Gating_Max_Duration_Minutes)) {
    params->m_Mean_Variance_Estimator_Uptime_Gating = F16(0.);
  }
}

static void voc_algorithm_mean_variance_estimator_process(VocAlgorithmParams *params, fix16_t sraw,
                                                          fix16_t voc_index_from_prior) {
  fix16_t delta_sgp;
  fix16_t c;
  fix16_t additional_scaling;

  if ((!params->m_Mean_Variance_Estimator_Initialized)) {
    params->m_Mean_Variance_Estimator_Initialized = true;
    params->m_Mean_Variance_Estimator_Sraw_Offset = sraw;
    params->m_Mean_Variance_Estimator_Mean = F16(0.);
  } else {
    if (((params->m_Mean_Variance_Estimator_Mean >= F16(100.)) ||
         (params->m_Mean_Variance_Estimator_Mean <= F16(-100.)))) {
      params->m_Mean_Variance_Estimator_Sraw_Offset =
          (params->m_Mean_Variance_Estimator_Sraw_Offset + params->m_Mean_Variance_Estimator_Mean);
      params->m_Mean_Variance_Estimator_Mean = F16(0.);
    }
    sraw = (sraw - params->m_Mean_Variance_Estimator_Sraw_Offset);
    voc_algorithm_mean_variance_estimator_calculate_gamma(params, voc_index_from_prior);
    delta_sgp = (fix16_div((sraw - params->m_Mean_Variance_Estimator_Mean),
                           F16(VOC_ALGORITHM_MEAN_VARIANCE_ESTIMATOR_GAMMA_SCALING)));
    if ((delta_sgp < F16(0.))) {
      c = (params->m_Mean_Variance_Estimator_Std - delta_sgp);
    } else {
      c = (params->m_Mean_Variance_Estimator_Std + delta_sgp);
    }
    additional_scaling = F16(1.);
    if ((c > F16(1440.))) {
      additional_scaling = F16(4.);
    }
    params->m_Mean_Variance_Estimator_Std = (fix16_mul(
        fix16_sqrt((fix16_mul(additional_scaling, (F16(VOC_ALGORITHM_MEAN_VARIANCE_ESTIMATOR_GAMMA_SCALING) -
                                                   params->m_Mean_Variance_Estimator_Gamma_Variance)))),
        fix16_sqrt(((fix16_mul(params->m_Mean_Variance_Estimator_Std,
                               (fix16_div(params->m_Mean_Variance_Estimator_Std,
                                          (fix16_mul(F16(VOC_ALGORITHM_MEAN_VARIANCE_ESTIMATOR_GAMMA_SCALING),
                                                     additional_scaling)))))) +
                    (fix16_mul((fix16_div((fix16_mul(params->m_Mean_Variance_Estimator_Gamma_Variance, delta_sgp)),
                                          additional_scaling)),
                               delta_sgp))))));
    params->m_Mean_Variance_Estimator_Mean =
        (params->m_Mean_Variance_Estimator_Mean + (fix16_mul(params->m_Mean_Variance_Estimator_Gamma_Mean, delta_sgp)));
  }
}

static void voc_algorithm_mean_variance_estimator_sigmoid_init(VocAlgorithmParams *params) {
  voc_algorithm_mean_variance_estimator_sigmoid_set_parameters(params, F16(0.), F16(0.), F16(0.));
}

static void voc_algorithm_mean_variance_estimator_sigmoid_set_parameters(VocAlgorithmParams *params, fix16_t l,
                                                                         fix16_t x0, fix16_t k) {
  params->m_Mean_Variance_Estimator_Sigmoid_L = l;
  params->m_Mean_Variance_Estimator_Sigmoid_K = k;
  params->m_Mean_Variance_Estimator_Sigmoid_X0 = x0;
}

static fix16_t voc_algorithm_mean_variance_estimator_sigmoid_process(VocAlgorithmParams *params, fix16_t sample) {
  fix16_t x;

  x = (fix16_mul(params->m_Mean_Variance_Estimator_Sigmoid_K, (sample - params->m_Mean_Variance_Estimator_Sigmoid_X0)));
  if ((x < F16(-50.))) {
    return params->m_Mean_Variance_Estimator_Sigmoid_L;
  } else if ((x > F16(50.))) {
    return F16(0.);
  } else {
    return (fix16_div(params->m_Mean_Variance_Estimator_Sigmoid_L, (F16(1.) + fix16_exp(x))));
  }
}

static void voc_algorithm_mox_model_init(VocAlgorithmParams *params) {
  voc_algorithm_mox_model_set_parameters(params, F16(1.), F16(0.));
}

static void voc_algorithm_mox_model_set_parameters(VocAlgorithmParams *params, fix16_t sraw_std, fix16_t sraw_mean) {
  params->m_Mox_Model_Sraw_Std = sraw_std;
  params->m_Mox_Model_Sraw_Mean = sraw_mean;
}

static fix16_t voc_algorithm_mox_model_process(VocAlgorithmParams *params, fix16_t sraw) {
  return (fix16_mul((fix16_div((sraw - params->m_Mox_Model_Sraw_Mean),
                               (-(params->m_Mox_Model_Sraw_Std + F16(VOC_ALGORITHM_SRAW_STD_BONUS))))),
                    F16(VOC_ALGORITHM_VOC_INDEX_GAIN)));
}

static void voc_algorithm_sigmoid_scaled_init(VocAlgorithmParams *params) {
  voc_algorithm_sigmoid_scaled_set_parameters(params, F16(0.));
}

static void voc_algorithm_sigmoid_scaled_set_parameters(VocAlgorithmParams *params, fix16_t offset) {
  params->m_Sigmoid_Scaled_Offset = offset;
}

static fix16_t voc_algorithm_sigmoid_scaled_process(VocAlgorithmParams *params, fix16_t sample) {
  fix16_t x;
  fix16_t shift;

  x = (fix16_mul(F16(VOC_ALGORITHM_SIGMOID_K), (sample - F16(VOC_ALGORITHM_SIGMOID_X0))));
  if ((x < F16(-50.))) {
    return F16(VOC_ALGORITHM_SIGMOID_L);
  } else if ((x > F16(50.))) {
    return F16(0.);
  } else {
    if ((sample >= F16(0.))) {
      shift =
          (fix16_div((F16(VOC_ALGORITHM_SIGMOID_L) - (fix16_mul(F16(5.), params->m_Sigmoid_Scaled_Offset))), F16(4.)));
      return ((fix16_div((F16(VOC_ALGORITHM_SIGMOID_L) + shift), (F16(1.) + fix16_exp(x)))) - shift);
    } else {
      return (fix16_mul((fix16_div(params->m_Sigmoid_Scaled_Offset, F16(VOC_ALGORITHM_VOC_INDEX_OFFSET_DEFAULT))),
                        (fix16_div(F16(VOC_ALGORITHM_SIGMOID_L), (F16(1.) + fix16_exp(x))))));
    }
  }
}

static void voc_algorithm_adaptive_lowpass_init(VocAlgorithmParams *params) {
  voc_algorithm_adaptive_lowpass_set_parameters(params);
}

static void voc_algorithm_adaptive_lowpass_set_parameters(VocAlgorithmParams *params) {
  params->m_Adaptive_Lowpass_A1 =
      F16((VOC_ALGORITHM_SAMPLING_INTERVAL / (VOC_ALGORITHM_LP_TAU_FAST + VOC_ALGORITHM_SAMPLING_INTERVAL)));
  params->m_Adaptive_Lowpass_A2 =
      F16((VOC_ALGORITHM_SAMPLING_INTERVAL / (VOC_ALGORITHM_LP_TAU_SLOW + VOC_ALGORITHM_SAMPLING_INTERVAL)));
  params->m_Adaptive_Lowpass_Initialized = false;
}

static fix16_t voc_algorithm_adaptive_lowpass_process(VocAlgorithmParams *params, fix16_t sample) {
  fix16_t abs_delta;
  fix16_t f1;
  fix16_t tau_a;
  fix16_t a3;

  if ((!params->m_Adaptive_Lowpass_Initialized)) {
    params->m_Adaptive_Lowpass_X1 = sample;
    params->m_Adaptive_Lowpass_X2 = sample;
    params->m_Adaptive_Lowpass_X3 = sample;
    params->m_Adaptive_Lowpass_Initialized = true;
  }
  params->m_Adaptive_Lowpass_X1 =
      ((fix16_mul((F16(1.) - params->m_Adaptive_Lowpass_A1), params->m_Adaptive_Lowpass_X1)) +
       (fix16_mul(params->m_Adaptive_Lowpass_A1, sample)));
  params->m_Adaptive_Lowpass_X2 =
      ((fix16_mul((F16(1.) - params->m_Adaptive_Lowpass_A2), params->m_Adaptive_Lowpass_X2)) +
       (fix16_mul(params->m_Adaptive_Lowpass_A2, sample)));
  abs_delta = (params->m_Adaptive_Lowpass_X1 - params->m_Adaptive_Lowpass_X2);
  if ((abs_delta < F16(0.))) {
    abs_delta = (-abs_delta);
  }
  f1 = fix16_exp((fix16_mul(F16(VOC_ALGORITHM_LP_ALPHA), abs_delta)));
  tau_a =
      ((fix16_mul(F16((VOC_ALGORITHM_LP_TAU_SLOW - VOC_ALGORITHM_LP_TAU_FAST)), f1)) + F16(VOC_ALGORITHM_LP_TAU_FAST));
  a3 = (fix16_div(F16(VOC_ALGORITHM_SAMPLING_INTERVAL), (F16(VOC_ALGORITHM_SAMPLING_INTERVAL) + tau_a)));
  params->m_Adaptive_Lowpass_X3 =
      ((fix16_mul((F16(1.) - a3), params->m_Adaptive_Lowpass_X3)) + (fix16_mul(a3, sample)));
  return params->m_Adaptive_Lowpass_X3;
}
}  // namespace sgp40
}  // namespace esphome
