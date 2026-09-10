#include "alpha3_select.h"

#if defined(USE_ESP32) && defined(USE_SELECT)
#include "esphome/core/log.h"

namespace esphome::alpha3 {

static const char *const TAG = "alpha3.select";

void Alpha3Select::control(size_t index) {
  switch (this->type_) {
    case Alpha3SelectType::ALPHA3_SELECT_TYPE_OPERATION_MODE:
      if (index < 4) {
        this->parent_->request_operation_mode(static_cast<uint8_t>(index));
        return;
      }
      break;
    case Alpha3SelectType::ALPHA3_SELECT_TYPE_CONTROL_MODE:
      if (index < CONTROL_MODE_VALUES.size()) {
        this->parent_->request_control_mode(CONTROL_MODE_VALUES[index]);
        return;
      }
      break;
  }
  ESP_LOGW(TAG, "Rejected unsupported option index %zu", index);
}

}  // namespace esphome::alpha3
#endif
