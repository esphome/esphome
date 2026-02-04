#pragma once

#ifdef USE_ESP8266

namespace esphome::esp8266 {

void setup_preferences();
void preferences_prevent_write(bool prevent);

}  // namespace esphome::esp8266

#endif  // USE_ESP8266
