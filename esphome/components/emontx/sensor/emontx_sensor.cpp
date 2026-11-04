#include "emontx_sensor.h"
#include "esphome/core/log.h"

namespace esphome::emontx {

ESPHOME_LOG_TAG(TAG, "emontx_sensor");

void EmonTxSensor::dump_config() { LOG_SENSOR("  ", "EmonTx Sensor", this); }

}  // namespace esphome::emontx
