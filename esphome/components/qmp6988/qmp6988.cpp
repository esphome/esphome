#include "qmp6988.h"
#include <cmath>

namespace esphome {
namespace qmp6988 {

static const uint8_t QMP6988_CHIP_ID = 0x5C;

static const uint8_t QMP6988_CHIP_ID_REG = 0xD1;     /* Chip ID confirmation Register */
static const uint8_t QMP6988_RESET_REG = 0xE0;       /* Device reset register */
static const uint8_t QMP6988_DEVICE_STAT_REG = 0xF3; /* Device state register */
static const uint8_t QMP6988_CTRLMEAS_REG = 0xF4;    /* Measurement Condition Control Register */
/* data */
static const uint8_t QMP6988_PRESSURE_MSB_REG = 0xF7;    /* Pressure MSB Register */
static const uint8_t QMP6988_TEMPERATURE_MSB_REG = 0xFA; /* Temperature MSB Reg */

/* compensation calculation */
static const uint8_t QMP6988_CALIBRATION_DATA_START = 0xA0; /* QMP6988 compensation coefficients */
static const uint8_t QMP6988_CALIBRATION_DATA_LENGTH = 25;

static const uint8_t SHIFT_RIGHT_4_POSITION = 4;
static const uint8_t SHIFT_LEFT_2_POSITION = 2;
static const uint8_t SHIFT_LEFT_4_POSITION = 4;
static const uint8_t SHIFT_LEFT_5_POSITION = 5;
static const uint8_t SHIFT_LEFT_8_POSITION = 8;
static const uint8_t SHIFT_LEFT_12_POSITION = 12;
static const uint8_t SHIFT_LEFT_16_POSITION = 16;

/* power mode */
static const uint8_t QMP6988_SLEEP_MODE = 0x00;
static const uint8_t QMP6988_FORCED_MODE = 0x01;
static const uint8_t QMP6988_NORMAL_MODE = 0x03;

static const uint8_t QMP6988_CTRLMEAS_REG_MODE_POS = 0;
static const uint8_t QMP6988_CTRLMEAS_REG_MODE_MSK = 0x03;
static const uint8_t QMP6988_CTRLMEAS_REG_MODE_LEN = 2;

static const uint8_t QMP6988_CTRLMEAS_REG_OSRST_POS = 5;
static const uint8_t QMP6988_CTRLMEAS_REG_OSRST_MSK = 0xE0;
static const uint8_t QMP6988_CTRLMEAS_REG_OSRST_LEN = 3;

static const uint8_t QMP6988_CTRLMEAS_REG_OSRSP_POS = 2;
static const uint8_t QMP6988_CTRLMEAS_REG_OSRSP_MSK = 0x1C;
static const uint8_t QMP6988_CTRLMEAS_REG_OSRSP_LEN = 3;

static const uint8_t QMP6988_CONFIG_REG = 0xF1; /*IIR filter co-efficient setting Register*/
static const uint8_t QMP6988_CONFIG_REG_FILTER_POS = 0;
static const uint8_t QMP6988_CONFIG_REG_FILTER_MSK = 0x07;
static const uint8_t QMP6988_CONFIG_REG_FILTER_LEN = 3;

static const uint32_t SUBTRACTOR = 8388608;

static const char *const TAG = "qmp6988";

static const char *oversampling_to_str(QMP6988Oversampling oversampling) {
  switch (oversampling) {
    case QMP6988_OVERSAMPLING_SKIPPED:
      return "None";
    case QMP6988_OVERSAMPLING_1X:
      return "1x";
    case QMP6988_OVERSAMPLING_2X:
      return "2x";
    case QMP6988_OVERSAMPLING_4X:
      return "4x";
    case QMP6988_OVERSAMPLING_8X:
      return "8x";
    case QMP6988_OVERSAMPLING_16X:
      return "16x";
    case QMP6988_OVERSAMPLING_32X:
      return "32x";
    case QMP6988_OVERSAMPLING_64X:
      return "64x";
    default:
      return "UNKNOWN";
  }
}

static const char *iir_filter_to_str(QMP6988IIRFilter filter) {
  switch (filter) {
    case QMP6988_IIR_FILTER_OFF:
      return "OFF";
    case QMP6988_IIR_FILTER_2X:
      return "2x";
    case QMP6988_IIR_FILTER_4X:
      return "4x";
    case QMP6988_IIR_FILTER_8X:
      return "8x";
    case QMP6988_IIR_FILTER_16X:
      return "16x";
    case QMP6988_IIR_FILTER_32X:
      return "32x";
    default:
      return "UNKNOWN";
  }
}

bool QMP6988Component::device_check_() {
  uint8_t ret = 0;

  ret = this->read_register(QMP6988_CHIP_ID_REG, &(qmp6988_data_.chip_id), 1);
  if (ret != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "%s: read chip ID (0xD1) failed", __func__);
  }
  ESP_LOGD(TAG, "qmp6988 read chip id = 0x%x", qmp6988_data_.chip_id);

