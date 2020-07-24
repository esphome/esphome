#include "esphome/core/esphal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/defines.h"
#include "esphome/core/log.h"

#ifdef ARDUINO_ARCH_ESP8266
extern "C" {
typedef struct {        // NOLINT
  void *interruptInfo;  // NOLINT
  void *functionInfo;   // NOLINT
} ArgStructure;

void ICACHE_RAM_ATTR __attachInterruptArg(uint8_t pin, void (*)(void *), void *fp,  // NOLINT
                                          int mode);
void ICACHE_RAM_ATTR __detachInterrupt(uint8_t pin);  // NOLINT
};
#endif

namespace esphome {

static const char *TAG = "esphal";

GPIOPin::GPIOPin(uint8_t pin, uint8_t mode, bool inverted)
    : pin_(pin),
      mode_(mode),
      inverted_(inverted),
#ifdef ARDUINO_ARCH_ESP8266
      gpio_read_(pin < 16 ? &GPI : &GP16I),
      gpio_mask_(pin < 16 ? (1UL << pin) : 1)
#endif
#ifdef ARDUINO_ARCH_ESP32
          gpio_set_(pin < 32 ? &GPIO.out_w1ts : &GPIO.out1_w1ts.val),
      gpio_clear_(pin < 32 ? &GPIO.out_w1tc : &GPIO.out1_w1tc.val),
      gpio_read_(pin < 32 ? &GPIO.in : &GPIO.in1.val),
      gpio_mask_(pin < 32 ? (1UL << pin) : (1UL << (pin - 32)))
#endif
{
}

const char *GPIOPin::get_pin_mode_name() const {
  const char *mode_s;
  switch (this->mode_) {
    case INPUT:
      mode_s = "INPUT";
      break;
    case OUTPUT:
      mode_s = "OUTPUT";
      break;
    case INPUT_PULLUP:
      mode_s = "INPUT_PULLUP";
      break;
    case OUTPUT_OPEN_DRAIN:
      mode_s = "OUTPUT_OPEN_DRAIN";
      break;
    case SPECIAL:
      mode_s = "SPECIAL";
      break;
    case FUNCTION_1:
      mode_s = "FUNCTION_1";
      break;
    case FUNCTION_2:
      mode_s = "FUNCTION_2";
      break;
    case FUNCTION_3:
      mode_s = "FUNCTION_3";
      break;
    case FUNCTION_4:
      mode_s = "FUNCTION_4";
      break;

#ifdef ARDUINO_ARCH_ESP32
    case PULLUP:
      mode_s = "PULLUP";
      break;
    case PULLDOWN:
      mode_s = "PULLDOWN";
      break;
    case INPUT_PULLDOWN:
      mode_s = "INPUT_PULLDOWN";
      break;
    case OPEN_DRAIN:
      mode_s = "OPEN_DRAIN";
      break;
    case FUNCTION_5:
      mode_s = "FUNCTION_5";
      break;
    case FUNCTION_6:
      mode_s = "FUNCTION_6";
      break;
    case ANALOG:
      mode_s = "ANALOG";
      break;
#endif
#ifdef ARDUINO_ARCH_ESP8266
    case FUNCTION_0:
      mode_s = "FUNCTION_0";
      break;
    case WAKEUP_PULLUP:
      mode_s = "WAKEUP_PULLUP";
      break;
    case WAKEUP_PULLDOWN:
      mode_s = "WAKEUP_PULLDOWN";
      break;
    case INPUT_PULLDOWN_16:
      mode_s = "INPUT_PULLDOWN_16";
      break;
#endif

    default:
      mode_s = "UNKNOWN";
      break;
  }

  return mode_s;
}

unsigned char GPIOPin::get_pin() const { return this->pin_; }
unsigned char GPIOPin::get_mode() const { return this->mode_; }

bool GPIOPin::is_inverted() const { return this->inverted_; }
void GPIOPin::setup() { this->pin_mode(this->mode_); }
bool ICACHE_RAM_ATTR HOT GPIOPin::digital_read() {
  return bool((*this->gpio_read_) & this->gpio_mask_) != this->inverted_;
}
bool ICACHE_RAM_ATTR HOT ISRInternalGPIOPin::digital_read() {
  return bool((*this->gpio_read_) & this->gpio_mask_) != this->inverted_;
}
void ICACHE_RAM_ATTR HOT GPIOPin::digital_write(bool value) {
#ifdef ARDUINO_ARCH_ESP8266
  if (this->pin_ != 16) {
    if (value != this->inverted_) {
      GPOS = this->gpio_mask_;
    } else {
      GPOC = this->gpio_mask_;
    }
  } else {
    if (value != this->inverted_) {
      GP16O |= 1;
    } else {
      GP16O &= ~1;
    }
  }
#endif
#ifdef ARDUINO_ARCH_ESP32
  if (value != this->inverted_) {
    (*this->gpio_set_) = this->gpio_mask_;
  } else {
    (*this->gpio_clear_) = this->gpio_mask_;
  }
#endif
}
void ICACHE_RAM_ATTR HOT ISRInternalGPIOPin::digital_write(bool value) {
#ifdef ARDUINO_ARCH_ESP8266
  if (this->pin_ != 16) {
    if (value != this->inverted_) {
      GPOS = this->gpio_mask_;
    } else {
      GPOC = this->gpio_mask_;
    }
  } else {
    if (value != this->inverted_) {
      GP16O |= 1;
    } else {
      GP16O &= ~1;
    }
  }
#endif
#ifdef ARDUINO_ARCH_ESP32
  if (value != this->inverted_) {
    (*this->gpio_set_) = this->gpio_mask_;
  } else {
    (*this->gpio_clear_) = this->gpio_mask_;
  }
#endif
}
ISRInternalGPIOPin::ISRInternalGPIOPin(uint8_t pin,
#ifdef ARDUINO_ARCH_ESP32
                                       volatile uint32_t *gpio_clear, volatile uint32_t *gpio_set,
#endif
                                       volatile uint32_t *gpio_read, uint32_t gpio_mask, bool inverted)
    : pin_(pin),
      inverted_(inverted),
      gpio_read_(gpio_read),
      gpio_mask_(gpio_mask)
#ifdef ARDUINO_ARCH_ESP32
      ,
      gpio_clear_(gpio_clear),
      gpio_set_(gpio_set)
#endif
{
}
void ICACHE_RAM_ATTR ISRInternalGPIOPin::clear_interrupt() {
#ifdef ARDUINO_ARCH_ESP8266
  GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, this->gpio_mask_);
#endif
#ifdef ARDUINO_ARCH_ESP32
  if (this->pin_ < 32) {
    GPIO.status_w1tc = this->gpio_mask_;
  } else {
    GPIO.status1_w1tc.intr_st = this->gpio_mask_;
  }
#endif
}

