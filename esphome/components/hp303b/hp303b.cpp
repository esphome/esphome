#include "hp303b.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace hp303b {

/**
 * Standard Constructor
 */
HP303BComponent::HP303BComponent() {
  // assume that initialization has failed before it has been done
  m_init_fail = 1U;
}

/**
 * Standard Destructor
 */
HP303BComponent::~HP303BComponent() { this->end(); }

void HP303BComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up HP303B...");
  init();
}

void HP303BComponent::init(void) {
  int16_t prod_id = read_byte_bitfield(HP303B__REG_INFO_PROD_ID);
  if (prod_id != HP303B__PROD_ID) {
    // Connected device is not a HP303B
    m_init_fail = 1U;
    return;
  }
  m_product_id = prod_id;

  int16_t rev_id = read_byte_bitfield(HP303B__REG_INFO_REV_ID);
  if (rev_id < 0) {
    m_init_fail = 1U;
    return;
  }
  m_revision_id = rev_id;

  // find out which temperature sensor is calibrated with coefficients...
  int16_t sensor = read_byte_bitfield(HP303B__REG_INFO_TEMP_SENSORREC);
  if (sensor < 0) {
    m_init_fail = 1U;
    return;
  }

  //...and use this sensor for temperature measurement
  m_temp_sensor = sensor;
  if (write_byte_bitfield((uint8_t) sensor, HP303B__REG_INFO_TEMP_SENSOR) < 0) {
    m_init_fail = 1U;
    return;
  }

  // read coefficients
  if (read_coeffs() < 0) {
    m_init_fail = 1U;
    return;
  }

  // set to standby for further configuration
  standby();

  // set measurement precision and rate to standard values;
  config_temp(HP303B__TEMP_STD_MR, HP303B__TEMP_STD_OSR);
  config_pressure(HP303B__PRS_STD_MR, HP303B__PRS_STD_OSR);

  // perform a first temperature measurement
  // the most recent temperature will be saved internally
  // and used for compensation when calculating pressure
  int32_t trash;
  measure_temp_once(trash);

  // make sure the HP303B is in standby after initialization
  standby();

  // Fix IC with a fuse bit problem, which lead to a wrong temperature
  // Should not affect ICs without this problem
  correct_temp();
}

void HP303BComponent::update() {
  int32_t pressure = 0;
  measure_pressure_once(pressure);  // library returns value in in Pa, which equals 1/100 hPa
  float hPa = pressure / 100.0;
  if (this->pressure_ != nullptr)
    this->pressure_->publish_state(hPa);

  ESP_LOGD(TAG, "Got Pressure=%.1f", hPa);

  this->status_clear_warning();
}

int16_t HP303BComponent::standby() {
  // abort if initialization failed
  if (m_init_fail) {
    return HP303B__FAIL_INIT_FAILED;
  }
  // set device to idling mode
  int16_t ret = set_op_mode(IDLE);
  if (ret != HP303B__SUCCEEDED) {
    return ret;
  }
  // flush the FIFO
  ret = write_byte_bitfield(1U, HP303B__REG_INFO_FIFO_FL);
  if (ret < 0) {
    return ret;
  }
  // disable the FIFO
  ret = write_byte_bitfield(0U, HP303B__REG_INFO_FIFO_EN);
  return ret;
}

void HP303BComponent::end(void) { standby(); }
uint8_t HP303BComponent::get_product_id() { return m_product_id; }
uint8_t HP303BComponent::get_revision_id() { return m_revision_id; }
int16_t HP303BComponent::measure_temp_once(int32_t &result) { return measure_temp_once(result, m_temp_osr); }
int16_t HP303BComponent::measure_temp_once(int32_t &result, uint8_t oversampling_rate) {
  // Start measurement
  int16_t ret = start_measure_temp_once(oversampling_rate);
  if (ret != HP303B__SUCCEEDED) {
    return ret;
  }

  // wait until measurement is finished
  delay(calc_busy_time(0U, m_temp_osr) / HP303B__BUSYTIME_SCALING);
  delay(HP303B__BUSYTIME_FAILSAFE);

  ret = get_single_result(result);
  if (ret != HP303B__SUCCEEDED) {
    standby();
  }
  return ret;
}

