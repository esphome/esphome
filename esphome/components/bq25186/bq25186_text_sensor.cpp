#include "bq25186_text_sensor.h"
#ifdef USE_TEXT_SENSOR

#include "esphome/core/progmem.h"

namespace esphome::bq25186 {

// STAT0[6:5] charge status, then STAT1[4:3] TS status
PROGMEM_STRING_TABLE(StatusStrings, "Not Charging", "Constant Current", "Constant Voltage", "Charge Done/Disabled",
                     "Normal", "Hot/Cold (Charging Suspended)", "Cool (Current Reduced)", "Warm (Voltage Reduced)");

void BQ25186TextSensor::on_status(const BQ25186StatusRegisters &status) {
  const uint8_t value = (status[this->reg_] >> this->shift_) & 0x03;
  if (value == this->last_)
    return;
  this->last_ = value;
  const ProgmemStr name = StatusStrings::get_progmem_str(this->first_ + value, StatusStrings::LAST_INDEX);
  char text[32];
  ESPHOME_strncpy_P(text, reinterpret_cast<ESPHOME_PGM_P>(name), sizeof(text));
  text[sizeof(text) - 1] = '\0';
  this->publish_state(text);
}

}  // namespace esphome::bq25186

#endif  // USE_TEXT_SENSOR
