#include "tormatic_switch.h"

namespace esphome::tormatic {

static const char *const TAG = "tormatic.switch";

void TormaticSwitch::dump_config() { LOG_SWITCH("", "Tormatic Light Switch", this); }

}  // namespace esphome::tormatic