int16_t HP303BComponent::start_measure_temp_once() { return start_measure_temp_once(m_temp_osr); }
int16_t HP303BComponent::start_measure_temp_once(uint8_t oversampling_rate) {
  // abort if initialization failed
  if (m_init_fail) {
    return HP303B__FAIL_INIT_FAILED;
  }
  // abort if device is not in idling mode
  if (m_op_mode != IDLE) {
    return HP303B__FAIL_TOOBUSY;
  }

  if (oversampling_rate != m_temp_osr) {
    // configuration of oversampling rate
    if (config_temp(0U, oversampling_rate) != HP303B__SUCCEEDED) {
      return HP303B__FAIL_UNKNOWN;
    }
  }

  // set device to temperature measuring mode
  return set_op_mode(0, 1, 0);
}

int16_t HP303BComponent::measure_pressure_once(int32_t &result, uint8_t oversamplingRate) {
  // start the measurement
  int16_t ret = start_measure_pressure_once(oversamplingRate);
  if (ret != HP303B__SUCCEEDED) {
    return ret;
  }

  // wait until measurement is finished
  delay(calc_busy_time(0U, m_prs_osr) / HP303B__BUSYTIME_SCALING);
  delay(HP303B__BUSYTIME_FAILSAFE);

  ret = get_single_result(result);
  if (ret != HP303B__SUCCEEDED) {
    standby();
  }
  return ret;
}

int16_t HP303BComponent::start_measure_pressure_once(uint8_t oversampling_rate) {
  // abort if initialization failed
  if (m_init_fail) {
    return HP303B__FAIL_INIT_FAILED;
  }
  // abort if device is not in idling mode
  if (m_op_mode != IDLE) {
    return HP303B__FAIL_TOOBUSY;
  }
  // configuration of oversampling rate, lowest measure rate to avoid conflicts
  if (oversampling_rate != m_prs_osr) {
    if (config_pressure(0U, oversampling_rate)) {
      return HP303B__FAIL_UNKNOWN;
    }
  }
  // set device to pressure measuring mode
  return set_op_mode(0, 0, 1);
}

int16_t HP303BComponent::get_single_result(int32_t &result) {
  // abort if initialization failed
  if (m_init_fail) {
    return HP303B__FAIL_INIT_FAILED;
  }

  // read finished bit for current opMode
  int16_t rdy;
  switch (m_op_mode) {
    case CMD_TEMP:  // temperature
      rdy = read_byte_bitfield(HP303B__REG_INFO_TEMP_RDY);
      break;
    case CMD_PRS:  // pressure
      rdy = read_byte_bitfield(HP303B__REG_INFO_PRS_RDY);
      break;
    default:  // HP303B not in command mode
      return HP303B__FAIL_TOOBUSY;
  }

  // read new measurement result
  switch (rdy) {
    case HP303B__FAIL_UNKNOWN:  // could not read ready flag
      return HP303B__FAIL_UNKNOWN;
    case 0:  // ready flag not set, measurement still in progress
      return HP303B__FAIL_UNFINISHED;
    case 1:  // measurement ready, expected case
      Mode old_mode = m_op_mode;
      m_op_mode = IDLE;  // opcode was automatically reseted by HP303B
      switch (old_mode) {
        case CMD_TEMP:                   // temperature
          return get_temp(&result);      // get and calculate the temperature value
        case CMD_PRS:                    // pressure
          return get_pressure(&result);  // get and calculate the pressure value
        default:
          return HP303B__FAIL_UNKNOWN;  // should already be filtered above
      }
  }
  return HP303B__FAIL_UNKNOWN;
}
int16_t HP303BComponent::start_measure_temp_cont(uint8_t measure_rate, uint8_t oversampling_rate) {
  // abort if initialization failed
  if (m_init_fail) {
    return HP303B__FAIL_INIT_FAILED;
  }
  // abort if device is not in idling mode
  if (m_op_mode != IDLE) {
    return HP303B__FAIL_TOOBUSY;
  }
  // abort if speed and precision are too high
  if (calc_busy_time(measure_rate, oversampling_rate) >= HP303B__MAX_BUSYTIME) {
    return HP303B__FAIL_UNFINISHED;
  }
  // update precision and measuring rate
  if (config_temp(measure_rate, oversampling_rate)) {
    return HP303B__FAIL_UNKNOWN;
  }
  // enable result FIFO
  if (write_byte_bitfield(1U, HP303B__REG_INFO_FIFO_EN)) {
    return HP303B__FAIL_UNKNOWN;
  }
  // Start measuring in background mode
  if (set_op_mode(1, 1, 0)) {
    return HP303B__FAIL_UNKNOWN;
  }
  return HP303B__SUCCEEDED;
}

