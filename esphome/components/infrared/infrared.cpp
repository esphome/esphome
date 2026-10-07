#include "infrared.h"

#include "esphome/core/log.h"

namespace esphome::infrared {

static const char *const TAG = "infrared";

void Infrared::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Infrared '%s'\n"
                "  Supports Transmitter: %s\n"
                "  Supports Receiver: %s",
                this->get_name().c_str(), YESNO(this->get_supports_transmitter()),
                YESNO(this->get_supports_receiver()));
}

}  // namespace esphome::infrared
