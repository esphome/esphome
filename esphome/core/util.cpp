#include "esphome/core/util.h"

// Compiled only on MQTT builds (core FILTER_SOURCE_FILES); without USE_MQTT
// the header provides inline stubs and this file must stay empty, so the
// guard protects builds that compile every source, such as clang-tidy.
#ifdef USE_MQTT
#include "esphome/components/mqtt/mqtt_client.h"

namespace esphome {

bool mqtt_is_connected() { return mqtt::global_mqtt_client != nullptr && mqtt::global_mqtt_client->is_connected(); }

bool remote_is_connected() { return api_is_connected() || mqtt_is_connected(); }

}  // namespace esphome
#endif
