#include "esphome/core/defines.h"

#ifdef USE_ESP32
#include "automation.h"
#elif defined(USE_BLE_GATT_CLIENT)
#include "automation_gatt.h"
#endif

#if defined(USE_ESP32) || defined(USE_BLE_GATT_CLIENT)
namespace esphome::ble_client {

const char *const Automation::TAG = "ble_client.automation";

}  // namespace esphome::ble_client
#endif