int16_t HP303BComponent::start_measure_pressure_cont(uint8_t measure_rate, uint8_t oversampling_rate) {
  // abort if initialization failed
  if (m_init_fail) {
    return HP303B__FAIL_INIT_FAILED;
  }
  // abort if device is not in idling mode
  if (m_op_mode != IDLE) {
    return HP303B__FAIL_TOOBUSY;
  }
  // abort if speed and precision are too high
  if (calc_busy_time(measure_rate, oversampling_rate) >= HP303B__MAX_BUSYTIME) {
    return HP303B__FAIL_UNFINISHED;
  }
  // update precision and measuring rate
  if (config_pressure(measure_rate, oversampling_rate))
    return HP303B__FAIL_UNKNOWN;
  // enable result FIFO
  if (write_byte_bitfield(1U, HP303B__REG_INFO_FIFO_EN)) {
    return HP303B__FAIL_UNKNOWN;
  }
  // Start measuring in background mode
  if (set_op_mode(1, 0, 1)) {
    return HP303B__FAIL_UNKNOWN;
  }
  return HP303B__SUCCEEDED;
}
int16_t HP303BComponent::start_measure_both_cont(uint8_t temp_mr, uint8_t temp_osr, uint8_t prs_mr, uint8_t prs_osr) {
  // abort if initialization failed
  if (m_init_fail) {
    return HP303B__FAIL_INIT_FAILED;
  }
  // abort if device is not in idling mode
  if (m_op_mode != IDLE) {
    return HP303B__FAIL_TOOBUSY;
  }
  // abort if speed and precision are too high
  if (calc_busy_time(temp_mr, temp_osr) + calc_busy_time(prs_mr, prs_osr) >= HP303B__MAX_BUSYTIME) {
    return HP303B__FAIL_UNFINISHED;
  }
  // update precision and measuring rate
  if (config_temp(temp_mr, temp_osr)) {
    return HP303B__FAIL_UNKNOWN;
  }
  // update precision and measuring rate
  if (config_pressure(prs_mr, prs_osr))
    return HP303B__FAIL_UNKNOWN;
  // enable result FIFO
  if (write_byte_bitfield(1U, HP303B__REG_INFO_FIFO_EN)) {
    return HP303B__FAIL_UNKNOWN;
  }
  // Start measuring in background mode
  if (set_op_mode(1, 1, 1)) {
    return HP303B__FAIL_UNKNOWN;
  }
  return HP303B__SUCCEEDED;
}

