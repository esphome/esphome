#include "raw_protocol.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace remote_base {

static const char *const TAG = "remote.raw";

bool RawDumper::dump(RemoteReceiveData src) {
  char buffer[256];
  size_t pos = buf_append_printf(buffer, sizeof(buffer), 0, "Received Raw: ");

  for (int32_t i = 0; i < src.size() - 1; i++) {
    const int32_t value = src[i];
    size_t prev_pos = pos;

    if (i + 1 < src.size() - 1) {
      pos = buf_append_printf(buffer, sizeof(buffer), pos, "%" PRId32 ", ", value);
    } else {
      pos = buf_append_printf(buffer, sizeof(buffer), pos, "%" PRId32, value);
    }

    if (pos >= sizeof(buffer) - 1) {
      // buffer full, flush and continue
      buffer[prev_pos] = '\0';
      ESP_LOGI(TAG, "%s", buffer);
      if (i + 1 < src.size() - 1) {
        pos = buf_append_printf(buffer, sizeof(buffer), 0, "  %" PRId32 ", ", value);
      } else {
        pos = buf_append_printf(buffer, sizeof(buffer), 0, "  %" PRId32, value);
      }
    }
  }
  if (pos != 0) {
    ESP_LOGI(TAG, "%s", buffer);
  }
  return true;
}

}  // namespace remote_base
}  // namespace esphome
