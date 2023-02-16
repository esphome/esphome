
#include "lg_hub.h"

namespace esphome {
namespace lg_uart {

/* Public */

bool LGUartHub::button_off() { return true; }
bool LGUartHub::button_on() { return true; }

/* Internal */

void LGUartHub::loop() {}

void LGUartHub::setup() { this->dump_config(); }

void LGUartHub::dump_config() { ESP_LOGCONFIG(TAG, "Config for screen_num: '%i'", this->screen_num_); }

}  // namespace lg_uart
}  // namespace esphome
