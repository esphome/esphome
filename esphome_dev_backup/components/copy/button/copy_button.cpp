#include "copy_button.h"
#include "esphome/core/log.h"

namespace esphome {
namespace copy {

static const char *const TAG = "copy.button";

void CopyButton::dump_config() { LOG_BUTTON("", "Copy Button", this); }

void CopyButton::press_action() { source_->press(); }

}  // namespace copy
}  // namespace esphome
