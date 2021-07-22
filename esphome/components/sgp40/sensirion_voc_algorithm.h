#pragma once
#include <cstdint>
namespace esphome {
namespace sgp40 {

/* The VOC code were originally created by
 *  https://github.com/Sensirion/embedded-sgp
 * The fixed point arithmetic parts of this code were originally created by
 * https://github.com/PetteriAimonen/libfixmath
 */

using fix16_t = int32_t;

#define F16(x) ((fix16_t)(((x) >= 0) ? ((x) *65536.0 + 0.5) : ((x) *65536.0 - 0.5)))

static const float VOC_ALGORITHM_SAMPLING_INTERVAL(1.);
static const float VOC_ALGORITHM_INITIAL_BLACKOUT(45.);
static const float VOC_ALGORITHM_VOC_INDEX_GAIN(230.);
static const float VOC_ALGORITHM_SRAW_STD_INITIAL(50.);
static const float VOC_ALGORITHM_SRAW_STD_BONUS(220.);
static const float VOC_ALGORITHM_TAU_MEAN_VARIANCE_HOURS(12.);
static const float VOC_ALGORITHM_TAU_INITIAL_MEAN(20.);
static const float VOC_ALGORITHM_INIT_DURATION_MEAN((3600. * 0.75));
static const float VOC_ALGORITHM_INIT_TRANSITION_MEAN(0.01);
static const float VOC_ALGORITHM_TAU_INITIAL_VARIANCE(2500.);
static const float VOC_ALGORITHM_INIT_DURATION_VARIANCE((3600. * 1.45));
static const float VOC_ALGORITHM_INIT_TRANSITION_VARIANCE(0.01);
static const float VOC_ALGORITHM_GATING_THRESHOLD(340.);
static const float VOC_ALGORITHM_GATING_THRESHOLD_INITIAL(510.);
static const float VOC_ALGORITHM_GATING_THRESHOLD_TRANSITION(0.09);
static const float VOC_ALGORITHM_GATING_MAX_DURATION_MINUTES((60. * 3.));
static const float VOC_ALGORITHM_GATING_MAX_RATIO(0.3);
static const float VOC_ALGORITHM_SIGMOID_L(500.);
static const float VOC_ALGORITHM_SIGMOID_K(-0.0065);
static const float VOC_ALGORITHM_SIGMOID_X0(213.);
static const float VOC_ALGORITHM_VOC_INDEX_OFFSET_DEFAULT(100.);
static const float VOC_ALGORITHM_LP_TAU_FAST(20.0);
static const float VOC_ALGORITHM_LP_TAU_SLOW(500.0);
static const float VOC_ALGORITHM_LP_ALPHA(-0.2);
static const float VOC_ALGORITHM_PERSISTENCE_UPTIME_GAMMA((3. * 3600.));
static const float VOC_ALGORITHM_MEAN_VARIANCE_ESTIMATOR_GAMMA_SCALING(64.);
static const float VOC_ALGORITHM_MEAN_VARIANCE_ESTIMATOR_FI_X16_MAX(32767.);

/**
 * Struct to hold all the states of the VOC algorithm.
 */
struct VocAlgorithmParams {
  fix16_t mVoc_Index_Offset;
  fix16_t mTau_Mean_Variance_Hours;
  fix16_t mGating_Max_Duration_Minutes;
  fix16_t mSraw_Std_Initial;
  fix16_t mUptime;
  fix16_t mSraw;
  fix16_t mVoc_Index;
  fix16_t m_Mean_Variance_Estimator_Gating_Max_Duration_Minutes;
  bool m_Mean_Variance_Estimator_Initialized;
  fix16_t m_Mean_Variance_Estimator_Mean;
  fix16_t m_Mean_Variance_Estimator_Sraw_Offset;
  fix16_t m_Mean_Variance_Estimator_Std;
  fix16_t m_Mean_Variance_Estimator_Gamma;
  fix16_t m_Mean_Variance_Estimator_Gamma_Initial_Mean;
  fix16_t m_Mean_Variance_Estimator_Gamma_Initial_Variance;
  fix16_t m_Mean_Variance_Estimator_Gamma_Mean;
  fix16_t m_Mean_Variance_Estimator_Gamma_Variance;
  fix16_t m_Mean_Variance_Estimator_Uptime_Gamma;
  fix16_t m_Mean_Variance_Estimator_Uptime_Gating;
  fix16_t m_Mean_Variance_Estimator_Gating_Duration_Minutes;
  fix16_t m_Mean_Variance_Estimator_Sigmoid_L;
  fix16_t m_Mean_Variance_Estimator_Sigmoid_K;
  fix16_t m_Mean_Variance_Estimator_Sigmoid_X0;
  fix16_t m_Mox_Model_Sraw_Std;
  fix16_t m_Mox_Model_Sraw_Mean;
  fix16_t m_Sigmoid_Scaled_Offset;
  fix16_t m_Adaptive_Lowpass_A1;
  fix16_t m_Adaptive_Lowpass_A2;
  bool m_Adaptive_Lowpass_Initialized;
  fix16_t m_Adaptive_Lowpass_X1;
  fix16_t m_Adaptive_Lowpass_X2;
  fix16_t m_Adaptive_Lowpass_X3;
};

/**
 * Initialize the VOC algorithm parameters. Call this once at the beginning or
 * whenever the sensor stopped measurements.
 * @param params    Pointer to the VocAlgorithmParams struct
 */
void voc_algorithm_init(VocAlgorithmParams *params);

/**
 * Get current algorithm states. Retrieved values can be used in
 * voc_algorithm_set_states() to resume operation after a short interruption,
 * skipping initial learning phase. This feature can only be used after at least
 * 3 hours of continuous operation.
 * @param params    Pointer to the VocAlgorithmParams struct
 * @param state0    State0 to be stored
 * @param state1    State1 to be stored
 */
void voc_algorithm_get_states(VocAlgorithmParams *params, int32_t *state0, int32_t *state1);

/**
 * Set previously retrieved algorithm states to resume operation after a short
 * interruption, skipping initial learning phase. This feature should not be
 * used after inerruptions of more than 10 minutes. Call this once after
 * voc_algorithm_init() and the optional voc_algorithm_set_tuning_parameters(), if
 * desired. Otherwise, the algorithm will start with initial learning phase.
 * @param params    Pointer to the VocAlgorithmParams struct
 * @param state0    State0 to be restored
 * @param state1    State1 to be restored
 */
void voc_algorithm_set_states(VocAlgorithmParams *params, int32_t state0, int32_t state1);

/**
 * Set parameters to customize the VOC algorithm. Call this once after
 * voc_algorithm_init(), if desired. Otherwise, the default values will be used.
 *
 * @param params                      Pointer to the VocAlgorithmParams struct
 * @param voc_index_offset            VOC index representing typical (average)
 *                                    conditions. Range 1..250, default 100
 * @param learning_time_hours         Time constant of long-term estimator.
 *                                    Past events will be forgotten after about
 *                                    twice the learning time.
 *                                    Range 1..72 [hours], default 12 [hours]
 * @param gating_max_duration_minutes Maximum duration of gating (freeze of
 *                                    estimator during high VOC index signal).
 *                                    0 (no gating) or range 1..720 [minutes],
 *                                    default 180 [minutes]
 * @param std_initial                 Initial estimate for standard deviation.
 *                                    Lower value boosts events during initial
 *                                    learning period, but may result in larger
 *                                    device-to-device variations.
 *                                    Range 10..500, default 50
 */
void voc_algorithm_set_tuning_parameters(VocAlgorithmParams *params, int32_t voc_index_offset,
                                         int32_t learning_time_hours, int32_t gating_max_duration_minutes,
                                         int32_t std_initial);

/**
 * Calculate the VOC index value from the raw sensor value.
 *
 * @param params    Pointer to the VocAlgorithmParams struct
 * @param sraw      Raw value from the SGP40 sensor
 * @param voc_index Calculated VOC index value from the raw sensor value. Zero
 *                  during initial blackout period and 1..500 afterwards
 */
void voc_algorithm_process(VocAlgorithmParams *params, int32_t sraw, int32_t *voc_index);
}  // namespace sgp40
}  // namespace esphome
