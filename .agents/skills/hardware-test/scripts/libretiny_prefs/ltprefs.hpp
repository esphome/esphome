#pragma once
// BK7238 bench harness for esphome#21007: one rtc and one flash preference, a step counter in flash,
// and an exit action per step so the board drives its own soft reboot, crash and watchdog reboot.
#include <libretiny.h>
#include <cstring>
#include <FreeRTOS.h>
#include <task.h>
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/core/preferences.h"

using namespace esphome;

// Boot report kept for the alive line, so a boot the host could not capture is still visible
static char ltprefs_report[96];
// Last boots' reports, kept in a flash preference so boots the host never saw stay readable
struct LtprefsHistory {
  char line[8][72];
};
static LtprefsHistory ltprefs_hist;

inline ESPPreferenceObject ltprefs_step_pref(uint32_t run) {
  return global_preferences->make_preference<int>(0x3B210070u ^ run, true);
}

// Loads the step and both preferences, logs them, saves this step's value.
// Returns the exit action: 0 idle, 1 lt_reboot, 2 crash, 3 watchdog.
inline int ltprefs_boot(int &step, uint32_t run) {
  auto reason = lt_get_reboot_reason();
  step = 0;
  ltprefs_step_pref(run).load(&step);
  auto rtcp = global_preferences->make_preference<uint32_t>(0x1B210070u ^ run, false);
  auto flp = global_preferences->make_preference<uint32_t>(0x2B210070u ^ run, true);
  uint32_t rv = 0, fv = 0;
  bool rok = rtcp.load(&rv);
  bool fok = flp.load(&fv);
  snprintf(ltprefs_report, sizeof(ltprefs_report), "step=%d reason=%s(%d) rtc=%s%u flash=%s%u", step,
           lt_get_reboot_reason_name(reason), (int) reason, rok ? "" : "none ", rv, fok ? "" : "none ", fv);
  ESP_LOGI("lttest", "%s", ltprefs_report);
  auto histp = global_preferences->make_preference<LtprefsHistory>(0x4B210070u ^ run, true);
  if (!histp.load(&ltprefs_hist))
    memset(&ltprefs_hist, 0, sizeof(ltprefs_hist));
  memmove(&ltprefs_hist.line[1], &ltprefs_hist.line[0], sizeof(ltprefs_hist.line) - sizeof(ltprefs_hist.line[0]));
  strncpy(ltprefs_hist.line[0], ltprefs_report, sizeof(ltprefs_hist.line[0]) - 1);
  ltprefs_hist.line[0][sizeof(ltprefs_hist.line[0]) - 1] = 0;
  histp.save(&ltprefs_hist);
  int act = 0;
  bool save = true;
  switch (step) {
    case 0:
      act = 1;
      break;  // soft reboot
    case 1:
      act = 2;
      break;  // crash
    case 2:
      act = 3;
      break;  // watchdog
    case 3:
      act = 0;
      break;  // idle: chip-enable reset from the host
    case 4:
      act = 1;
      break;  // soft reboot after the hard reset
    case 5:
      act = 0;
      break;  // idle: chip-enable reset from the host
    case 6:
      act = 1;
      save = false;
      break;  // no save, soft reboot: deleted, not skipped?
    case 7:
      act = 0;
      break;  // idle
    case 8:
      act = 2;
      break;  // crash
    case 9:
      act = 0;
      break;  // idle
    case 10:
      act = 0;
      break;  // idle: power cut by the user
    default:
      act = 0;
      save = false;
      break;
  }
  if (save) {
    uint32_t v = 100 + step;
    rtcp.save(&v);
    flp.save(&v);
    global_preferences->sync();
    ESP_LOGI("lttest", "saved %u to rtc and flash, synced", v);
  }
  ESP_LOGI("lttest", "exit action=%d", act);
  return act;
}

inline void ltprefs_exit(int &step, uint32_t run, int act) {
  step++;
  ltprefs_step_pref(run).save(&step);
  global_preferences->sync();
  ESP_LOGI("lttest", "step %d saved and synced", step);
  vTaskDelay(pdMS_TO_TICKS(200));
  switch (act) {
    case 1:
      ESP_LOGW("lttest", "soft reboot now");
      vTaskDelay(pdMS_TO_TICKS(50));
      lt_reboot();
      break;
    case 2:
      ESP_LOGW("lttest", "crashing now");
      vTaskDelay(pdMS_TO_TICKS(50));
      __builtin_trap();  // undefined instruction: a plain bad store does not fault on the ARM968
      break;
    case 3:
      ESP_LOGW("lttest", "watchdog reboot now");
      vTaskDelay(pdMS_TO_TICKS(50));
      lt_reboot_wdt();
      break;
    default:
      ESP_LOGW("lttest", "idle, waiting for an external reset");
      break;
  }
}

inline void ltprefs_alive(int step, int act) {
  ESP_LOGI("lttest", "alive step=%d act=%d boot: %s", step, act, ltprefs_report);
  for (int i = 7; i >= 0; i--) {
    if (ltprefs_hist.line[i][0] != 0) {
      ESP_LOGI("lttest", "  history[-%d]: %s", i, ltprefs_hist.line[i]);
    }
  }
}
