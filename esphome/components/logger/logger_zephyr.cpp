#ifdef USE_ZEPHYR

#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "logger.h"

#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/printk.h>
#include <zephyr/usb/usb_device.h>
#ifdef USE_LOGGER_EARLY_MESSAGE
#include <esphome/components/zephyr/reset_reason.h>
#endif
#ifdef USE_ZEPHYR_LOG_BACKEND
#include <cinttypes>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_backend.h>
#include <zephyr/logging/log_core.h>
#include <zephyr/logging/log_output.h>
#endif

namespace esphome::zephyr_coredump {

__attribute__((weak)) void print_coredump() {}

}  // namespace esphome::zephyr_coredump

namespace esphome::logger {

__attribute__((section(".noinit"))) struct {
  uint32_t magic;
  uint32_t reason;
  uint32_t pc;
  uint32_t lr;
#if defined(CONFIG_THREAD_NAME)
  char thread[CONFIG_THREAD_MAX_NAME_LEN];
#endif
} crash_buf;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

static const char *const TAG = "logger";

#ifdef USE_LOGGER_UART_SELECTION_USB_CDC
void Logger::cdc_loop_() {
  if (this->uart_ != UART_SELECTION_USB_CDC || this->uart_dev_ == nullptr) {
    return;
  }
  static bool opened = false;
  uint32_t dtr = 0;
  uart_line_ctrl_get(this->uart_dev_, UART_LINE_CTRL_DTR, &dtr);

  /* Poll if the DTR flag was set, optional */
  if (opened == dtr) {
    return;
  }

  if (!opened) {
    App.schedule_dump_config();
  }
  opened = !opened;
}
#endif

void Logger::pre_setup() {
  if (this->baud_rate_ > 0) {
    static const struct device *uart_dev = nullptr;
    switch (this->uart_) {
      case UART_SELECTION_UART0:  // NOLINT(bugprone-branch-clone)
        uart_dev = DEVICE_DT_GET_OR_NULL(DT_NODELABEL(uart0));
        break;
      case UART_SELECTION_UART1:
        uart_dev = DEVICE_DT_GET_OR_NULL(DT_NODELABEL(uart1));
        break;
#ifdef USE_LOGGER_USB_CDC
      case UART_SELECTION_USB_CDC:
#ifdef CONFIG_USB_DEVICE_STACK
        uart_dev = DEVICE_DT_GET_OR_NULL(DT_NODELABEL(cdc_acm_uart0));
        if (device_is_ready(uart_dev)) {
          usb_enable(nullptr);
        }
#endif
        break;
#endif
    }
    if (device_is_ready(uart_dev)) {
      this->uart_dev_ = uart_dev;
#if defined(USE_LOGGER_WAIT_FOR_CDC) && defined(USE_LOGGER_UART_SELECTION_USB_CDC)
      uint32_t dtr = 0;
      int32_t count = (10 * 100);  // wait 10 sec for USB CDC to have early logs
      while (dtr == 0 && count-- > 0) {
        uart_line_ctrl_get(this->uart_dev_, UART_LINE_CTRL_DTR, &dtr);
        delay(10);
        arch_feed_wdt();
      }
#endif
    }
  }
  global_logger = this;
  ESP_LOGI(TAG, "Log initialized");
#ifdef USE_LOGGER_EARLY_MESSAGE
  char reason_buffer[zephyr::RESET_REASON_BUFFER_SIZE];
  const char *reset_reason = zephyr::get_reset_reason(std::span<char, zephyr::RESET_REASON_BUFFER_SIZE>(reason_buffer));
  ESP_LOGI(TAG, "Reset reason: %s", reset_reason);
  dump_crash_();
  zephyr_coredump::print_coredump();
#endif
}

void HOT Logger::write_msg_(const char *msg, uint16_t len) {
  // Single write with newline already in buffer (added by caller)
#ifdef CONFIG_PRINTK
  // Requires the debug component and an active SWD connection.
  // It is used for pyocd rtt -t nrf52840
  printk("%.*s", static_cast<int>(len), msg);
#endif
  if (this->uart_dev_ == nullptr) {
    return;
  }
  for (uint16_t i = 0; i < len; ++i) {
    uart_poll_out(this->uart_dev_, msg[i]);
  }
}

const LogString *Logger::get_uart_selection_() {
  switch (this->uart_) {
    case UART_SELECTION_UART0:
      return LOG_STR("UART0");
    case UART_SELECTION_UART1:
      return LOG_STR("UART1");
#ifdef USE_LOGGER_USB_CDC
    case UART_SELECTION_USB_CDC:
      return LOG_STR("USB_CDC");
#endif
    default:
      return LOG_STR("UNKNOWN");
  }
}

static const uint8_t REASON_BUF_SIZE = 32;

static const char *reason_to_str(unsigned int reason, char *buf) {
  switch (reason) {
    case K_ERR_CPU_EXCEPTION:
      return "CPU exception";
    case K_ERR_SPURIOUS_IRQ:
      return "Unhandled interrupt";
    case K_ERR_STACK_CHK_FAIL:
      return "Stack overflow";
    case K_ERR_KERNEL_OOPS:
      return "Kernel oops";
    case K_ERR_KERNEL_PANIC:
      return "Kernel panic";
    default:
      snprintf(buf, REASON_BUF_SIZE, "Unknown error (%u)", reason);
      return buf;
  }
}

void Logger::dump_crash_() {
  ESP_LOGD(TAG, "Crash buffer address %p", &crash_buf);
  if (crash_buf.magic == App.get_config_hash()) {
    char reason_buf[REASON_BUF_SIZE];
    ESP_LOGE(TAG, "Last crash:");
    ESP_LOGE(TAG, "Reason=%s PC=0x%08x LR=0x%08x", reason_to_str(crash_buf.reason, reason_buf), crash_buf.pc,
             crash_buf.lr);
#if defined(CONFIG_THREAD_NAME)
    ESP_LOGE(TAG, "Thread: %s", crash_buf.thread);
#endif
    int32_t count = (2 * 100);  // wait 2 sec to give a chance to print crash
    while (count-- > 0) {
      delay(10);
      arch_feed_wdt();
    }
  }
}

void k_sys_fatal_error_handler(unsigned int reason, const z_arch_esf_t *esf) {
  crash_buf.magic = App.get_config_hash();
  crash_buf.reason = reason;
  if (esf) {
    crash_buf.pc = esf->basic.pc;
    crash_buf.lr = esf->basic.lr;
  }
#if defined(CONFIG_THREAD_NAME)
  auto thread = k_current_get();
  const char *name = k_thread_name_get(thread);
  if (name) {
    strncpy(crash_buf.thread, name, sizeof(crash_buf.thread) - 1);
    crash_buf.thread[sizeof(crash_buf.thread) - 1] = '\0';
  } else {
    crash_buf.thread[0] = '\0';
  }
#endif
  arch_restart();
}

}  // namespace esphome::logger