  return qmp6988_data_.chip_id == QMP6988_CHIP_ID;
}

bool QMP6988Component::get_calibration_data_() {
  uint8_t status = 0;
  // BITFIELDS temp_COE;
  uint8_t a_data_uint8_tr[QMP6988_CALIBRATION_DATA_LENGTH] = {0};
  int len;

  for (len = 0; len < QMP6988_CALIBRATION_DATA_LENGTH; len += 1) {
    status = this->read_register(QMP6988_CALIBRATION_DATA_START + len, &a_data_uint8_tr[len], 1);
    if (status != i2c::ERROR_OK) {
      ESP_LOGE(TAG, "qmp6988 read calibration data (0xA0) error!");
      return false;
    }
  }

  qmp6988_data_.qmp6988_cali.COE_a0 =
      (QMP6988_S32_t) (((a_data_uint8_tr[18] << SHIFT_LEFT_12_POSITION) |
                        (a_data_uint8_tr[19] << SHIFT_LEFT_4_POSITION) | (a_data_uint8_tr[24] & 0x0f))
                       << 12);
  qmp6988_data_.qmp6988_cali.COE_a0 = qmp6988_data_.qmp6988_cali.COE_a0 >> 12;

  qmp6988_data_.qmp6988_cali.COE_a1 =
      (QMP6988_S16_t) (((a_data_uint8_tr[20]) << SHIFT_LEFT_8_POSITION) | a_data_uint8_tr[21]);
  qmp6988_data_.qmp6988_cali.COE_a2 =
      (QMP6988_S16_t) (((a_data_uint8_tr[22]) << SHIFT_LEFT_8_POSITION) | a_data_uint8_tr[23]);

  qmp6988_data_.qmp6988_cali.COE_b00 =
      (QMP6988_S32_t) (((a_data_uint8_tr[0] << SHIFT_LEFT_12_POSITION) | (a_data_uint8_tr[1] << SHIFT_LEFT_4_POSITION) |
                        ((a_data_uint8_tr[24] & 0xf0) >> SHIFT_RIGHT_4_POSITION))
                       << 12);
  qmp6988_data_.qmp6988_cali.COE_b00 = qmp6988_data_.qmp6988_cali.COE_b00 >> 12;

  qmp6988_data_.qmp6988_cali.COE_bt1 =
      (QMP6988_S16_t) (((a_data_uint8_tr[2]) << SHIFT_LEFT_8_POSITION) | a_data_uint8_tr[3]);
  qmp6988_data_.qmp6988_cali.COE_bt2 =
      (QMP6988_S16_t) (((a_data_uint8_tr[4]) << SHIFT_LEFT_8_POSITION) | a_data_uint8_tr[5]);
  qmp6988_data_.qmp6988_cali.COE_bp1 =
      (QMP6988_S16_t) (((a_data_uint8_tr[6]) << SHIFT_LEFT_8_POSITION) | a_data_uint8_tr[7]);
  qmp6988_data_.qmp6988_cali.COE_b11 =
      (QMP6988_S16_t) (((a_data_uint8_tr[8]) << SHIFT_LEFT_8_POSITION) | a_data_uint8_tr[9]);
  qmp6988_data_.qmp6988_cali.COE_bp2 =
      (QMP6988_S16_t) (((a_data_uint8_tr[10]) << SHIFT_LEFT_8_POSITION) | a_data_uint8_tr[11]);
  qmp6988_data_.qmp6988_cali.COE_b12 =
      (QMP6988_S16_t) (((a_data_uint8_tr[12]) << SHIFT_LEFT_8_POSITION) | a_data_uint8_tr[13]);
  qmp6988_data_.qmp6988_cali.COE_b21 =
      (QMP6988_S16_t) (((a_data_uint8_tr[14]) << SHIFT_LEFT_8_POSITION) | a_data_uint8_tr[15]);
  qmp6988_data_.qmp6988_cali.COE_bp3 =
      (QMP6988_S16_t) (((a_data_uint8_tr[16]) << SHIFT_LEFT_8_POSITION) | a_data_uint8_tr[17]);

  ESP_LOGV(TAG, "<-----------calibration data-------------->\r\n");
  ESP_LOGV(TAG, "COE_a0[%d] COE_a1[%d] COE_a2[%d] COE_b00[%d]\r\n", qmp6988_data_.qmp6988_cali.COE_a0,
           qmp6988_data_.qmp6988_cali.COE_a1, qmp6988_data_.qmp6988_cali.COE_a2, qmp6988_data_.qmp6988_cali.COE_b00);
  ESP_LOGV(TAG, "COE_bt1[%d] COE_bt2[%d] COE_bp1[%d] COE_b11[%d]\r\n", qmp6988_data_.qmp6988_cali.COE_bt1,
           qmp6988_data_.qmp6988_cali.COE_bt2, qmp6988_data_.qmp6988_cali.COE_bp1, qmp6988_data_.qmp6988_cali.COE_b11);
  ESP_LOGV(TAG, "COE_bp2[%d] COE_b12[%d] COE_b21[%d] COE_bp3[%d]\r\n", qmp6988_data_.qmp6988_cali.COE_bp2,
           qmp6988_data_.qmp6988_cali.COE_b12, qmp6988_data_.qmp6988_cali.COE_b21, qmp6988_data_.qmp6988_cali.COE_bp3);
  ESP_LOGV(TAG, "<-----------calibration data-------------->\r\n");

  qmp6988_data_.ik.a0 = qmp6988_data_.qmp6988_cali.COE_a0;    // 20Q4
  qmp6988_data_.ik.b00 = qmp6988_data_.qmp6988_cali.COE_b00;  // 20Q4

  qmp6988_data_.ik.a1 = 3608L * (QMP6988_S32_t) qmp6988_data_.qmp6988_cali.COE_a1 - 1731677965L;  // 31Q23
  qmp6988_data_.ik.a2 = 16889L * (QMP6988_S32_t) qmp6988_data_.qmp6988_cali.COE_a2 - 87619360L;   // 30Q47

  qmp6988_data_.ik.bt1 = 2982L * (QMP6988_S64_t) qmp6988_data_.qmp6988_cali.COE_bt1 + 107370906L;    // 28Q15
  qmp6988_data_.ik.bt2 = 329854L * (QMP6988_S64_t) qmp6988_data_.qmp6988_cali.COE_bt2 + 108083093L;  // 34Q38
  qmp6988_data_.ik.bp1 = 19923L * (QMP6988_S64_t) qmp6988_data_.qmp6988_cali.COE_bp1 + 1133836764L;  // 31Q20
  qmp6988_data_.ik.b11 = 2406L * (QMP6988_S64_t) qmp6988_data_.qmp6988_cali.COE_b11 + 118215883L;    // 28Q34
  qmp6988_data_.ik.bp2 = 3079L * (QMP6988_S64_t) qmp6988_data_.qmp6988_cali.COE_bp2 - 181579595L;    // 29Q43
  qmp6988_data_.ik.b12 = 6846L * (QMP6988_S64_t) qmp6988_data_.qmp6988_cali.COE_b12 + 85590281L;     // 29Q53
  qmp6988_data_.ik.b21 = 13836L * (QMP6988_S64_t) qmp6988_data_.qmp6988_cali.COE_b21 + 79333336L;    // 29Q60
  qmp6988_data_.ik.bp3 = 2915L * (QMP6988_S64_t) qmp6988_data_.qmp6988_cali.COE_bp3 + 157155561L;    // 28Q65
  ESP_LOGV(TAG, "<----------- int calibration data -------------->\r\n");
  ESP_LOGV(TAG, "a0[%d] a1[%d] a2[%d] b00[%d]\r\n", qmp6988_data_.ik.a0, qmp6988_data_.ik.a1, qmp6988_data_.ik.a2,
           qmp6988_data_.ik.b00);
  ESP_LOGV(TAG, "bt1[%lld] bt2[%lld] bp1[%lld] b11[%lld]\r\n", qmp6988_data_.ik.bt1, qmp6988_data_.ik.bt2,
           qmp6988_data_.ik.bp1, qmp6988_data_.ik.b11);
  ESP_LOGV(TAG, "bp2[%lld] b12[%lld] b21[%lld] bp3[%lld]\r\n", qmp6988_data_.ik.bp2, qmp6988_data_.ik.b12,
           qmp6988_data_.ik.b21, qmp6988_data_.ik.bp3);
  ESP_LOGV(TAG, "<----------- int calibration data -------------->\r\n");
  return true;
}

QMP6988_S16_t QMP6988Component::get_compensated_temperature_(qmp6988_ik_data_t *ik, QMP6988_S32_t dt) {
  QMP6988_S16_t ret;
  QMP6988_S64_t wk1, wk2;

  // wk1: 60Q4 // bit size
  wk1 = ((QMP6988_S64_t) ik->a1 * (QMP6988_S64_t) dt);        // 31Q23+24-1=54 (54Q23)
  wk2 = ((QMP6988_S64_t) ik->a2 * (QMP6988_S64_t) dt) >> 14;  // 30Q47+24-1=53 (39Q33)
  wk2 = (wk2 * (QMP6988_S64_t) dt) >> 10;                     // 39Q33+24-1=62 (52Q23)
  wk2 = ((wk1 + wk2) / 32767) >> 19;                          // 54,52->55Q23 (20Q04)
  ret = (QMP6988_S16_t) ((ik->a0 + wk2) >> 4);                // 21Q4 -> 17Q0
  return ret;
}

QMP6988_S32_t QMP6988Component::get_compensated_pressure_(qmp6988_ik_data_t *ik, QMP6988_S32_t dp, QMP6988_S16_t tx) {
  QMP6988_S32_t ret;
  QMP6988_S64_t wk1, wk2, wk3;

  // wk1 = 48Q16 // bit size
  wk1 = ((QMP6988_S64_t) ik->bt1 * (QMP6988_S64_t) tx);        // 28Q15+16-1=43 (43Q15)
  wk2 = ((QMP6988_S64_t) ik->bp1 * (QMP6988_S64_t) dp) >> 5;   // 31Q20+24-1=54 (49Q15)
  wk1 += wk2;                                                  // 43,49->50Q15
  wk2 = ((QMP6988_S64_t) ik->bt2 * (QMP6988_S64_t) tx) >> 1;   // 34Q38+16-1=49 (48Q37)
  wk2 = (wk2 * (QMP6988_S64_t) tx) >> 8;                       // 48Q37+16-1=63 (55Q29)
  wk3 = wk2;                                                   // 55Q29
  wk2 = ((QMP6988_S64_t) ik->b11 * (QMP6988_S64_t) tx) >> 4;   // 28Q34+16-1=43 (39Q30)
  wk2 = (wk2 * (QMP6988_S64_t) dp) >> 1;                       // 39Q30+24-1=62 (61Q29)
  wk3 += wk2;                                                  // 55,61->62Q29
  wk2 = ((QMP6988_S64_t) ik->bp2 * (QMP6988_S64_t) dp) >> 13;  // 29Q43+24-1=52 (39Q30)
  wk2 = (wk2 * (QMP6988_S64_t) dp) >> 1;                       // 39Q30+24-1=62 (61Q29)
  wk3 += wk2;                                                  // 62,61->63Q29
  wk1 += wk3 >> 14;                                            // Q29 >> 14 -> Q15
  wk2 = ((QMP6988_S64_t) ik->b12 * (QMP6988_S64_t) tx);        // 29Q53+16-1=45 (45Q53)
  wk2 = (wk2 * (QMP6988_S64_t) tx) >> 22;                      // 45Q53+16-1=61 (39Q31)
  wk2 = (wk2 * (QMP6988_S64_t) dp) >> 1;                       // 39Q31+24-1=62 (61Q30)
  wk3 = wk2;                                                   // 61Q30
  wk2 = ((QMP6988_S64_t) ik->b21 * (QMP6988_S64_t) tx) >> 6;   // 29Q60+16-1=45 (39Q54)
  wk2 = (wk2 * (QMP6988_S64_t) dp) >> 23;                      // 39Q54+24-1=62 (39Q31)
  wk2 = (wk2 * (QMP6988_S64_t) dp) >> 1;                       // 39Q31+24-1=62 (61Q20)
  wk3 += wk2;                                                  // 61,61->62Q30
  wk2 = ((QMP6988_S64_t) ik->bp3 * (QMP6988_S64_t) dp) >> 12;  // 28Q65+24-1=51 (39Q53)
  wk2 = (wk2 * (QMP6988_S64_t) dp) >> 23;                      // 39Q53+24-1=62 (39Q30)
  wk2 = (wk2 * (QMP6988_S64_t) dp);                            // 39Q30+24-1=62 (62Q30)
  wk3 += wk2;                                                  // 62,62->63Q30
  wk1 += wk3 >> 15;                                            // Q30 >> 15 = Q15
  wk1 /= 32767L;
  wk1 >>= 11;      // Q15 >> 7 = Q4
  wk1 += ik->b00;  // Q4 + 20Q4
  // wk1 >>= 4; // 28Q4 -> 24Q0
  ret = (QMP6988_S32_t) wk1;
  return ret;
}

void QMP6988Component::software_reset_() {
  uint8_t ret = 0;

  ret = this->write_byte(QMP6988_RESET_REG, 0xe6);
  if (ret != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Software Reset (0xe6) failed");
  }
  delay(10);

  this->write_byte(QMP6988_RESET_REG, 0x00);
}

void QMP6988Component::set_power_mode_(uint8_t power_mode) {
  uint8_t data;

  ESP_LOGD(TAG, "Setting Power mode to: %d", power_mode);

  qmp6988_data_.power_mode = power_mode;
  this->read_register(QMP6988_CTRLMEAS_REG, &data, 1);
  data = data & 0xfc;
  if (power_mode == QMP6988_SLEEP_MODE) {
    data |= 0x00;
  } else if (power_mode == QMP6988_FORCED_MODE) {
    data |= 0x01;
  } else if (power_mode == QMP6988_NORMAL_MODE) {
    data |= 0x03;
  }
  this->write_byte(QMP6988_CTRLMEAS_REG, data);

  ESP_LOGD(TAG, "Set Power mode 0xf4=0x%x \r\n", data);

  delay(10);
}

void QMP6988Component::write_filter_(unsigned char filter) {
  uint8_t data;

  data = (filter & 0x03);
  this->write_byte(QMP6988_CONFIG_REG, data);
  delay(10);
}

void QMP6988Component::write_oversampling_pressure_(unsigned char oversampling_p) {
  uint8_t data;

  this->read_register(QMP6988_CTRLMEAS_REG, &data, 1);
  data &= 0xe3;
  data |= (oversampling_p << 2);
  this->write_byte(QMP6988_CTRLMEAS_REG, data);
  delay(10);
}

void QMP6988Component::write_oversampling_temperature_(unsigned char oversampling_t) {
  uint8_t data;

  this->read_register(QMP6988_CTRLMEAS_REG, &data, 1);
  data &= 0x1f;
  data |= (oversampling_t << 5);
  this->write_byte(QMP6988_CTRLMEAS_REG, data);
  delay(10);
}

void QMP6988Component::set_temperature_oversampling(QMP6988Oversampling oversampling_t) {
  this->temperature_oversampling_ = oversampling_t;
}

void QMP6988Component::set_pressure_oversampling(QMP6988Oversampling oversampling_p) {
  this->pressure_oversampling_ = oversampling_p;
}

void QMP6988Component::set_iir_filter(QMP6988IIRFilter iirfilter) { this->iir_filter_ = iirfilter; }

void QMP6988Component::calculate_altitude_(float pressure, float temp) {
  float altitude;
  altitude = (pow((101325 / pressure), 1 / 5.257) - 1) * (temp + 273.15) / 0.0065;
  this->qmp6988_data_.altitude = altitude;
}

void QMP6988Component::calculate_pressure_() {
  uint8_t err = 0;
  QMP6988_U32_t p_read, t_read;
  QMP6988_S32_t p_raw, t_raw;
  uint8_t a_data_uint8_tr[6] = {0};
  QMP6988_S32_t t_int, p_int;
  this->qmp6988_data_.temperature = 0;
  this->qmp6988_data_.pressure = 0;

  err = this->read_register(QMP6988_PRESSURE_MSB_REG, a_data_uint8_tr, 6);
  if (err != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Error reading raw pressure/temp values");
    return;
  }
  p_read = (QMP6988_U32_t) ((((QMP6988_U32_t) (a_data_uint8_tr[0])) << SHIFT_LEFT_16_POSITION) |
                            (((QMP6988_U16_t) (a_data_uint8_tr[1])) << SHIFT_LEFT_8_POSITION) | (a_data_uint8_tr[2]));
  p_raw = (QMP6988_S32_t) (p_read - SUBTRACTOR);

  t_read = (QMP6988_U32_t) ((((QMP6988_U32_t) (a_data_uint8_tr[3])) << SHIFT_LEFT_16_POSITION) |
                            (((QMP6988_U16_t) (a_data_uint8_tr[4])) << SHIFT_LEFT_8_POSITION) | (a_data_uint8_tr[5]));
  t_raw = (QMP6988_S32_t) (t_read - SUBTRACTOR);

  t_int = this->get_compensated_temperature_(&(qmp6988_data_.ik), t_raw);
  p_int = this->get_compensated_pressure_(&(qmp6988_data_.ik), p_raw, t_int);

  this->qmp6988_data_.temperature = (float) t_int / 256.0f;
  this->qmp6988_data_.pressure = (float) p_int / 16.0f;
}

void QMP6988Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up QMP6988");

