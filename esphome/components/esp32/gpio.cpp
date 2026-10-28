#include "esphome/core/defines.h"
// Always built: InternalGPIOPin is this class, so components that merely hold a
// pointer to it reference these methods even in configs without a pin.
#ifdef USE_ESP32

#include "gpio.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "driver/gpio.h"
#include "esp_sleep.h"
#include "esp_pm.h"
#include "driver/rtc_io.h"
#include "hal/gpio_hal.h"
#include "soc/soc_caps.h"
#include "soc/gpio_periph.h"
#include "freertos/FreeRTOS.h"
#include <cinttypes>

#if (SOC_RTCIO_PIN_COUNT > 0)
#include "hal/rtc_io_hal.h"
#endif

// some variants don't map rtcio to gpio 1:1
#if defined(USE_ESP32_VARIANT_ESP32) || defined(USE_ESP32_VARIANT_ESP32H2) || defined(USE_ESP32_VARIANT_ESP32H21)
#define RTCIO_NEED_MAPPING 1  // NOLINT
#endif

// Need use_rtc variable for: esp32, rtc capable + hold enabled, or rtc wake capable + peripheral powerdown
#if defined(USE_ESP32_VARIANT_ESP32) || \
    (SOC_RTCIO_PIN_COUNT > 0 && (defined(USE_GPIO_HOLD) || defined(CONFIG_PM_POWER_DOWN_PERIPHERAL_IN_LIGHT_SLEEP)))
#define ISR_PIN_USE_RTC 1  // NOLINT
#endif