#ifdef USE_ZEPHYR_LOG_BACKEND
// Zephyr log backend that forwards messages emitted by the Zephyr logging
// subsystem (Bluetooth, USB, drivers, etc.) into the ESPHome logger. Without
// this the messages are dropped, because ESPHome disables Zephyr's own console
// and UART backends to keep exclusive ownership of the serial port.
namespace {

// Tag used for every message that originates from the Zephyr subsystem.
const char *const ZEPHYR_BACKEND_TAG = "zephyr";

// Line accumulation buffer handed to Zephyr's log_output helper. A single log
// message is rendered into this buffer before being flushed to ESPHome.
uint8_t esphome_zephyr_log_buf[ESPHOME_ZEPHYR_LOG_BUFFER_SIZE];

// Level of the message currently being rendered. log_output does not pass the
// level to the character callback, so it is stashed here just before rendering.
// The Zephyr logging subsystem processes messages one at a time, so a plain
// variable is sufficient.
uint8_t esphome_zephyr_level = ESPHOME_LOG_LEVEL_INFO;

// Guard against re-entering the backend if writing to the console itself causes
// a Zephyr log message (e.g. from the UART driver).
bool esphome_zephyr_in_backend = false;

uint8_t map_zephyr_level(uint8_t zephyr_level) {
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
      return ESPHOME_LOG_LEVEL_VERBOSE;
  }
}

int esphome_zephyr_char_out(uint8_t *data, size_t length, void *ctx) {
  // Drop the trailing newline that Zephyr appends; the ESPHome logger adds its
  // own line ending. Keep the original length as the return value so log_output
  // sees the whole buffer as consumed.
  size_t text_length = length;
  while (text_length > 0 && (data[text_length - 1] == '\n' || data[text_length - 1] == '\r')) {
    text_length--;
  }
  if (text_length > 0 && esphome::logger::global_logger != nullptr && !esphome_zephyr_in_backend) {
    esphome_zephyr_in_backend = true;
    esphome::esp_log_printf_(esphome_zephyr_level, ZEPHYR_BACKEND_TAG, 0, "%.*s", static_cast<int>(text_length),
                             reinterpret_cast<char *>(data));
    esphome_zephyr_in_backend = false;
  }
  return static_cast<int>(length);
}

LOG_OUTPUT_DEFINE(esphome_zephyr_log_output, esphome_zephyr_char_out, esphome_zephyr_log_buf,
                  sizeof(esphome_zephyr_log_buf));

void esphome_zephyr_backend_process(const struct log_backend *const backend, union log_msg_generic *msg) {
  if (esphome::logger::global_logger == nullptr) {
    return;
  }
  esphome_zephyr_level = map_zephyr_level(log_msg_get_level(&msg->log));
  // flags = 0: no timestamp, level marker or color prefix -- the ESPHome logger
  // renders its own header. log_output still prefixes the source name.
  log_output_msg_process(&esphome_zephyr_log_output, &msg->log, 0);
}

void esphome_zephyr_backend_dropped(const struct log_backend *const backend, uint32_t cnt) {
  if (esphome::logger::global_logger != nullptr && !esphome_zephyr_in_backend) {
    esphome_zephyr_in_backend = true;
    esphome::esp_log_printf_(ESPHOME_LOG_LEVEL_WARN, ZEPHYR_BACKEND_TAG, 0, "%" PRIu32 " messages dropped", cnt);
    esphome_zephyr_in_backend = false;
  }
}

void esphome_zephyr_backend_panic(const struct log_backend *const backend) {
  log_output_flush(&esphome_zephyr_log_output);
}

void esphome_zephyr_backend_init(const struct log_backend *const backend) {}

const struct log_backend_api esphome_zephyr_backend_api = {
    .process = esphome_zephyr_backend_process,
    .dropped = esphome_zephyr_backend_dropped,
    .panic = esphome_zephyr_backend_panic,
    .init = esphome_zephyr_backend_init,
};

}  // namespace

LOG_BACKEND_DEFINE(esphome_zephyr_backend, esphome_zephyr_backend_api, true);
#endif  // USE_ZEPHYR_LOG_BACKEND

extern "C" {

void k_sys_fatal_error_handler(unsigned int reason, const z_arch_esf_t *esf) {
  esphome::logger::k_sys_fatal_error_handler(reason, esf);
}
}

#endif
