#pragma once
#include "esphome/components/esp32/gpio.h"
#include "esphome/core/log.h"
#include <driver/gpio.h>
#include <driver/rtc_io.h>

// Drives held pads through the ISR pin API and reads the pad back.
namespace holdtest {
using esphome::esp32::ESP32InternalGPIOPin;
namespace gpio = esphome::gpio;
static const char *const TAG = "holdtest";

// One pin and one ISR argument block per GPIO and hold setting, made on first use and reused by later rounds,
// so a test that repeats forever does not use up the heap.
struct PinPair {
  ESP32InternalGPIOPin *pin{nullptr};
  esphome::ISRInternalGPIOPin isr;
};
static PinPair pairs[2][GPIO_NUM_MAX];

static PinPair &get(int num, bool hold) {
  PinPair &pp = pairs[hold][num];
  if (pp.pin == nullptr) {
    pp.pin = new ESP32InternalGPIOPin();  // NOLINT
    pp.pin->set_pin(static_cast<gpio_num_t>(num));
    pp.pin->set_flags(gpio::FLAG_INPUT | gpio::FLAG_OUTPUT | (hold ? gpio::FLAG_HOLD : gpio::FLAG_NONE));
    pp.isr = pp.pin->to_isr();
  }
  pp.pin->setup();  // the previous round left the pad unheld with no mode
  return pp;
}

static int lvl(int num) {
  esphome::delayMicroseconds(50);
  return gpio_get_level(static_cast<gpio_num_t>(num));
}

// Returns failures for one pin.
static int run_pin(int num, bool hold) {
  int fail = 0;
  auto &isr = get(num, hold).isr;
  isr.pin_mode(gpio::FLAG_INPUT | gpio::FLAG_OUTPUT);
  int wr_ok = 0;
  for (int i = 0; i < 20; i++) {
    bool v = i & 1;
    isr.digital_write(v);
    if (lvl(num) == (int) v) {
      wr_ok++;
    } else {
      fail++;
    }
  }
  // Control: a raw register write without a hold refresh must not reach a held pad.
  int before = lvl(num);
  gpio_set_level(static_cast<gpio_num_t>(num), !before);
  int ctrl = lvl(num);
  bool ctrl_ok = hold ? (ctrl == before) : (ctrl != before);
  if (!ctrl_ok)
    fail++;
  isr.digital_write(before);  // back in sync

  // Pulls through the ISR pin_mode: input must toggle in the flags diff to apply them.
  int pd1, pu, pd2;
  isr.pin_mode(gpio::FLAG_NONE);
  isr.pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLDOWN);
  pd1 = lvl(num);
  isr.pin_mode(gpio::FLAG_NONE);
  isr.pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
  pu = lvl(num);
  isr.pin_mode(gpio::FLAG_NONE);
  isr.pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLDOWN);
  pd2 = lvl(num);
  if (pd1 != 0 || pu != 1 || pd2 != 0)
    fail++;
  ESP_LOGI(TAG, "GPIO%d hold=%d rtc=%d write %d/20 control %s pulls pd=%d pu=%d pd=%d -> %s", num, hold,
           (int) rtc_gpio_is_valid_gpio(static_cast<gpio_num_t>(num)), wr_ok, ctrl_ok ? "ok" : "BAD", pd1, pu, pd2,
           fail ? "FAIL" : "PASS");
  // Leave the pad free for the next run.
  if (hold)
    gpio_hold_dis(static_cast<gpio_num_t>(num));
  isr.pin_mode(gpio::FLAG_NONE);
  return fail;
}

static void run(const int *pins, int n, int round) {
  int fail = 0;
  for (int i = 0; i < n; i++) {
    fail += run_pin(pins[i], true);
    fail += run_pin(pins[i], false);
  }
  ESP_LOGI(TAG, "ROUND %d %s failures=%d", round, fail ? "FAIL" : "PASS", fail);
}
}  // namespace holdtest
