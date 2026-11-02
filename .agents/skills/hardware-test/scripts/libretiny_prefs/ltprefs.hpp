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
static char ltprefs_report[128];
// Last boots' reports, kept in a flash preference so boots the host never saw stay readable
struct LtprefsHistory {
  char line[8][128];
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
  // a second pair sharing one key as rtc and as flash, and a third rtc entry, so a hard reset drops three
  auto rtcp2 = global_preferences->make_preference<uint32_t>(0x5B210070u ^ run, false);
  auto flp2 = global_preferences->make_preference<uint32_t>(0x5B210070u ^ run, true);
  auto rtcp3 = global_preferences->make_preference<uint32_t>(0x6B210070u ^ run, false);
  uint32_t rv2 = 0, fv2 = 0, rv3 = 0;
  bool rok = rtcp.load(&rv);
  bool fok = flp.load(&fv);
  bool rok2 = rtcp2.load(&rv2);
  bool fok2 = flp2.load(&fv2);
  bool rok3 = rtcp3.load(&rv3);
  snprintf(ltprefs_report, sizeof(ltprefs_report),
           "step=%d reason=%s(%d) rtc=%s%u flash=%s%u sharedkey rtc=%s%u flash=%s%u rtc3=%s%u", step,
           lt_get_reboot_reason_name(reason), (int) reason, rok ? "" : "none ", rv, fok ? "" : "none ", fv,
           rok2 ? "" : "none ", rv2, fok2 ? "" : "none ", fv2, rok3 ? "" : "none ", rv3);
  ESP_LOGI("lttest", "%s", ltprefs_report);
  auto histp = global_preferences->make_preference<LtprefsHistory>(0x4B210070u ^ run, true);
  if (!histp.load(&ltprefs_hist))
    memset(&ltprefs_hist, 0, sizeof(ltprefs_hist));
  memmove(&ltprefs_hist.line[1], &ltprefs_hist.line[0], sizeof(ltprefs_hist.line) - sizeof(ltprefs_hist.line[0]));
  strncpy(ltprefs_hist.line[0], ltprefs_report, sizeof(ltprefs_hist.line[0]) - 1);
  ltprefs_hist.line[0][sizeof(ltprefs_hist.line[0]) - 1] = 0;
  if (!histp.save(&ltprefs_hist)) {
    ESP_LOGW("lttest", "history save failed");
  }
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
    uint32_t v = 100 + step, v2r = 200 + step, v2f = 300 + step, v3 = 400 + step;
    bool srtc = rtcp.save(&v);
    bool sfl = flp.save(&v);
    bool srtc2 = rtcp2.save(&v2r);
    bool sfl2 = flp2.save(&v2f);
    bool srtc3 = rtcp3.save(&v3);
    bool synced = global_preferences->sync();
    ESP_LOGI("lttest", "save %u/%u/%u/%u: rtc=%d flash=%d sharedkey rtc=%d flash=%d rtc3=%d sync=%d", v, v2r, v2f, v3,
             srtc, sfl, srtc2, sfl2, srtc3, synced);
  }
  ESP_LOGI("lttest", "exit action=%d", act);
  return act;
}

inline void ltprefs_exit(int &step, uint32_t run, int act) {
  step++;
  bool saved = ltprefs_step_pref(run).save(&step);
  bool synced = global_preferences->sync();
  ESP_LOGI("lttest", "step %d save=%d sync=%d", step, saved, synced);
  if (!saved || !synced) {
    ESP_LOGE("lttest", "step not stored, staying idle instead of repeating the step");
    return;
  }
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
