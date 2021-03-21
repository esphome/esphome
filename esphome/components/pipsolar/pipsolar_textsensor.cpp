#include "pipsolar_textsensor.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome {
namespace pipsolar {

static const char *TAG = "pipsolar.textsensor";

void PipsolarTextSensor::dump_config() { LOG_TEXT_SENSOR("", "Pipsolar TextSensor", this); }
// void PipsolarSwitch::write_state(bool state) {
//     this->parent_->switch_command(this->command_);
// }

}  // namespace pipsolar
}  // namespace esphome
