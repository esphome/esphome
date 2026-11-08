#include "device_decoders.h"
#include <cstdlib>
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32
namespace esphome::fendt_caravan {
ESPHOME_LOG_TAG(TAG, "fendt_caravan.decoders");

float DeviceDecoders::decode_float(const char *data) {
  char buffer[128];
  memset(buffer, 0, sizeof(buffer));
  if (strlen(data) >= sizeof(buffer) - 1) {
    ESP_LOGW(TAG, "Data too long for decode_float: '%s'", data);
    return 0.0f;
  }
  std::strncpy(buffer, data, strlen(data));
  for (char *p = buffer; *p != '\0'; p++) {
    if (*p == ',')
      *p = '.';
  }
  float out = strtof(buffer, nullptr);
  return out;
}

int DeviceDecoders::decode_int(const char *value) {
  char data[64];
  size_t len = strlen(value);
  if (len >= sizeof(data))
    len = sizeof(data) - 1;
  memcpy(data, value, len);
  data[len] = '\0';

  // left trim
  char *start = data;
  while (*start != '\0' && std::isspace(static_cast<unsigned char>(*start)))
    start++;
  // right trim
  char *end = data + strlen(data);
  while (end > start && std::isspace(static_cast<unsigned char>(*(end - 1))))
    end--;
  *end = '\0';

  // shift trimmed content to beginning
  size_t trimmed_len = end - start;
  memmove(data, start, trimmed_len + 1);

  auto val = parse_number<int>(data);
  if (val.has_value())
    return val.value();
  return 0;
}

}  // namespace esphome::fendt_caravan
#endif
