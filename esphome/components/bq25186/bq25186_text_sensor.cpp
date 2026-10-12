#include "bq25186_text_sensor.h"
#ifdef USE_TEXT_SENSOR

#include "esphome/core/progmem.h"

namespace esphome::bq25186 {

// Indexed by STAT0[6:5]
PROGMEM_STRING_TABLE(ChargeStatusStrings, "Not Charging", "Constant Current", "Constant Voltage",
                     "Charge Done/Disabled");
// Indexed by STAT1[4:3]
PROGMEM_STRING_TABLE(TsStatusStrings, "Normal", "Hot/Cold (Charging Suspended)", "Cool (Current Reduced)",
                     "Warm (Voltage Reduced)");

void BQ25186TextSensor::on_status(const BQ25186StatusRegisters &status) {
  const bool charge = this->kind_ == BQ25186TextSensorKind::CHARGE_STATUS;
  const uint8_t value = charge ? (status[BQ25186_REG_STAT0] >> 5) & 0x03 : (status[BQ25186_REG_STAT1] >> 3) & 0x03;
  if (value == this->last_)
    return;
  this->last_ = value;
  const ProgmemStr name = charge ? ChargeStatusStrings::get_progmem_str(value, ChargeStatusStrings::LAST_INDEX)
                                 : TsStatusStrings::get_progmem_str(value, TsStatusStrings::LAST_INDEX);
  char text[32];
  ESPHOME_strncpy_P(text, reinterpret_cast<ESPHOME_PGM_P>(name), sizeof(text));
  text[sizeof(text) - 1] = '\0';
  this->publish_state(text);
}

}  // namespace esphome::bq25186

#endif  // USE_TEXT_SENSOR
