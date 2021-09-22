#include "pipsolar_textsensor.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome {
namespace pipsolar {

static const char *const TAG = "pipsolar.text_sensor";

void PipsolarTextSensor::dump_config() { LOG_TEXT_SENSOR("", "Pipsolar TextSensor", this); }

}  // namespace pipsolar
}  // namespace esphome
