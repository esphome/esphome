#include "cstxxx.h"
#include "esphome/core/log.h"

namespace esphome::cstxxx {

ESPHOME_LOG_TAG(TAG, "cstxxx");

void CSTTouchscreen::fail_check_code_(uint16_t check_code) {
  ESP_LOGE(TAG, "Firmware check code 0x%04X", check_code);
  this->mark_failed(LOG_STR("Invalid firmware check code"));
}

}  // namespace esphome::cstxxx
