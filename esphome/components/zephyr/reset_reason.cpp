#include "reset_reason.h"

#if defined(USE_ZEPHYR) && (defined(USE_LOGGER_EARLY_MESSAGE) || defined(USE_DEBUG))
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include <zephyr/drivers/hwinfo.h>

namespace esphome::zephyr {

static const char *const TAG = "zephyr";

static size_t append_reset_reason(char *buf, size_t size, size_t pos, bool set, const char *reason) {
  if (!set) {
    return pos;
  }
  if (pos > 0) {
    pos = buf_append_printf(buf, size, pos, ", ");
  }
  return buf_append_printf(buf, size, pos, "%s", reason);
}

const char *get_reset_reason(std::span<char, RESET_REASON_BUFFER_SIZE> buffer) {
  char *buf = buffer.data();
  const size_t size = RESET_REASON_BUFFER_SIZE;

  uint32_t cause;
  auto ret = hwinfo_get_reset_cause(&cause);
  if (ret) {
    ESP_LOGE(TAG, "Unable to get reset cause: %d", ret);
    buf[0] = '\0';
    return buf;
  }
  size_t pos = 0;

  if (cause == 0) {
    pos = append_reset_reason(buf, size, pos, true, "None");
  } else {
    pos = append_reset_reason(buf, size, pos, cause & RESET_PIN, "External pin");
    pos = append_reset_reason(buf, size, pos, cause & RESET_SOFTWARE, "Software reset");
    pos = append_reset_reason(buf, size, pos, cause & RESET_BROWNOUT, "Brownout (drop in voltage)");
    pos = append_reset_reason(buf, size, pos, cause & RESET_POR, "Power-on reset (POR)");
    pos = append_reset_reason(buf, size, pos, cause & RESET_WATCHDOG, "Watchdog timer expiration");
    pos = append_reset_reason(buf, size, pos, cause & RESET_DEBUG, "Debug event");
    pos = append_reset_reason(buf, size, pos, cause & RESET_SECURITY, "Security violation");
    pos = append_reset_reason(buf, size, pos, cause & RESET_LOW_POWER_WAKE, "Waking up from low power mode");
    pos = append_reset_reason(buf, size, pos, cause & RESET_CPU_LOCKUP, "CPU lock-up detected");
    pos = append_reset_reason(buf, size, pos, cause & RESET_PARITY, "Parity error");
    pos = append_reset_reason(buf, size, pos, cause & RESET_PLL, "PLL error");
    pos = append_reset_reason(buf, size, pos, cause & RESET_CLOCK, "Clock error");
    pos = append_reset_reason(buf, size, pos, cause & RESET_HARDWARE, "Hardware reset");
    pos = append_reset_reason(buf, size, pos, cause & RESET_USER, "User reset");
    pos = append_reset_reason(buf, size, pos, cause & RESET_TEMPERATURE, "Temperature reset");
  }

  // Ensure null termination if nothing was written
  if (pos == 0) {
    buf[0] = '\0';
  }
  return buf;
}
}  // namespace esphome::zephyr

#endif