int16_t HP303BComponent::get_cont_results(int32_t *temp_buffer, uint8_t &temp_count, int32_t *prs_buffer,
                                          uint8_t &prs_count) {
  if (m_init_fail) {
    return HP303B__FAIL_INIT_FAILED;
  }
  // abort if device is not in background mode
  if (!(m_op_mode & INVAL_OP_CONT_NONE)) {
    return HP303B__FAIL_TOOBUSY;
  }

  // prepare parameters for buffer length and count
  uint8_t temp_len = temp_count;
  uint8_t prs_len = prs_count;
  temp_count = 0U;
  prs_count = 0U;

  // while FIFO is not empty
  while (read_byte_bitfield(HP303B__REG_INFO_FIFO_EMPTY) == 0) {
    int32_t result;
    // read next result from FIFO
    int16_t type = getFIFOvalue(&result);
    switch (type) {
      case 0:  // temperature
        // calculate compensated pressure value
        result = calc_temp(result);
        // if buffer exists and is not full
        // write result to buffer and increase temperature result counter
        if (temp_buffer != NULL) {
          if (temp_count < temp_len) {
            temp_buffer[temp_count++] = result;
          }
        }
        break;
      case 1:  // pressure
        // calculate compensated pressure value
        result = calc_pressure(result);
        // if buffer exists and is not full
        // write result to buffer and increase pressure result counter
        if (prs_buffer != NULL) {
          if (prs_count < prs_len) {
            prs_buffer[prs_count++] = result;
          }
        }
        break;
      case -1:  // read failed
        break;  // continue while loop
                // if connection failed permanently,
                // while condition will become false
                // if read failed only once, loop will try again
    }
  }
  return HP303B__SUCCEEDED;
}

int16_t HP303BComponent::set_interrupt_sources(uint8_t fifo_full, uint8_t temp_ready, uint8_t prs_ready) {
  // Interrupts are not supported with 4 Wire SPI
  return HP303B__FAIL_UNKNOWN;
}

int16_t HP303BComponent::correct_temp(void) {
  if (m_init_fail) {
    return HP303B__FAIL_INIT_FAILED;
  }
  write_byte(0x0E, 0xA5);
  write_byte(0x0F, 0x96);
  write_byte(0x62, 0x02);
  write_byte(0x0E, 0x00);
  write_byte(0x0F, 0x00);

  // perform a first temperature measurement (again)
  // the most recent temperature will be saved internally
  // and used for compensation when calculating pressure
  int32_t trash;
  measure_temp_once(trash);

  return HP303B__SUCCEEDED;
}

