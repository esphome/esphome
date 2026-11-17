// BK72xx crash probe: forces one crash per step and shows what the SDK and the bootloader leave behind.
// Steps: 0 undefined instruction, 1 call into unmapped memory, 2 unmapped and unaligned accesses (no fault),
// 3 lt_reboot(). The step lives in a flash preference saved and synced before the crash.
#pragma once
#include <Arduino.h>
#include <FreeRTOS.h>
#include <task.h>
#include <cstdint>
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/core/preferences.h"

extern "C" unsigned bk_misc_get_start_type(void);
extern "C" char _bss_end[];

static constexpr uint32_t PROBE_CANARY_LOW = 0x004000F8;   // free word in the SDK-reserved low SRAM
static constexpr uint32_t PROBE_CANARY_ITCM = 0x003F3FF0;  // top of ITCM
static char bkprobe_report[112];
static constexpr uint32_t PROBE_MARKS[] = {0x40A1B0, 0x40B000, 0x40C000, 0x40E000, 0x410000, 0x414000, 0x418000,
                                           0x41C000, 0x420000, 0x428000, 0x430000, 0x438000, 0x43C000, 0x43E000,
                                           0x43F000, 0x43F800, 0x43FC00, 0x43FE00, 0x43FFF0};
static constexpr size_t PROBE_MARK_COUNT = sizeof(PROBE_MARKS) / sizeof(PROBE_MARKS[0]);

static inline uint32_t rd(uint32_t a) { return *(volatile uint32_t *) a; }

inline esphome::ESPPreferenceObject bkprobe_pref(uint32_t run) {
  return esphome::global_preferences->make_preference<int>(0xBC000000u + run);
}

inline int bkprobe_boot(uint32_t run) {
  using namespace esphome;
  int step = 0;
  bkprobe_pref(run).load(&step);
  unsigned st = bk_misc_get_start_type();
  snprintf(bkprobe_report, sizeof(bkprobe_report), "step=%d reason=%s(%d) start_type=%u low=%08lx itcm=%08lx top=%08lx",
           step, lt_get_reboot_reason_name(lt_get_reboot_reason()), (int) lt_get_reboot_reason(), st,
           (unsigned long) rd(PROBE_CANARY_LOW), (unsigned long) rd(PROBE_CANARY_ITCM),
           (unsigned long) rd(0x0043FFF0u));
  ESP_LOGI("probe", "%s", bkprobe_report);
  for (uint32_t a = 0x400000; a < 0x400100; a += 32) {
    ESP_LOGI("probe", "%06lx: %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx", (unsigned long) a,
             (unsigned long) rd(a), (unsigned long) rd(a + 4), (unsigned long) rd(a + 8), (unsigned long) rd(a + 12),
             (unsigned long) rd(a + 16), (unsigned long) rd(a + 20), (unsigned long) rd(a + 24),
             (unsigned long) rd(a + 28));
  }
  for (size_t i = 0; i < PROBE_MARK_COUNT; i += 7) {
    char line[120];
    int n = 0;
    for (size_t j = i; j < i + 7 && j < PROBE_MARK_COUNT; j++)
      n += snprintf(line + n, sizeof(line) - n, " %06lx=%08lx", (unsigned long) PROBE_MARKS[j],
                    (unsigned long) rd(PROBE_MARKS[j]));
    ESP_LOGI("probe", "marks%s", line);
  }
  uint32_t rec = ((uint32_t) _bss_end + 7) & ~7u;
  for (uint32_t a = rec; a < rec + 64; a += 32) {
    ESP_LOGI("probe", "rec %06lx: %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx", (unsigned long) a,
             (unsigned long) rd(a), (unsigned long) rd(a + 4), (unsigned long) rd(a + 8), (unsigned long) rd(a + 12),
             (unsigned long) rd(a + 16), (unsigned long) rd(a + 20), (unsigned long) rd(a + 24),
             (unsigned long) rd(a + 28));
  }
  *(volatile uint32_t *) PROBE_CANARY_LOW = 0xC0DE0000u | (uint32_t) step;
  *(volatile uint32_t *) PROBE_CANARY_ITCM = 0x17C40000u | (uint32_t) step;
  return step;
}

__attribute__((noinline)) void bkprobe_data_abort() {
  using namespace esphome;
  volatile uint32_t v = rd(0xFFFFFFF0);
  ESP_LOGI("probe", "read 0xFFFFFFF0 -> %08lx (no fault)", (unsigned long) v);
  uint32_t r;
  __asm__ volatile("ldr %0, [%1]" : "=r"(r) : "r"(0x00400101) : "memory");
  ESP_LOGI("probe", "unaligned ldr 0x400101 -> %08lx (no fault)", (unsigned long) r);
  *(volatile uint32_t *) 0xFFFFFFF0 = 1;
  ESP_LOGI("probe", "store 0xFFFFFFF0 (no fault)");
}

// act is the step number before it was bumped
inline void bkprobe_act(uint32_t run, int act) {
  using namespace esphome;
  int next = act + 1;
  bkprobe_pref(run).save(&next);
  global_preferences->sync();
  ESP_LOGI("probe", "acting on step %d (saved %d)", act, next);
  // mark candidate storage areas so the next boot shows what survives
  uint32_t rec = ((uint32_t) _bss_end + 7) & ~7u;
  for (uint32_t i = 0; i < 8; i++)
    *(volatile uint32_t *) (rec + 32 + i * 4) = 0xA5A50000u | i;  // second half of the record area
  for (size_t i = 0; i < PROBE_MARK_COUNT; i++)
    *(volatile uint32_t *) PROBE_MARKS[i] = 0x70700000u | (uint32_t) i;
  ESP_LOGI("probe", "ramtop before: %08lx", (unsigned long) rd(0x0043FFF0u));
  vTaskDelay(pdMS_TO_TICKS(50));
  switch (act) {
    case 0:
      __builtin_trap();  // undefined instruction
      break;
    case 1: {
      void (*fn)() = (void (*)()) 0xFFFFFFF1;  // prefetch abort (unmapped, Thumb)
      fn();
      ESP_LOGI("probe", "call 0xFFFFFFF1 returned (no fault)");
      break;
    }
    case 2:
      bkprobe_data_abort();
      break;
    case 3:
      lt_reboot();
      break;
    default:
      break;
  }
}

inline void bkprobe_alive(int step) {
  using namespace esphome;
  ESP_LOGI("probe", "alive step=%d uptime=%lus boot: %s", step, (unsigned long) (millis() / 1000), bkprobe_report);
}
