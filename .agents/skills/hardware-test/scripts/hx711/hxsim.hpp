#pragma once
// Fake HX711 inside the same ESP32. SCK (component output) and DOUT (component input) are GPIO32/GPIO33,
// both switched to input+output so the fake reads the clock and drives DOUT that the component reads back.
// A task pinned to core 0 (the ESPHome loop runs on core 1) watches SCK: rising edges shift out 24 bits MSB
// first, extra pulses pick the next conversion (25 A/128, 26 B/32, 27 A/64), SCK high > 60 us powers down.
// Values encode what was converted: A/128 -> 128000+seq, A/64 -> 64000+seq, B/32 -> -(32000+seq).
#include <atomic>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "soc/gpio_reg.h"
#include "esp_cpu.h"
#include "esp_private/esp_clk.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "esphome/core/log.h"
#include "esp_attr.h"

namespace hxsim {
static const char *const TAG = "hxsim";
static constexpr int SCK = 32, DOUT = 33;
static constexpr uint32_t SCK_BIT = 1u << (SCK - 32), DOUT_BIT = 1u << (DOUT - 32);

// The timing critical part lives in static (not inline member) functions: IRAM_ATTR on an inline member is
// either inlined into flash code or rejected by the linker ("literal placed after use"). From flash, a cold
// cache made the first clock after boot go unseen.
static inline uint32_t in() { return REG_READ(GPIO_IN1_REG); }
static inline void dout(bool high) { REG_WRITE(high ? GPIO_OUT1_W1TS_REG : GPIO_OUT1_W1TC_REG, DOUT_BIT); }

// Wait up to `lim` CPU cycles for SCK to reach `level`. Interrupts are off.
static IRAM_ATTR __attribute__((noinline)) bool wait_sck(bool level, uint32_t lim) {
  uint32_t start = esp_cpu_get_cycle_count();
  while (((in() & SCK_BIT) != 0) != level)
    if (esp_cpu_get_cycle_count() - start > lim)
      return false;
  return true;
}

// Shift out one conversion once SCK is high and DOUT low. Returns the pulse count; held_high is set when SCK
// stayed high at the end (a power down, that pulse not counted).
static IRAM_ATTR __attribute__((noinline)) int shift_out(uint32_t data, bool stay_low, uint32_t cpu, bool &held_high) {
  int pulses = 0;
  held_high = false;
  while (true) {
    if (pulses > 0 && !wait_sck(true, 40 * cpu))
      break;  // no more pulses
    pulses++;
    if (pulses <= 24)
      dout((data >> (24 - pulses)) & 1);
    else if (pulses == 25)
      dout(!stay_low);
    if (!wait_sck(false, 60 * cpu)) {
      held_high = true;
      return pulses - 1;
    }
  }
  return pulses;
}

// DOUT low (when arming a new conversion), then wait for the first clock with interrupts off in 2 ms windows for up
// to 200 ms. Each window ends with a short gap that lets pending interrupts and tasks run. -1 when no clock came.
static IRAM_ATTR __attribute__((noinline)) int arm_and_shift(bool arm, uint32_t data, bool stay_low, uint32_t cpu,
                                                             std::atomic<int> *mode, bool &held_high) {
  if (arm)
    dout(false);
  for (int window = 0; window < 100; window++) {
    portDISABLE_INTERRUPTS();
    if ((in() & SCK_BIT) || wait_sck(true, 2000 * cpu)) {
      int pulses = shift_out(data, stay_low, cpu, held_high);
      portENABLE_INTERRUPTS();
      return pulses;
    }
    portENABLE_INTERRUPTS();
    if (mode->load(std::memory_order_relaxed) != 0)
      break;
  }
  return -1;
}

enum Mode : int { NORMAL = 0, STUCK_BUSY = 1, DOUT_STAYS_LOW = 2 };

enum EventKind : uint8_t { EV_TRANSFER = 1, EV_POWER_DOWN = 2, EV_POWER_UP = 3, EV_SHORT = 4 };

struct Event {
  uint8_t kind;  // EventKind
  uint8_t pulses;
  uint8_t chan_gain;  // conversion that was read: 1 A128, 2 B32, 3 A64
  uint8_t next;
  int32_t value;
  uint32_t t_ms;
};

struct Chip {
  std::atomic<int> mode{NORMAL};
  std::atomic<int> conv_ms{100};  // 10 SPS
  std::atomic<int> transfers{0}, short_transfers{0}, power_downs{0}, power_ups{0};
  Event ring[64];
  std::atomic<uint32_t> head{0};
  uint32_t tail{0};
  uint32_t seq{0};
  uint8_t sel{1};  // conversion in progress: 1 A128, 2 B32, 3 A64
  bool powered{false};
  int64_t ready_at{0};
  int32_t value{0};
  uint32_t cyc_per_us{240};

