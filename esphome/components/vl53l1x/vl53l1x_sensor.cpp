/*
 * Most of the code in this integration is based on the Adafruit_VL53L1X library
 * by Adafruit Industries, which in turn is based on
 * the VL53L1X API provided by ST (STSW-IMG007).
 *
 * For more information about licensing, please view the included LICENSE.txt file
 * in the vl53l1x integration directory.
 */

#include "vl53l1x_sensor.h"

namespace esphome {
namespace vl53l1x {

std::list<VL53L1XSensor *> VL53L1XSensor::vl53l1x_sensors;
bool VL53L1XSensor::enable_pin_setup_complete = false;

VL53L1XSensor::VL53L1XSensor() { VL53L1XSensor::vl53l1x_sensors.push_back(this); }

void VL53L1XSensor::dump_config() {
  LOG_SENSOR("", "VL53L1X", this);
  LOG_UPDATE_INTERVAL(this);
  LOG_I2C_DEVICE(this);
  LOG_PIN("  Enable Pin: ", this->enable_pin_);
  LOG_PIN("  IRQ Pin: ", this->irq_pin_);
  ESP_LOGCONFIG(TAG, "  Model 0x%X", this->sensor_id());
  ESP_LOGCONFIG(TAG, "  io_2v8: %s", this->io_2v8_ ? "true" : "false");
  ESP_LOGCONFIG(TAG, "  long_range: %s", this->long_range_ ? "true" : "false");
  ESP_LOGCONFIG(TAG, "  timing_budget: %i", this->timing_budget_);
  ESP_LOGCONFIG(TAG, "  offset: %i", this->offset_);
}

void VL53L1XSensor::setup() {
  if (!esphome::vl53l1x::VL53L1XSensor::enable_pin_setup_complete) {
    for (auto &vl53_sensor : vl53l1x_sensors) {
      if (vl53_sensor->enable_pin_ != nullptr) {
        // Set enable pin as OUTPUT and disable the enable pin to force vl53 to HW Standby mode
        vl53_sensor->enable_pin_->setup();
        vl53_sensor->enable_pin_->digital_write(false);
      }
    }
    esphome::vl53l1x::VL53L1XSensor::enable_pin_setup_complete = true;
  }

  if (this->enable_pin_ != nullptr) {
    enable_pin_->pin_mode(gpio::FLAG_OUTPUT);
  }
  if (this->irq_pin_ != nullptr) {
    irq_pin_->pin_mode(gpio::FLAG_OUTPUT);
  }

  uint8_t address_to_set = address_;
  set_i2c_address(this->VL53L1X_I2C_ADDR);

  if (!this->begin(address_to_set)) {
    ESP_LOGE(TAG, "'%s' - Sensor init failed", this->name_.c_str());
    this->mark_failed();
  }

  if (this->io_2v8_) {
    uint8_t val;
    VL53L1X_Error status;
    status = this->vl53l1x_rd_byte(this->PAD_I2C_HV_EXTSUP_CONFIG, &val);
    if (status == this->VL53L1X_ERROR_NONE) {
      val = (val & 0xfe) | 0x01;
      this->write_byte(this->PAD_I2C_HV_EXTSUP_CONFIG, val);
    }
  }

  this->vl53l1x_set_distance_mode(this->long_range_ ? 2 : 1);
  // Valid timing budgets: 15, 20, 33, 50, 100, 200 and 500ms!
  this->vl53l1x_set_timing_budget_in_ms(this->timing_budget_);

  if (this->offset_ != 0) {
    this->vl53l1x_set_offset(this->offset_);
  }

  if (!this->start_ranging()) {
    ESP_LOGE(TAG, "'%s' - Couldn't start ranging. Error code %i", this->name_.c_str(), this->vl_status);
    this->mark_failed();
  }
}

void VL53L1XSensor::update() {
  int16_t distance = 0;

  if (this->data_ready()) {
    // new measurement for the taking!
    distance = this->distance();
    if (distance == -1) {
      // something went wrong!
      ESP_LOGE(TAG, "Couldn't get distance. Error code %i", this->vl_status);
      return;
    }
    // data is read out, time for another reading!
    this->clear_interrupt();
  }
  publish_state(distance / 1000.0);  // convert from mm to m and publish the result
}

// void VL53L1XSensor::loop() {
//  uint16_t range_m = 0;
//  range_m = this->read_range(true);
//  publish_state(range_m);
//}

/**************************************************************************/
/*!
   @brief  Setups the I2C interface and hardware
   @param  i2c_addr Optional I2C address the sensor can be found on. Default is
  0x29
   @param  theWire Optional The Wire bus object to use.
   @returns  True if device is set up, false on any failure
*/
/**************************************************************************/
bool VL53L1XSensor::begin(uint8_t i2c_addr) {
  if (this->enable_pin_ != nullptr) {
    this->enable_pin_->digital_write(true);
    this->enable_pin_->digital_write(false);
    delay(5);
    this->enable_pin_->digital_write(true);
  }
  delay(5);

  vl_status = this->init_sensor(i2c_addr);
  if (vl_status != this->VL53L1X_ERROR_NONE) {
    return false;
  }

  if (this->sensor_id() != 0xEACC) {
    return false;
  }
  return true;
}

/**************************************************************************/
/*!
   @brief  Get the sensor ID.
   @returns The sensor ID.
*/
/**************************************************************************/
uint16_t VL53L1XSensor::sensor_id() {
  uint16_t sensor_id = 0;
  vl_status = this->vl53l1x_get_sensor_id(&sensor_id);
  return sensor_id;
}

/**************************************************************************/
/*!
   @brief  Get the distance.
   @returns The distance.
*/
/**************************************************************************/
int16_t VL53L1XSensor::distance() {
  uint16_t distance;

  vl_status = this->vl53l1x_get_distance(&distance);
  if (vl_status != this->VL53L1X_ERROR_NONE) {
    return -1;
  }
  return (int16_t) distance;
}

/**************************************************************************/
/*!
   @brief  Clear the interrupt.
   @returns True if successful, otherwise false.
*/
/**************************************************************************/
bool VL53L1XSensor::clear_interrupt() {
  vl_status = this->vl53l1x_clear_interrupt();
  return (vl_status == this->VL53L1X_ERROR_NONE);
}

/**************************************************************************/
/*!
   @brief  Set the interrupt polarity.
   @param polarity The polarity to set as a boolean.
   @returns True if successful, otherwise false.
*/
/**************************************************************************/
bool VL53L1XSensor::set_int_polarity(bool polarity) {
  vl_status = this->vl53l1x_set_interrupt_polarity(polarity);
  return (vl_status == this->VL53L1X_ERROR_NONE);
}

/**************************************************************************/
/*!
   @brief  Get the interrupt polarity.
   @returns Polarity as a boolean.
*/
/**************************************************************************/
bool VL53L1XSensor::get_int_polarity() {
  uint8_t polarity = 0;
  vl_status = this->vl53l1x_get_interrupt_polarity(&polarity);
  return (bool) polarity;
}

/**************************************************************************/
/*!
   @brief  Start ranging operations.
   @returns True if successful, otherwise false.
*/
/**************************************************************************/
bool VL53L1XSensor::start_ranging() {
  vl_status = this->vl53l1x_start_ranging();
  return (vl_status == this->VL53L1X_ERROR_NONE);
}

/**************************************************************************/
/*!
   @brief  Stop ranging operations.
   @returns True if successful, otherwise false.
*/
/**************************************************************************/
bool VL53L1XSensor::stop_ranging() {
  vl_status = this->vl53l1x_stop_ranging();
  return (vl_status == this->VL53L1X_ERROR_NONE);
}

/**************************************************************************/
/*!
   @brief  Check status of new data.
   @returns True if new data available, otherwise false.
*/
/**************************************************************************/
bool VL53L1XSensor::data_ready() {
  uint8_t data_rdy = 0;
  vl_status = this->vl53l1x_check_for_data_ready(&data_rdy);
  return (bool) data_rdy;
}

/* vl53l1x_api.h functions */

VL53L1X_Error VL53L1XSensor::vl53l1x_set_i2c_address(uint8_t new_address) {
  VL53L1X_Error status = 0;

  status = this->vl53l1x_i2c_write(this->VL53L1X_I2C_SENSOR_DEVICE_ADDRESS, &new_address, 1);
  set_i2c_address(new_address);
  delay(5);
  return status;
}

VL53L1X_Error VL53L1XSensor::vl53l1x_sensor_init() {
  VL53L1X_Error status;
  uint8_t addr, tmp = 0;

  for (addr = 0x2D; addr <= 0x87; addr++) {
    status = this->vl53l1x_wr_byte(addr, VL51L1X_DEFAULT_CONFIGURATION[addr - 0x2D]);
  }

  status = this->vl53l1x_start_ranging();
  while (tmp == 0) {
    status = this->vl53l1x_check_for_data_ready(&tmp);
  }
  tmp = 0;
  status = this->vl53l1x_clear_interrupt();
  status = this->vl53l1x_stop_ranging();
  status = this->vl53l1x_wr_byte(this->VL53L1X_VHV_CONFIG_TIMEOUT_MACROP_LOOP_BOUND, 0x09); /* two bounds VHV */
  status = this->vl53l1x_wr_byte(0x0B, 0); /* start VHV from the previous temperature */
  return status;
}

VL53L1X_Error VL53L1XSensor::vl53l1x_clear_interrupt() {
  VL53L1X_Error status = 0;

  status = this->vl53l1x_wr_byte(this->SYSTEM_INTERRUPT_CLEAR, 0x01);
  return status;
}

VL53L1X_Error VL53L1XSensor::vl53l1x_set_interrupt_polarity(uint8_t int_pol) {
  uint8_t temp;
  VL53L1X_Error status = 0;

  status = this->vl53l1x_rd_byte(this->GPIO_HV_MUX_CTRL, &temp);
  temp = temp & 0xEF;
  status = this->vl53l1x_wr_byte(this->GPIO_HV_MUX_CTRL, temp | (!(int_pol & 1)) << 4);
  return status;
}

VL53L1X_Error VL53L1XSensor::vl53l1x_get_interrupt_polarity(uint8_t *p_int_pol) {
  uint8_t temp;
  VL53L1X_Error status = 0;

  status = this->vl53l1x_rd_byte(this->GPIO_HV_MUX_CTRL, &temp);
  temp = temp & 0x10;
  *p_int_pol = !(temp >> 4);
  return status;
}

VL53L1X_Error VL53L1XSensor::vl53l1x_start_ranging() {
  VL53L1X_Error status = 0;

  status = this->vl53l1x_wr_byte(this->SYSTEM_MODE_START, 0x40); /* Enable VL53L1XSensor */
  return status;
}

VL53L1X_Error VL53L1XSensor::vl53l1x_stop_ranging() {
  VL53L1X_Error status = 0;

  status = this->vl53l1x_wr_byte(this->SYSTEM_MODE_START, 0x00); /* Disable VL53L1XSensor */
  return status;
}

VL53L1X_Error VL53L1XSensor::vl53l1x_check_for_data_ready(uint8_t *is_data_ready) {
  uint8_t temp;
  uint8_t int_pol;
  VL53L1X_Error status = 0;

  status = this->vl53l1x_get_interrupt_polarity(&int_pol);
  status = this->vl53l1x_rd_byte(this->GPIO_TIO_HV_STATUS, &temp);
  /* Read in the register to check if a new value is available */
  if (status == 0) {
    if ((temp & 1) == int_pol) {
      *is_data_ready = 1;
    } else {
      *is_data_ready = 0;
    }
  }
  return status;
}

VL53L1X_Error VL53L1XSensor::vl53l1x_set_timing_budget_in_ms(uint16_t timing_budget_in_ms) {
  uint16_t dist_mode;
  VL53L1X_Error status = 0;

  status = this->vl53l1x_get_distance_mode(&dist_mode);

  if (dist_mode == 0) {
    return 1;
  } else if (dist_mode == 1) {
    /* Short DistanceMode */
    switch (timing_budget_in_ms) {
      case 15: /* only available in short distance mode */
        this->vl53l1x_wr_word(this->RANGE_CONFIG_TIMEOUT_MACROP_A_HI, 0x01D);
        this->vl53l1x_wr_word(this->RANGE_CONFIG_TIMEOUT_MACROP_B_HI, 0x0027);
        break;
      case 20:
        this->vl53l1x_wr_word(this->RANGE_CONFIG_TIMEOUT_MACROP_A_HI, 0x0051);
        this->vl53l1x_wr_word(this->RANGE_CONFIG_TIMEOUT_MACROP_B_HI, 0x006E);
        break;
      case 33:
        this->vl53l1x_wr_word(this->RANGE_CONFIG_TIMEOUT_MACROP_A_HI, 0x00D6);
        this->vl53l1x_wr_word(this->RANGE_CONFIG_TIMEOUT_MACROP_B_HI, 0x006E);
        break;
      case 50:
        this->vl53l1x_wr_word(this->RANGE_CONFIG_TIMEOUT_MACROP_A_HI, 0x1AE);
        this->vl53l1x_wr_word(this->RANGE_CONFIG_TIMEOUT_MACROP_B_HI, 0x01E8);
        break;
      case 100:
        this->vl53l1x_wr_word(this->RANGE_CONFIG_TIMEOUT_MACROP_A_HI, 0x02E1);
        this->vl53l1x_wr_word(this->RANGE_CONFIG_TIMEOUT_MACROP_B_HI, 0x0388);
        break;
      case 200:
        this->vl53l1x_wr_word(this->RANGE_CONFIG_TIMEOUT_MACROP_A_HI, 0x03E1);
        this->vl53l1x_wr_word(this->RANGE_CONFIG_TIMEOUT_MACROP_B_HI, 0x0496);
        break;
      case 500:
        this->vl53l1x_wr_word(this->RANGE_CONFIG_TIMEOUT_MACROP_A_HI, 0x0591);
        this->vl53l1x_wr_word(this->RANGE_CONFIG_TIMEOUT_MACROP_B_HI, 0x05C1);
        break;
      default:
        status = 1;
        break;
    }
  } else {
    switch (timing_budget_in_ms) {
      case 20:
        this->vl53l1x_wr_word(this->RANGE_CONFIG_TIMEOUT_MACROP_A_HI, 0x001E);
        this->vl53l1x_wr_word(this->RANGE_CONFIG_TIMEOUT_MACROP_B_HI, 0x0022);
        break;
      case 33:
        this->vl53l1x_wr_word(this->RANGE_CONFIG_TIMEOUT_MACROP_A_HI, 0x0060);
        this->vl53l1x_wr_word(this->RANGE_CONFIG_TIMEOUT_MACROP_B_HI, 0x006E);
        break;
      case 50:
        this->vl53l1x_wr_word(this->RANGE_CONFIG_TIMEOUT_MACROP_A_HI, 0x00AD);
        this->vl53l1x_wr_word(this->RANGE_CONFIG_TIMEOUT_MACROP_B_HI, 0x00C6);
        break;
      case 100:
        this->vl53l1x_wr_word(this->RANGE_CONFIG_TIMEOUT_MACROP_A_HI, 0x01CC);
        this->vl53l1x_wr_word(this->RANGE_CONFIG_TIMEOUT_MACROP_B_HI, 0x01EA);
        break;
      case 200:
        this->vl53l1x_wr_word(this->RANGE_CONFIG_TIMEOUT_MACROP_A_HI, 0x02D9);
        this->vl53l1x_wr_word(this->RANGE_CONFIG_TIMEOUT_MACROP_B_HI, 0x02F8);
        break;
      case 500:
        this->vl53l1x_wr_word(this->RANGE_CONFIG_TIMEOUT_MACROP_A_HI, 0x048F);
        this->vl53l1x_wr_word(this->RANGE_CONFIG_TIMEOUT_MACROP_B_HI, 0x04A4);
        break;
      default:
        status = 1;
        break;
    }
  }
  return status;
}

VL53L1X_Error VL53L1XSensor::vl53l1x_get_timing_budget_in_ms(uint16_t *p_timing_budget_in_ms) {
  uint16_t temp;
  VL53L1X_Error status = 0;

  status = this->vl53l1x_rd_word(this->RANGE_CONFIG_TIMEOUT_MACROP_A_HI, &temp);
  switch (temp) {
    case 0x001D:
      *p_timing_budget_in_ms = 15;
      break;
    case 0x0051:
    case 0x001E:
      *p_timing_budget_in_ms = 20;
      break;
    case 0x00D6:
    case 0x0060:
      *p_timing_budget_in_ms = 33;
      break;
    case 0x1AE:
    case 0x00AD:
      *p_timing_budget_in_ms = 50;
      break;
    case 0x02E1:
    case 0x01CC:
      *p_timing_budget_in_ms = 100;
      break;
    case 0x03E1:
    case 0x02D9:
      *p_timing_budget_in_ms = 200;
      break;
    case 0x0591:
    case 0x048F:
      *p_timing_budget_in_ms = 500;
      break;
    default:
      *p_timing_budget_in_ms = 0;
      break;
  }
  return status;
}

VL53L1X_Error VL53L1XSensor::vl53l1x_set_distance_mode(uint16_t dist_mode) {
  uint16_t timing_budget;
  VL53L1X_Error status = 0;

  status = this->vl53l1x_get_timing_budget_in_ms(&timing_budget);

  switch (dist_mode) {
    case 1:
      status = this->vl53l1x_wr_byte(this->PHASECAL_CONFIG_TIMEOUT_MACROP, 0x14);
      status = this->vl53l1x_wr_byte(this->RANGE_CONFIG_VCSEL_PERIOD_A, 0x07);
      status = this->vl53l1x_wr_byte(this->RANGE_CONFIG_VCSEL_PERIOD_B, 0x05);
      status = this->vl53l1x_wr_byte(this->RANGE_CONFIG_VALID_PHASE_HIGH, 0x38);
      status = this->vl53l1x_wr_word(this->SD_CONFIG_WOI_SD0, 0x0705);
      status = this->vl53l1x_wr_word(this->SD_CONFIG_INITIAL_PHASE_SD0, 0x0606);
      break;
    case 2:
      status = this->vl53l1x_wr_byte(this->PHASECAL_CONFIG_TIMEOUT_MACROP, 0x0A);
      status = this->vl53l1x_wr_byte(this->RANGE_CONFIG_VCSEL_PERIOD_A, 0x0F);
      status = this->vl53l1x_wr_byte(this->RANGE_CONFIG_VCSEL_PERIOD_B, 0x0D);
      status = this->vl53l1x_wr_byte(this->RANGE_CONFIG_VALID_PHASE_HIGH, 0xB8);
      status = this->vl53l1x_wr_word(this->SD_CONFIG_WOI_SD0, 0x0F0D);
      status = this->vl53l1x_wr_word(this->SD_CONFIG_INITIAL_PHASE_SD0, 0x0E0E);
      break;
    default:
      break;
  }
  status = this->vl53l1x_set_timing_budget_in_ms(timing_budget);
  return status;
}

VL53L1X_Error VL53L1XSensor::vl53l1x_get_distance_mode(uint16_t *dist_mode) {
  uint8_t temp_dm, status = 0;

  status = this->vl53l1x_rd_byte(this->PHASECAL_CONFIG_TIMEOUT_MACROP, &temp_dm);
  if (temp_dm == 0x14)
    *dist_mode = 1;
  if (temp_dm == 0x0A)
    *dist_mode = 2;
  return status;
}

VL53L1X_Error VL53L1XSensor::vl53l1x_set_inter_measurement_in_ms(uint16_t inter_measurement_in_ms) {
  uint16_t clock_pll;
  VL53L1X_Error status = 0;

  status = this->vl53l1x_rd_word(this->VL53L1X_RESULT_OSC_CALIBRATE_VAL, &clock_pll);
  clock_pll = clock_pll & 0x3FF;
  this->vl53l1x_wr_dword(this->VL53L1X_SYSTEM_INTERMEASUREMENT_PERIOD,
                         (uint32_t)(clock_pll * inter_measurement_in_ms * 1.075));
  return status;
}

VL53L1X_Error VL53L1XSensor::vl53l1x_get_inter_measurement_in_ms(uint16_t *p_im) {
  uint16_t clock_pll;
  VL53L1X_Error status = 0;
  uint32_t tmp;

  status = this->vl53l1x_rd_dword(this->VL53L1X_SYSTEM_INTERMEASUREMENT_PERIOD, &tmp);
  *p_im = (uint16_t) tmp;
  status = this->vl53l1x_rd_word(this->VL53L1X_RESULT_OSC_CALIBRATE_VAL, &clock_pll);
  clock_pll = clock_pll & 0x3FF;
  *p_im = (uint16_t)(*p_im / (clock_pll * 1.065));
  return status;
}

VL53L1X_Error VL53L1XSensor::vl53l1x_boot_state(uint8_t *state) {
  VL53L1X_Error status = 0;
  uint8_t tmp = 0;

  status = this->vl53l1x_rd_byte(this->VL53L1X_FIRMWARE_SYSTEM_STATUS, &tmp);
  *state = tmp;
  return status;
}

VL53L1X_Error VL53L1XSensor::vl53l1x_get_sensor_id(uint16_t *sensor_id) {
  VL53L1X_Error status = 0;
  uint16_t tmp = 0;

  status = this->vl53l1x_rd_word(this->VL53L1X_IDENTIFICATION_MODEL_ID, &tmp);
  *sensor_id = tmp;
  return status;
}

VL53L1X_Error VL53L1XSensor::vl53l1x_get_distance(uint16_t *distance) {
  VL53L1X_Error status = 0;
  uint16_t tmp = 0;

  status = (this->vl53l1x_rd_word(this->VL53L1X_RESULT_FINAL_CROSSTALK_CORRECTED_RANGE_MM_SD0, &tmp));
  *distance = tmp;
  return status;
}

VL53L1X_Error VL53L1XSensor::vl53l1x_get_signal_per_spad(uint16_t *signal_per_sp) {
  VL53L1X_Error status = 0;
  uint16_t sp_nb = 1, signal;

  status = this->vl53l1x_rd_word(this->VL53L1X_RESULT_PEAK_SIGNAL_COUNT_RATE_CROSSTALK_CORRECTED_MCPS_SD0, &signal);
  status = this->vl53l1x_rd_word(this->VL53L1X_RESULT_DSS_ACTUAL_EFFECTIVE_SPADS_SD0, &sp_nb);
  *signal_per_sp = (uint16_t)(2000.0 * signal / sp_nb);
  return status;
}

VL53L1X_Error VL53L1XSensor::vl53l1x_get_ambient_per_spad(uint16_t *amb) {
  VL53L1X_Error status = 0;
  uint16_t ambient_rate, sp_nb = 1;

  status = this->vl53l1x_rd_word(this->RESULT_AMBIENT_COUNT_RATE_MCPS_SD, &ambient_rate);
  status = this->vl53l1x_rd_word(this->VL53L1X_RESULT_DSS_ACTUAL_EFFECTIVE_SPADS_SD0, &sp_nb);
  *amb = (uint16_t)(2000.0 * ambient_rate / sp_nb);
  return status;
}

VL53L1X_Error VL53L1XSensor::vl53l1x_get_signal_rate(uint16_t *signal_rate) {
  VL53L1X_Error status = 0;
  uint16_t tmp;

  status = this->vl53l1x_rd_word(this->VL53L1X_RESULT_PEAK_SIGNAL_COUNT_RATE_CROSSTALK_CORRECTED_MCPS_SD0, &tmp);
  *signal_rate = tmp * 8;
  return status;
}

VL53L1X_Error VL53L1XSensor::vl53l1x_get_spad_nb(uint16_t *sp_nb) {
  VL53L1X_Error status = 0;
  uint16_t tmp;

  status = this->vl53l1x_rd_word(this->VL53L1X_RESULT_DSS_ACTUAL_EFFECTIVE_SPADS_SD0, &tmp);
  *sp_nb = tmp >> 8;
  return status;
}

VL53L1X_Error VL53L1XSensor::vl53l1x_get_ambient_rate(uint16_t *amb_rate) {
  VL53L1X_Error status = 0;
  uint16_t tmp;

  status = this->vl53l1x_rd_word(this->RESULT_AMBIENT_COUNT_RATE_MCPS_SD, &tmp);
  *amb_rate = tmp * 8;
  return status;
}

VL53L1X_Error VL53L1XSensor::vl53l1x_get_range_status(uint8_t *range_status) {
  VL53L1X_Error status = 0;
  uint8_t rg_st;

  status = this->vl53l1x_rd_byte(this->VL53L1X_RESULT_RANGE_STATUS, &rg_st);
  rg_st = rg_st & 0x1F;
  switch (rg_st) {
    case 9:
      rg_st = 0;
      break;
    case 6:
      rg_st = 1;
      break;
    case 4:
      rg_st = 2;
      break;
    case 8:
      rg_st = 3;
      break;
    case 5:
      rg_st = 4;
      break;
    case 3:
      rg_st = 5;
      break;
    case 19:
      rg_st = 6;
      break;
    case 7:
      rg_st = 7;
      break;
    case 12:
      rg_st = 9;
      break;
    case 18:
      rg_st = 10;
      break;
    case 22:
      rg_st = 11;
      break;
    case 23:
      rg_st = 12;
      break;
    case 13:
      rg_st = 13;
      break;
    default:
      rg_st = 255;
      break;
  }
  *range_status = rg_st;
  return status;
}

VL53L1X_Error VL53L1XSensor::vl53l1x_set_offset(int16_t offset_value) {
  VL53L1X_Error status = 0;
  int16_t temp;

  temp = (offset_value * 4);
  this->vl53l1x_wr_word(this->ALGO_PART_TO_PART_RANGE_OFFSET_MM, (uint16_t) temp);
  this->vl53l1x_wr_word(this->MM_CONFIG_INNER_OFFSET_MM, 0x0);
  this->vl53l1x_wr_word(this->MM_CONFIG_OUTER_OFFSET_MM, 0x0);
  return status;
}

VL53L1X_Error VL53L1XSensor::vl53l1x_get_offset(int16_t *offset) {
  VL53L1X_Error status = 0;
  uint16_t temp;

  status = this->vl53l1x_rd_word(this->ALGO_PART_TO_PART_RANGE_OFFSET_MM, &temp);
  temp = temp << 3;
  *offset = (int16_t)(temp);
  *offset = *offset / 32;
  return status;
}

VL53L1X_Error VL53L1XSensor::vl53l1x_set_x_talk(uint16_t xtalk_value) {
  /* XTalkValue in count per second to avoid float type */
  VL53L1X_Error status = 0;

  status = this->vl53l1x_wr_word(this->ALGO_CROSSTALK_COMPENSATION_X_PLANE_GRADIENT_KCPS, 0x0000);
  status = this->vl53l1x_wr_word(this->ALGO_CROSSTALK_COMPENSATION_Y_PLANE_GRADIENT_KCPS, 0x0000);
  status = this->vl53l1x_wr_word(this->ALGO_CROSSTALK_COMPENSATION_PLANE_OFFSET_KCPS,
                                 (xtalk_value << 9) / 1000); /* * << 9 (7.9 format) and /1000 to convert cps to kpcs */
  return status;
}

VL53L1X_Error VL53L1XSensor::vl53l1x_get_x_talk(uint16_t *xtalk) {
  VL53L1X_Error status = 0;
  uint16_t tmp;

  status = this->vl53l1x_rd_word(this->ALGO_CROSSTALK_COMPENSATION_PLANE_OFFSET_KCPS, &tmp);
  *xtalk = (tmp * 1000) >> 9; /* * 1000 to convert kcps to cps and >> 9 (7.9 format) */
  return status;
}

VL53L1X_Error VL53L1XSensor::vl53l1x_set_distance_threshold(uint16_t thresh_low, uint16_t thresh_high, uint8_t window,
                                                            uint8_t int_on_no_target) {
  VL53L1X_Error status = 0;
  uint8_t temp = 0;

  status = this->vl53l1x_rd_byte(this->SYSTEM_INTERRUPT_CONFIG_GPIO, &temp);
  temp = temp & 0x47;
  if (int_on_no_target == 0) {
    status = this->vl53l1x_wr_byte(this->SYSTEM_INTERRUPT_CONFIG_GPIO, (temp | (window & 0x07)));
  } else {
    status = this->vl53l1x_wr_byte(SYSTEM_INTERRUPT_CONFIG_GPIO, ((temp | (window & 0x07)) | 0x40));
  }
  status = this->vl53l1x_wr_word(this->SYSTEM_THRESH_HIGH, thresh_high);
  status = this->vl53l1x_wr_word(this->SYSTEM_THRESH_LOW, thresh_low);
  return status;
}

VL53L1X_Error VL53L1XSensor::vl53l1x_get_distance_threshold_window(uint16_t *window) {
  VL53L1X_Error status = 0;
  uint8_t tmp;
  status = this->vl53l1x_rd_byte(this->SYSTEM_INTERRUPT_CONFIG_GPIO, &tmp);
  *window = (uint16_t)(tmp & 0x7);
  return status;
}

VL53L1X_Error VL53L1XSensor::vl53l1x_get_distance_threshold_low(uint16_t *low) {
  VL53L1X_Error status = 0;
  uint16_t tmp;

  status = this->vl53l1x_rd_word(this->SYSTEM_THRESH_LOW, &tmp);
  *low = tmp;
  return status;
}

VL53L1X_Error VL53L1XSensor::vl53l1x_get_distance_threshold_high(uint16_t *high) {
  VL53L1X_Error status = 0;
  uint16_t tmp;

  status = this->vl53l1x_rd_word(this->SYSTEM_THRESH_HIGH, &tmp);
  *high = tmp;
  return status;
}

VL53L1X_Error VL53L1XSensor::vl53l1x_set_roi(uint16_t roi_x, uint16_t roi_y) {
  uint8_t optical_center;
  VL53L1X_Error status = 0;

  status = this->vl53l1x_rd_byte(this->VL53L1X_ROI_CONFIG_MODE_ROI_CENTRE_SPAD, &optical_center);
  if (roi_x > 16) {
    roi_x = 16;
  }
  if (roi_y > 16) {
    roi_y = 16;
  }
  if (roi_x > 10 || roi_y > 10) {
    optical_center = 199;
  }
  status = this->vl53l1x_wr_byte(this->ROI_CONFIG_USER_ROI_CENTRE_SPAD, optical_center);
  status = this->vl53l1x_wr_byte(this->ROI_CONFIG_USER_ROI_REQUESTED_GLOBAL_XY_SIZE, (roi_y - 1) << 4 | (roi_x - 1));
  return status;
}

VL53L1X_Error VL53L1XSensor::vl53l1x_get_roi_xy(uint16_t *roi_x, uint16_t *roi_y) {
  VL53L1X_Error status = 0;
  uint8_t tmp;

  status = this->vl53l1x_rd_byte(this->ROI_CONFIG_USER_ROI_REQUESTED_GLOBAL_XY_SIZE, &tmp);
  *roi_x = ((uint16_t) tmp & 0x0F) + 1;
  *roi_y = (((uint16_t) tmp & 0xF0) >> 4) + 1;
  return status;
}

VL53L1X_Error VL53L1XSensor::vl53l1x_set_roi_center(uint8_t roi_center) {
  VL53L1X_Error status = 0;
  status = this->vl53l1x_wr_byte(this->ROI_CONFIG_USER_ROI_CENTRE_SPAD, roi_center);
  return status;
}

VL53L1X_Error VL53L1XSensor::vl53l1x_get_roi_center(uint8_t *roi_center) {
  VL53L1X_Error status = 0;
  uint8_t tmp;
  status = this->vl53l1x_rd_byte(this->ROI_CONFIG_USER_ROI_CENTRE_SPAD, &tmp);
  *roi_center = tmp;
  return status;
}

VL53L1X_Error VL53L1XSensor::vl53l1x_set_signal_threshold(uint16_t signal) {
  VL53L1X_Error status = 0;

  this->vl53l1x_wr_word(this->RANGE_CONFIG_MIN_COUNT_RATE_RTN_LIMIT_MCPS, signal >> 3);
  return status;
}

VL53L1X_Error VL53L1XSensor::vl53l1x_get_signal_threshold(uint16_t *signal) {
  VL53L1X_Error status = 0;
  uint16_t tmp;

  status = this->vl53l1x_rd_word(this->RANGE_CONFIG_MIN_COUNT_RATE_RTN_LIMIT_MCPS, &tmp);
  *signal = tmp << 3;
  return status;
}

VL53L1X_Error VL53L1XSensor::vl53l1x_set_sigma_threshold(uint16_t sigma) {
  VL53L1X_Error status = 0;

  if (sigma > (0xFFFF >> 2)) {
    return 1;
  }
  /* 16 bits register 14.2 format */
  status = this->vl53l1x_wr_word(this->RANGE_CONFIG_SIGMA_THRESH, sigma << 2);
  return status;
}

VL53L1X_Error VL53L1XSensor::vl53l1x_get_sigma_threshold(uint16_t *signal) {
  VL53L1X_Error status = 0;
  uint16_t tmp;

  status = this->vl53l1x_rd_word(this->RANGE_CONFIG_SIGMA_THRESH, &tmp);
  *signal = tmp >> 2;
  return status;
}

VL53L1X_Error VL53L1XSensor::vl53l1x_start_temperature_update() {
  VL53L1X_Error status = 0;
  uint8_t tmp = 0;

  status = this->vl53l1x_wr_byte(this->VL53L1X_VHV_CONFIG_TIMEOUT_MACROP_LOOP_BOUND, 0x81); /* full VHV */
  status = this->vl53l1x_wr_byte(0x0B, 0x92);
  status = this->vl53l1x_start_ranging();
  while (tmp == 0) {
    status = vl53l1x_check_for_data_ready(&tmp);
  }
  tmp = 0;
  status = this->vl53l1x_clear_interrupt();
  status = this->vl53l1x_stop_ranging();
  status = this->vl53l1x_wr_byte(this->VL53L1X_VHV_CONFIG_TIMEOUT_MACROP_LOOP_BOUND, 0x09); /* two bounds VHV */
  status = this->vl53l1x_wr_byte(0x0B, 0); /* start VHV from the previous temperature */
  return status;
}

/* vl53l1x_calibration.h functions */

int8_t VL53L1XSensor::vl53l1x_calibrate_offset(uint16_t target_dist_in_mm, int16_t *offset) {
  uint8_t cnt = 0, tmp;
  int16_t average_distance = 0;
  uint16_t distance;
  VL53L1X_Error status = 0;

  status = this->vl53l1x_wr_word(this->ALGO_PART_TO_PART_RANGE_OFFSET_MM, 0x0);
  status = this->vl53l1x_wr_word(this->MM_CONFIG_INNER_OFFSET_MM, 0x0);
  status = this->vl53l1x_wr_word(this->MM_CONFIG_OUTER_OFFSET_MM, 0x0);
  status = this->vl53l1x_start_ranging(); /* Enable VL53L1X sensor */
  for (cnt = 0; cnt < 50; cnt++) {
    while (tmp == 0) {
      status = this->vl53l1x_check_for_data_ready(&tmp);
    }
    tmp = 0;
    status = this->vl53l1x_get_distance(&distance);
    status = this->vl53l1x_clear_interrupt();
    average_distance = average_distance + distance;
  }
  status = this->vl53l1x_stop_ranging();
  average_distance = average_distance / 50;
  *offset = target_dist_in_mm - average_distance;
  status = this->vl53l1x_wr_word(this->ALGO_PART_TO_PART_RANGE_OFFSET_MM, *offset * 4);
  return status;
}

int8_t VL53L1XSensor::vl53l1x_calibrate_x_talk(uint16_t target_dist_in_mm, uint16_t *xtalk) {
  uint8_t cnt, tmp = 0;
  float average_signal_rate = 0;
  float average_distance = 0;
  float average_spad_nb = 0;
  uint16_t distance = 0, spad_num;
  uint16_t signal_rate;
  VL53L1X_Error status = 0;

  status = this->vl53l1x_wr_word(0x0016, 0);
  status = this->vl53l1x_start_ranging();
  for (cnt = 0; cnt < 50; cnt++) {
    while (tmp == 0) {
      status = this->vl53l1x_check_for_data_ready(&tmp);
    }
    tmp = 0;
    status = this->vl53l1x_get_signal_rate(&signal_rate);
    status = this->vl53l1x_get_distance(&distance);
    status = this->vl53l1x_clear_interrupt();
    average_distance = average_distance + distance;
    status = this->vl53l1x_get_spad_nb(&spad_num);
    average_spad_nb = average_spad_nb + spad_num;
    average_signal_rate = average_signal_rate + signal_rate;
  }
  status = this->vl53l1x_stop_ranging();
  average_distance = average_distance / 50;
  average_spad_nb = average_spad_nb / 50;
  average_signal_rate = average_signal_rate / 50;
  /* Calculate Xtalk value */
  *xtalk = (uint16_t)(512 * (average_signal_rate * (1 - (average_distance / target_dist_in_mm))) / average_spad_nb);
  status = this->vl53l1x_wr_word(0x0016, *xtalk);
  return status;
}

/* Write and read functions from I2C */

VL53L1X_Error VL53L1XSensor::vl53l1x_write_multi(uint16_t index, uint8_t *pdata, uint32_t count) {
  int status;

  status = this->vl53l1x_i2c_write(index, pdata, (uint16_t) count);
  return status;
}

VL53L1X_Error VL53L1XSensor::vl53l1x_read_multi(uint16_t index, uint8_t *pdata, uint32_t count) {
  int status;

  status = this->vl53l1x_i2c_read(index, pdata, (uint16_t) count);
  return status;
}

VL53L1X_Error VL53L1XSensor::vl53l1x_wr_byte(uint16_t index, uint8_t data) {
  return this->vl53l1x_i2c_write(index, &data, 1);
}

VL53L1X_Error VL53L1XSensor::vl53l1x_wr_word(uint16_t index, uint16_t data) {
  uint8_t buffer[2];

  buffer[0] = data >> 8;
  buffer[1] = data & 0x00FF;
  return this->vl53l1x_i2c_write(index, buffer, 2);
}

VL53L1X_Error VL53L1XSensor::vl53l1x_wr_dword(uint16_t index, uint32_t data) {
  uint8_t buffer[4];

  buffer[0] = (data >> 24) & 0xFF;
  buffer[1] = (data >> 16) & 0xFF;
  buffer[2] = (data >> 8) & 0xFF;
  buffer[3] = (data >> 0) & 0xFF;
  return this->vl53l1x_i2c_write(index, buffer, 4);
}

VL53L1X_Error VL53L1XSensor::vl53l1x_rd_byte(uint16_t index, uint8_t *data) {
  int status;

  status = this->vl53l1x_i2c_read(index, data, 1);
  if (status)
    return -1;
  return 0;
}

VL53L1X_Error VL53L1XSensor::vl53l1x_rd_word(uint16_t index, uint16_t *data) {
  VL53L1X_Error status;
  uint8_t buffer[2] = {0, 0};

  status = this->vl53l1x_i2c_read(index, buffer, 2);
  if (!status) {
    *data = (buffer[0] << 8) + buffer[1];
  }
  return status;
}

VL53L1X_Error VL53L1XSensor::vl53l1x_rd_dword(uint16_t index, uint32_t *data) {
  VL53L1X_Error status;
  uint8_t buffer[4] = {0, 0, 0, 0};

  status = this->vl53l1x_i2c_read(index, buffer, 4);
  if (!status) {
    *data = ((uint32_t) buffer[0] << 24) + ((uint32_t) buffer[1] << 16) + ((uint32_t) buffer[2] << 8) +
            (uint32_t) buffer[3];
  }
  return status;
}

VL53L1X_Error VL53L1XSensor::vl53l1x_update_byte(uint16_t index, uint8_t and_data, uint8_t or_data) {
  VL53L1X_Error status;
  uint8_t buffer = 0;

  /* read data direct onto buffer */
  status = this->vl53l1x_i2c_read(index, &buffer, 1);
  if (!status) {
    buffer = (buffer & and_data) | or_data;
    status = this->vl53l1x_i2c_write(index, &buffer, (uint16_t) 1);
  }
  return status;
}

VL53L1X_Error VL53L1XSensor::vl53l1x_i2c_write(uint16_t index, const uint8_t *data, uint16_t number_of_bytes) {
  uint8_t buffer[2 + number_of_bytes];
  uint16_t idx;
  buffer[0] = (uint8_t)(index >> 8);
  buffer[1] = (uint8_t)(index & 0xFF);
  for (idx = 0; idx < number_of_bytes; idx++) {
    buffer[idx + 2] = data[idx];
  }
  this->write(buffer, 2 + number_of_bytes, true);

  return 0;
}

VL53L1X_Error VL53L1XSensor::vl53l1x_i2c_read(uint16_t index, uint8_t *data, uint16_t number_of_bytes) {
  VL53L1X_Error status = 0;

  // Loop until the port is transmitted correctly
  do {
    uint8_t buffer[2];
    buffer[0] = (uint8_t)(index >> 8);
    buffer[1] = (uint8_t)(index & 0xFF);
    status = this->write(buffer, 2, false);
  } while (status != 0);

  status = this->read(data, number_of_bytes);
  return status;
}

VL53L1X_Error VL53L1XSensor::vl53l1x_get_tick_count(uint32_t *ptick_count_ms) {
  /* Returns current tick count in [ms] */

  VL53L1X_Error status = this->VL53L1X_ERROR_NONE;

  //*ptick_count_ms = timeGetTime();
  *ptick_count_ms = 0;

  return status;
}

VL53L1X_Error VL53L1XSensor::vl53l1x_wait_us(int32_t wait_us) {
  delay(wait_us / 1000);
  return this->VL53L1X_ERROR_NONE;
}

VL53L1X_Error VL53L1XSensor::vl53l1x_wait_ms(int32_t wait_ms) {
  delay(wait_ms);
  return this->VL53L1X_ERROR_NONE;
}

VL53L1X_Error VL53L1XSensor::vl53l1x_wait_value_mask_ex(uint32_t timeout_ms, uint16_t index, uint8_t value,
                                                        uint8_t mask, uint32_t poll_delay_ms) {
  VL53L1X_Error status = this->VL53L1X_ERROR_NONE;
  uint32_t start_time_ms = 0;
  uint32_t current_time_ms = 0;
  uint32_t polling_time_ms = 0;
  uint8_t byte_value = 0;
  uint8_t found = 0;

  /* calculate time limit in absolute time */
  this->vl53l1x_get_tick_count(&start_time_ms);

  /* wait until value is found, timeout reached on error occurred */
  while ((status == this->VL53L1X_ERROR_NONE) && (polling_time_ms < timeout_ms) && (found == 0)) {
    if (status == this->VL53L1X_ERROR_NONE) {
      status = this->vl53l1x_rd_byte(index, &byte_value);
    }
    if ((byte_value & mask) == value) {
      found = 1;
    }
    if (status == this->VL53L1X_ERROR_NONE && found == 0 && poll_delay_ms > 0) {
      status = this->vl53l1x_wait_ms(poll_delay_ms);
    }

    /* Update polling time (Compare difference rather than absolute to
    negate 32bit wrap around issue) */
    this->vl53l1x_get_tick_count(&current_time_ms);
    polling_time_ms = current_time_ms - start_time_ms;
  }

  if (found == 0 && status == this->VL53L1X_ERROR_NONE)
    status = this->VL53L1X_ERROR_TIME_OUT;

  return status;
}

}  // namespace vl53l1x
}  // namespace esphome