void ICACHE_RAM_ATTR HOT GPIOPin::pin_mode(uint8_t mode) {
#ifdef ARDUINO_ARCH_ESP8266
  if (this->pin_ == 16 && mode == INPUT_PULLUP) {
    // pullups are not available on GPIO16, manually override with
    // input mode.
    pinMode(16, INPUT);
    return;
  }
#endif
  pinMode(this->pin_, mode);
}

#ifdef ARDUINO_ARCH_ESP8266
struct ESPHomeInterruptFuncInfo {
  void (*func)(void *);
  void *arg;
};

void ICACHE_RAM_ATTR interrupt_handler(void *arg) {
  ArgStructure *as = static_cast<ArgStructure *>(arg);
  auto *info = static_cast<ESPHomeInterruptFuncInfo *>(as->functionInfo);
  info->func(info->arg);
}
#endif

void GPIOPin::detach_interrupt() const { this->detach_interrupt_(); }
void GPIOPin::detach_interrupt_() const {
#ifdef ARDUINO_ARCH_ESP8266
  __detachInterrupt(get_pin());
#endif
#ifdef ARDUINO_ARCH_ESP32
  detachInterrupt(get_pin());
#endif
}
void GPIOPin::attach_interrupt_(void (*func)(void *), void *arg, int mode) const {
  if (this->inverted_) {
    if (mode == RISING) {
      mode = FALLING;
    } else if (mode == FALLING) {
      mode = RISING;
    }
  }
#ifdef ARDUINO_ARCH_ESP8266
  ArgStructure *as = new ArgStructure;
  as->interruptInfo = nullptr;

  as->functionInfo = new ESPHomeInterruptFuncInfo{
      .func = func,
      .arg = arg,
  };

  __attachInterruptArg(this->pin_, interrupt_handler, as, mode);
#endif
#ifdef ARDUINO_ARCH_ESP32
  // work around issue https://github.com/espressif/arduino-esp32/pull/1776 in arduino core
  // yet again proves how horrible code is there :( - how could that have been accepted...
  auto *attach = reinterpret_cast<void (*)(uint8_t, void (*)(void *), void *, int)>(attachInterruptArg);
  attach(this->pin_, func, arg, mode);
#endif
}

ISRInternalGPIOPin *GPIOPin::to_isr() const {
  return new ISRInternalGPIOPin(this->pin_,
#ifdef ARDUINO_ARCH_ESP32
                                this->gpio_clear_, this->gpio_set_,
#endif
                                this->gpio_read_, this->gpio_mask_, this->inverted_);
}

void force_link_symbols() {
#ifdef ARDUINO_ARCH_ESP8266
  // Tasmota uses magic bytes in the binary to check if an OTA firmware is compatible
  // with their settings - ESPHome uses a different settings system (that can also survive
  // erases). So set magic bytes indicating all tasmota versions are supported.
  // This only adds 12 bytes of binary size, which is an acceptable price to pay for easier support
  // for Tasmota.
  // https://github.com/arendst/Tasmota/blob/b05301b1497942167a015a6113b7f424e42942cd/tasmota/settings.ino#L346-L380
  // https://github.com/arendst/Tasmota/blob/b05301b1497942167a015a6113b7f424e42942cd/tasmota/i18n.h#L652-L654
  const static uint32_t TASMOTA_MAGIC_BYTES[] PROGMEM = {0x5AA55AA5, 0xFFFFFFFF, 0xA55AA55A};
  // Force link symbol by using a volatile integer (GCC attribute used does not work because of LTO)
  volatile int x = 0;
  x = TASMOTA_MAGIC_BYTES[x];
#endif
}

}  // namespace esphome

#ifdef ARDUINO_ESP8266_RELEASE_2_3_0
// Fix 2.3.0 std missing memchr
extern "C" {
void *memchr(const void *s, int c, size_t n) {
  if (n == 0)
    return nullptr;
  const uint8_t *p = reinterpret_cast<const uint8_t *>(s);
  do {
    if (*p++ == c)
      return const_cast<void *>(reinterpret_cast<const void *>(p - 1));
  } while (--n != 0);
  return nullptr;
}
};
#endif

#ifdef ARDUINO_ARCH_ESP8266
extern "C" {
extern void resetPins() {  // NOLINT
  // Added in framework 2.7.0
  // usually this sets up all pins to be in INPUT mode
  // however, not strictly needed as we set up the pins properly
  // ourselves and this causes pins to toggle during reboot.
}
}
#endif
