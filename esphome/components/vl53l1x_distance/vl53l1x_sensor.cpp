#include "esphome/core/log.h"
#include "vl53l1x_sensor.h"

namespace esphome {
namespace vl53l1x {

static const char *TAG = "vl53l1x";

void VL53L1XSensor::dump_config() {
  LOG_SENSOR("", "VL53L1X", this);
  LOG_UPDATE_INTERVAL(this);
  LOG_I2C_DEVICE(this);
}

void VL53L1XSensor::setup() {
  ESP_LOGCONFIG(TAG, "Setting up VL53L1X...");
  // begin and log if not connected
  if (VL53L1X_begin() != 0) {
    ESP_LOGW(TAG, "'%s' - failed to begin. Please check wiring.", this->name_.c_str());
  }

  VL53L1X_set_distance_mode(static_cast<DistanceMode>(distance_mode_));
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
  VL53L1X_set_timing_budget_in_ms(timing_budget_);
}

void VL53L1XSensor::update() {
  // initiate single shot measurement]
  uint16_t distance_mm;

  VL53L1X_start_ranging();  // Write configuration bytes to initiate measurement
  if (VL53L1X_check_for_data_ready() == 0) {
    distance_mm = VL53L1X_get_distance();  // Get the result of the measurement from the sensor
    // VL53L1X_clearInterrupt();
    VL53L1X_stop_ranging();
    float distance_m = static_cast<float>(distance_mm) / 1000.0;
    ESP_LOGD(TAG, "'%s' - Got distance %.3f m", this->name_.c_str(), distance_m);
    this->publish_state(distance_m);
  }
}

void VL53L1XSensor::loop() {
  uint16_t distance_mm;

  VL53L1X_start_ranging();  // Write configuration bytes to initiate measurement
  if (VL53L1X_check_for_data_ready() == 0) {
    distance_mm = VL53L1X_get_distance();  // Get the result of the measurement from the sensor
    // VL53L1X_clearInterrupt();
    VL53L1X_stop_ranging();
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
  //   the zone delayMicroseconds(50); VL53L1X_set_timing_budget_in_ms(50); // Could this be set into setup as well?
  //   VL53L1X_start_ranging(); //Write configuration bytes to initiate measurement
  //   distance_mm = VL53L1X_getDistance(); //Get the result of the measurement from the sensor
  //   VL53L1X_stop_ranging();
}
// VL53L1X_

bool VL53L1XSensor::VL53L1X_begin() {
  if (VL53L1X_check_id() == false)
    return (VL53L1_ERROR_PLATFORM_SPECIFIC_START);

  // return _device->VL53L1X_SensorInit();

  int8_t status = 0;
  uint8_t Addr = 0x00, dataReady = 0, timeout = 0;

  // write the default configuration
  for (Addr = 0x2D; Addr <= 0x87; Addr++) {
    this->write_byte(Addr, VL51L1X_DEFAULT_CONFIGURATION[Addr - 0x2D]);
  }
  VL53L1X_start_ranging();

  // We need to wait at least the default intermeasurement period of 103ms before dataready will occur
  // But if a unit has already been powered and polling, it may happen much faster
  while (dataReady == 0) {
    status = VL53L1X_check_for_data_ready(&dataReady);
    if (timeout++ > 150)
      return VL53L1_ERROR_TIME_OUT;
    delay(1);
  }
  // status = VL53L1_WrByte(Device, SYSTEM__MODE_START, 0x00); /* Disable VL53L1X */

  VL53L1X_stop_ranging();
  status = this->write_byte(VL53L1_VHV_CONFIG__TIMEOUT_MACROP_LOOP_BOUND, 0x09); /* two bounds VHV */
  status = this->write_byte(0x0B, 0); /* start VHV from the previous temperature */
  return false;
}

/*Checks the ID of the device, returns true if ID is correct*/

bool VL53L1XSensor::VL53L1X_check_id() {
  uint16_t sensorId;
  uint16_t tmp = 0;

  this->read_byte_16(VL53L1_IDENTIFICATION__MODEL_ID, &tmp);
  sensorId = tmp;

  if (sensorId == 0xEACC)
    return true;
  return false;
}

uint16_t VL53L1XSensor::VL53L1X_get_distance() {
  uint16_t distance;
  // _device->VL53L1X_GetDistance(&distance);
  VL53L1X_ERROR status = 0;
  uint16_t tmp;

  status = (read_byte_16(VL53L1_RESULT__FINAL_CROSSTALK_CORRECTED_RANGE_MM_SD0, &tmp));

  distance = tmp;
  // return status;
  return (int) distance;
}

void VL53L1XSensor::VL53L1X_start_ranging() {
  // _device->VL53L1X_StartRanging();
  int8_t status = 0;

  this->write_byte(SYSTEM__MODE_START, 0x40); /* Enable VL53L1X */
}

void VL53L1XSensor::VL53L1X_stop_ranging() {
  // _device->VL53L1X_StopRanging();

  int8_t status = 0;

  this->write_byte(SYSTEM__MODE_START, 0x00); /* Disable VL53L1X */
}

bool VL53L1XSensor::VL53L1X_check_for_data_ready(uint8_t *isdataReady) {
  uint8_t dataReady;
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
      isdataReady = 1;
    else
      isdataReady = 0;
  }
  dataReady = isdataReady;

  return (bool) dataReady;
}

int8_t VL53L1XSensor::VL53L1X_set_timing_budget_in_ms(uint16_t TimingBudgetInMs) {
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
    switch (TimingBudgetInMs) {
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
    switch (TimingBudgetInMs) {
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

uint16_t VL53L1XSensor::VL53L1X_get_timing_budget_in_ms(
    uint16_t *pTimingBudgetInMs)  // See sparkfun_VL531LX.cpp line 153 and vl531lx_class.cpp line 365
{
  uint16_t TimingBudgetInMs;
  //_device->VL53L1X_GetTimingBudgetInMs(&timingBudget);

  uint16_t Temp;
  VL53L1X_ERROR status = 0;

  status = this->read_byte(RANGE_CONFIG__TIMEOUT_MACROP_A_HI, &Temp);
  switch (Temp) {
    case 0x001D:
      TimingBudgetInMs = 15;
      break;
    case 0x0051:
    case 0x001E:
      TimingBudgetInMs = 20;
      break;
    case 0x00D6:
    case 0x0060:
      TimingBudgetInMs = 33;
      break;
    case 0x1AE:
    case 0x00AD:
      TimingBudgetInMs = 50;
      break;
    case 0x02E1:
    case 0x01CC:
      TimingBudgetInMs = 100;
      break;
    case 0x03E1:
    case 0x02D9:
      TimingBudgetInMs = 200;
      break;
    case 0x0591:
    case 0x048F:
      TimingBudgetInMs = 500;
      break;
    default:
      TimingBudgetInMs = 0;
      break;
  }
  // return status;
  return TimingBudgetInMs;
}

int8_t VL53L1XSensor::VL53L1X_set_distance_mode_long() { VL53L1X_set_distance_mode(SHORT); }

int8_t VL53L1XSensor::VL53L1X_set_distance_mode_short() { VL53L1X_set_distance_mode(LONG); }

int8_t VL53L1XSensor::VL53L1X_set_distance_mode(DistanceMode mode) {
  uint16_t TB;
  VL53L1X_ERROR status = 0;

  status = VL53L1X_get_timing_budget_in_ms(&TB);
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
  status = VL53L1X_set_timing_budget_in_ms(TB);
  return status;
}
// ROI
uint8_t VL53L1XSensor::VL53L1X_set_ROI(uint8_t X, uint8_t Y, uint8_t opticalCenter) {
  // _device->VL53L1X_SetROI(x, y, opticalCenter);

  VL53L1X_ERROR status = 0;
  if (X > 16)
    X = 16;
  if (Y > 16)
    Y = 16;
  if (X > 10 || Y > 10) {
    opticalCenter = 199;
  }
  status = this->write_byte(ROI_CONFIG__USER_ROI_CENTRE_SPAD, opticalCenter);
  status = this->write_byte(ROI_CONFIG__USER_ROI_REQUESTED_GLOBAL_XY_SIZE, (Y - 1) << 4 | (X - 1));
  return status;
}

uint16_t VL53L1XSensor::VL53L1X_get_ROIX() {
  VL53L1X_ERROR status = 0;

  uint16_t ROI_X;
  uint8_t tmp;
  status = this->read_byte(ROI_CONFIG__USER_ROI_REQUESTED_GLOBAL_XY_SIZE, &tmp);
  ROI_X = ((uint16_t) tmp & 0x0F) + 1;

  return ROI_X;
}

uint16_t VL53L1XSensor::VL53L1X_get_ROIY() {
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
}  // namespace vl53l1x
}  // namespace esphome