#include "build_info.h"
#include "build_info_data.h"
#include <cstring>

#ifdef USE_ESP8266
#include <pgmspace.h>
#endif

namespace esphome {

uint32_t get_config_hash() { return ESPHOME_CONFIG_HASH; }

time_t get_build_time() { return ESPHOME_BUILD_TIME; }

void get_build_time_string(std::span<char, BUILD_TIME_STR_SIZE> buffer) {
#ifdef USE_ESP8266
  strncpy_P(buffer.data(), ESPHOME_BUILD_TIME_STR, buffer.size());
#else
  strncpy(buffer.data(), ESPHOME_BUILD_TIME_STR, buffer.size());
#endif
  buffer[buffer.size() - 1] = '\0';
}

}  // namespace esphome