int16_t HP303BComponent::read_coeffs(void) {
  uint8_t buffer[HP303B__REG_LEN_COEF];
  // read COEF registers to buffer
  int16_t ret = read_block(HP303B__REG_ADR_COEF, HP303B__REG_LEN_COEF, buffer);
  // abort if less than REG_LEN_COEF bytes were read
  if (ret < HP303B__REG_LEN_COEF) {
    return HP303B__FAIL_UNKNOWN;
  }

  // compose coefficients from buffer content
  m_c0Half = ((uint32_t) buffer[0] << 4) | (((uint32_t) buffer[1] >> 4) & 0x0F);
  // this construction recognizes non-32-bit negative numbers
  // and converts them to 32-bit negative numbers with 2's complement
  if (m_c0Half & ((uint32_t) 1 << 11)) {
    m_c0Half -= (uint32_t) 1 << 12;
  }
  // c0 is only used as c0*0.5, so c0_half is calculated immediately
  m_c0Half = m_c0Half / 2U;

  // now do the same thing for all other coefficients
  m_c1 = (((uint32_t) buffer[1] & 0x0F) << 8) | (uint32_t) buffer[2];
  if (m_c1 & ((uint32_t) 1 << 11)) {
    m_c1 -= (uint32_t) 1 << 12;
  }

  m_c00 = ((uint32_t) buffer[3] << 12) | ((uint32_t) buffer[4] << 4) | (((uint32_t) buffer[5] >> 4) & 0x0F);
  if (m_c00 & ((uint32_t) 1 << 19)) {
    m_c00 -= (uint32_t) 1 << 20;
  }

  m_c10 = (((uint32_t) buffer[5] & 0x0F) << 16) | ((uint32_t) buffer[6] << 8) | (uint32_t) buffer[7];
  if (m_c10 & ((uint32_t) 1 << 19)) {
    m_c10 -= (uint32_t) 1 << 20;
  }

  m_c01 = ((uint32_t) buffer[8] << 8) | (uint32_t) buffer[9];
  if (m_c01 & ((uint32_t) 1 << 15)) {
    m_c01 -= (uint32_t) 1 << 16;
  }

  m_c11 = ((uint32_t) buffer[10] << 8) | (uint32_t) buffer[11];
  if (m_c11 & ((uint32_t) 1 << 15)) {
    m_c11 -= (uint32_t) 1 << 16;
  }

  m_c20 = ((uint32_t) buffer[12] << 8) | (uint32_t) buffer[13];
  if (m_c20 & ((uint32_t) 1 << 15)) {
    m_c20 -= (uint32_t) 1 << 16;
  }

  m_c21 = ((uint32_t) buffer[14] << 8) | (uint32_t) buffer[15];
  if (m_c21 & ((uint32_t) 1 << 15)) {
    m_c21 -= (uint32_t) 1 << 16;
  }

  m_c30 = ((uint32_t) buffer[16] << 8) | (uint32_t) buffer[17];
  if (m_c30 & ((uint32_t) 1 << 15)) {
    m_c30 -= (uint32_t) 1 << 16;
  }

  return HP303B__SUCCEEDED;
}

int16_t HP303BComponent::set_op_mode(uint8_t background, uint8_t temperature, uint8_t pressure) {
  uint8_t op_mode = (background & HP303B__LSB) << 2U | (temperature & HP303B__LSB) << 1U | (pressure & HP303B__LSB);
  return set_op_mode(op_mode);
}

int16_t HP303BComponent::set_op_mode(uint8_t op_mode) {
  // Filter irrelevant bits
  op_mode &= HP303B__REG_MASK_OPMODE >> HP303B__REG_SHIFT_OPMODE;
  // Filter invalid OpModes
  if (op_mode == INVAL_OP_CMD_BOTH || op_mode == INVAL_OP_CONT_NONE) {
    return HP303B__FAIL_UNKNOWN;
  }
  // Set OpMode
  if (write_byte(HP303B__REG_ADR_OPMODE, op_mode)) {
    return HP303B__FAIL_UNKNOWN;
  }
  m_op_mode = (HP303BComponent::Mode) op_mode;
  return HP303B__SUCCEEDED;
}
int16_t HP303BComponent::config_temp(uint8_t temp_mr, uint8_t temp_osr) {
  // mask parameters
  temp_mr &= HP303B__REG_MASK_TEMP_MR >> HP303B__REG_SHIFT_TEMP_MR;
  temp_osr &= HP303B__REG_MASK_TEMP_OSR >> HP303B__REG_SHIFT_TEMP_OSR;

  // set config register according to parameters
  uint8_t to_write = temp_mr << HP303B__REG_SHIFT_TEMP_MR;
  to_write |= temp_osr << HP303B__REG_SHIFT_TEMP_OSR;
  // using recommended temperature sensor
  to_write |= HP303B__REG_MASK_TEMP_SENSOR & (m_temp_sensor << HP303B__REG_SHIFT_TEMP_SENSOR);
  int16_t ret = write_byte(HP303B__REG_ADR_TEMP_MR, to_write);
  // abort immediately on fail
  if (ret != HP303B__SUCCEEDED) {
    return HP303B__FAIL_UNKNOWN;
  }

  // set TEMP SHIFT ENABLE if oversampling rate higher than eight(2^3)
  if (temp_osr > HP303B__OSR_SE) {
    ret = write_byte_bitfield(1U, HP303B__REG_INFO_TEMP_SE);
  } else {
    ret = write_byte_bitfield(0U, HP303B__REG_INFO_TEMP_SE);
  }

  if (ret == HP303B__SUCCEEDED) {  // save new settings
    m_temp_mr = temp_mr;
    m_temp_osr = temp_osr;
  } else {
    // try to rollback on fail avoiding endless recursion
    // this is to make sure that shift enable and oversampling rate
    // are always consistent
    if (temp_mr != m_temp_mr || temp_osr != m_temp_osr) {
      config_temp(m_temp_mr, m_temp_osr);
    }
  }
  return ret;
}