  bool ret;
  ret = this->device_check_();
  if (!ret) {
    ESP_LOGCONFIG(TAG, "Setup failed - device not found");
  }

  this->software_reset_();
  this->get_calibration_data_();
  this->set_power_mode_(QMP6988_NORMAL_MODE);
  this->write_filter_(iir_filter_);
  this->write_oversampling_pressure_(this->pressure_oversampling_);
  this->write_oversampling_temperature_(this->temperature_oversampling_);
}

void QMP6988Component::dump_config() {
  ESP_LOGCONFIG(TAG, "QMP6988:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with QMP6988 failed!");
  }
  LOG_UPDATE_INTERVAL(this);

  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  ESP_LOGCONFIG(TAG, "    Temperature Oversampling: %s", oversampling_to_str(this->temperature_oversampling_));
  LOG_SENSOR("  ", "Pressure", this->pressure_sensor_);
  ESP_LOGCONFIG(TAG, "    Pressure Oversampling: %s", oversampling_to_str(this->pressure_oversampling_));
  ESP_LOGCONFIG(TAG, "  IIR Filter: %s", iir_filter_to_str(this->iir_filter_));
}

float QMP6988Component::get_setup_priority() const { return setup_priority::DATA; }

void QMP6988Component::update() {
  this->calculate_pressure_();
  float pressurehectopascals = this->qmp6988_data_.pressure / 100;
  float temperature = this->qmp6988_data_.temperature;

  ESP_LOGD(TAG, "Temperature=%.2f°C, Pressure=%.2fhPa", temperature, pressurehectopascals);
  if (this->temperature_sensor_ != nullptr)
    this->temperature_sensor_->publish_state(temperature);
  if (this->pressure_sensor_ != nullptr)
    this->pressure_sensor_->publish_state(pressurehectopascals);
}

}  // namespace qmp6988
}  // namespace esphome
