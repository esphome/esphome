#include "copy_button.h"
#include "esphome/core/log.h"

namespace esphome::copy {

ESPHOME_LOG_TAG(TAG, "copy.button");

void CopyButton::dump_config() { LOG_BUTTON("", "Copy Button", this); }

void CopyButton::press_action() { source_->press(); }

}  // namespace esphome::copy
