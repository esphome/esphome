#include "ir_rf_proxy.h"
#include "esphome/core/log.h"

namespace esphome::ir_rf_proxy {

static const char *const TAG = "ir_rf_proxy";

void IrRfProxy::dump_config() {
  ESP_LOGCONFIG(TAG,
                "IR/RF Proxy '%s'\n"
                "  Supports Transmitter: %s\n"
                "  Supports Receiver: %s",
                this->get_name().c_str(), YESNO(this->traits_.get_supports_transmitter()),
                YESNO(this->traits_.get_supports_receiver()));

  if (this->is_rf()) {
    ESP_LOGCONFIG(TAG, "  Hardware Type: RF (%.3f MHz)", this->frequency_khz_ / 1e3f);
  } else {
    ESP_LOGCONFIG(TAG, "  Hardware Type: Infrared");
  }
}

}  // namespace esphome::ir_rf_proxy