  void push(uint8_t kind, uint8_t pulses, uint8_t cg, uint8_t next, int32_t v) {
    uint32_t h = head.load();
    ring[h % 64] = Event{kind, pulses, cg, next, v, (uint32_t) (esp_timer_get_time() / 1000)};
    head.store(h + 1);
  }
  int32_t make_value() {
    seq = (seq + 1) % 1000;
    if (sel == 2)
      return -(int32_t) (32000 + seq);
    return (sel == 3 ? 64000 : 128000) + (int32_t) seq;
  }
  void start_conversion(int ms) {
    dout(true);
    ready_at = esp_timer_get_time() + (int64_t) ms * 1000;
  }
  void power_down() {
    powered = false;
    sel = 1;  // power-up resets to channel A gain 128
    dout(true);
    power_downs++;
    push(EV_POWER_DOWN, 0, 0, 0, 0);
  }

  // Bookkeeping after shift_out, interrupts on again.
  void finish_transfer(int pulses, bool held_high) {
    uint8_t read_sel = sel;
    if (held_high) {
      if (pulses > 0)
        push(pulses >= 24 ? EV_TRANSFER : EV_SHORT, pulses, read_sel, 0, value);
      if (pulses > 0 && pulses < 24)
        short_transfers++;
      power_down();
      return;
    }
    if (pulses < 25) {
      short_transfers++;
      push(EV_SHORT, pulses, read_sel, 0, value);
      start_conversion(conv_ms);
      return;
    }
    sel = pulses >= 27 ? 3 : pulses == 26 ? 2 : 1;
    transfers++;
    push(EV_TRANSFER, pulses, read_sel, sel, value);
    if (mode == DOUT_STAYS_LOW) {
      dout(false);
      ready_at = esp_timer_get_time();  // never shows the busy state
      return;
    }
    start_conversion(conv_ms);
  }

  void run() {
    esp_task_wdt_delete(xTaskGetIdleTaskHandleForCore(0));
    cyc_per_us = esp_clk_cpu_freq() / 1000000;
    gpio_set_direction((gpio_num_t) SCK, GPIO_MODE_INPUT_OUTPUT);
    gpio_set_direction((gpio_num_t) DOUT, GPIO_MODE_INPUT_OUTPUT);
    dout(true);
    powered = false;
    while (true) {
      bool sck = in() & SCK_BIT;
      if (!powered) {
        if (!sck) {
          powered = true;
          sel = 1;
          power_ups++;
          push(EV_POWER_UP, 0, 0, 0, 0);
          start_conversion(conv_ms);
        } else {
          vTaskDelay(1);
        }
        continue;
      }
      int64_t now = esp_timer_get_time();
      if (mode == STUCK_BUSY && !(REG_READ(GPIO_OUT1_REG) & DOUT_BIT))
        dout(true);
      bool ready = now >= ready_at && mode != STUCK_BUSY;
      if (ready) {
        bool arm = REG_READ(GPIO_OUT1_REG) & DOUT_BIT;
        if (arm)
          value = make_value();
        bool held_high = false;
        int pulses =
            arm_and_shift(arm, (uint32_t) value & 0xFFFFFF, mode == DOUT_STAYS_LOW, cyc_per_us, &mode, held_high);
        if (pulses >= 0)
          finish_transfer(pulses, held_high);
        continue;
      }
      if (sck) {
        // SCK high outside a transfer: a power down once it holds for 60 us
        uint32_t s = esp_cpu_get_cycle_count();
        while ((in() & SCK_BIT) && esp_cpu_get_cycle_count() - s < 70 * cyc_per_us) {
        }
        if (in() & SCK_BIT)
          power_down();
        continue;
      }
      if (ready_at - now > 2000 || mode == STUCK_BUSY)
        vTaskDelay(1);
    }
  }

  void log_events() {
    static const char *const CG[4] = {"-", "A128", "B32", "A64"};
    while (tail != head.load()) {
      Event &e = ring[tail % 64];
      tail++;
      switch (e.kind) {
        case EV_TRANSFER:
          ESP_LOGI(TAG, "HX read %s value %d, %u pulses -> next %s (t=%u)", CG[e.chan_gain & 3], (int) e.value,
                   e.pulses, CG[e.next & 3], (unsigned) e.t_ms);
          break;
        case EV_POWER_DOWN:
          ESP_LOGI(TAG, "HX power down (t=%u)", (unsigned) e.t_ms);
          break;
        case EV_POWER_UP:
          ESP_LOGI(TAG, "HX power up (t=%u)", (unsigned) e.t_ms);
          break;
        case EV_SHORT:
          ESP_LOGW(TAG, "HX short transfer %u pulses (t=%u)", e.pulses, (unsigned) e.t_ms);
          break;
      }
    }
  }

  static void task(void *arg) { static_cast<Chip *>(arg)->run(); }
  void start() { xTaskCreatePinnedToCore(task, "hxsim", 4096, this, 1, nullptr, 0); }
};

inline Chip chip;
}  // namespace hxsim
