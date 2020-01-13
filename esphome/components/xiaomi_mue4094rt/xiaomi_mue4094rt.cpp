#include "xiaomi_mue4094rt.h"
#include "esphome/core/log.h"

#ifdef ARDUINO_ARCH_ESP32

namespace esphome {
namespace xiaomi_mue4094rt {

static const char *TAG = "xiaomi_mue4094rt";

void XiaomiMUE4094RT::dump_config() { LOG_BINARY_SENSOR("", "BLE Motion", this); }

}  // namespace xiaomi_mue4094rt
}  // namespace esphome

#endif
