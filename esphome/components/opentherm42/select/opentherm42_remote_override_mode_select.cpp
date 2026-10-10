#include "opentherm42_remote_override_mode_select.h"

namespace esphome::opentherm42 {

ESPHOME_LOG_TAG(TAG, "opentherm42.select");

void OpenTherm42RemoteOverrideModeSelect::dump_config() {
  LOG_SELECT("", "OpenTherm 4.2 Remote Override Mode Select", this);
}

}  // namespace esphome::opentherm42