namespace esphome {
namespace esp32 {

ESPHOME_LOG_TAG(TAG, "esp32");

static const gpio_hal_context_t GPIO_HAL = {.dev = GPIO_HAL_GET_HW(GPIO_PORT_0)};

bool ESP32InternalGPIOPin::isr_service_installed = false;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

static gpio_mode_t flags_to_mode(gpio::Flags flags) {
  flags = flags & (gpio::FLAG_INPUT | gpio::FLAG_OUTPUT | gpio::FLAG_OPEN_DRAIN);
  if (flags == gpio::FLAG_INPUT)
    return GPIO_MODE_INPUT;
  if (flags == gpio::FLAG_OUTPUT)
    return GPIO_MODE_OUTPUT;
  if (flags == (gpio::FLAG_OUTPUT | gpio::FLAG_OPEN_DRAIN))
    return GPIO_MODE_OUTPUT_OD;
  if (flags == (gpio::FLAG_INPUT | gpio::FLAG_OUTPUT | gpio::FLAG_OPEN_DRAIN))
    return GPIO_MODE_INPUT_OUTPUT_OD;
  if (flags == (gpio::FLAG_INPUT | gpio::FLAG_OUTPUT))
    return GPIO_MODE_INPUT_OUTPUT;
  // unsupported or gpio::FLAG_NONE
  return GPIO_MODE_DISABLE;
}

struct ISRPinArg {
  gpio_num_t pin;
  gpio::Flags flags;
  bool inverted;
#ifdef USE_GPIO_HOLD
  bool hold;
#endif
#ifdef ISR_PIN_USE_RTC
  bool use_rtc;
#ifdef RTCIO_NEED_MAPPING
  int rtc_pin;
#endif
#endif
};

#ifdef CONFIG_FREERTOS_USE_TICKLESS_IDLE
#ifdef CONFIG_PM_POWER_DOWN_PERIPHERAL_IN_LIGHT_SLEEP
static ISRFuncArg *wakeup_args = nullptr;
static portMUX_TYPE wakeup_args_mux = portMUX_INITIALIZER_UNLOCKED;
static bool sleep_callback_registered = false;

static void link_wakeup_arg(ISRFuncArg *func_arg) {
  portENTER_CRITICAL(&wakeup_args_mux);
  ISRFuncArg **current = &wakeup_args;
  while (*current != nullptr && *current != func_arg)
    current = &(*current)->next;
  if (*current == nullptr) {
    func_arg->next = wakeup_args;
    wakeup_args = func_arg;
  }
  portEXIT_CRITICAL(&wakeup_args_mux);
}

static void unlink_wakeup_arg(ISRFuncArg *func_arg) {
  portENTER_CRITICAL(&wakeup_args_mux);
  ISRFuncArg **current = &wakeup_args;
  while (*current != nullptr && *current != func_arg)
    current = &(*current)->next;
  if (*current == func_arg) {
    *current = func_arg->next;
    func_arg->next = nullptr;
  }
  portEXIT_CRITICAL(&wakeup_args_mux);
}

static esp_err_t apply_ext1(gpio_num_t pin, bool level) {
  if (level) {
    return esp_sleep_enable_ext1_wakeup_io(1ULL << pin, ESP_EXT1_WAKEUP_ANY_LOW);
  }
  return esp_sleep_enable_ext1_wakeup_io(1ULL << pin, ESP_EXT1_WAKEUP_ANY_HIGH);
}

static IRAM_ATTR esp_err_t light_sleep_enter_cb(int64_t, void *) {
  portENTER_CRITICAL(&wakeup_args_mux);
  for (ISRFuncArg *func_arg = wakeup_args; func_arg != nullptr; func_arg = func_arg->next) {
    if (func_arg->wake_level == func_arg->level)
      continue;
    esp_err_t ret = apply_ext1(func_arg->pin, func_arg->level);
    if (ret != ESP_OK) {
      portEXIT_CRITICAL(&wakeup_args_mux);
      return ret;
    }
    func_arg->wake_level = func_arg->level;
  }
  portEXIT_CRITICAL(&wakeup_args_mux);
  return ESP_OK;
}

static void register_sleep_callback_once() {
  if (sleep_callback_registered)
    return;
  static esp_pm_sleep_cbs_register_config_t cbs_conf = {
      .enter_cb = &light_sleep_enter_cb,
      .exit_cb = nullptr,
      .enter_cb_user_arg = nullptr,
      .exit_cb_user_arg = nullptr,
      .enter_cb_prior = 1,
      .exit_cb_prior = 0,
  };
  if (esp_pm_light_sleep_register_cbs(&cbs_conf) == ESP_OK)
    sleep_callback_registered = true;
}

static void enable_wakeup(gpio_num_t pin, gpio_int_type_t type) {
  esp_sleep_enable_ext1_wakeup_io(1ULL << pin,
                                  type == GPIO_INTR_HIGH_LEVEL ? ESP_EXT1_WAKEUP_ANY_HIGH : ESP_EXT1_WAKEUP_ANY_LOW);
}

static bool can_wake(gpio_num_t pin) { return esp_sleep_is_valid_wakeup_gpio(pin); }
#else
static void enable_wakeup(gpio_num_t pin, gpio_int_type_t type) {
  gpio_wakeup_enable(pin, type);
  esp_sleep_enable_gpio_wakeup();
}

static bool can_wake(gpio_num_t pin) { return true; }
#endif
#endif

#ifdef USE_GPIO_HOLD
// Re-latch a held pad onto the values just written to its registers.
static inline void refresh_hold(const ESP32InternalGPIOPin &pin) {
  pin.set_hold(false);
  pin.set_hold(true);
}

static inline void IRAM_ATTR isr_refresh_hold(const ISRPinArg *arg) {
  if (!arg->hold)
    return;
#if defined(ISR_PIN_USE_RTC) && defined(SOC_RTCIO_HOLD_SUPPORTED)
  if (arg->use_rtc) {
#ifdef RTCIO_NEED_MAPPING
    rtcio_hal_hold_disable(arg->rtc_pin);
    rtcio_hal_hold_enable(arg->rtc_pin);
#else
    rtcio_hal_hold_disable(arg->pin);
    rtcio_hal_hold_enable(arg->pin);
#endif
    return;
  }
#endif
  gpio_hal_hold_dis(&GPIO_HAL, arg->pin);
  gpio_hal_hold_en(&GPIO_HAL, arg->pin);
}
#endif

ISRInternalGPIOPin ESP32InternalGPIOPin::to_isr() const {
  auto *arg = new ISRPinArg{};  // NOLINT(cppcoreguidelines-owning-memory)
  arg->pin = this->get_pin_num();
  arg->flags = gpio::FLAG_NONE;
  arg->inverted = this->pin_flags_.inverted;
#ifdef USE_GPIO_HOLD
  arg->hold = this->get_hold();
#endif
#ifdef ISR_PIN_USE_RTC
  arg->use_rtc = rtc_gpio_is_valid_gpio(this->get_pin_num());
#ifdef RTCIO_NEED_MAPPING
  if (arg->use_rtc)
    arg->rtc_pin = rtc_io_number_get(this->get_pin_num());
#endif
#endif
  return ISRInternalGPIOPin((void *) arg);
}

#ifdef CONFIG_FREERTOS_USE_TICKLESS_IDLE
static void IRAM_ATTR arm_opposite(const ISRFuncArg *arg, bool level) {
  gpio_int_type_t type = level ? GPIO_INTR_LOW_LEVEL : GPIO_INTR_HIGH_LEVEL;
  gpio_hal_set_intr_type(&GPIO_HAL, arg->pin, type);
#if SOC_RTCIO_WAKE_SUPPORTED && !defined(CONFIG_PM_POWER_DOWN_PERIPHERAL_IN_LIGHT_SLEEP)
  if (arg->rtc_pin >= 0)
    rtcio_hal_wakeup_enable(arg->rtc_pin, type);
#endif
}

static void IRAM_ATTR gpio_intr(void *arg) {
  // emulate edge interrupt
  auto *func_arg = reinterpret_cast<ISRFuncArg *>(arg);
  gpio_num_t pin = func_arg->pin;
  gpio_int_type_t type = func_arg->type;
  bool level = gpio_hal_get_level(&GPIO_HAL, pin);
  arm_opposite(func_arg, level);
#ifdef CONFIG_PM_POWER_DOWN_PERIPHERAL_IN_LIGHT_SLEEP
  func_arg->level = level;
#endif
  if (level) {
    if ((type == GPIO_INTR_POSEDGE) || (type == GPIO_INTR_ANYEDGE)) {
      func_arg->func(func_arg->arg);
    }
  } else {
    if ((type == GPIO_INTR_NEGEDGE) || (type == GPIO_INTR_ANYEDGE)) {
      func_arg->func(func_arg->arg);
    }
  }
  gpio_hal_clear_intr_status_bit(&GPIO_HAL, pin);
}
#endif

void ESP32InternalGPIOPin::attach_interrupt_(void (*func)(void *), void *arg, gpio::InterruptType type) const {
  gpio_int_type_t idf_type = GPIO_INTR_ANYEDGE;
  switch (type) {
    case gpio::INTERRUPT_RISING_EDGE:
      idf_type = this->pin_flags_.inverted ? GPIO_INTR_NEGEDGE : GPIO_INTR_POSEDGE;
      break;
    case gpio::INTERRUPT_FALLING_EDGE:
      idf_type = this->pin_flags_.inverted ? GPIO_INTR_POSEDGE : GPIO_INTR_NEGEDGE;
      break;
    case gpio::INTERRUPT_ANY_EDGE:
      idf_type = GPIO_INTR_ANYEDGE;
      break;
    case gpio::INTERRUPT_LOW_LEVEL:
      idf_type = this->pin_flags_.inverted ? GPIO_INTR_HIGH_LEVEL : GPIO_INTR_LOW_LEVEL;
      break;
    case gpio::INTERRUPT_HIGH_LEVEL:
      idf_type = this->pin_flags_.inverted ? GPIO_INTR_LOW_LEVEL : GPIO_INTR_HIGH_LEVEL;
      break;
  }

  if (!isr_service_installed) {
    auto res = gpio_install_isr_service(ESP_INTR_FLAG_LEVEL3);
    if (res != ESP_OK) {
      ESP_LOGE(TAG, "attach_interrupt(): call to gpio_install_isr_service() failed, error code: %d", res);
      return;
    }
    isr_service_installed = true;
  }

#ifndef CONFIG_FREERTOS_USE_TICKLESS_IDLE
  gpio_set_intr_type(this->get_pin_num(), idf_type);
  gpio_isr_handler_add(this->get_pin_num(), func, arg);
  return;
#else
  if (!can_wake(this->get_pin_num())) {
    ESP_LOGW(TAG, "GPIO%u is not a RTC pin; interrupts may be missed in light sleep", this->pin_);
    gpio_set_intr_type(this->get_pin_num(), idf_type);
    gpio_isr_handler_add(this->get_pin_num(), func, arg);
    return;
  }
  // Edge interrupts are not working with wakeup from light sleep. We emulate them with
  // wakeup + level interrupts. This adds some latency and short pulses might be missed.
  // Could let components opt-out and hold a sleep lock instead.

  ISRFuncArg *&func_args = this->isr_func_arg_;
  gpio_isr_handler_remove(this->get_pin_num());
#ifdef CONFIG_PM_POWER_DOWN_PERIPHERAL_IN_LIGHT_SLEEP
  if (func_args != nullptr) {
    unlink_wakeup_arg(func_args);
    esp_sleep_disable_ext1_wakeup_io(1ULL << this->get_pin_num());
  }
#endif

  if (idf_type == GPIO_INTR_LOW_LEVEL || idf_type == GPIO_INTR_HIGH_LEVEL) {
    gpio_set_intr_type(this->get_pin_num(), idf_type);
    enable_wakeup(this->get_pin_num(), idf_type);
    gpio_isr_handler_add(this->get_pin_num(), func, arg);
  } else {
    bool level = gpio_get_level(this->get_pin_num());
    if (func_args == nullptr)
      func_args = new ISRFuncArg();  // NOLINT(cppcoreguidelines-owning-memory)
    func_args->pin = this->get_pin_num();
    func_args->rtc_pin = -1;
#if SOC_RTCIO_WAKE_SUPPORTED
    if (rtc_gpio_is_valid_gpio(func_args->pin)) {
#if defined(RTCIO_NEED_MAPPING)
      func_args->rtc_pin = rtc_io_number_get(func_args->pin);
#else
      func_args->rtc_pin = func_args->pin;
#endif
    }
#endif
    func_args->type = idf_type;
#ifdef CONFIG_PM_POWER_DOWN_PERIPHERAL_IN_LIGHT_SLEEP
    func_args->level = level;
    func_arg->wake_level = level;
#endif
    func_args->func = func;
    func_args->arg = arg;
    arm_opposite(func_args, level);
    gpio_int_type_t opposite = level ? GPIO_INTR_LOW_LEVEL : GPIO_INTR_HIGH_LEVEL;
    enable_wakeup(this->get_pin_num(), opposite);
    gpio_isr_handler_add(this->get_pin_num(), gpio_intr, func_args);
#ifdef CONFIG_PM_POWER_DOWN_PERIPHERAL_IN_LIGHT_SLEEP
    link_wakeup_arg(func_args);
    register_sleep_callback_once();
#endif
  }
#endif
}

size_t ESP32InternalGPIOPin::dump_summary(char *buffer, size_t len) const {
  return snprintf(buffer, len, "GPIO%" PRIu32, static_cast<uint32_t>(this->pin_));
}

#ifdef USE_GPIO_HOLD
void ESP32InternalGPIOPin::set_hold(bool hold) const {
  if (!this->get_hold())
    return;
  if (hold) {
    gpio_hold_en(this->get_pin_num());
  } else {
    gpio_hold_dis(this->get_pin_num());
  }
}
#endif

void ESP32InternalGPIOPin::setup() {
#ifdef USE_GPIO_HOLD
  // Hold before gpio_config so the pad keeps its state while the registers change
  this->set_hold(true);
#endif
  gpio_config_t conf{};
  conf.pin_bit_mask = 1ULL << static_cast<uint32_t>(this->pin_);
  conf.mode = flags_to_mode(this->flags_);
  conf.pull_up_en = this->flags_ & gpio::FLAG_PULLUP ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE;
  conf.pull_down_en = this->flags_ & gpio::FLAG_PULLDOWN ? GPIO_PULLDOWN_ENABLE : GPIO_PULLDOWN_DISABLE;
  conf.intr_type = GPIO_INTR_DISABLE;
  gpio_config(&conf);
#ifdef CONFIG_FREERTOS_USE_TICKLESS_IDLE
  // If auto light sleep is used gpio sleep mode needs to be disabled for the pin
  gpio_sleep_sel_dis(this->get_pin_num());
#endif
  if (this->flags_ & gpio::FLAG_OUTPUT) {
    gpio_set_drive_capability(this->get_pin_num(), this->get_drive_strength());
  }
#ifdef USE_GPIO_HOLD
  // gpio_hold_dis logs an error on input-only pads
  if (GPIO_IS_VALID_OUTPUT_GPIO(this->get_pin_num())) {
    if (!this->get_hold()) {
      // Release a hold left behind by an earlier boot
      gpio_hold_dis(this->get_pin_num());
    } else if (this->flags_ & gpio::FLAG_INPUT) {
      // Inputs apply the config now so reads work; outputs wait for the first write
      refresh_hold(*this);
    }
  }
#endif
}

void ESP32InternalGPIOPin::pin_mode(gpio::Flags flags) {
#if defined(CONFIG_PM_POWER_DOWN_PERIPHERAL_IN_LIGHT_SLEEP) && defined(SOC_RTCIO_WAKE_SUPPORTED)
  // in case ext1 wakeup was used normal gpio mode needs to be re-enabled
  if (rtc_gpio_is_valid_gpio(this->get_pin_num())) {
    rtc_gpio_deinit(this->get_pin_num());
    rtc_gpio_hold_dis(this->get_pin_num());
  }
#endif
  // can't call gpio_config here because that logs in esp-idf which may cause issues
  gpio_set_direction(this->get_pin_num(), flags_to_mode(flags));
  gpio_pull_mode_t pull_mode = GPIO_FLOATING;
  if ((flags & gpio::FLAG_PULLUP) && (flags & gpio::FLAG_PULLDOWN)) {
    pull_mode = GPIO_PULLUP_PULLDOWN;
  } else if (flags & gpio::FLAG_PULLUP) {
    pull_mode = GPIO_PULLUP_ONLY;
  } else if (flags & gpio::FLAG_PULLDOWN) {
    pull_mode = GPIO_PULLDOWN_ONLY;
  }
  gpio_set_pull_mode(this->get_pin_num(), pull_mode);
#ifdef USE_GPIO_HOLD
  refresh_hold(*this);
#endif
}

bool ESP32InternalGPIOPin::digital_read() {
  return bool(gpio_get_level(this->get_pin_num())) != this->pin_flags_.inverted;
}
void ESP32InternalGPIOPin::digital_write(bool value) {
  gpio_set_level(this->get_pin_num(), value != this->pin_flags_.inverted ? 1 : 0);
#ifdef USE_GPIO_HOLD
  refresh_hold(*this);
#endif
}
void ESP32InternalGPIOPin::detach_interrupt() const {
  gpio_intr_disable(this->get_pin_num());
#ifdef CONFIG_FREERTOS_USE_TICKLESS_IDLE
#ifdef CONFIG_PM_POWER_DOWN_PERIPHERAL_IN_LIGHT_SLEEP
  gpio_isr_handler_remove(this->get_pin_num());
  if (esp_sleep_is_valid_wakeup_gpio(this->get_pin_num()))
    esp_sleep_disable_ext1_wakeup_io(1ULL << this->get_pin_num());
  if (this->isr_func_arg_ != nullptr) {
    unlink_wakeup_arg(this->isr_func_arg_);
  }
#else
  gpio_isr_handler_remove(this->get_pin_num());
  gpio_wakeup_disable(this->get_pin_num());
  gpio_sleep_sel_dis(this->get_pin_num());
#endif
#endif
}

}  // namespace esp32

using namespace esp32;

// NOLINTBEGIN(clang-analyzer-core.FixedAddressDereference) -- some gpio_hal functions use MMIO at fixed addresses
// internally
bool IRAM_ATTR ISRInternalGPIOPin::digital_read() {
  auto *arg = reinterpret_cast<ISRPinArg *>(this->arg_);
  return bool(gpio_hal_get_level(&GPIO_HAL, arg->pin)) != arg->inverted;
}

void IRAM_ATTR ISRInternalGPIOPin::digital_write(bool value) {
  auto *arg = reinterpret_cast<ISRPinArg *>(this->arg_);
  gpio_hal_set_level(&GPIO_HAL, arg->pin, value != arg->inverted);
#ifdef USE_GPIO_HOLD
  isr_refresh_hold(arg);
#endif
}

void IRAM_ATTR ISRInternalGPIOPin::clear_interrupt() {
  // not supported
}

void IRAM_ATTR ISRInternalGPIOPin::pin_mode(gpio::Flags flags) {
  auto *arg = reinterpret_cast<ISRPinArg *>(arg_);
#if defined(CONFIG_PM_POWER_DOWN_PERIPHERAL_IN_LIGHT_SLEEP) && (SOC_RTCIO_PIN_COUNT > 0)
  // in case ext1 wakeup was used normal gpio mode needs to be re-enabled
  if (arg->use_rtc) {
#ifdef RTCIO_NEED_MAPPING
    int rtc_num = arg->rtc_pin;
#else
    int rtc_num = (int) arg->pin;
#endif
    rtcio_hal_hold_disable(rtc_num);
#ifdef SOC_RTCIO_WAKE_SUPPORTED
    rtcio_hal_function_select(rtc_num, RTCIO_LL_FUNC_DIGITAL);
#if SOC_RTCIO_INPUT_OUTPUT_SUPPORTED
    // Disable any configuration of the RTC IO that may affect the GPIO behavior
    rtcio_hal_set_direction(rtc_num, RTC_GPIO_MODE_DISABLED);
#endif
    // Note: ext1 wakeup enables the RTC/LP IO clock without registering it, but we
    // cannot disable it here from ISR context. Any other caller of rtc_gpio_deinit
    // would disable it if it is not needed anymore.
#endif
  }
#endif

  gpio::Flags diff = (gpio::Flags) (flags ^ arg->flags);
  if (diff & gpio::FLAG_OUTPUT) {
    if (flags & gpio::FLAG_OUTPUT) {
      gpio_hal_output_enable(&GPIO_HAL, arg->pin);
      if (flags & gpio::FLAG_OPEN_DRAIN)
        gpio_hal_od_enable(&GPIO_HAL, arg->pin);
    } else {
      gpio_hal_output_disable(&GPIO_HAL, arg->pin);
    }
  }
  if (diff & gpio::FLAG_INPUT) {
    if (flags & gpio::FLAG_INPUT) {
      gpio_hal_input_enable(&GPIO_HAL, arg->pin);
#ifdef USE_ESP32_VARIANT_ESP32
      // Only ESP32 uses the RTC registers for pullup/pulldown
      if (arg->use_rtc) {
        if (flags & gpio::FLAG_PULLUP) {
          rtcio_hal_pullup_enable(arg->rtc_pin);
        } else {
          rtcio_hal_pullup_disable(arg->rtc_pin);
        }
        if (flags & gpio::FLAG_PULLDOWN) {
          rtcio_hal_pulldown_enable(arg->rtc_pin);
        } else {
          rtcio_hal_pulldown_disable(arg->rtc_pin);
        }
      } else
#endif
      {
        if (flags & gpio::FLAG_PULLUP) {
          gpio_hal_pullup_en(&GPIO_HAL, arg->pin);
        } else {
          gpio_hal_pullup_dis(&GPIO_HAL, arg->pin);
        }
        if (flags & gpio::FLAG_PULLDOWN) {
          gpio_hal_pulldown_en(&GPIO_HAL, arg->pin);
        } else {
          gpio_hal_pulldown_dis(&GPIO_HAL, arg->pin);
        }
      }
    } else {
      gpio_hal_input_disable(&GPIO_HAL, arg->pin);
    }
  }
#ifdef USE_GPIO_HOLD
  isr_refresh_hold(arg);
#endif
  arg->flags = flags;
}
// NOLINTEND(clang-analyzer-core.FixedAddressDereference)

}  // namespace esphome

#endif  // USE_ESP32
