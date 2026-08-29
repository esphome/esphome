// Custom Zephyr log backend forwarding native Zephyr/OpenThread/kernel logs through
// Logger::write_zephyr_native_msg(), instead of Zephyr's built-in LOG_BACKEND_UART --
// that would contend with Logger's own UART writer and never reach log-callback
// listeners (e.g. the API's log streaming to Home Assistant). Mirrors ESP-IDF's
// esp_log_set_vprintf().
#ifdef USE_ZEPHYR
#ifdef CONFIG_LOG

#include <cstring>

#include <zephyr/logging/log_backend.h>
#include <zephyr/logging/log_backend_std.h>
#include <zephyr/logging/log_core.h>
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/logging/log_msg.h>
#include <zephyr/logging/log_output.h>

#include "esphome/core/log.h"
#include "logger.h"

namespace esphome::logger {

namespace {

// Accumulates one fully-rendered log line across however many chunks Zephyr's formatter
// flushes it in, so exactly one write_zephyr_native_msg() call happens per line. Longer
// lines are truncated rather than split across notifications.
constexpr size_t ACCUM_BUF_SIZE = 256;
char accum_buf[ACCUM_BUF_SIZE];  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
size_t accum_len = 0;            // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

int char_out(uint8_t *data, size_t length, void *ctx) {
  size_t remaining = ACCUM_BUF_SIZE - accum_len;
  size_t copy_len = length < remaining ? length : remaining;
  if (copy_len > 0) {
    memcpy(accum_buf + accum_len, data, copy_len);
    accum_len += copy_len;
  }
  return static_cast<int>(length);
}

// Zephyr's own internal formatting chunk buffer -- not the line accumulator above, just
// scratch space log_output uses while rendering before handing chunks to char_out().
uint8_t log_output_scratch[32];  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
LOG_OUTPUT_DEFINE(esphome_zephyr_log_output, char_out, log_output_scratch, sizeof(log_output_scratch));

uint8_t zephyr_to_esphome_level(uint8_t zephyr_level) {
  switch (zephyr_level) {
    case LOG_LEVEL_ERR:
      return ESPHOME_LOG_LEVEL_ERROR;
    case LOG_LEVEL_WRN:
      return ESPHOME_LOG_LEVEL_WARN;
    case LOG_LEVEL_INF:
      return ESPHOME_LOG_LEVEL_INFO;
    case LOG_LEVEL_DBG:
      return ESPHOME_LOG_LEVEL_DEBUG;
    default:
      return ESPHOME_LOG_LEVEL_NONE;
  }
}

void process(const struct log_backend *const backend, union log_msg_generic *msg) {
  accum_len = 0;
  // Body text only, no CRLF (ESPHome's LogBuffer adds its own line ending). SKIP_SOURCE
  // isn't available on nrf52's older NCS Zephyr fork; omitting it there just means the
  // source name may appear twice (tag + body).
#ifdef USE_NRF52
  log_output_msg_process(&esphome_zephyr_log_output, &msg->log, LOG_OUTPUT_FLAG_CRLF_NONE);
#else
  log_output_msg_process(&esphome_zephyr_log_output, &msg->log,
                         LOG_OUTPUT_FLAG_SKIP_SOURCE | LOG_OUTPUT_FLAG_CRLF_NONE);
#endif

  if (global_logger == nullptr || accum_len == 0)
    return;

  uint8_t level = zephyr_to_esphome_level(log_msg_get_level(&msg->log));
  // log_msg_get_source_id() isn't available on nrf52's older NCS Zephyr fork -- fall
  // back to a generic "zephyr" tag there instead of resolving the real module name.
#ifdef USE_NRF52
  const char *tag = "zephyr";
#else
  const char *tag = log_source_name_get(log_msg_get_domain(&msg->log), log_msg_get_source_id(&msg->log));
  if (tag == nullptr)
    tag = "zephyr";
#endif
  global_logger->write_zephyr_native_msg(level, tag, accum_buf, static_cast<uint16_t>(accum_len));
}

void panic(const struct log_backend *const backend) { log_backend_std_panic(&esphome_zephyr_log_output); }

void dropped(const struct log_backend *const backend, uint32_t cnt) {
  log_backend_std_dropped(&esphome_zephyr_log_output, cnt);
}

void init(const struct log_backend *const backend) { log_output_ctx_set(&esphome_zephyr_log_output, nullptr); }

const struct log_backend_api esphome_zephyr_log_backend_api = {
    .process = process,
    .dropped = IS_ENABLED(CONFIG_LOG_MODE_IMMEDIATE) ? nullptr : dropped,
    .panic = panic,
    .init = init,
    .format_set = nullptr,
};

}  // namespace

}  // namespace esphome::logger

LOG_BACKEND_DEFINE(esphome_zephyr_log_backend, esphome::logger::esphome_zephyr_log_backend_api, true, nullptr);

#endif  // CONFIG_LOG
#endif  // USE_ZEPHYR
