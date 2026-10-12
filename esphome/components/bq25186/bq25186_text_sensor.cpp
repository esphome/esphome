#include "bq25186_text_sensor.h"
#ifdef USE_TEXT_SENSOR

namespace esphome::bq25186 {

static const char *charge_status_name(uint8_t status) {
  switch (status) {
    case 0:
      return "Not Charging";
    case 1:
      return "Constant Current";
    case 2:
      return "Constant Voltage";
    default:
      return "Charge Done/Disabled";
  }
}

static const char *ts_status_name(uint8_t status) {
  switch (status) {
    case 0:
      return "Normal";
    case 1:
      return "Hot/Cold (Charging Suspended)";
    case 2:
      return "Cool (Current Reduced)";
    default:
      return "Warm (Voltage Reduced)";
  }
}

void BQ25186TextSensor::on_status(const BQ25186StatusRegisters &status) {
  // STAT0[6:5] holds the charge state, STAT1[4:3] the TS state
  const bool charge = this->kind_ == BQ25186TextSensorKind::CHARGE_STATUS;
  const uint8_t value = charge ? (status[BQ25186_REG_STAT0] >> 5) & 0x03 : (status[BQ25186_REG_STAT1] >> 3) & 0x03;
  if (value == this->last_)
    return;
  this->last_ = value;
  this->publish_state(charge ? charge_status_name(value) : ts_status_name(value));
}

}  // namespace esphome::bq25186

#endif  // USE_TEXT_SENSOR
