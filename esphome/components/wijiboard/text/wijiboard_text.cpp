#include "wijiboard_text.h"

#include "esphome/core/log.h"

namespace esphome::wijiboard {

static const char *const TAG = "wijiboard.text";

void WijiBoardText::control(const std::string &value) {
  this->publish_state(value);
  this->parent_->write_word(value);
}

void WijiBoardText::dump_config() { LOG_TEXT("", "WijiBoard Text", this); }

}  // namespace esphome::wijiboard