int16_t HP303BComponent::config_pressure(uint8_t prsMr, uint8_t prsOsr) {
  // mask parameters
  prsMr &= HP303B__REG_MASK_PRS_MR >> HP303B__REG_SHIFT_PRS_MR;
  prsOsr &= HP303B__REG_MASK_PRS_OSR >> HP303B__REG_SHIFT_PRS_OSR;

  // set config register according to parameters
  uint8_t toWrite = prsMr << HP303B__REG_SHIFT_PRS_MR;
  toWrite |= prsOsr << HP303B__REG_SHIFT_PRS_OSR;
  int16_t ret = write_byte(HP303B__REG_ADR_PRS_MR, toWrite);
  // abort immediately on fail
  if (ret != HP303B__SUCCEEDED) {
    return HP303B__FAIL_UNKNOWN;
  }

  // set PM SHIFT ENABLE if oversampling rate higher than eight(2^3)
  if (prsOsr > HP303B__OSR_SE) {
    ret = write_byte_bitfield(1U, HP303B__REG_INFO_PRS_SE);
  } else {
    ret = write_byte_bitfield(0U, HP303B__REG_INFO_PRS_SE);
  }

  if (ret == HP303B__SUCCEEDED) {  // save new settings
    m_prs_mr = prsMr;
    m_prs_osr = prsOsr;
  } else {  // try to rollback on fail avoiding endless recursion
    // this is to make sure that shift enable and oversampling rate
    // are always consistent
    if (prsMr != m_prs_mr || prsOsr != m_prs_osr) {
      config_pressure(m_prs_mr, m_prs_osr);
    }
  }
  return ret;
}

uint16_t HP303BComponent::calc_busy_time(uint16_t mr, uint16_t osr) {
  // mask parameters first
  mr &= HP303B__REG_MASK_TEMP_MR >> HP303B__REG_SHIFT_TEMP_MR;
  osr &= HP303B__REG_MASK_TEMP_OSR >> HP303B__REG_SHIFT_TEMP_OSR;
  // formula from datasheet (optimized)
  return ((uint32_t) 20U << mr) + ((uint32_t) 16U << (osr + mr));
}

int16_t HP303BComponent::get_temp(int32_t *result) {
  uint8_t buffer[3] = {0};
  // read raw pressure data to buffer

  int16_t i = read_block(HP303B__REG_ADR_TEMP, HP303B__REG_LEN_TEMP, buffer);
  if (i != HP303B__REG_LEN_TEMP) {
    // something went wrong
    return HP303B__FAIL_UNKNOWN;
  }

  // compose raw temperature value from buffer
  int32_t temp = (uint32_t) buffer[0] << 16 | (uint32_t) buffer[1] << 8 | (uint32_t) buffer[2];
  // recognize non-32-bit negative numbers
  // and convert them to 32-bit negative numbers using 2's complement
  if (temp & ((uint32_t) 1 << 23)) {
    temp -= (uint32_t) 1 << 24;
  }

  // return temperature
  *result = calc_temp(temp);
  return HP303B__SUCCEEDED;
}

