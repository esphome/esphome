#include "bq25820_text_sensor.h"
#ifdef USE_TEXT_SENSOR

#include "esphome/core/progmem.h"

namespace esphome::bq25820 {

// CHARGE_STAT[2:0] (code 5 is reserved), then TS_STAT[2:0] (codes 5-7 are not defined)
PROGMEM_STRING_TABLE(StatusStrings, "Not Charging", "Trickle", "Precharge", "Fast Charge", "Taper", "Unknown",
                     "Top-Off", "Done", "Normal", "Warm", "Cool", "Cold", "Hot", "Unknown");

void BQ25820TextSensor::on_status(const BQ25820StatusRegisters &status) {
  const uint8_t value = (status[this->reg_] >> this->shift_) & 0x07;
  if (value == this->last_)
    return;
  this->last_ = value;
  this->publish_state(StatusStrings::get_progmem_str(this->first_ + value, StatusStrings::LAST_INDEX));
}

}  // namespace esphome::bq25820

#endif  // USE_TEXT_SENSOR
