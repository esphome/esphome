#include "esphome/core/log.h"
#include "vl53l1x_sensor.h"

namespace esphome {
namespace vl53l1x_distance {

static const char *TAG = "vl53l1x";

void VL53L1XSensor::dump_config() {
  LOG_SENSOR("", "VL53L1X", this);
  LOG_UPDATE_INTERVAL(this);
  LOG_I2C_DEVICE(this);
}

void VL53L1XSensor::setup() {
  ESP_LOGCONFIG(TAG, "Setting up VL53L1X...");
  // begin and log if not connected
  if (begin() != 0) {
    ESP_LOGW(TAG, "'%s' - failed to begin. Please check wiring.", this->name_.c_str());
  }

  set_distance_mode(static_cast<DistanceMode>(distance_mode_));
  switch (distance_mode_) {
    case DistanceMode::SHORT:
      if (timing_budget_ < 20)
        timing_budget_ = 20;
      break;

    case DistanceMode::MEDIUM:
    case DistanceMode::LONG:
      if (timing_budget_ < 33)
        timing_budget_ = 33;
      break;
  }
  apply_timing_budget_in_ms(timing_budget_);
}

void VL53L1XSensor::update() {
  // initiate single shot measurement]
  uint16_t distance_mm;

  start_ranging();  // Write configuration bytes to initiate measurement
  if (check_for_data_ready() == 0) {
    distance_mm = get_distance();  // Get the result of the measurement from the sensor
    // VL53L1X_clearInterrupt();
    stop_ranging();
    float distance_m = static_cast<float>(distance_mm) / 1000.0;
    ESP_LOGD(TAG, "'%s' - Got distance %.3f m", this->name_.c_str(), distance_m);
    this->publish_state(distance_m);
  }
}

void VL53L1XSensor::loop() {
  uint16_t distance_mm;

  start_ranging();  // Write configuration bytes to initiate measurement
  if (check_for_data_ready() == 0) {
    distance_mm = get_distance();  // Get the result of the measurement from the sensor
    // VL53L1X_clearInterrupt();
    stop_ranging();
    float distance_m = static_cast<float>(distance_mm) / 1000.0;
    ESP_LOGD(TAG, "'%s' - Got distance %.3f m", this->name_.c_str(), distance_m);
    this->publish_state(distance_m);
  } else {
    ESP_LOGW(TAG, "'%s' - There is an error data not ready.", this->name_.c_str());
    this->publish_state(NAN);
  }

  /*   TO DO */
  //   ADD RETRY WHEN FAILED TO TAKE MEASUREMENT

  //   Presence zone smartness

  //   Already some presence zone smartness
  //   VL53L1X_set_ROI(ROI_height, ROI_width, center[Zone]);  // first value: height of the zone, second value: width of
  //   the zone delayMicroseconds(50); apply_timing_budget_in_ms(50); // Could this be set into setup as well?
  //   VL53L1X_start_ranging(); //Write configuration bytes to initiate measurement
  //   distance_mm = VL53L1X_getDistance(); //Get the result of the measurement from the sensor
  //   VL53L1X_stop_ranging();
}
// VL53L1X_

bool VL53L1XSensor::begin() {
  if (check_id() == false)
    ESP_LOGW(TAG, VL53L1_ERROR_PLATFORM_SPECIFIC_START);

  // return _device->VL53L1X_SensorInit();

  int8_t status = 0;
  uint8_t Addr = 0x00, dataReady = 0, timeout = 0;

  // write the default configuration
  for (Addr = 0x2D; Addr <= 0x87; Addr++) {
    this->write_byte(Addr, VL51L1X_DEFAULT_CONFIGURATION[Addr - 0x2D]);
  }
  start_ranging();

  // We need to wait at least the default intermeasurement period of 103ms before dataready will occur
  // But if a unit has already been powered and polling, it may happen much faster
  while (data_ready == 0) {
    status = check_for_data_ready(&data_ready);
    if (timeout++ > 150)
      ESP_LOGW(TAG, VL53L1_ERROR_TIME_OUT);
    delay(1);
  }
  // status = VL53L1_WrByte(Device, SYSTEM__MODE_START, 0x00); /* Disable VL53L1X */

  stop_ranging();
  status = this->write_byte(VL53L1_VHV_CONFIG__TIMEOUT_MACROP_LOOP_BOUND, 0x09); /* two bounds VHV */
  status = this->write_byte(0x0B, 0); /* start VHV from the previous temperature */
  return false;
}

/*Checks the ID of the device, returns true if ID is correct*/

bool VL53L1XSensor::check_id() {
  uint16_t sensorId;
  uint16_t tmp = 0;

  this->read_byte_16(VL53L1_IDENTIFICATION__MODEL_ID, &tmp);
  sensorId = tmp;

  if (sensorId == 0xEACC)
    return true;
  return false;
}

uint16_t VL53L1XSensor::get_distance() {
  uint16_t distance;
  // _device->VL53L1X_GetDistance(&distance);
  VL53L1X_ERROR status = 0;
  uint16_t tmp;

  status = this->read_byte_16(VL53L1_RESULT__FINAL_CROSSTALK_CORRECTED_RANGE_MM_SD0, &tmp);

  distance = tmp;
  // return status;
  return (int) distance;
}

void VL53L1XSensor::start_ranging() {
  // _device->VL53L1X_StartRanging();
  int8_t status = 0;

  this->write_byte(SYSTEM__MODE_START, 0x40); /* Enable VL53L1X */
}

void VL53L1XSensor::stop_ranging() {
  // _device->VL53L1X_StopRanging();

  int8_t status = 0;

  this->write_byte(SYSTEM__MODE_START, 0x00); /* Disable VL53L1X */
}

bool VL53L1XSensor::check_for_data_ready(uint8_t *is_data_ready) {
  uint8_t data_ready;
  uint8_t IntPol;
  VL53L1X_ERROR status = 0;

  // translated from the library VL53L1X_GetInterruptPolarity();
  uint8_t Temp;

  status = this->read_byte(GPIO_HV_MUX__CTRL, &Temp);
  Temp = Temp & 0x10;
  IntPol = !(Temp >> 4);

  uint8_t Temp = 0;
  status = this->read_byte(GPIO__TIO_HV_STATUS, &Temp);
  if (status == 0) {
    if ((Temp & 1) == IntPol)
      is_data_ready = 1;
    else
      is_data_ready = 0;
  }
  data_ready = is_data_ready;

  return (bool) data_ready;
}

int8_t VL53L1XSensor::apply_timing_budget_in_ms(uint16_t timing_budget_in_ms) {
  // _device->VL53L1X_SetTimingBudgetInMs(timingBudget);
  int8_t status = 0;
  uint16_t DM;

  // status = VL53L1X_GetDistanceMode(&DM);
  uint8_t TempDM = 0;

  status = this->read_byte(PHASECAL_CONFIG__TIMEOUT_MACROP, &TempDM);
  if (TempDM == 0x14)
    DM = 1;
  if (TempDM == 0x0A)
    DM = 2;

  if (DM == 0)
    return 1;
  else if (DM == 1) { /* Short DistanceMode */
    switch (timing_budget_in_ms) {
      case 15: /* only available in short distance mode */
        this->write_byte_16(RANGE_CONFIG__TIMEOUT_MACROP_A_HI, 0x01D);
        this->write_byte_16(RANGE_CONFIG__TIMEOUT_MACROP_B_HI, 0x0027);
        break;
      case 20:
        this->write_byte_16(RANGE_CONFIG__TIMEOUT_MACROP_A_HI, 0x0051);
        this->write_byte_16(RANGE_CONFIG__TIMEOUT_MACROP_B_HI, 0x006E);
        break;
      case 33:
        this->write_byte_16(RANGE_CONFIG__TIMEOUT_MACROP_A_HI, 0x00D6);
        this->write_byte_16(RANGE_CONFIG__TIMEOUT_MACROP_B_HI, 0x006E);
        break;
      case 50:
        this->write_byte_16(RANGE_CONFIG__TIMEOUT_MACROP_A_HI, 0x1AE);
        this->write_byte_16(RANGE_CONFIG__TIMEOUT_MACROP_B_HI, 0x01E8);
        break;
      case 100:
        this->write_byte_16(RANGE_CONFIG__TIMEOUT_MACROP_A_HI, 0x02E1);
        this->write_byte_16(RANGE_CONFIG__TIMEOUT_MACROP_B_HI, 0x0388);
        break;
      case 200:
        this->write_byte_16(RANGE_CONFIG__TIMEOUT_MACROP_A_HI, 0x03E1);
        this->write_byte_16(RANGE_CONFIG__TIMEOUT_MACROP_B_HI, 0x0496);
        break;
      case 500:
        this->write_byte_16(RANGE_CONFIG__TIMEOUT_MACROP_A_HI, 0x0591);
        this->write_byte_16(RANGE_CONFIG__TIMEOUT_MACROP_B_HI, 0x05C1);
        break;
      default:
        status = 1;
        break;
    }
  } else {
    switch (timing_budget_in_ms) {
      case 20:
        this->write_byte_16(RANGE_CONFIG__TIMEOUT_MACROP_A_HI, 0x001E);
        this->write_byte_16(RANGE_CONFIG__TIMEOUT_MACROP_B_HI, 0x0022);
        break;
      case 33:
        this->write_byte_16(RANGE_CONFIG__TIMEOUT_MACROP_A_HI, 0x0060);
        this->write_byte_16(RANGE_CONFIG__TIMEOUT_MACROP_B_HI, 0x006E);
        break;
      case 50:
        this->write_byte_16(RANGE_CONFIG__TIMEOUT_MACROP_A_HI, 0x00AD);
        this->write_byte_16(RANGE_CONFIG__TIMEOUT_MACROP_B_HI, 0x00C6);
        break;
      case 100:
        this->write_byte_16(RANGE_CONFIG__TIMEOUT_MACROP_A_HI, 0x01CC);
        this->write_byte_16(RANGE_CONFIG__TIMEOUT_MACROP_B_HI, 0x01EA);
        break;
      case 200:
        this->write_byte_16(RANGE_CONFIG__TIMEOUT_MACROP_A_HI, 0x02D9);
        this->write_byte_16(RANGE_CONFIG__TIMEOUT_MACROP_B_HI, 0x02F8);
        break;
      case 500:
        this->write_byte_16(RANGE_CONFIG__TIMEOUT_MACROP_A_HI, 0x048F);
        this->write_byte_16(RANGE_CONFIG__TIMEOUT_MACROP_B_HI, 0x04A4);
        break;
      default:
        status = 1;
        break;
    }
  }
  return status;
}

uint16_t VL53L1XSensor::get_timing_budget_in_ms(uint16_t *p_timing_budget_in_ms) {  
  // See sparkfun_VL531LX.cpp line 153 and vl531lx_class.cpp line 365
  
  uint16_t timing_budget_in_ms;
  //_device->VL53L1X_GetTimingBudgetInMs(&timingBudget);

  uint16_t Temp;
  VL53L1X_ERROR status = 0;

  status = this->read_byte(RANGE_CONFIG__TIMEOUT_MACROP_A_HI, &Temp);
  switch (Temp) {
    case 0x001D:
      timing_budget_in_ms = 15;
      break;
    case 0x0051:
    case 0x001E:
      timing_budget_in_ms = 20;
      break;
    case 0x00D6:
    case 0x0060:
      timing_budget_in_ms = 33;
      break;
    case 0x1AE:
    case 0x00AD:
      timing_budget_in_ms = 50;
      break;
    case 0x02E1:
    case 0x01CC:
      timing_budget_in_ms = 100;
      break;
    case 0x03E1:
    case 0x02D9:
      timing_budget_in_ms = 200;
      break;
    case 0x0591:
    case 0x048F:
      timing_budget_in_ms = 500;
      break;
    default:
      timing_budget_in_ms = 0;
      break;
  }
  // return status;
  return timing_budget_in_ms;
}

int8_t VL53L1XSensor::apply_distance_mode(DistanceMode mode) {
  uint16_t TB;
  VL53L1X_ERROR status = 0;

  status = get_timing_budget_in_ms(&TB);
  switch (mode) {
    case SHORT:
      status = this->write_byte(PHASECAL_CONFIG__TIMEOUT_MACROP, 0x14);
      status = this->write_byte(RANGE_CONFIG__VCSEL_PERIOD_A, 0x07);
      status = this->write_byte(RANGE_CONFIG__VCSEL_PERIOD_B, 0x05);
      status = this->write_byte(RANGE_CONFIG__VALID_PHASE_HIGH, 0x38);
      status = this->write_byte_16(SD_CONFIG__WOI_SD0, 0x0705);
      status = this->write_byte_16(SD_CONFIG__INITIAL_PHASE_SD0, 0x0606);
      break;
    case MEDIUM:
      break;
    case LONG:
      status = this->write_byte(PHASECAL_CONFIG__TIMEOUT_MACROP, 0x0A);
      status = this->write_byte(RANGE_CONFIG__VCSEL_PERIOD_A, 0x0F);
      status = this->write_byte(RANGE_CONFIG__VCSEL_PERIOD_B, 0x0D);
      status = this->write_byte(RANGE_CONFIG__VALID_PHASE_HIGH, 0xB8);
      status = this->write_byte_16(SD_CONFIG__WOI_SD0, 0x0F0D);
      status = this->write_byte_16(SD_CONFIG__INITIAL_PHASE_SD0, 0x0E0E);
      break;
    default:
      break;
  }
  status = apply_timing_budget_in_ms(TB);
  return status;
}

// ROI
uint8_t VL53L1XSensor::apply_roi(uint8_t x, uint8_t y, uint8_t optical_center) {
  // _device->VL53L1X_SetROI(x, y, optical_center);

  VL53L1X_ERROR status = 0;
  if (x > 16)
    x = 16;
  if (y > 16)
    y = 16;
  if (x > 10 || y > 10) {
    optical_center = 199;
  }
  status = this->write_byte(ROI_CONFIG__USER_ROI_CENTRE_SPAD, optical_center);
  status = this->write_byte(ROI_CONFIG__USER_ROI_REQUESTED_GLOBAL_XY_SIZE, (y - 1) << 4 | (x - 1));
  return status;
}

int8_t VL53L1XSensor::apply_distance_mode_long() { apply_distance_mode(DistanceMode(SHORT)); }

int8_t VL53L1XSensor::apply_distance_mode_short() { apply_distance_mode(DistanceMode(LONG)); }

uint16_t VL53L1XSensor::get_roi_x() {
  VL53L1X_ERROR status = 0;

  uint16_t ROI_X;
  uint8_t tmp;
  status = this->read_byte(ROI_CONFIG__USER_ROI_REQUESTED_GLOBAL_XY_SIZE, &tmp);
  ROI_X = ((uint16_t) tmp & 0x0F) + 1;

  return ROI_X;
}

uint16_t VL53L1XSensor::get_roi_y() {
  VL53L1X_ERROR status = 0;
  uint16_t ROI_Y;
  uint8_t tmp;
  status = this->read_byte(ROI_CONFIG__USER_ROI_REQUESTED_GLOBAL_XY_SIZE, &tmp);
  ROI_Y = (((uint16_t) tmp & 0xF0) >> 4) + 1;

  return ROI_Y;
}

// int8_t VL53L1XSensor::GetROI_XY(uint16_t *ROI_X, uint16_t *ROI_Y)
// {
// 	VL53L1X_ERROR status = 0;
// 	uint8_t tmp;

// 	status = this->read_byte(ROI_CONFIG__USER_ROI_REQUESTED_GLOBAL_XY_SIZE, &tmp);
// 	*ROI_X = ((uint16_t)tmp & 0x0F) + 1;
// 	*ROI_Y = (((uint16_t)tmp & 0xF0) >> 4) + 1;
// 	return status;
// }

// uint16_t VL53L1XSensor::getROIX()
// {
// 	uint16_t tempX;
// 	uint16_t tempY;
// 	// this->VL53L1X_GetROI_XY(&tempX, &tempY);
// 	return tempX;
// }

// uint16_t VL53L1XSensor::getROIY()
// {
// 	uint16_t tempX;
// 	uint16_t tempY;
// 	_device->VL53L1X_GetROI_XY(&tempX, &tempY);
// 	return tempY;
}  // namespace vl53l1x_distance
}  // namespace esphome