int16_t HP303BComponent::get_pressure(int32_t *result) {
  uint8_t buffer[3] = {0};
  // read raw pressure data to buffer
  int16_t i = read_block(HP303B__REG_ADR_PRS, HP303B__REG_LEN_PRS, buffer);
  if (i != HP303B__REG_LEN_PRS) {
    // something went wrong
    // negative pressure is not allowed
    return HP303B__FAIL_UNKNOWN;
  }

  // compose raw pressure value from buffer
  int32_t prs = (uint32_t) buffer[0] << 16 | (uint32_t) buffer[1] << 8 | (uint32_t) buffer[2];
  // recognize non-32-bit negative numbers
  // and convert them to 32-bit negative numbers using 2's complement
  if (prs & ((uint32_t) 1 << 23)) {
    prs -= (uint32_t) 1 << 24;
  }

  *result = calc_pressure(prs);
  return HP303B__SUCCEEDED;
}

int16_t HP303BComponent::getFIFOvalue(int32_t *value) {
  // abort on invalid argument
  if (value == NULL) {
    return HP303B__FAIL_UNKNOWN;
  }

  uint8_t buffer[HP303B__REG_LEN_PRS] = {0};
  // always read from pressure raw value register
  int16_t i = read_block(HP303B__REG_ADR_PRS, HP303B__REG_LEN_PRS, buffer);
  if (i != HP303B__REG_LEN_PRS) {
    // something went wrong
    // return error code
    return HP303B__FAIL_UNKNOWN;
  }
  // compose raw pressure value from buffer
  *value = (uint32_t) buffer[0] << 16 | (uint32_t) buffer[1] << 8 | (uint32_t) buffer[2];
  // recognize non-32-bit negative numbers
  // and convert them to 32-bit negative numbers using 2's complement
  if (*value & ((uint32_t) 1 << 23)) {
    *value -= (uint32_t) 1 << 24;
  }

  // least significant bit shows measurement type
  return buffer[2] & HP303B__LSB;
}

int32_t HP303BComponent::calc_temp(int32_t raw) {
  double temp = raw;

  // scale temperature according to scaling table and oversampling
  temp /= scaling_facts[m_temp_osr];

  // update last measured temperature
  // it will be used for pressure compensation
  m_last_temp_scal = temp;

  // Calculate compensated temperature
  temp = m_c0Half + m_c1 * temp;

  // return temperature
  return (int32_t) temp;
}

int32_t HP303BComponent::calc_pressure(int32_t raw) {
  double prs = raw;

  // scale pressure according to scaling table and oversampling
  prs /= scaling_facts[m_prs_osr];

  // Calculate compensated pressure
  prs = m_c00 + prs * (m_c10 + prs * (m_c20 + prs * m_c30)) + m_last_temp_scal * (m_c01 + prs * (m_c11 + prs * m_c21));

  // return pressure
  return (int32_t) prs;
}

int16_t HP303BComponent::write_byte(uint8_t reg_address, uint8_t data) { return write_byte(reg_address, data, 0U); }

int16_t HP303BComponent::write_byte_bitfield(uint8_t data, uint8_t reg_address, uint8_t mask, uint8_t shift,
                                             uint8_t check) {
  int16_t old = read_byte(reg_address);
  if (old < 0) {
    // fail while reading
    return old;
  }
  return write_byte(reg_address, ((uint8_t) old & ~mask) | ((data << shift) & mask), check);
}

int16_t HP303BComponent::write_byte_bitfield(uint8_t reg_address, uint8_t mask, uint8_t shift) {
  int16_t ret = read_byte(reg_address);
  if (ret < 0) {
    return ret;
  }
  return (((uint8_t) ret) & mask) >> shift;
}

bool HP303BSensor::process(std::vector<uint8_t> &data) {
  if (data.size() != this->uid_.size())
    return false;

  for (size_t i = 0; i < data.size(); i++) {
    if (data[i] != this->uid_[i])
      return false;
  }

  this->publish_state(true);
  this->found_ = true;
  return true;
}

}  // namespace hp303b
}  // namespace esphome