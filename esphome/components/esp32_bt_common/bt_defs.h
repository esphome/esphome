#pragma once

#ifdef USE_ESP32

#include <esp_bt.h>

// Define the configured BT operation mode:
#ifdef USE_ARDUINO
#include <esp32-hal-bt.h>
#else
#ifdef CONFIG_BTDM_CONTROLLER_MODE_BTDM
#define BT_MODE ESP_BT_MODE_BTDM
#elif defined(CONFIG_BTDM_CONTROLLER_MODE_BR_EDR_ONLY)
#define BT_MODE ESP_BT_MODE_CLASSIC_BT
#else
#define BT_MODE ESP_BT_MODE_BLE
#endif
#endif

namespace esphome {
namespace esp32_bt_common {

}  // namespace esp32_bt_common
}  // namespace esphome

#endif
