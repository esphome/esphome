#pragma once

#ifdef USE_ESP8266

namespace esphome {
namespace esp8266 {

void setup_preferences();
void preferences_prevent_write(bool prevent);

}  // namespace esp8266
}  // namespace esphome

#endif  // USE_ESP8266
