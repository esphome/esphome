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
      case UART_SELECTION_UART0:
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
    if (!device_is_ready(uart_dev)) {
      ESP_LOGE(TAG, "%s is not ready.", LOG_STR_ARG(get_uart_selection_()));
    } else {
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

extern "C" {

void k_sys_fatal_error_handler(unsigned int reason, const z_arch_esf_t *esf) {
  esphome::logger::k_sys_fatal_error_handler(reason, esf);
}
}

#endif
