#include "infrared.h"

#include "esphome/core/log.h"

namespace esphome::infrared {

ESPHOME_LOG_TAG(TAG, "infrared");

void Infrared::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Infrared '%s'\n"
                "  Supports Transmitter: %s\n"
                "  Supports Receiver: %s",
                LOG_STR_ARG(this->get_log_name()), YESNO(this->get_supports_transmitter()),
                YESNO(this->get_supports_receiver()));
}

}  // namespace esphome::infrared
