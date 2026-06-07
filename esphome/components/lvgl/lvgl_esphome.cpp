#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "lvgl_esphome.h"

#include "core/lv_obj_class_private.h"
#include "display/lv_display_private.h"
#include "misc/lv_ll.h"

#ifdef USE_MIPI_DSI
#include "esphome/components/mipi_dsi/mipi_dsi.h"
#endif
#ifdef USE_ESP32
#include "esp_cache.h"
#include "esp_heap_caps.h"
#include "esp_memory_utils.h"
#include "esp_timer.h"
#include "esp_private/esp_cache_private.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#endif

#ifdef USE_LVGL_PPA
#include "driver/ppa.h"
extern "C" {
void lv_draw_ppa_init(void);
uint32_t lv_draw_ppa_get_fill_task_count(void);
uint32_t lv_draw_ppa_get_img_task_count(void);
void lvgl_port_ppa_v9_init(lv_display_t *display);
}
#endif

#ifdef USE_LVGL_FPS_BENCHMARK
extern "C" {
void lvgl_fps_attach_v2(lv_display_t *display);
void lvgl_esphome_note_frame(void);
}
#endif

#if LV_USE_PROFILER && LV_USE_PROFILER_BUILTIN
#include "misc/lv_profiler_builtin.h"
#include "misc/lv_profiler_builtin_private.h"
#endif

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <numeric>

namespace esphome::lvgl {
static const char *const TAG = "lvgl";

#ifdef USE_MIPI_DSI
static void lvgl_mipi_async_flush_ready(void *arg) { lv_display_flush_ready(static_cast<lv_display_t *>(arg)); }
#endif

// Published CPU% (real work, flush wait excluded) for the LVGL sysmon
// overlay. Updated each second by loop(). Read by __wrap_lv_timer_get_idle
// below to override lv_sysmon's broken FreeRTOS-mode CPU calculation.
// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
static volatile uint32_t s_cpu_pct = 0;
static volatile uint32_t s_flush_ms = 0;
static volatile uint32_t s_direct_mode_active = 0;
static volatile uint32_t s_loop_max_ms = 0;
static volatile uint32_t s_flush_max_ms = 0;
static volatile uint32_t s_invalidated_kpx = 0;
static volatile uint32_t s_perf_logging_enabled = 0;
static volatile uint32_t s_swipe_logging_enabled = 0;

#if LV_USE_PROFILER && LV_USE_PROFILER_BUILTIN
static volatile uint32_t s_profiler_enabled = 0;
static bool s_profiler_initialized = false;
static constexpr size_t PROFILER_MAX_STACK = 96;
static constexpr size_t PROFILER_MAX_AGG = 96;
static constexpr size_t PROFILER_NAME_LEN = 56;

struct ProfilerStackEntry {
  char name[PROFILER_NAME_LEN];
  uint64_t start_us;
};

struct ProfilerAggEntry {
  char name[PROFILER_NAME_LEN];
  uint32_t count;
  uint64_t total_us;
  uint32_t max_us;
};

static ProfilerStackEntry *s_profiler_stack = nullptr;
static size_t s_profiler_stack_depth = 0;
static ProfilerAggEntry *s_profiler_aggs = nullptr;
static size_t s_profiler_agg_count = 0;
static uint32_t s_profiler_stack_overflow = 0;
static uint32_t s_profiler_parse_drops = 0;
static uint64_t s_profiler_first_us = 0;
static uint64_t s_profiler_last_us = 0;

struct ProfilerManualStats {
  uint64_t started_us;
  uint64_t ended_us;
  uint32_t loop_calls;
  uint64_t loop_total_us;
  uint32_t loop_max_us;
  uint32_t loop_over_30ms;
  uint32_t flush_calls;
  uint64_t flush_total_us;
  uint32_t flush_max_us;
  uint64_t flush_px;
  uint32_t invalidated_areas;
  uint64_t invalidated_px;
  uint32_t ppa_fill_start;
  uint32_t ppa_img_start;
};

static ProfilerManualStats s_profiler_manual = {};

static uint64_t profiler_tick_us() {
#ifdef USE_ESP32
  return (uint64_t) esp_timer_get_time();
#else
  return (uint64_t) micros();
#endif
}

static int profiler_tid_get() { return 1; }

static int profiler_cpu_get() {
#ifdef USE_ESP32
  return xPortGetCoreID();
#else
  return 0;
#endif
}

static void profiler_copy_name(char *dst, const char *src) {
  size_t len = 0;
  while (src[len] != '\0' && src[len] != '\n' && src[len] != '\r' && src[len] != '\x1b' && len < PROFILER_NAME_LEN - 1)
    len++;
  memcpy(dst, src, len);
  dst[len] = '\0';
}

static void profiler_reset_summary() {
  memset(&s_profiler_manual, 0, sizeof(s_profiler_manual));
  s_profiler_manual.started_us = profiler_tick_us();
#ifdef USE_LVGL_PPA
  s_profiler_manual.ppa_fill_start = lv_draw_ppa_get_fill_task_count();
  s_profiler_manual.ppa_img_start = lv_draw_ppa_get_img_task_count();
#endif
  if (s_profiler_stack == nullptr || s_profiler_aggs == nullptr)
    return;
  s_profiler_stack_depth = 0;
  s_profiler_agg_count = 0;
  s_profiler_stack_overflow = 0;
  s_profiler_parse_drops = 0;
  s_profiler_first_us = 0;
  s_profiler_last_us = 0;
  memset(s_profiler_stack, 0, sizeof(ProfilerStackEntry) * PROFILER_MAX_STACK);
  memset(s_profiler_aggs, 0, sizeof(ProfilerAggEntry) * PROFILER_MAX_AGG);
}

static void profiler_note_loop(uint32_t duration_us) {
  if (!s_profiler_enabled)
    return;
  s_profiler_manual.loop_calls++;
  s_profiler_manual.loop_total_us += duration_us;
  if (duration_us > s_profiler_manual.loop_max_us)
    s_profiler_manual.loop_max_us = duration_us;
  if (duration_us > 30000)
    s_profiler_manual.loop_over_30ms++;
}

static void profiler_note_flush(uint32_t duration_us, uint32_t px) {
  if (!s_profiler_enabled)
    return;
  s_profiler_manual.flush_calls++;
  s_profiler_manual.flush_total_us += duration_us;
  if (duration_us > s_profiler_manual.flush_max_us)
    s_profiler_manual.flush_max_us = duration_us;
  s_profiler_manual.flush_px += px;
}

static void profiler_note_invalidated(uint32_t px) {
  if (!s_profiler_enabled)
    return;
  s_profiler_manual.invalidated_areas++;
  s_profiler_manual.invalidated_px += px;
}

static bool profiler_parse_line(const char *buf, char *phase, uint64_t *time_us, const char **name) {
  const char *stamp = strstr(buf, "] ");
  const char *mark = strstr(buf, "tracing_mark_write: ");
  if (stamp == nullptr || mark == nullptr)
    return false;
  stamp += 2;
  char *end = nullptr;
  uint64_t sec = strtoull(stamp, &end, 10);
  if (end == nullptr || *end != '.')
    return false;
  uint64_t nsec = strtoull(end + 1, &end, 10);
  *time_us = sec * 1000000ULL + nsec / 1000ULL;

  const char *payload = mark + strlen("tracing_mark_write: ");
  if ((payload[0] != 'B' && payload[0] != 'E') || payload[1] != '|' || payload[2] != '1' || payload[3] != '|')
    return false;
  *phase = payload[0];
  *name = payload + 4;
  return true;
}

static ProfilerAggEntry *profiler_get_agg(const char *name) {
  char copied[PROFILER_NAME_LEN];
  profiler_copy_name(copied, name);
  for (size_t i = 0; i < s_profiler_agg_count; i++) {
    if (strncmp(s_profiler_aggs[i].name, copied, PROFILER_NAME_LEN) == 0)
      return &s_profiler_aggs[i];
  }
  if (s_profiler_agg_count >= PROFILER_MAX_AGG)
    return nullptr;
  ProfilerAggEntry *entry = &s_profiler_aggs[s_profiler_agg_count++];
  strncpy(entry->name, copied, PROFILER_NAME_LEN - 1);
  entry->name[PROFILER_NAME_LEN - 1] = '\0';
  entry->count = 0;
  entry->total_us = 0;
  entry->max_us = 0;
  return entry;
}

static void profiler_add_duration(const char *name, uint32_t duration_us) {
  ProfilerAggEntry *entry = profiler_get_agg(name);
  if (entry == nullptr) {
    s_profiler_parse_drops++;
    return;
  }
  entry->count++;
  entry->total_us += duration_us;
  if (duration_us > entry->max_us)
    entry->max_us = duration_us;
}

static void profiler_flush_cb(const char *buf) {
  if (buf == nullptr || s_profiler_stack == nullptr || s_profiler_aggs == nullptr)
    return;
  char phase = '\0';
  uint64_t time_us = 0;
  const char *name = nullptr;
  if (!profiler_parse_line(buf, &phase, &time_us, &name)) {
    if (buf[0] != '#')
      s_profiler_parse_drops++;
    return;
  }
  if (s_profiler_first_us == 0)
    s_profiler_first_us = time_us;
  s_profiler_last_us = time_us;

  if (phase == 'B') {
    if (s_profiler_stack_depth >= PROFILER_MAX_STACK) {
      s_profiler_stack_overflow++;
      return;
    }
    ProfilerStackEntry *entry = &s_profiler_stack[s_profiler_stack_depth++];
    profiler_copy_name(entry->name, name);
    entry->start_us = time_us;
    return;
  }

  for (size_t i = s_profiler_stack_depth; i > 0; i--) {
    ProfilerStackEntry *entry = &s_profiler_stack[i - 1];
    if (strncmp(entry->name, name, PROFILER_NAME_LEN) == 0) {
      uint64_t duration = time_us >= entry->start_us ? time_us - entry->start_us : 0;
      profiler_add_duration(name, duration > UINT32_MAX ? UINT32_MAX : (uint32_t) duration);
      s_profiler_stack_depth = i - 1;
      return;
    }
  }
  s_profiler_parse_drops++;
}

static void profiler_print_summary() {
  if (s_profiler_aggs == nullptr)
    return;
  uint64_t window_us = s_profiler_last_us > s_profiler_first_us ? s_profiler_last_us - s_profiler_first_us : 0;
  ESP_LOGI("lvgl.prof", "SUMMARY window=%lluus unique=%u drops=%u stack_overflow=%u open=%u",
           (unsigned long long) window_us, (unsigned) s_profiler_agg_count, (unsigned) s_profiler_parse_drops,
           (unsigned) s_profiler_stack_overflow, (unsigned) s_profiler_stack_depth);

  bool selected[PROFILER_MAX_AGG] = {};
  const size_t limit = std::min<size_t>(12, s_profiler_agg_count);
  for (size_t rank = 0; rank < limit; rank++) {
    size_t best = PROFILER_MAX_AGG;
    for (size_t i = 0; i < s_profiler_agg_count; i++) {
      if (selected[i])
        continue;
      if (best == PROFILER_MAX_AGG || s_profiler_aggs[i].total_us > s_profiler_aggs[best].total_us)
        best = i;
    }
    if (best == PROFILER_MAX_AGG)
      break;
    selected[best] = true;
    const ProfilerAggEntry *entry = &s_profiler_aggs[best];
    uint32_t avg_us = entry->count == 0 ? 0 : (uint32_t) (entry->total_us / entry->count);
    ESP_LOGI("lvgl.prof", "COST rank=%u total=%lluus avg=%uus max=%uus count=%u name=%s", (unsigned) (rank + 1),
             (unsigned long long) entry->total_us, (unsigned) avg_us, (unsigned) entry->max_us, (unsigned) entry->count,
             entry->name);
  }
}

static void profiler_print_manual_summary() {
  s_profiler_manual.ended_us = profiler_tick_us();
  uint64_t window_us = s_profiler_manual.ended_us > s_profiler_manual.started_us
                           ? s_profiler_manual.ended_us - s_profiler_manual.started_us
                           : 0;
  uint32_t loop_avg_us = s_profiler_manual.loop_calls == 0
                             ? 0
                             : (uint32_t) (s_profiler_manual.loop_total_us / s_profiler_manual.loop_calls);
  uint32_t flush_avg_us = s_profiler_manual.flush_calls == 0
                              ? 0
                              : (uint32_t) (s_profiler_manual.flush_total_us / s_profiler_manual.flush_calls);
  uint32_t ppa_fill_delta = 0;
  uint32_t ppa_img_delta = 0;
#ifdef USE_LVGL_PPA
  uint32_t ppa_fill_now = lv_draw_ppa_get_fill_task_count();
  uint32_t ppa_img_now = lv_draw_ppa_get_img_task_count();
  ppa_fill_delta = ppa_fill_now - s_profiler_manual.ppa_fill_start;
  ppa_img_delta = ppa_img_now - s_profiler_manual.ppa_img_start;
#endif
  ESP_LOGI("lvgl.prof",
           "PROFILE_COST window=%lluus loop_calls=%u loop_total=%lluus loop_avg=%uus loop_max=%uus loop_over_30ms=%u",
           (unsigned long long) window_us, (unsigned) s_profiler_manual.loop_calls,
           (unsigned long long) s_profiler_manual.loop_total_us, (unsigned) loop_avg_us,
           (unsigned) s_profiler_manual.loop_max_us, (unsigned) s_profiler_manual.loop_over_30ms);
  ESP_LOGI("lvgl.prof", "PROFILE_COST flush_calls=%u flush_total=%lluus flush_avg=%uus flush_max=%uus flush_px=%llukpx",
           (unsigned) s_profiler_manual.flush_calls, (unsigned long long) s_profiler_manual.flush_total_us,
           (unsigned) flush_avg_us, (unsigned) s_profiler_manual.flush_max_us,
           (unsigned long long) (s_profiler_manual.flush_px / 1000ULL));
  ESP_LOGI("lvgl.prof", "PROFILE_COST invalidated=%u areas/%llukpx ppa_delta=%u/%u",
           (unsigned) s_profiler_manual.invalidated_areas,
           (unsigned long long) (s_profiler_manual.invalidated_px / 1000ULL), (unsigned) ppa_fill_delta,
           (unsigned) ppa_img_delta);
}

static void profiler_init_custom() {
  if (s_profiler_stack == nullptr) {
#ifdef USE_ESP32
    s_profiler_stack = static_cast<ProfilerStackEntry *>(
        heap_caps_calloc(PROFILER_MAX_STACK, sizeof(ProfilerStackEntry), MALLOC_CAP_SPIRAM));
#endif
    if (s_profiler_stack == nullptr)
      s_profiler_stack = static_cast<ProfilerStackEntry *>(calloc(PROFILER_MAX_STACK, sizeof(ProfilerStackEntry)));
  }
  if (s_profiler_aggs == nullptr) {
#ifdef USE_ESP32
    s_profiler_aggs = static_cast<ProfilerAggEntry *>(
        heap_caps_calloc(PROFILER_MAX_AGG, sizeof(ProfilerAggEntry), MALLOC_CAP_SPIRAM));
#endif
    if (s_profiler_aggs == nullptr)
      s_profiler_aggs = static_cast<ProfilerAggEntry *>(calloc(PROFILER_MAX_AGG, sizeof(ProfilerAggEntry)));
  }
  if (s_profiler_stack == nullptr || s_profiler_aggs == nullptr) {
    ESP_LOGW("lvgl.prof", "profiler summary buffers allocation failed");
    return;
  }
  lv_profiler_builtin_config_t config;
  lv_profiler_builtin_config_init(&config);
  config.buf_size = LV_PROFILER_BUILTIN_BUF_SIZE;
  config.tick_per_sec = 1000000;
  config.tick_get_cb = profiler_tick_us;
  config.flush_cb = profiler_flush_cb;
  config.tid_get_cb = profiler_tid_get;
  config.cpu_get_cb = profiler_cpu_get;
  lv_profiler_builtin_init(&config);
  lv_profiler_builtin_set_enable(false);
  s_profiler_enabled = 0;
  s_profiler_initialized = true;
}
#else
static volatile uint32_t s_profiler_enabled = 0;
static bool s_profiler_initialized = false;

struct ProfilerManualStats {
  uint64_t started_us;
  uint64_t ended_us;
  uint32_t loop_calls;
  uint64_t loop_total_us;
  uint32_t loop_max_us;
  uint32_t loop_over_30ms;
  uint32_t flush_calls;
  uint64_t flush_total_us;
  uint32_t flush_max_us;
  uint64_t flush_px;
  uint32_t invalidated_areas;
  uint64_t invalidated_px;
  uint32_t ppa_fill_start;
  uint32_t ppa_img_start;
};

static ProfilerManualStats s_profiler_manual = {};

static uint64_t profiler_tick_us() {
#ifdef USE_ESP32
  return (uint64_t) esp_timer_get_time();
#else
  return (uint64_t) micros();
#endif
}

static void profiler_reset_summary() {
  memset(&s_profiler_manual, 0, sizeof(s_profiler_manual));
  s_profiler_manual.started_us = profiler_tick_us();
#ifdef USE_LVGL_PPA
  s_profiler_manual.ppa_fill_start = lv_draw_ppa_get_fill_task_count();
  s_profiler_manual.ppa_img_start = lv_draw_ppa_get_img_task_count();
#endif
}

static void profiler_note_loop(uint32_t duration_us) {
  if (!s_profiler_enabled)
    return;
  s_profiler_manual.loop_calls++;
  s_profiler_manual.loop_total_us += duration_us;
  if (duration_us > s_profiler_manual.loop_max_us)
    s_profiler_manual.loop_max_us = duration_us;
  if (duration_us > 30000)
    s_profiler_manual.loop_over_30ms++;
}

static void profiler_note_flush(uint32_t duration_us, uint32_t px) {
  if (!s_profiler_enabled)
    return;
  s_profiler_manual.flush_calls++;
  s_profiler_manual.flush_total_us += duration_us;
  if (duration_us > s_profiler_manual.flush_max_us)
    s_profiler_manual.flush_max_us = duration_us;
  s_profiler_manual.flush_px += px;
}

static void profiler_note_invalidated(uint32_t px) {
  if (!s_profiler_enabled)
    return;
  s_profiler_manual.invalidated_areas++;
  s_profiler_manual.invalidated_px += px;
}

static void profiler_print_manual_summary() {
  s_profiler_manual.ended_us = profiler_tick_us();
  uint64_t window_us = s_profiler_manual.ended_us > s_profiler_manual.started_us
                           ? s_profiler_manual.ended_us - s_profiler_manual.started_us
                           : 0;
  uint32_t loop_avg_us = s_profiler_manual.loop_calls == 0
                             ? 0
                             : (uint32_t) (s_profiler_manual.loop_total_us / s_profiler_manual.loop_calls);
  uint32_t flush_avg_us = s_profiler_manual.flush_calls == 0
                              ? 0
                              : (uint32_t) (s_profiler_manual.flush_total_us / s_profiler_manual.flush_calls);
  uint32_t ppa_fill_delta = 0;
  uint32_t ppa_img_delta = 0;
#ifdef USE_LVGL_PPA
  uint32_t ppa_fill_now = lv_draw_ppa_get_fill_task_count();
  uint32_t ppa_img_now = lv_draw_ppa_get_img_task_count();
  ppa_fill_delta = ppa_fill_now - s_profiler_manual.ppa_fill_start;
  ppa_img_delta = ppa_img_now - s_profiler_manual.ppa_img_start;
#endif
  ESP_LOGI("lvgl.prof",
           "PROFILE_COST window=%lluus loop_calls=%u loop_total=%lluus loop_avg=%uus loop_max=%uus loop_over_30ms=%u",
           (unsigned long long) window_us, (unsigned) s_profiler_manual.loop_calls,
           (unsigned long long) s_profiler_manual.loop_total_us, (unsigned) loop_avg_us,
           (unsigned) s_profiler_manual.loop_max_us, (unsigned) s_profiler_manual.loop_over_30ms);
  ESP_LOGI("lvgl.prof", "PROFILE_COST flush_calls=%u flush_total=%lluus flush_avg=%uus flush_max=%uus flush_px=%llukpx",
           (unsigned) s_profiler_manual.flush_calls, (unsigned long long) s_profiler_manual.flush_total_us,
           (unsigned) flush_avg_us, (unsigned) s_profiler_manual.flush_max_us,
           (unsigned long long) (s_profiler_manual.flush_px / 1000ULL));
  ESP_LOGI("lvgl.prof", "PROFILE_COST invalidated=%u areas/%llukpx ppa_delta=%u/%u",
           (unsigned) s_profiler_manual.invalidated_areas,
           (unsigned long long) (s_profiler_manual.invalidated_px / 1000ULL), (unsigned) ppa_fill_delta,
           (unsigned) ppa_img_delta);
}

static void profiler_init_custom() {
  s_profiler_enabled = 0;
  s_profiler_initialized = true;
}
#endif
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome::lvgl

extern "C" uint32_t lvgl_esphome_get_cpu_pct(void) {
  uint32_t cpu = esphome::lvgl::s_cpu_pct;
  return cpu > 100 ? 100 : cpu;
}

extern "C" uint32_t lvgl_esphome_get_flush_ms(void) { return esphome::lvgl::s_flush_ms; }

extern "C" uint32_t lvgl_esphome_get_direct_mode_active(void) { return esphome::lvgl::s_direct_mode_active; }

extern "C" uint32_t lvgl_esphome_get_loop_max_ms(void) { return esphome::lvgl::s_loop_max_ms; }

extern "C" uint32_t lvgl_esphome_get_flush_max_ms(void) { return esphome::lvgl::s_flush_max_ms; }

extern "C" uint32_t lvgl_esphome_get_invalidated_kpx(void) { return esphome::lvgl::s_invalidated_kpx; }

extern "C" uint32_t lvgl_esphome_get_perf_logging_enabled(void) { return esphome::lvgl::s_perf_logging_enabled; }

extern "C" uint32_t lvgl_esphome_get_swipe_logging_enabled(void) { return esphome::lvgl::s_swipe_logging_enabled; }

extern "C" void lvgl_esphome_set_perf_logging_enabled(bool enabled) {
  esphome::lvgl::s_perf_logging_enabled = enabled ? 1 : 0;
}

extern "C" void lvgl_esphome_set_swipe_logging_enabled(bool enabled) {
  esphome::lvgl::s_swipe_logging_enabled = enabled ? 1 : 0;
}

extern "C" uint32_t lvgl_esphome_get_profiler_enabled(void) { return esphome::lvgl::s_profiler_enabled; }

extern "C" void lvgl_esphome_set_profiler_enabled(bool enabled) {
  if (!esphome::lvgl::s_profiler_initialized)
    esphome::lvgl::profiler_init_custom();
  if (enabled)
    esphome::lvgl::profiler_reset_summary();
  esphome::lvgl::s_profiler_enabled = enabled ? 1 : 0;
#if LV_USE_PROFILER && LV_USE_PROFILER_BUILTIN
  // Enable LVGL's built-in trace only for short diagnostic windows. The
  // always-on path remains the lightweight manual sampler.
  lv_profiler_builtin_set_enable(enabled);
  ESP_LOGI("lvgl.prof", "profiler %s (builtin trace + manual sampler)", enabled ? "enabled" : "disabled");
#else
  ESP_LOGI("lvgl.prof", "profiler %s (manual sampler)", enabled ? "enabled" : "disabled");
#endif
}

extern "C" void lvgl_esphome_profiler_flush(void) {
  if (!esphome::lvgl::s_profiler_initialized)
    return;
#if LV_USE_PROFILER && LV_USE_PROFILER_BUILTIN
  lv_profiler_builtin_flush();
  lv_profiler_builtin_set_enable(false);
#endif
  esphome::lvgl::s_profiler_enabled = 0;
  esphome::lvgl::profiler_print_manual_summary();
#if LV_USE_PROFILER && LV_USE_PROFILER_BUILTIN
  if (esphome::lvgl::s_profiler_agg_count > 0)
    esphome::lvgl::profiler_print_summary();
#endif
  ESP_LOGI("lvgl.prof", "profiler flushed");
}

extern "C" void lvgl_esphome_profiler_mark(const char *name) {
  if (!esphome::lvgl::s_profiler_initialized || !esphome::lvgl::s_profiler_enabled || name == nullptr)
    return;
  ESP_LOGI("lvgl.prof", "PROFILE_MARK t=%lluus name=%s", (unsigned long long) esphome::lvgl::profiler_tick_us(), name);
}

// Linker wrap (PlatformIO LDFLAGs -Wl,--wrap=lv_timer_get_idle and
// -Wl,--wrap=lv_os_get_idle_percent).
// LVGL sysmon's perf widget reads CPU%% via lv_os_get_idle_percent()
// when LV_USE_OS=LV_OS_FREERTOS (and via lv_timer_get_idle() under
// LV_OS_NONE). Wrap both so the overlay reads our s_cpu_pct regardless
// of the OS mode. Returns 100 - cpu, the "idle %" sysmon expects.
// NOLINTNEXTLINE(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)
extern "C" uint32_t __wrap_lv_timer_get_idle(void) {
  uint32_t cpu = esphome::lvgl::s_cpu_pct;
  if (cpu > 100)
    cpu = 100;
  return 100 - cpu;
}

// NOLINTNEXTLINE(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)
extern "C" uint32_t __wrap_lv_os_get_idle_percent(void) {
  uint32_t cpu = esphome::lvgl::s_cpu_pct;
  if (cpu > 100)
    cpu = 100;
  return 100 - cpu;
}

namespace esphome::lvgl {

#ifdef USE_LVGL_PPA
/// Dedicated PPA SRM client for display framebuffer rotation (separate from LVGL draw unit).
static ppa_client_handle_t s_display_srm_client = nullptr;
/// Dedicated PPA SRM client for the partial framebuffer compositor.
static ppa_client_handle_t s_compositor_srm_client = nullptr;
static size_t s_compositor_srm_alignment = 0;

/**
 * Attempt to rotate a display framebuffer using the PPA SRM hardware.
 * Returns true if PPA rotation succeeded, false if software fallback is needed.
 *
 * Angle mapping (PPA uses CCW, ESPHome uses CW):
 *   90 deg CW  -> PPA_SRM_ROTATION_ANGLE_270 (270 deg CCW)
 *   180 deg    -> PPA_SRM_ROTATION_ANGLE_180
 *   270 deg CW -> PPA_SRM_ROTATION_ANGLE_90  (90 deg CCW)
 */
static bool ppa_rotate_display_buf(const void *src, void *dst, int32_t w, int32_t h, display::DisplayRotation rot) {
  if (s_display_srm_client == nullptr || w < 2 || h < 2)
    return false;

  // ESP32-P4 PPA requires both buffer address and buffer_size to be aligned
  // to the data cache line size - 64 B by default, 128 B if
  // CONFIG_CACHE_L2_CACHE_LINE_128B=y. Use the larger value so the check
  // passes under both sdkconfigs.
  constexpr uintptr_t CACHE_LINE = 128;
  if ((reinterpret_cast<uintptr_t>(src) & (CACHE_LINE - 1)) != 0)
    return false;
  if ((reinterpret_cast<uintptr_t>(dst) & (CACHE_LINE - 1)) != 0)
    return false;

  ppa_srm_rotation_angle_t ppa_angle;
  int32_t out_w, out_h;
  switch (rot) {
    case display::DISPLAY_ROTATION_90_DEGREES:
      ppa_angle = PPA_SRM_ROTATION_ANGLE_270;
      out_w = h;
      out_h = w;
      break;
    case display::DISPLAY_ROTATION_180_DEGREES:
      ppa_angle = PPA_SRM_ROTATION_ANGLE_180;
      out_w = w;
      out_h = h;
      break;
    case display::DISPLAY_ROTATION_270_DEGREES:
      ppa_angle = PPA_SRM_ROTATION_ANGLE_90;
      out_w = h;
      out_h = w;
      break;
    default:
      return false;
  }

#if LV_COLOR_DEPTH == 32
  constexpr ppa_srm_color_mode_t PPA_CM = PPA_SRM_COLOR_MODE_RGB888;
  constexpr size_t BPP = 3;
#else
  constexpr ppa_srm_color_mode_t PPA_CM = PPA_SRM_COLOR_MODE_RGB565;
  constexpr size_t BPP = 2;
#endif

  size_t out_bytes = (size_t) out_w * out_h * BPP;
  size_t aligned_out_bytes = (out_bytes + CACHE_LINE - 1) & ~(CACHE_LINE - 1);

  ppa_srm_oper_config_t cfg = {};
  cfg.in.buffer = (void *) src;
  cfg.in.pic_w = w;
  cfg.in.pic_h = h;
  cfg.in.block_w = w;
  cfg.in.block_h = h;
  cfg.in.srm_cm = PPA_CM;
  cfg.out.buffer = dst;
  cfg.out.buffer_size = aligned_out_bytes;
  cfg.out.pic_w = out_w;
  cfg.out.pic_h = out_h;
  cfg.out.srm_cm = PPA_CM;
  cfg.rotation_angle = ppa_angle;
  cfg.scale_x = 1.0f;  // must be 1.0f, not 0.0f (default after zero-init)
  cfg.scale_y = 1.0f;
  cfg.alpha_update_mode = PPA_ALPHA_NO_CHANGE;
  cfg.mode = PPA_TRANS_MODE_BLOCKING;

  esp_err_t ret = ppa_do_scale_rotate_mirror(s_display_srm_client, &cfg);
  if (ret != ESP_OK) {
    static bool warned = false;
    if (!warned) {
      ESP_LOGW(TAG, "PPA display rotation unavailable (err=%d), using SW fallback", ret);
      warned = true;
    }
  }
  return ret == ESP_OK;
}
#endif  // USE_LVGL_PPA

static const size_t MIN_BUFFER_FRAC = 8;

static const char *const EVENT_NAMES[] = {
    "NONE",
    "PRESSED",
    "PRESSING",
    "PRESS_LOST",
    "SHORT_CLICKED",
    "LONG_PRESSED",
    "LONG_PRESSED_REPEAT",
    "CLICKED",
    "RELEASED",
    "SCROLL_BEGIN",
    "SCROLL_END",
    "SCROLL",
    "GESTURE",
    "KEY",
    "FOCUSED",
    "DEFOCUSED",
    "LEAVE",
    "HIT_TEST",
    "COVER_CHECK",
    "REFR_EXT_DRAW_SIZE",
    "DRAW_MAIN_BEGIN",
    "DRAW_MAIN",
    "DRAW_MAIN_END",
    "DRAW_POST_BEGIN",
    "DRAW_POST",
    "DRAW_POST_END",
    "DRAW_PART_BEGIN",
    "DRAW_PART_END",
    "VALUE_CHANGED",
    "INSERT",
    "REFRESH",
    "READY",
    "CANCEL",
    "DELETE",
    "CHILD_CHANGED",
    "CHILD_CREATED",
    "CHILD_DELETED",
    "SCREEN_UNLOAD_START",
    "SCREEN_LOAD_START",
    "SCREEN_LOADED",
    "SCREEN_UNLOADED",
    "SIZE_CHANGED",
    "STYLE_CHANGED",
    "LAYOUT_CHANGED",
    "GET_SELF_SIZE",
};

static const unsigned LOG_LEVEL_MAP[] = {
    ESPHOME_LOG_LEVEL_DEBUG, ESPHOME_LOG_LEVEL_INFO,  ESPHOME_LOG_LEVEL_WARN,
    ESPHOME_LOG_LEVEL_ERROR, ESPHOME_LOG_LEVEL_ERROR, ESPHOME_LOG_LEVEL_NONE,

};

std::string lv_event_code_name_for(lv_event_t *event) {
  auto event_code = lv_event_get_code(event);
  if (event_code < sizeof(EVENT_NAMES) / sizeof(EVENT_NAMES[0])) {
    return EVENT_NAMES[event_code];
  }
  char buffer[8];
  snprintf(buffer, sizeof(buffer), "%2u", static_cast<unsigned>(event_code));
  return buffer;
}

static void rounder_cb(lv_event_t *event) {
  auto *comp = static_cast<LvglComponent *>(lv_event_get_user_data(event));
  auto *area = static_cast<lv_area_t *>(lv_event_get_param(event));
  // cater for display driver chips with special requirements for bounds of partial
  // draw areas. Extend the draw area to satisfy:
  // * Coordinates must be a multiple of draw_rounding
  auto draw_rounding = comp->draw_rounding;
  // round down the start coordinates
  area->x1 = area->x1 / draw_rounding * draw_rounding;
  area->y1 = area->y1 / draw_rounding * draw_rounding;
  // round up the end coordinates
  area->x2 = (area->x2 + draw_rounding) / draw_rounding * draw_rounding - 1;
  area->y2 = (area->y2 + draw_rounding) / draw_rounding * draw_rounding - 1;
  comp->record_invalidated_area(area);
}

void LvglComponent::record_invalidated_area(const lv_area_t *area) {
  uint32_t px = (uint32_t) lv_area_get_width(area) * (uint32_t) lv_area_get_height(area);
  this->perf_invalidated_px_ += px;
  this->perf_invalidated_areas_++;
  profiler_note_invalidated(px);
}

void LvglComponent::render_end_cb(lv_event_t *event) {
  auto *comp = static_cast<LvglComponent *>(lv_event_get_user_data(event));
  comp->draw_end_();
}

void LvglComponent::render_start_cb(lv_event_t *event) {
  ESP_LOGVV(TAG, "Draw start");
  auto *comp = static_cast<LvglComponent *>(lv_event_get_user_data(event));
  comp->draw_start_();
}

lv_event_code_t lv_update_event;  // NOLINT
void LvglComponent::dump_config() {
  ESP_LOGCONFIG(TAG,
                "LVGL:\n"
                "  Display width/height: %d x %d\n"
                "  Buffer size: %zu%%\n"
                "  Rotation: %d\n"
                "  Draw rounding: %d",
                this->width_, this->height_, 100 / this->buffer_frac_, this->rotation, (int) this->draw_rounding);
  if (this->rotation_type_ != ROTATION_UNUSED) {
    ESP_LOGCONFIG(TAG, "  Rotation type: %s",
                  this->rotation_type_ == RotationType::ROTATION_HARDWARE ? "hardware via display driver" : "software");
  }
#ifdef USE_LVGL_PPA
  ESP_LOGCONFIG(TAG, "  PPA SRM (display rotation): %s",
                s_display_srm_client != nullptr ? "registered (HW)" : "failed (SW fallback)");
  ESP_LOGCONFIG(TAG, "  PPA SW-blend handler (v9):  registered (RGB565 fills/blends -> HW)");
  ESP_LOGCONFIG(TAG, "  PPA draw unit:              registered (canvas/image -> HW)");
#else
  ESP_LOGCONFIG(TAG, "  PPA acceleration: disabled (use_ppa: false)");
#endif
}

void LvglComponent::set_rotation(display::DisplayRotation rotation) {
  if (this->rotation_type_ == RotationType::ROTATION_UNUSED) {
    ESP_LOGW(TAG, "Display rotation cannot be changed unless rotation was enabled during setup.");
    return;
  }
  this->rotation = rotation;
  if (this->is_ready()) {
    this->set_resolution_();
    lv_obj_update_layout(this->get_screen_active());
    lv_obj_invalidate(this->get_screen_active());
  }
}

void LvglComponent::set_resolution_() const {
  int32_t width = this->width_;
  int32_t height = this->height_;
  if (this->rotation == display::DISPLAY_ROTATION_90_DEGREES ||
      this->rotation == display::DISPLAY_ROTATION_270_DEGREES) {
    std::swap(width, height);
  }
  if (this->rotation_type_ == RotationType::ROTATION_HARDWARE) {
    for (auto *disp : this->displays_)
      disp->set_rotation(this->rotation);
  } else if (this->rotation_type_ == RotationType::ROTATION_SOFTWARE) {
    for (auto *disp : this->displays_)
      disp->set_rotation(display::DISPLAY_ROTATION_0_DEGREES);
  }
  lv_display_set_resolution(this->disp_, width, height);
}

void LvglComponent::set_paused(bool paused, bool show_snow) {
  this->paused_ = paused;
  this->show_snow_ = show_snow;
  if (!paused && lv_screen_active() != nullptr) {
    lv_display_trigger_activity(this->disp_);  // resets the inactivity time
    lv_obj_invalidate(lv_screen_active());
  }
  if (paused && this->pause_callback_ != nullptr)
    this->pause_callback_->trigger();
  if (!paused && this->resume_callback_ != nullptr)
    this->resume_callback_->trigger();
}

void LvglComponent::esphome_lvgl_init() {
  lv_init();
#ifdef USE_LVGL_PPA
  // Two PPA paths active at once for max coverage:
  //
  //   1) lv_draw_ppa unit (full draw unit, lv_draw_ppa_init)
  //      -> accelerates IMAGE draw tasks (canvas widget, lv_image)
  //      -> critical for camera streaming through lv_canvas
  //
  //   2) lvgl_ppa_accel_v9 (SW-blend handler, lvgl_port_ppa_v9_init)
  //      -> accelerates RGB565 fills/blends in the SW pipeline
  //      -> catches what the draw unit rejects (radius != 0, opa < max,
  //        gradients, etc.)
  //
  // Espressif's esp_lvgl_adapter only uses (2), but that leaves canvas/
  // image draws going through the slow SW image renderer. With a 640x480
  // RGB565 camera canvas, this added ~50 ms of LVGL overhead per frame.
  // Enabling (1) brings image drawing back onto PPA hardware.
  lv_draw_ppa_init();

  // Register a dedicated PPA SRM client for display framebuffer rotation.
  // This is independent of the LVGL draw pipeline and stays enabled.
  if (s_display_srm_client == nullptr) {
    ppa_client_config_t srm_cfg = {};
    srm_cfg.oper_type = PPA_OPERATION_SRM;
    srm_cfg.max_pending_trans_num = 1;
    srm_cfg.data_burst_length = PPA_DATA_BURST_LENGTH_128;
    if (ppa_register_client(&srm_cfg, &s_display_srm_client) == ESP_OK) {
      ESP_LOGI(TAG, "PPA display rotation SRM client registered");
    } else {
      ESP_LOGW(TAG, "PPA display rotation SRM client failed, SW rotation will be used");
      s_display_srm_client = nullptr;
    }
  }
#endif
  lv_tick_set_cb([] { return millis(); });
#if LV_USE_PROFILER && LV_USE_PROFILER_BUILTIN
  profiler_init_custom();
#endif
  lv_update_event = static_cast<lv_event_code_t>(lv_event_register_id());
}

void LvglComponent::add_event_cb(lv_obj_t *obj, event_callback_t callback, lv_event_code_t event) {
  lv_obj_add_event_cb(obj, callback, event, nullptr);
}

void LvglComponent::add_event_cb(lv_obj_t *obj, event_callback_t callback, lv_event_code_t event1,
                                 lv_event_code_t event2) {
  add_event_cb(obj, callback, event1);
  add_event_cb(obj, callback, event2);
}

void LvglComponent::add_event_cb(lv_obj_t *obj, event_callback_t callback, lv_event_code_t event1,
                                 lv_event_code_t event2, lv_event_code_t event3) {
  add_event_cb(obj, callback, event1);
  add_event_cb(obj, callback, event2);
  add_event_cb(obj, callback, event3);
}

void LvglComponent::add_page(LvPageType *page) {
  this->pages_.push_back(page);
  page->set_parent(this);
  lv_display_set_default(this->disp_);
  page->setup(this->pages_.size() - 1);
}

void LvglComponent::show_page(size_t index, lv_screen_load_anim_t anim, uint32_t time) {
  if (index >= this->pages_.size())
    return;
  this->current_page_ = index;
  if (anim == LV_SCREEN_LOAD_ANIM_NONE) {
    lv_screen_load(this->pages_[this->current_page_]->obj);
  } else {
    lv_screen_load_anim(this->pages_[this->current_page_]->obj, anim, time, 0, false);
  }
}

void LvglComponent::show_next_page(lv_screen_load_anim_t anim, uint32_t time) {
  if (this->pages_.empty() || (this->current_page_ == this->pages_.size() - 1 && !this->page_wrap_))
    return;
  size_t start = this->current_page_;
  do {
    this->current_page_ = (this->current_page_ + 1) % this->pages_.size();
    if (this->current_page_ == start)
      return;  // all pages have skip=true (guaranteed not to happen by YAML validation)
  } while (this->pages_[this->current_page_]->skip);  // skip empty pages()
  this->show_page(this->current_page_, anim, time);
}

void LvglComponent::show_prev_page(lv_screen_load_anim_t anim, uint32_t time) {
  if (this->pages_.empty() || (this->current_page_ == 0 && !this->page_wrap_))
    return;
  size_t start = this->current_page_;
  do {
    this->current_page_ = (this->current_page_ + this->pages_.size() - 1) % this->pages_.size();
    if (this->current_page_ == start)
      return;  // all pages have skip=true (guaranteed not to happen by YAML validation)
  } while (this->pages_[this->current_page_]->skip);  // skip empty pages()
  this->show_page(this->current_page_, anim, time);
}

size_t LvglComponent::get_current_page() const { return this->current_page_; }
bool LvPageType::is_showing() const { return this->parent_->get_current_page() == this->index; }

void LvglComponent::draw_buffer_(const lv_area_t *area, lv_color_data *ptr) {
  auto width = lv_area_get_width(area);
  auto height = lv_area_get_height(area);
  auto height_rounded = (height + this->draw_rounding - 1) / this->draw_rounding * this->draw_rounding;
  auto x1 = area->x1;
  auto y1 = area->y1;
  auto *dst = reinterpret_cast<lv_color_data *>(this->rotate_buf_);
  const auto *src8 = reinterpret_cast<const uint8_t *>(ptr);
  const bool direct_full_buffer =
      this->direct_mode_active_ &&
      (src8 == this->draw_buf_ || (this->draw_buf2_ != nullptr && src8 == this->draw_buf2_));

#ifdef USE_LVGL_PPA
  // Try PPA hardware rotation first (zero CPU cost, ~10x faster than SW loops).
  // Falls back to software automatically if PPA rejects the operation.
  if (this->rotation_type_ == RotationType::ROTATION_SOFTWARE && s_display_srm_client != nullptr &&
      this->rotation != display::DISPLAY_ROTATION_0_DEGREES) {
    if (ppa_rotate_display_buf(ptr, this->rotate_buf_, width, height, this->rotation)) {
      // dst already points to rotate_buf_ (initialized above)
      // Coordinate update: identical geometry to the software path
      switch (this->rotation) {
        case display::DISPLAY_ROTATION_90_DEGREES:
          y1 = x1;
          x1 = this->height_ - area->y1 - height;
          height = width;
          width = height_rounded;
          break;
        case display::DISPLAY_ROTATION_180_DEGREES:
          x1 = this->width_ - x1 - width;
          y1 = this->height_ - y1 - height;
          break;
        case display::DISPLAY_ROTATION_270_DEGREES:
          x1 = y1;
          y1 = this->width_ - area->x1 - width;
          height = width;
          width = height_rounded;
          break;
        default:
          break;
      }
      for (auto *display : this->displays_) {
        display->draw_pixels_at(x1, y1, width, height, (const uint8_t *) dst, display::COLOR_ORDER_RGB, LV_BITNESS,
                                this->big_endian_);
      }
      return;
    }
    // PPA failed -> fall through to software rotation below
  }
#endif  // USE_LVGL_PPA

  switch (this->rotation_type_ == RotationType::ROTATION_SOFTWARE ? this->rotation
                                                                  : display::DISPLAY_ROTATION_0_DEGREES) {
    case display::DISPLAY_ROTATION_90_DEGREES:
#if LV_COLOR_DEPTH == 32
    {
      // RGB888: 3 bytes per pixel
      auto *dst8 = reinterpret_cast<uint8_t *>(this->rotate_buf_);
      auto *ptr8 = reinterpret_cast<const uint8_t *>(ptr);
      for (lv_coord_t x = height; x-- != 0;) {
        for (lv_coord_t y = 0; y != width; y++) {
          size_t out = (size_t(y) * height_rounded + x) * 3;
          dst8[out + 0] = *ptr8++;
          dst8[out + 1] = *ptr8++;
          dst8[out + 2] = *ptr8++;
        }
      }
    }
#else
      for (lv_coord_t x = height; x-- != 0;) {
        for (lv_coord_t y = 0; y != width; y++) {
          dst[y * height_rounded + x] = *ptr++;
        }
      }
#endif
      y1 = x1;
      x1 = this->height_ - area->y1 - height;
      height = width;
      width = height_rounded;
      break;

    case display::DISPLAY_ROTATION_180_DEGREES:
#if LV_COLOR_DEPTH == 32
    {
      // RGB888: 3 bytes per pixel
      auto *dst8 = reinterpret_cast<uint8_t *>(this->rotate_buf_);
      auto *ptr8 = reinterpret_cast<const uint8_t *>(ptr);
      for (lv_coord_t y = height; y-- != 0;) {
        for (lv_coord_t x = width; x-- != 0;) {
          size_t out = (size_t(y) * width + x) * 3;
          dst8[out + 0] = *ptr8++;
          dst8[out + 1] = *ptr8++;
          dst8[out + 2] = *ptr8++;
        }
      }
    }
#else
      for (lv_coord_t y = height; y-- != 0;) {
        for (lv_coord_t x = width; x-- != 0;) {
          dst[y * width + x] = *ptr++;
        }
      }
#endif
      x1 = this->width_ - x1 - width;
      y1 = this->height_ - y1 - height;
      break;

    case display::DISPLAY_ROTATION_270_DEGREES:
#if LV_COLOR_DEPTH == 32
    {
      // RGB888: 3 bytes per pixel
      auto *dst8 = reinterpret_cast<uint8_t *>(this->rotate_buf_);
      auto *ptr8 = reinterpret_cast<const uint8_t *>(ptr);
      for (lv_coord_t x = 0; x != height; x++) {
        for (lv_coord_t y = width; y-- != 0;) {
          size_t out = (size_t(y) * height_rounded + x) * 3;
          dst8[out + 0] = *ptr8++;
          dst8[out + 1] = *ptr8++;
          dst8[out + 2] = *ptr8++;
        }
      }
    }
#else
      for (lv_coord_t x = 0; x != height; x++) {
        for (lv_coord_t y = width; y-- != 0;) {
          dst[y * height_rounded + x] = *ptr++;
        }
      }
#endif
      x1 = y1;
      y1 = this->width_ - area->x1 - width;
      height = width;
      width = height_rounded;
      break;

    default:
      if (direct_full_buffer) {
        for (auto *display : this->displays_) {
          display->draw_pixels_at(x1, y1, width, height, src8, display::COLOR_ORDER_RGB, LV_BITNESS, this->big_endian_);
        }
        return;
      }
      dst = ptr;
      break;
  }
  for (auto *display : this->displays_) {
    display->draw_pixels_at(x1, y1, width, height, (const uint8_t *) dst, display::COLOR_ORDER_RGB, LV_BITNESS,
                            this->big_endian_);
  }
}

void LvglComponent::flush_cb_(lv_display_t *disp_drv, const lv_area_t *area, uint8_t *color_p) {
  if (!this->is_paused()) {
    uint64_t t0 = profiler_tick_us();
#ifdef USE_MIPI_DSI
#ifdef USE_ESP32
    if (this->partial_compositor_flush_(disp_drv, area, color_p, t0)) {
      return;
    }
#endif
    if (!this->direct_mode_active_ && this->rotation == display::DISPLAY_ROTATION_0_DEGREES &&
        this->displays_.size() == 1) {
      auto *mipi_display = static_cast<mipi_dsi::MipiDsi *>(this->displays_[0]);
      const int width = lv_area_get_width(area);
      const int height = lv_area_get_height(area);
      if (mipi_display->draw_pixels_at_async(area->x1, area->y1, width, height, color_p, display::COLOR_ORDER_RGB,
                                             LV_BITNESS, this->big_endian_, 0, 0, 0, lvgl_mipi_async_flush_ready,
                                             disp_drv)) {
        const uint32_t flush_us = (uint32_t) (profiler_tick_us() - t0);
        if (flush_us > this->perf_flush_max_us_)
          this->perf_flush_max_us_ = flush_us;
        const uint32_t flush_px = (uint32_t) width * (uint32_t) height;
        this->perf_flush_px_ += flush_px;
        profiler_note_flush(flush_us, flush_px);
        this->perf_flush_us_ += flush_us;
        ESP_LOGV(TAG, "async flush_cb, area=%d/%d, %d/%d scheduled in %lu us", area->x1, area->y1, width, height,
                 (unsigned long) flush_us);
        return;
      }
    }
#endif
    if (this->direct_mode_active_) {
      this->direct_last_flushed_buf_ = color_p;
      if (lv_display_flush_is_last(disp_drv)) {
        this->draw_buffer_(area, reinterpret_cast<lv_color_data *>(color_p));
        // Direct mode renders into the MIPI panel framebuffers.  Let the panel
        // finish one refresh before LVGL starts drawing into the next buffer;
        // otherwise fast animated widgets can occasionally race the scanout and
        // show short horizontal artifacts.
        this->wait_for_direct_frame_presented(20);
      } else {
        this->sync_direct_framebuffer_area_(area, color_p);
      }
    } else {
      this->draw_buffer_(area, reinterpret_cast<lv_color_data *>(color_p));
    }
    uint64_t dt = profiler_tick_us() - t0;
    uint32_t flush_us = (uint32_t) dt;
    if (flush_us > this->perf_flush_max_us_)
      this->perf_flush_max_us_ = flush_us;
    uint32_t flush_px = (uint32_t) lv_area_get_width(area) * (uint32_t) lv_area_get_height(area);
    this->perf_flush_px_ += flush_px;
    profiler_note_flush(flush_us, flush_px);
    // Track flush wait time so loop() can subtract it when computing
    // CPU%% - the synchronous DMA push isn't real CPU work.
    this->perf_flush_us_ += dt;
    ESP_LOGV(TAG, "flush_cb, area=%d/%d, %d/%d took %llu us", area->x1, area->y1, lv_area_get_width(area),
             lv_area_get_height(area), (unsigned long long) dt);
  }
  lv_display_flush_ready(disp_drv);
}

void LvglComponent::sync_direct_framebuffer_area_(const lv_area_t *area, const uint8_t *color_p) {
#ifdef USE_ESP32
#if LV_COLOR_DEPTH == 32
  constexpr size_t bytes_per_pixel = 3;
#else
  constexpr size_t bytes_per_pixel = LV_COLOR_DEPTH / 8;
#endif
  const int32_t y1 = std::max<int32_t>(0, area->y1);
  const int32_t y2 = std::min<int32_t>(this->height_ - 1, area->y2);
  if (y2 < y1) {
    return;
  }
  if (this->draw_buf_ == nullptr) {
    return;
  }

  const size_t row_bytes = this->width_ * bytes_per_pixel;
  const size_t fb_bytes = this->width_ * this->height_ * bytes_per_pixel;
  uint8_t *framebuffer = nullptr;
  if (color_p >= this->draw_buf_ && color_p < this->draw_buf_ + fb_bytes) {
    framebuffer = this->draw_buf_;
  } else if (this->draw_buf2_ != nullptr && color_p >= this->draw_buf2_ && color_p < this->draw_buf2_ + fb_bytes) {
    framebuffer = this->draw_buf2_;
  } else {
    return;
  }

  uint8_t *sync_start = framebuffer + (size_t) y1 * row_bytes;
  const size_t sync_size = (size_t) (y2 - y1 + 1) * row_bytes;
  esp_cache_msync(sync_start, sync_size, ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED);
#endif
}

void LvglComponent::sync_direct_other_buffer_(const lv_area_t *area, const uint8_t *color_p) {
#ifdef USE_ESP32
#if LV_COLOR_DEPTH == 32
  constexpr size_t bytes_per_pixel = 3;
#else
  constexpr size_t bytes_per_pixel = LV_COLOR_DEPTH / 8;
#endif
  const int32_t x1 = std::max<int32_t>(0, area->x1);
  const int32_t y1 = std::max<int32_t>(0, area->y1);
  const int32_t x2 = std::min<int32_t>(this->width_ - 1, area->x2);
  const int32_t y2 = std::min<int32_t>(this->height_ - 1, area->y2);
  if (x2 < x1 || y2 < y1) {
    return;
  }
  if (this->draw_buf_ == nullptr || this->draw_buf2_ == nullptr) {
    return;
  }

  auto sync_range = [](const uint8_t *ptr, size_t len) {
    if (len > 0) {
      esp_cache_msync(const_cast<uint8_t *>(ptr), len, ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED);
    }
  };

  const size_t row_bytes = this->width_ * bytes_per_pixel;
  const size_t area_width_bytes = (x2 - x1 + 1) * bytes_per_pixel;
  const size_t fb_bytes = this->width_ * this->height_ * bytes_per_pixel;
  uint8_t *src = nullptr;
  uint8_t *dst = nullptr;
  if (color_p >= this->draw_buf_ && color_p < this->draw_buf_ + fb_bytes) {
    src = this->draw_buf_;
    dst = this->draw_buf2_;
  } else if (color_p >= this->draw_buf2_ && color_p < this->draw_buf2_ + fb_bytes) {
    src = this->draw_buf2_;
    dst = this->draw_buf_;
  } else {
    return;
  }

  if (x1 == 0 && area_width_bytes == row_bytes) {
    uint8_t *dst_block = dst + y1 * row_bytes;
    const uint8_t *src_block = src + y1 * row_bytes;
    const size_t block_bytes = (size_t) (y2 - y1 + 1) * row_bytes;
    sync_range(src_block, block_bytes);
    memcpy(dst_block, src_block, block_bytes);
    sync_range(dst_block, block_bytes);
  } else {
    for (int32_t y = y1; y <= y2; y++) {
      uint8_t *dst_line = dst + y * row_bytes + x1 * bytes_per_pixel;
      const uint8_t *src_line = src + y * row_bytes + x1 * bytes_per_pixel;
      sync_range(src_line, area_width_bytes);
      memcpy(dst_line, src_line, area_width_bytes);
      sync_range(dst_line, area_width_bytes);
    }
  }
#endif
}

uint8_t *LvglComponent::next_direct_render_buffer_() const {
  if (!this->direct_mode_active_ || this->draw_buf_ == nullptr)
    return this->draw_buf_;
  if (this->draw_buf2_ == nullptr)
    return this->draw_buf_;
  if (this->direct_last_flushed_buf_ == this->draw_buf_)
    return this->draw_buf2_;
  if (this->direct_last_flushed_buf_ == this->draw_buf2_)
    return this->draw_buf_;
  return this->draw_buf_;
}

void LvglComponent::present_direct_render_buffer_(uint8_t *buffer) {
  if (buffer == nullptr)
    return;
  for (auto *display : this->displays_) {
    display->draw_pixels_at(0, 0, this->width_, this->height_, buffer, display::COLOR_ORDER_RGB, LV_BITNESS,
                            this->big_endian_);
  }
  this->direct_last_flushed_buf_ = buffer;
}

#ifdef USE_ESP32
bool LvglComponent::start_partial_compositor_() {
#ifdef USE_MIPI_DSI
  // Prefer ESP-IDF's native DPI async framebuffer copy path for partial LVGL
  // buffers. The manual compositor remains in-tree for future experiments, but
  // on ESP32-P4 DSI framebuffers the custom PPA/SRM path can hit cache msync
  // validation errors and the CPU-copy fallback contends heavily with scanout.
  ESP_LOGI(TAG, "LVGL partial framebuffer compositor disabled; using native MIPI DSI async flush");
  return false;

  if (this->rotation != display::DISPLAY_ROTATION_0_DEGREES || this->displays_.size() != 1 ||
      this->draw_buf2_ == nullptr)
    return false;
  auto *mipi_display = static_cast<mipi_dsi::MipiDsi *>(this->displays_[0]);
  if (mipi_display == nullptr || mipi_display->get_frame_buffer(0) == nullptr ||
      mipi_display->get_frame_buffer(1) == nullptr) {
    return false;
  }
#ifdef USE_LVGL_PPA
  if (s_compositor_srm_alignment == 0) {
    if (esp_cache_get_alignment(MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA, &s_compositor_srm_alignment) != ESP_OK ||
        s_compositor_srm_alignment == 0) {
      s_compositor_srm_alignment = 64;
    }
  }
  if (s_compositor_srm_client == nullptr) {
    ppa_client_config_t srm_cfg = {};
    srm_cfg.oper_type = PPA_OPERATION_SRM;
    srm_cfg.max_pending_trans_num = 1;
    srm_cfg.data_burst_length = PPA_DATA_BURST_LENGTH_128;
    esp_err_t ret = ppa_register_client(&srm_cfg, &s_compositor_srm_client);
    if (ret == ESP_OK) {
      ESP_LOGI(TAG, "LVGL partial framebuffer compositor PPA SRM client registered (alignment=%u)",
               (unsigned) s_compositor_srm_alignment);
    } else {
      s_compositor_srm_client = nullptr;
      ESP_LOGW(TAG, "LVGL partial framebuffer compositor PPA SRM client failed (%s); using CPU copy",
               esp_err_to_name(ret));
    }
  }
#endif
  this->partial_compositor_front_buffer_ = mipi_display->get_frame_buffer(0);
  this->partial_compositor_back_buffer_ = mipi_display->get_frame_buffer(1);
  this->partial_compositor_queue_ = xQueueCreate(4, sizeof(PartialCompositorJob));
  if (this->partial_compositor_queue_ == nullptr) {
    ESP_LOGW(TAG, "LVGL partial framebuffer compositor queue allocation failed");
    return false;
  }
#if CONFIG_FREERTOS_UNICORE
  constexpr BaseType_t compositor_core = tskNO_AFFINITY;
#else
  constexpr BaseType_t compositor_core = 0;
#endif
  TaskHandle_t task_handle = nullptr;
  const BaseType_t ok = xTaskCreatePinnedToCore(&LvglComponent::partial_compositor_task_trampoline, "lvgl_fb_worker",
                                                6144, this, 8, &task_handle, compositor_core);
  if (ok != pdPASS) {
    ESP_LOGW(TAG, "LVGL partial framebuffer compositor task allocation failed");
    vQueueDelete(this->partial_compositor_queue_);
    this->partial_compositor_queue_ = nullptr;
    return false;
  }
  this->partial_compositor_task_handle_ = task_handle;
  this->partial_compositor_active_ = true;
  ESP_LOGI(TAG, "LVGL partial framebuffer compositor enabled on core %d", (int) compositor_core);
  return true;
#else
  return false;
#endif
}

bool LvglComponent::partial_compositor_flush_(lv_display_t *disp_drv, const lv_area_t *area, uint8_t *color_p,
                                              uint64_t t0) {
  if (!this->partial_compositor_active_ || this->partial_compositor_queue_ == nullptr)
    return false;
  PartialCompositorJob job{};
  job.disp = disp_drv;
  job.area = *area;
  job.color_p = color_p;
  job.last = lv_display_flush_is_last(disp_drv);
  job.t0 = t0;
  if (xQueueSend(this->partial_compositor_queue_, &job, pdMS_TO_TICKS(20)) != pdTRUE) {
    ESP_LOGW(TAG, "LVGL partial framebuffer compositor queue full; falling back to synchronous flush");
    return false;
  }

  const uint32_t schedule_us = (uint32_t) (profiler_tick_us() - t0);
  if (schedule_us > this->perf_flush_max_us_)
    this->perf_flush_max_us_ = schedule_us;
  const uint32_t flush_px = (uint32_t) lv_area_get_width(area) * (uint32_t) lv_area_get_height(area);
  this->perf_flush_px_ += flush_px;
  this->perf_flush_us_ += schedule_us;
  profiler_note_flush(schedule_us, flush_px);
  return true;
}

void LvglComponent::partial_compositor_task_trampoline(void *arg) {
  static_cast<LvglComponent *>(arg)->partial_compositor_task_();
}

void LvglComponent::partial_compositor_task_() {
  while (true) {
    PartialCompositorJob job{};
    if (xQueueReceive(this->partial_compositor_queue_, &job, portMAX_DELAY) != pdTRUE)
      continue;

    const uint64_t start_us = profiler_tick_us();
    this->partial_compositor_copy_area_(this->partial_compositor_back_buffer_, job.area, job.color_p);
    this->partial_compositor_record_dirty_(job.area);

    bool frame_presented = false;
    bool sync_full_dirty = false;
    size_t sync_dirty_count = 0;
    lv_area_t sync_dirty_areas[PARTIAL_COMPOSITOR_MAX_DIRTY_AREAS]{};

    if (job.last) {
      int y_start;
      int y_end;
      if (this->partial_compositor_full_dirty_ || this->partial_compositor_dirty_count_ == 0) {
        y_start = 0;
        y_end = this->height_ - 1;
      } else {
        y_start = this->height_ - 1;
        y_end = 0;
        for (size_t i = 0; i < this->partial_compositor_dirty_count_; i++) {
          y_start = std::min<int>(y_start, this->partial_compositor_dirty_areas_[i].y1);
          y_end = std::max<int>(y_end, this->partial_compositor_dirty_areas_[i].y2);
        }
      }
      sync_full_dirty = this->partial_compositor_full_dirty_;
      sync_dirty_count = this->partial_compositor_dirty_count_;
      for (size_t i = 0; i < sync_dirty_count; i++) {
        sync_dirty_areas[i] = this->partial_compositor_dirty_areas_[i];
      }

#ifndef USE_MIPI_DSI
      LV_UNUSED(y_start);
      LV_UNUSED(y_end);
#endif
#ifdef USE_MIPI_DSI
      auto *mipi_display = static_cast<mipi_dsi::MipiDsi *>(this->displays_[0]);
      mipi_display->present_frame_buffer(this->partial_compositor_back_buffer_, y_start, y_end);
#endif
      std::swap(this->partial_compositor_front_buffer_, this->partial_compositor_back_buffer_);
      this->partial_compositor_dirty_count_ = 0;
      this->partial_compositor_full_dirty_ = false;
      frame_presented = true;
    }

    const uint32_t ready_dt_us = (uint32_t) (profiler_tick_us() - start_us);
    this->perf_compositor_ready_us_ += ready_dt_us;
    this->perf_compositor_jobs_++;
    if (ready_dt_us > this->perf_compositor_ready_max_us_)
      this->perf_compositor_ready_max_us_ = ready_dt_us;
    const uint32_t px = (uint32_t) lv_area_get_width(&job.area) * (uint32_t) lv_area_get_height(&job.area);
    this->perf_compositor_px_ += px;
    lv_display_flush_ready(job.disp);

    if (frame_presented) {
      if (sync_full_dirty) {
        lv_area_t full{};
        full.x1 = 0;
        full.y1 = 0;
        full.x2 = this->width_ - 1;
        full.y2 = this->height_ - 1;
        this->partial_compositor_copy_area_(this->partial_compositor_back_buffer_, full,
                                            this->partial_compositor_front_buffer_, true);
      } else {
        for (size_t i = 0; i < sync_dirty_count; i++) {
          this->partial_compositor_copy_area_(this->partial_compositor_back_buffer_, sync_dirty_areas[i],
                                              this->partial_compositor_front_buffer_, true);
        }
      }
    }

    const uint32_t dt_us = (uint32_t) (profiler_tick_us() - start_us);
    this->perf_compositor_us_ += dt_us;
    if (dt_us > this->perf_compositor_max_us_)
      this->perf_compositor_max_us_ = dt_us;
  }
}

void LvglComponent::partial_compositor_copy_area_(uint8_t *dst, const lv_area_t &area, const uint8_t *src,
                                                  bool src_is_framebuffer) {
  if (dst == nullptr || src == nullptr)
    return;
#if LV_COLOR_DEPTH == 32
  constexpr size_t bytes_per_pixel = 3;
#else
  constexpr size_t bytes_per_pixel = LV_COLOR_DEPTH / 8;
#endif
  const int32_t x1 = std::max<int32_t>(0, area.x1);
  const int32_t y1 = std::max<int32_t>(0, area.y1);
  const int32_t x2 = std::min<int32_t>(this->width_ - 1, area.x2);
  const int32_t y2 = std::min<int32_t>(this->height_ - 1, area.y2);
  if (x2 < x1 || y2 < y1)
    return;

  const size_t row_bytes = (size_t) this->width_ * bytes_per_pixel;
  const size_t copy_bytes = (size_t) (x2 - x1 + 1) * bytes_per_pixel;
  const size_t src_stride = src_is_framebuffer ? row_bytes : ((size_t) lv_area_get_width(&area) * bytes_per_pixel);

#ifdef USE_LVGL_PPA
  const size_t framebuffer_bytes = row_bytes * (size_t) this->height_;
  const int32_t copy_w = x2 - x1 + 1;
  const int32_t copy_h = y2 - y1 + 1;
  const int32_t src_pic_w = src_is_framebuffer ? this->width_ : lv_area_get_width(&area);
  const int32_t src_pic_h = src_is_framebuffer ? this->height_ : lv_area_get_height(&area);
  const int32_t src_off_x = src_is_framebuffer ? x1 : (x1 - area.x1);
  const int32_t src_off_y = src_is_framebuffer ? y1 : (y1 - area.y1);
  const bool src_geometry_ok = src_pic_w > 0 && src_pic_h > 0 && src_off_x >= 0 && src_off_y >= 0 &&
                               (src_off_x + copy_w) <= src_pic_w && (src_off_y + copy_h) <= src_pic_h;
  const size_t align = s_compositor_srm_alignment == 0 ? 64 : s_compositor_srm_alignment;
  const bool out_aligned = (align != 0) && ((reinterpret_cast<uintptr_t>(dst) & (align - 1)) == 0) &&
                           ((framebuffer_bytes & (align - 1)) == 0);
  const bool ppa_safe_memory = esp_ptr_external_ram(src) && esp_ptr_external_ram(dst);
  if (s_compositor_srm_client != nullptr && src_geometry_ok && out_aligned && ppa_safe_memory) {
#if LV_COLOR_DEPTH == 32
    constexpr ppa_srm_color_mode_t PPA_CM = PPA_SRM_COLOR_MODE_RGB888;
#else
    constexpr ppa_srm_color_mode_t PPA_CM = PPA_SRM_COLOR_MODE_RGB565;
#endif
    ppa_srm_oper_config_t cfg = {};
    cfg.in.buffer = const_cast<uint8_t *>(src);
    cfg.in.pic_w = src_pic_w;
    cfg.in.pic_h = src_pic_h;
    cfg.in.block_w = copy_w;
    cfg.in.block_h = copy_h;
    cfg.in.block_offset_x = src_off_x;
    cfg.in.block_offset_y = src_off_y;
    cfg.in.srm_cm = PPA_CM;
    cfg.out.buffer = dst;
    cfg.out.buffer_size = framebuffer_bytes;
    cfg.out.pic_w = this->width_;
    cfg.out.pic_h = this->height_;
    cfg.out.block_offset_x = x1;
    cfg.out.block_offset_y = y1;
    cfg.out.srm_cm = PPA_CM;
    cfg.rotation_angle = PPA_SRM_ROTATION_ANGLE_0;
    cfg.scale_x = 1.0f;
    cfg.scale_y = 1.0f;
    cfg.alpha_update_mode = PPA_ALPHA_NO_CHANGE;
    cfg.mode = PPA_TRANS_MODE_BLOCKING;
    esp_err_t ret = ppa_do_scale_rotate_mirror(s_compositor_srm_client, &cfg);
    if (ret == ESP_OK)
      return;

    static uint32_t ppa_warn_count = 0;
    if (ppa_warn_count < 8) {
      ESP_LOGW(TAG,
               "LVGL partial compositor PPA copy failed (%s): area=%d,%d-%d,%d src_fb=%d src_pic=%dx%d src_off=%d,%d "
               "align=%u dst=%p fb=%u",
               esp_err_to_name(ret), (int) x1, (int) y1, (int) x2, (int) y2, src_is_framebuffer ? 1 : 0,
               (int) src_pic_w, (int) src_pic_h, (int) src_off_x, (int) src_off_y, (unsigned) align, dst,
               (unsigned) framebuffer_bytes);
      ppa_warn_count++;
    }
  }
#endif

  const uint8_t *src_row =
      src_is_framebuffer ? src + ((size_t) y1 * row_bytes) + ((size_t) x1 * bytes_per_pixel)
                         : src + ((size_t) (y1 - area.y1) * src_stride) + ((size_t) (x1 - area.x1) * bytes_per_pixel);
  uint8_t *dst_row = dst + ((size_t) y1 * row_bytes) + ((size_t) x1 * bytes_per_pixel);
  for (int32_t y = y1; y <= y2; y++) {
    memcpy(dst_row, src_row, copy_bytes);
    dst_row += row_bytes;
    src_row += src_stride;
  }
  uint8_t *sync_start = dst + (size_t) y1 * row_bytes;
  const size_t sync_size = (size_t) (y2 - y1 + 1) * row_bytes;
  esp_cache_msync(sync_start, sync_size, ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED);
}

void LvglComponent::partial_compositor_record_dirty_(const lv_area_t &area) {
  if (this->partial_compositor_full_dirty_)
    return;
  if (this->partial_compositor_dirty_count_ >= PARTIAL_COMPOSITOR_MAX_DIRTY_AREAS) {
    this->partial_compositor_full_dirty_ = true;
    return;
  }
  lv_area_t clipped{};
  clipped.x1 = std::max<int32_t>(0, area.x1);
  clipped.y1 = std::max<int32_t>(0, area.y1);
  clipped.x2 = std::min<int32_t>(this->width_ - 1, area.x2);
  clipped.y2 = std::min<int32_t>(this->height_ - 1, area.y2);
  if (clipped.x2 < clipped.x1 || clipped.y2 < clipped.y1)
    return;
  this->partial_compositor_dirty_areas_[this->partial_compositor_dirty_count_++] = clipped;
}

void LvglComponent::partial_compositor_sync_dirty_to_idle_() {
  if (this->partial_compositor_front_buffer_ == nullptr || this->partial_compositor_back_buffer_ == nullptr)
    return;
  if (this->partial_compositor_full_dirty_) {
    lv_area_t full{};
    full.x1 = 0;
    full.y1 = 0;
    full.x2 = this->width_ - 1;
    full.y2 = this->height_ - 1;
    this->partial_compositor_copy_area_(this->partial_compositor_back_buffer_, full,
                                        this->partial_compositor_front_buffer_, true);
    return;
  }
  for (size_t i = 0; i < this->partial_compositor_dirty_count_; i++) {
    this->partial_compositor_copy_area_(this->partial_compositor_back_buffer_, this->partial_compositor_dirty_areas_[i],
                                        this->partial_compositor_front_buffer_, true);
  }
}
#endif  // USE_ESP32

bool LvglComponent::wait_for_direct_frame_presented(uint32_t timeout_ms) {
#ifdef USE_MIPI_DSI
  if (this->displays_.empty())
    return false;
  auto *mipi_display = static_cast<mipi_dsi::MipiDsi *>(this->displays_[0]);
  return mipi_display != nullptr && mipi_display->wait_for_refresh_done(timeout_ms);
#else
  return false;
#endif
}

void LvglComponent::realign_direct_buffer_after_manual_present() {
  if (!this->direct_mode_active_ || this->disp_ == nullptr || this->direct_last_flushed_buf_ == nullptr)
    return;
  if (this->disp_->buf_1 == nullptr || this->disp_->buf_2 == nullptr)
    return;

  lv_draw_buf_t *next_lvgl_buf = nullptr;
  if (this->disp_->buf_1->data == this->direct_last_flushed_buf_) {
    next_lvgl_buf = this->disp_->buf_2;
  } else if (this->disp_->buf_2->data == this->direct_last_flushed_buf_) {
    next_lvgl_buf = this->disp_->buf_1;
  }

  if (next_lvgl_buf != nullptr && this->disp_->buf_act != next_lvgl_buf) {
    ESP_LOGD(TAG, "direct mode: realigning LVGL buf_act away from presented framebuffer");
    this->disp_->buf_act = next_lvgl_buf;
  }
}

IdleTrigger::IdleTrigger(LvglComponent *parent, TemplatableFn<uint32_t> timeout) : timeout_(timeout) {
  parent->add_on_idle_callback([this](uint32_t idle_time) {
    if (!this->is_idle_ && idle_time > this->timeout_.value()) {
      this->is_idle_ = true;
      this->trigger();
    } else if (this->is_idle_ && idle_time < this->timeout_.value()) {
      this->is_idle_ = false;
    }
  });
}

#ifdef USE_LVGL_TOUCHSCREEN
LVTouchListener::LVTouchListener(uint16_t long_press_time, uint16_t long_press_repeat_time, LvglComponent *parent) {
  this->set_parent(parent);
  this->drv_ = lv_indev_create();
  lv_indev_set_type(this->drv_, LV_INDEV_TYPE_POINTER);
  lv_indev_set_disp(this->drv_, parent->get_disp());
  lv_indev_set_long_press_time(this->drv_, long_press_time);
  lv_indev_set_gesture_min_distance(this->drv_, 45);
  lv_indev_set_gesture_min_velocity(this->drv_, 4);
  // long press repeat time TBD
  lv_indev_set_user_data(this->drv_, this);
  lv_indev_set_read_cb(this->drv_, [](lv_indev_t *d, lv_indev_data_t *data) {
    auto *l = static_cast<LVTouchListener *>(lv_indev_get_user_data(d));
    if (l->touch_pressed_) {
      data->point.x = l->touch_point_.x;
      data->point.y = l->touch_point_.y;
      data->state = LV_INDEV_STATE_PRESSED;
    } else {
      data->state = LV_INDEV_STATE_RELEASED;
    }
  });
}

void LVTouchListener::update(const touchscreen::TouchPoints_t &tpoints) {
  this->touch_pressed_ = !this->parent_->is_paused() && !tpoints.empty();
  if (this->touch_pressed_)
    this->touch_point_ = tpoints[0];
}
#endif  // USE_LVGL_TOUCHSCREEN

#ifdef USE_LVGL_METER

int16_t lv_get_needle_angle_for_value(lv_obj_t *obj, int32_t value) {
  auto *scale = lv_obj_get_parent(obj);
  auto min_value = lv_scale_get_range_min_value(scale);
  auto max_value = lv_scale_get_range_max_value(scale);
  value = clamp(value, min_value, max_value);
  return ((value - min_value) * lv_scale_get_angle_range(scale) / (max_value - min_value) +
          lv_scale_get_rotation((scale))) %
         360;
}

void IndicatorLine::set_obj(lv_obj_t *lv_obj) {
  LvCompound::set_obj(lv_obj);
  lv_line_set_points(lv_obj, this->points_, 2);
  lv_obj_add_event_cb(
      lv_obj_get_parent(obj),
      [](lv_event_t *e) {
        auto *indicator = static_cast<IndicatorLine *>(lv_event_get_user_data(e));
        indicator->update_length_();
        ESP_LOGD(TAG, "Updated length, value = %d", indicator->angle_);
      },
      LV_EVENT_SIZE_CHANGED, this);
}

void IndicatorLine::set_value(int value) {
  auto angle = lv_get_needle_angle_for_value(this->obj, value);
  if (angle != this->angle_) {
    this->angle_ = angle;
    this->update_length_();
  }
}

void IndicatorLine::update_length_() {
  auto cx = lv_obj_get_width(lv_obj_get_parent(this->obj)) / 2;
  auto cy = lv_obj_get_height(lv_obj_get_parent(this->obj)) / 2;
  auto radius = clamp_at_most(cx, cy);
  auto length = lv_obj_get_style_length(this->obj, LV_PART_MAIN);
  auto radial_offset = lv_obj_get_style_radial_offset(this->obj, LV_PART_MAIN);
  if (LV_COORD_IS_PCT(radial_offset)) {
    radial_offset = radius * LV_COORD_GET_PCT(radial_offset) / 100;
  }
  if (LV_COORD_IS_PCT(length)) {
    length = radius * LV_COORD_GET_PCT(length) / 100;
  } else if (length < 0) {
    length += radius;
  }
  auto x = lv_trigo_cos(this->angle_) / 32768.0f;
  auto y = lv_trigo_sin(this->angle_) / 32768.0f;
  // radius here also represents the offset of the scale center from top left
  this->points_[0].x = radius + radial_offset * x;
  this->points_[0].y = radius + radial_offset * y;
  this->points_[1].x = radius + x * (radial_offset + length);
  this->points_[1].y = radius + y * (radial_offset + length);
  lv_obj_refresh_self_size(this->obj);
  lv_obj_invalidate(this->obj);
}
#endif

#ifdef USE_LVGL_KEY_LISTENER
LVEncoderListener::LVEncoderListener(lv_indev_type_t type, uint16_t long_press_time, uint16_t long_press_repeat_time) {
  this->drv_ = lv_indev_create();
  lv_indev_set_type(this->drv_, type);
  lv_indev_set_long_press_time(this->drv_, long_press_time);
  lv_indev_set_long_press_repeat_time(this->drv_, long_press_repeat_time);
  lv_indev_set_user_data(this->drv_, this);
  lv_indev_set_read_cb(this->drv_, [](lv_indev_t *d, lv_indev_data_t *data) {
    auto *l = static_cast<LVEncoderListener *>(lv_indev_get_user_data(d));
    data->state = l->pressed_ ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    data->key = l->key_;
    // LVGL 9.5: Apply rotary sensitivity multiplier
    auto raw_diff = (int16_t) (l->count_ - l->last_count_);
    data->enc_diff = (int16_t) (raw_diff * l->sensitivity_);
    l->last_count_ = l->count_;
    data->continue_reading = false;
  });
}
#endif  // USE_LVGL_KEY_LISTENER

#if defined(USE_LVGL_DROPDOWN) || defined(LV_USE_ROLLER)
std::string LvSelectable::get_selected_text() {
  auto selected = this->get_selected_index();
  if (selected >= this->options_.size())
    return "";
  return this->options_[selected];
}

static std::string join_string(std::vector<std::string> options) {
  return std::accumulate(
      options.begin(), options.end(), std::string(),
      [](const std::string &a, const std::string &b) -> std::string { return a + (!a.empty() ? "\n" : "") + b; });
}

void LvSelectable::set_selected_text(const std::string &text, lv_anim_enable_t anim) {
  auto index = std::find(this->options_.begin(), this->options_.end(), text);
  if (index != this->options_.end()) {
    this->set_selected_index(index - this->options_.begin(), anim);
    lv_obj_send_event(this->obj, lv_update_event, nullptr);
  }
}

void LvSelectable::set_options(std::vector<std::string> options) {
  auto index = this->get_selected_index();
  if (index >= options.size())
    index = options.size() - 1;
  this->options_ = std::move(options);
  this->set_option_string(join_string(this->options_).c_str());
  lv_obj_send_event(this->obj, LV_EVENT_REFRESH, nullptr);
  this->set_selected_index(index, LV_ANIM_OFF);
}
#endif  // USE_LVGL_DROPDOWN || LV_USE_ROLLER

#ifdef USE_LVGL_BUTTONMATRIX
void LvButtonMatrixType::set_obj(lv_obj_t *lv_obj) {
  LvCompound::set_obj(lv_obj);
  lv_obj_add_event_cb(
      lv_obj,
      [](lv_event_t *event) {
        auto *self = static_cast<LvButtonMatrixType *>(lv_event_get_user_data(event));
        if (self->key_callback_.size() == 0)
          return;
        auto key_idx = lv_buttonmatrix_get_selected_button(self->obj);
        if (key_idx == LV_BUTTONMATRIX_BUTTON_NONE)
          return;
        if (self->key_map_.contains(key_idx)) {
          self->send_key_(self->key_map_[key_idx]);
          return;
        }
        const auto *str = lv_buttonmatrix_get_button_text(self->obj, key_idx);
        auto len = strlen(str);
        while (len--)
          self->send_key_(*str++);
      },
      LV_EVENT_PRESSED, this);
}
#endif  // USE_LVGL_BUTTONMATRIX

#ifdef USE_LVGL_KEYBOARD
static const char *const KB_SPECIAL_KEYS[] = {
    "abc", "ABC", "1#",
    // maybe add other special keys here
};

void LvKeyboardType::set_obj(lv_obj_t *lv_obj) {
  LvCompound::set_obj(lv_obj);
  lv_obj_add_event_cb(
      lv_obj,
      [](lv_event_t *event) {
        auto *self = static_cast<LvKeyboardType *>(lv_event_get_user_data(event));
        if (self->key_callback_.size() == 0)
          return;

        auto key_idx = lv_buttonmatrix_get_selected_button(self->obj);
        if (key_idx == LV_BUTTONMATRIX_BUTTON_NONE)
          return;
        const char *txt = lv_buttonmatrix_get_button_text(self->obj, key_idx);
        if (txt == nullptr)
          return;
        for (const auto *kb_special_key : KB_SPECIAL_KEYS) {
          if (strcmp(txt, kb_special_key) == 0)
            return;
        }
        while (*txt != 0)
          self->send_key_(*txt++);
      },
      LV_EVENT_PRESSED, this);
}
#endif  // USE_LVGL_KEYBOARD

void LvglComponent::draw_end_() {
  if (this->draw_end_callback_ != nullptr)
    this->draw_end_callback_->trigger();
  if (this->update_when_display_idle_) {
    for (auto *disp : this->displays_)
      disp->update();
  }
}

bool LvglComponent::is_paused() const {
  if (this->paused_)
    return true;
  if (this->update_when_display_idle_) {
    for (auto *disp : this->displays_) {
      if (!disp->is_idle())
        return true;
    }
  }
  return false;
}

void LvglComponent::write_random_() {
  int iterations = 6 - lv_display_get_inactive_time(this->disp_) / 60000;
  if (iterations <= 0)
    iterations = 1;
  int16_t width = lv_display_get_horizontal_resolution(this->disp_);
  int16_t height = lv_display_get_vertical_resolution(this->disp_);
  while (iterations-- != 0) {
    int32_t col = random_uint32() % width;
    col = col / this->draw_rounding * this->draw_rounding;
    int32_t row = random_uint32() % height;
    row = row / this->draw_rounding * this->draw_rounding;
    // size will be between 8 and 32, and a multiple of draw_rounding
    int32_t size = (random_uint32() % 25 + 8) / this->draw_rounding * this->draw_rounding;
    lv_area_t area{.x1 = col, .y1 = row, .x2 = col + size - 1, .y2 = row + size - 1};
    // clip to display bounds just in case
    if (area.x2 >= width)
      area.x2 = width - 1;
    if (area.y2 >= height)
      area.y2 = height - 1;

    size_t line_len = lv_area_get_width(&area) * lv_area_get_height(&area) / 2;
    for (size_t i = 0; i != line_len; i++) {
      reinterpret_cast<uint32_t *>(this->draw_buf_)[i] = random_uint32();
    }
    this->draw_buffer_(&area, reinterpret_cast<lv_color_data *>(this->draw_buf_));
  }
}

/**
 * @class LvglComponent
 * @brief Component for rendering LVGL.
 *
 * This component renders LVGL widgets on a display. Some initialisation must be done in the constructor
 * since LVGL needs to be initialised before any widgets can be created.
 *
 * @param displays a list of displays to render onto. All displays must have the same
 *                 resolution.
 * @param buffer_frac the fraction of the display resolution to use for the LVGL
 *                    draw buffer. A higher value will make animations smoother but
 *                    also increase memory usage.
 * @param full_refresh if true, the display will be fully refreshed on every frame.
 *                     If false, only changed areas will be updated.
 * @param draw_rounding the rounding to use when drawing. A value of 1 will draw
 *                      without any rounding, a value of 2 will round to the nearest
 *                      multiple of 2, and so on.
 * @param resume_on_input if true, this component will resume rendering when the user
 *                         presses a key or clicks on the screen.
 */
LvglComponent::LvglComponent(std::vector<display::Display *> displays, float buffer_frac, bool full_refresh,
                             bool direct_mode, int draw_rounding, bool resume_on_input, bool update_when_display_idle,
                             RotationType rotation_type)
    : draw_rounding(draw_rounding),
      displays_(std::move(displays)),
      buffer_frac_(buffer_frac),
      full_refresh_(full_refresh),
      direct_mode_(direct_mode),
      resume_on_input_(resume_on_input),
      update_when_display_idle_(update_when_display_idle),
      rotation_type_(rotation_type) {
  this->disp_ = lv_display_create(240, 240);
}

void LvglComponent::setup() {
  auto *display = this->displays_[0];
  auto rounding = this->draw_rounding;
  // cater for displays with dimensions that don't divide by the required rounding
  this->width_ = display->get_width();
  this->height_ = display->get_height();
  auto width = (display->get_width() + rounding - 1) / rounding * rounding;
  auto height = (display->get_height() + rounding - 1) / rounding * rounding;
  auto frac = this->buffer_frac_;
  if (this->rotation_type_ == RotationType::ROTATION_UNUSED)
    this->rotation = display->get_rotation();
  if (frac == 0) {
    frac = 1;
  }
  // LV_COLOR_FORMAT_RGB888 uses 3 bytes/pixel even when LV_COLOR_DEPTH=32
#if LV_COLOR_DEPTH == 32
  constexpr size_t bytes_per_pixel = 3;  // RGB888
#else
  constexpr size_t bytes_per_pixel = LV_COLOR_DEPTH / 8;
#endif
  auto buf_bytes = width * height / frac * bytes_per_pixel;
  // Align buffer size to the data cache line (128 B if
  // CONFIG_CACHE_L2_CACHE_LINE_128B=y, else 64 B is enough). 128 satisfies
  // both - esp_cache_msync() + PPA require both address AND size to be
  // cache-line aligned. Without this, PPA operations fail on PSRAM buffers
  // ('out.buffer addr or out.buffer_size not aligned to cache line size').
  constexpr size_t buf_size_align = 128;
  buf_bytes = (buf_bytes + buf_size_align - 1) & ~(buf_size_align - 1);
  void *buffer = nullptr;

  // Helper lambda to allocate an aligned DMA-capable buffer.
  // When USE_LVGL_PPA is defined, we try internal DMA-capable SRAM first
  // (required for PPA on ESP32-P4), then fall back to PSRAM with cache sync.
  auto alloc_draw_buf = [](size_t sz) -> void * {
#if defined(USE_LVGL_PPA) && defined(USE_ESP32)
    // Round size up to 128-byte cache line so PPA buffer_size checks pass
    // on both 64 B and 128 B cache-line sdkconfigs.
    size_t aligned_sz = (sz + 127) & ~size_t{127};
    void *p = heap_caps_aligned_alloc(128, aligned_sz, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (p != nullptr)
      return p;
    // Internal DMA SRAM full -> PSRAM (128-byte aligned for 128 B cache line)
    p = heap_caps_aligned_alloc(128, aligned_sz, MALLOC_CAP_SPIRAM);
    if (p != nullptr)
      return p;
#endif
    return lv_malloc_core(sz);
  };

#ifdef USE_MIPI_DSI
  if (this->direct_mode_) {
    auto *mipi_display = static_cast<mipi_dsi::MipiDsi *>(display);
    if (mipi_display != nullptr && this->rotation == display::DISPLAY_ROTATION_0_DEGREES &&
        mipi_display->get_frame_buffer(0) != nullptr && mipi_display->get_frame_buffer(1) != nullptr &&
        mipi_display->get_frame_buffer_size() >= buf_bytes) {
      buffer = mipi_display->get_frame_buffer();
      this->draw_buf2_ = mipi_display->get_frame_buffer(1);
      this->direct_mode_active_ = true;
      s_direct_mode_active = 1;
      ESP_LOGI(TAG, "LVGL direct mode enabled on MIPI framebuffer (%zu bytes)", buf_bytes);
    } else {
      ESP_LOGW(TAG, "LVGL direct mode requested but unavailable, falling back to partial flush");
    }
  }
#else
  if (this->direct_mode_) {
    ESP_LOGW(TAG, "LVGL direct mode requested but MIPI DSI is not enabled");
  }
#endif

  if (buffer == nullptr)
    buffer = alloc_draw_buf(buf_bytes);
  // if specific buffer size not set and can't get 100%, try for a smaller one
  if (buffer == nullptr && this->buffer_frac_ == 0) {
    frac = MIN_BUFFER_FRAC;
    buf_bytes /= MIN_BUFFER_FRAC;
    buffer = alloc_draw_buf(buf_bytes);
  }
  this->buffer_frac_ = frac;
  if (buffer == nullptr) {
    this->status_set_error(LOG_STR("Memory allocation failure"));
    this->mark_failed();
    return;
  }
  this->draw_buf_ = static_cast<uint8_t *>(buffer);
#ifdef USE_MIPI_DSI
  if (!this->direct_mode_active_ && this->rotation == display::DISPLAY_ROTATION_0_DEGREES &&
      this->displays_.size() == 1) {
    auto *mipi_display = static_cast<mipi_dsi::MipiDsi *>(display);
    if (mipi_display != nullptr && mipi_display->get_frame_buffer(0) != nullptr &&
        mipi_display->get_frame_buffer(1) != nullptr) {
      this->draw_buf2_ = static_cast<uint8_t *>(alloc_draw_buf(buf_bytes));
      if (this->draw_buf2_ == nullptr) {
        ESP_LOGW(TAG, "LVGL partial framebuffer compositor disabled: second draw buffer allocation failed");
      }
    }
  }
#endif
#ifdef USE_ESP32
  auto memory_region = [](const void *ptr) -> const char * {
    if (ptr == nullptr)
      return "none";
    if (esp_ptr_internal(ptr))
      return "internal";
    if (esp_ptr_external_ram(ptr))
      return "psram";
    return "other";
  };
#endif
#ifdef USE_ESP32
  ESP_LOGI(TAG, "LVGL draw buffer 1: %p %zu bytes in %s", this->draw_buf_, buf_bytes, memory_region(this->draw_buf_));
  if (this->draw_buf2_ != nullptr) {
    ESP_LOGI(TAG, "LVGL draw buffer 2: %p %zu bytes in %s", this->draw_buf2_, buf_bytes,
             memory_region(this->draw_buf2_));
  }
#endif
  this->set_resolution_();
#if LV_COLOR_DEPTH == 32
  // RGB888: 3 bytes per pixel, fully supported by PPA as destination
  lv_display_set_color_format(this->disp_, LV_COLOR_FORMAT_RGB888);
#else
  lv_display_set_color_format(this->disp_, LV_COLOR_FORMAT_RGB565);
#endif
  // CRITICAL: Set user_data BEFORE flush_cb, as flush_cb uses user_data
  lv_display_set_user_data(this->disp_, this);
  lv_display_set_flush_cb(this->disp_, static_flush_cb);
  lv_display_add_event_cb(this->disp_, rounder_cb, LV_EVENT_INVALIDATE_AREA, this);
  // Store buf_bytes - lv_display_set_buffers() is called at the END of setup()
  // to avoid triggering rendering before all callbacks and pages are configured.
  this->buf_bytes_ = buf_bytes;
  if (this->rotation_type_ == RotationType::ROTATION_SOFTWARE) {
    this->rotate_buf_ = static_cast<lv_color_t *>(alloc_draw_buf(buf_bytes));
    if (this->rotate_buf_ == nullptr) {
      this->status_set_error(LOG_STR("Memory allocation failure"));
      this->mark_failed();
      return;
    }
#ifdef USE_LVGL_PPA
    if (s_display_srm_client != nullptr) {
      ESP_LOGI(TAG, "Display rotation will use PPA SRM hardware acceleration");
    }
#endif
  }
  if (this->draw_start_callback_ != nullptr) {
    lv_display_add_event_cb(this->disp_, render_start_cb, LV_EVENT_RENDER_START, this);
  }
  if (this->draw_end_callback_ != nullptr || this->update_when_display_idle_) {
    lv_display_add_event_cb(this->disp_, render_end_cb, LV_EVENT_REFR_READY, this);
  }
#if LV_USE_LOG
  lv_log_register_print_cb([](lv_log_level_t level, const char *buf) {
    auto next = strchr(buf, ')');
    if (next != nullptr)
      buf = next + 1;
    while (isspace(*buf))
      buf++;
    if (level >= sizeof(LOG_LEVEL_MAP) / sizeof(LOG_LEVEL_MAP[0]))
      level = sizeof(LOG_LEVEL_MAP) / sizeof(LOG_LEVEL_MAP[0]) - 1;
    esp_log_printf_(LOG_LEVEL_MAP[level], TAG, 0, "%.*s", (int) strlen(buf) - 1, buf);
  });
#endif
  this->set_resolution_();
  this->show_page(0, LV_SCREEN_LOAD_ANIM_NONE, 0);
  lv_display_trigger_activity(this->disp_);

#ifdef USE_ESP32
  if (!this->direct_mode_active_ && !this->full_refresh_) {
    this->start_partial_compositor_();
  }
#endif

  // CRITICAL: Configure buffers at the VERY END of setup()
  // This avoids deadlock while ensuring buffers are ready before any callbacks execute
  lv_display_set_buffers(this->disp_, this->draw_buf_, this->draw_buf2_, this->buf_bytes_,
                         this->direct_mode_active_
                             ? LV_DISPLAY_RENDER_MODE_DIRECT
                             : (this->full_refresh_ ? LV_DISPLAY_RENDER_MODE_FULL : LV_DISPLAY_RENDER_MODE_PARTIAL));
  this->buffers_configured_ = true;

#ifdef USE_LVGL_PPA
  // Espressif esp-iot-solution PPA SW blend handler - accelerates all
  // RGB565 SW blend paths (text, gradients post-rasterize, partial blends).
  // Complements the higher-level PPA draw unit in lv_draw_ppa.c.
  lvgl_port_ppa_v9_init(this->disp_);
#endif

#ifdef USE_LVGL_FPS_BENCHMARK
  // Espressif esp_lvgl_adapter FPS sampler - prints a P10/25/50/75/90
  // report after ~200 samples (or sustained low-FPS detection).
  if (s_perf_logging_enabled)
    ESP_LOGI(TAG, "FPS benchmark: calling attach() for disp=%p", this->disp_);
  lvgl_fps_attach_v2(this->disp_);
  if (s_perf_logging_enabled)
    ESP_LOGI(TAG, "FPS benchmark: attach() returned");
#else
  if (s_perf_logging_enabled)
    ESP_LOGI(TAG, "FPS benchmark: not compiled in (USE_LVGL_FPS_BENCHMARK undefined)");
#endif
}

void LvglComponent::update() {
  // update indicators
  if (this->is_paused()) {
    return;
  }
  this->idle_callbacks_.call(lv_display_get_inactive_time(this->disp_));
}

void LvglComponent::loop() {
  if (!this->buffers_configured_)
    return;  // setup() not complete or failed, skip rendering

  if (!this->loop_started_) {
    this->loop_started_ = true;
    ESP_LOGD(TAG, "LVGL loop started - system is now fully ready");
  }

  if (this->is_paused()) {
    if (this->paused_ && this->show_snow_) {
      this->write_random_();
    }
  } else {
    // Time the LVGL handler. flush_cb_ separately accumulates the DSI
    // DMA wait into perf_flush_us_; subtract it so the reported CPU%%
    // counts only real render work (matches lvgl_camera_display's
    // approach: cpu_time / frame_interval).
    uint64_t t0 = profiler_tick_us();
    lv_timer_handler();
    uint64_t t1 = profiler_tick_us();
    uint64_t loop_dt = t1 - t0;
    profiler_note_loop(loop_dt > UINT32_MAX ? UINT32_MAX : (uint32_t) loop_dt);
    this->perf_busy_us_ += loop_dt;
    if (loop_dt > this->perf_loop_max_us_)
      this->perf_loop_max_us_ = (uint32_t) loop_dt;
    uint64_t now_us = t1;
    if (this->perf_window_start_us_ == 0)
      this->perf_window_start_us_ = now_us;
    uint64_t elapsed_us = now_us - this->perf_window_start_us_;
    if (elapsed_us >= 1000000) {
      uint64_t cpu_us = (this->perf_busy_us_ > this->perf_flush_us_) ? (this->perf_busy_us_ - this->perf_flush_us_) : 0;
      uint32_t cpu_pct = (uint32_t) ((cpu_us * 100ULL) / elapsed_us);
      if (cpu_pct > 100)
        cpu_pct = 100;
      s_cpu_pct = cpu_pct;  // publish to __wrap_lv_timer_get_idle / sysmon overlay
      s_flush_ms = (uint32_t) (this->perf_flush_us_ / 1000ULL);
      s_loop_max_ms = this->perf_loop_max_us_ / 1000U;
      s_flush_max_ms = this->perf_flush_max_us_ / 1000U;
      s_invalidated_kpx = (uint32_t) (this->perf_invalidated_px_ / 1000ULL);
#ifdef USE_ESP32
      uint32_t free_psram_kb = heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024U;
      uint32_t free_internal_kb = heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024U;
#else
      uint32_t free_psram_kb = 0;
      uint32_t free_internal_kb = 0;
#endif
      uint32_t ppa_fill_tasks = 0;
      uint32_t ppa_img_tasks = 0;
#ifdef USE_ESP32
      const uint64_t compositor_us = this->perf_compositor_us_;
      const uint64_t compositor_ready_us = this->perf_compositor_ready_us_;
      const uint64_t compositor_px = this->perf_compositor_px_;
      const uint32_t compositor_jobs = this->perf_compositor_jobs_;
      const uint32_t compositor_max_ms = this->perf_compositor_max_us_ / 1000U;
      const uint32_t compositor_ready_max_ms = this->perf_compositor_ready_max_us_ / 1000U;
#else
      const uint64_t compositor_us = 0;
      const uint64_t compositor_ready_us = 0;
      const uint64_t compositor_px = 0;
      const uint32_t compositor_jobs = 0;
      const uint32_t compositor_max_ms = 0;
      const uint32_t compositor_ready_max_ms = 0;
#endif
#ifdef USE_LVGL_PPA
      ppa_fill_tasks = lv_draw_ppa_get_fill_task_count();
      ppa_img_tasks = lv_draw_ppa_get_img_task_count();
#endif
#ifdef USE_MIPI_DSI
      mipi_dsi::AsyncFlushPerfStats dsi_stats{};
      if (!this->displays_.empty()) {
        auto *mipi_display = static_cast<mipi_dsi::MipiDsi *>(this->displays_[0]);
        if (mipi_display != nullptr)
          mipi_display->consume_async_flush_perf(&dsi_stats);
      }
#else
      struct {
        uint32_t flushes{};
        uint32_t zero_copy_flushes{};
        uint32_t staged_flushes{};
        uint32_t done_flushes{};
        uint32_t unsafe_addr_flushes{};
        uint32_t unsafe_row_flushes{};
        uint32_t unsafe_size_flushes{};
        uint64_t staged_bytes{};
        uint64_t sync_us{};
        uint64_t copy_us{};
        uint64_t submit_us{};
        uint64_t done_us{};
        uint32_t sync_max_us{};
        uint32_t copy_max_us{};
        uint32_t submit_max_us{};
        uint32_t done_max_us{};
      } dsi_stats;
#endif
      if (s_perf_logging_enabled) {
        ESP_LOGI(TAG,
                 "perf1s: cpu=%u%% loop=%lluus flush=%lluus dsi_sync=%lluus max=%ums dsi_copy=%lluus/%u max=%ums "
                 "dsi_submit=%lluus max=%ums dsi_done=%lluus/%u max=%ums zc=%u stage=%u/%u unsafe=%u/%u/%u %lluKB "
                 "comp=%lluus ready=%lluus/%u jobs max_comp=%ums max_ready=%ums max_loop=%ums max_flush=%ums inv=%lu "
                 "areas/%lu kpx flush_px=%llu kpx comp_px=%llu kpx free=%uK/%uK dir=%u ppa=%u/%u",
                 (unsigned) cpu_pct, (unsigned long long) cpu_us, (unsigned long long) this->perf_flush_us_,
                 (unsigned long long) dsi_stats.sync_us, (unsigned) (dsi_stats.sync_max_us / 1000U),
                 (unsigned long long) dsi_stats.copy_us, (unsigned) dsi_stats.staged_flushes,
                 (unsigned) (dsi_stats.copy_max_us / 1000U), (unsigned long long) dsi_stats.submit_us,
                 (unsigned) (dsi_stats.submit_max_us / 1000U), (unsigned long long) dsi_stats.done_us,
                 (unsigned) dsi_stats.done_flushes, (unsigned) (dsi_stats.done_max_us / 1000U),
                 (unsigned) dsi_stats.zero_copy_flushes, (unsigned) dsi_stats.staged_flushes,
                 (unsigned) dsi_stats.flushes, (unsigned) dsi_stats.unsafe_addr_flushes,
                 (unsigned) dsi_stats.unsafe_row_flushes, (unsigned) dsi_stats.unsafe_size_flushes,
                 (unsigned long long) (dsi_stats.staged_bytes / 1024ULL), (unsigned long long) compositor_us,
                 (unsigned long long) compositor_ready_us, (unsigned) compositor_jobs, (unsigned) compositor_max_ms,
                 (unsigned) compositor_ready_max_ms, (unsigned) (this->perf_loop_max_us_ / 1000U),
                 (unsigned) (this->perf_flush_max_us_ / 1000U), (unsigned long) this->perf_invalidated_areas_,
                 (unsigned long) (this->perf_invalidated_px_ / 1000ULL),
                 (unsigned long long) (this->perf_flush_px_ / 1000ULL), (unsigned long long) (compositor_px / 1000ULL),
                 (unsigned) free_psram_kb, (unsigned) free_internal_kb, (unsigned) s_direct_mode_active,
                 (unsigned) ppa_fill_tasks, (unsigned) ppa_img_tasks);
      }
      // Verbose-only log: enable via 'logs: lvgl: VERBOSE' in YAML if you
      // need the breakdown. Default DEBUG/INFO levels stay silent.
      ESP_LOGV(TAG, "perf: CPU %u%% (render %llu us, flush %llu us / wall %llu us)", (unsigned) cpu_pct,
               (unsigned long long) cpu_us, (unsigned long long) this->perf_flush_us_, (unsigned long long) elapsed_us);
      this->perf_busy_us_ = 0;
      this->perf_flush_us_ = 0;
      this->perf_invalidated_px_ = 0;
      this->perf_invalidated_areas_ = 0;
      this->perf_flush_px_ = 0;
#ifdef USE_ESP32
      this->perf_compositor_us_ = 0;
      this->perf_compositor_ready_us_ = 0;
      this->perf_compositor_px_ = 0;
      this->perf_compositor_jobs_ = 0;
      this->perf_compositor_max_us_ = 0;
      this->perf_compositor_ready_max_us_ = 0;
#endif
      this->perf_loop_max_us_ = 0;
      this->perf_flush_max_us_ = 0;
      this->perf_window_start_us_ = now_us;
    }
  }
}

#ifdef USE_LVGL_ANIMIMG
void lv_animimg_stop(lv_obj_t *obj) {
  int32_t duration = lv_animimg_get_duration(obj);
  lv_animimg_set_duration(obj, 0);
  lv_animimg_start(obj);
  lv_animimg_set_duration(obj, duration);
}
#endif

void LvglComponent::static_flush_cb(lv_display_t *disp_drv, const lv_area_t *area, uint8_t *color_p) {
  reinterpret_cast<LvglComponent *>(lv_display_get_user_data(disp_drv))->flush_cb_(disp_drv, area, color_p);
}

#if LV_USE_SCALE
void lv_scale_draw_event_cb(lv_event_t *e, int32_t range_start, int32_t range_end, lv_color_t color_start,
                            lv_color_t color_end, int width, bool local) {
  auto *scale = static_cast<lv_obj_t *>(lv_event_get_target(e));
  lv_draw_task_t *task = lv_event_get_draw_task(e);

  if (lv_draw_task_get_type(task) == LV_DRAW_TASK_TYPE_LINE) {
    auto *line_dsc = static_cast<lv_draw_line_dsc_t *>(lv_draw_task_get_draw_dsc(task));
    int32_t tick_value = line_dsc->base.id2;
    if (tick_value >= range_start && tick_value <= range_end) {
      int ratio;
      if (local) {
        int32_t range = range_end - range_start;
        ratio = range == 0 ? 0 : ((tick_value - range_start) * 255) / range;
      } else {
        auto tick_count = lv_scale_get_total_tick_count(scale);
        ratio = tick_count <= 1 ? 0 : (line_dsc->base.id1 * 255) / (tick_count - 1);
      }
      line_dsc->color = lv_color_mix(color_end, color_start, ratio);
      line_dsc->width += width;
    }
  }
}

void lv_scale_tick_offset_event_cb(lv_event_t *e, uint16_t offset, uint16_t stride) {
  auto *scale = static_cast<lv_obj_t *>(lv_event_get_target(e));
  lv_draw_task_t *task = lv_event_get_draw_task(e);
  auto type = lv_draw_task_get_type(task);

  if (type == LV_DRAW_TASK_TYPE_LINE) {
    auto *line_dsc = static_cast<lv_draw_line_dsc_t *>(lv_draw_task_get_draw_dsc(task));
    auto tick_idx = line_dsc->base.id1;

    bool is_major = (tick_idx >= offset) && ((tick_idx - offset) % stride == 0);

    if (!is_major) {
      line_dsc->color = lv_obj_get_style_line_color(scale, LV_PART_ITEMS);
      line_dsc->width = lv_obj_get_style_line_width(scale, LV_PART_ITEMS);

      int32_t minor_len = lv_obj_get_style_length(scale, LV_PART_ITEMS);
      int32_t major_len = lv_obj_get_style_length(scale, LV_PART_INDICATOR);
      if (major_len > 0 && minor_len > 0 && minor_len != major_len) {
        auto dx = line_dsc->p1.x - line_dsc->p2.x;
        auto dy = line_dsc->p1.y - line_dsc->p2.y;
        line_dsc->p1.x = line_dsc->p2.x + dx * minor_len / major_len;
        line_dsc->p1.y = line_dsc->p2.y + dy * minor_len / major_len;
      }
    }
  } else if (type == LV_DRAW_TASK_TYPE_LABEL) {
    auto *label_dsc = static_cast<lv_draw_label_dsc_t *>(lv_draw_task_get_draw_dsc(task));
    auto tick_idx = label_dsc->base.id1;

    bool is_major = (tick_idx >= offset) && ((tick_idx - offset) % stride == 0);

    if (!is_major) {
      label_dsc->opa = LV_OPA_TRANSP;
    }
  }
}
#endif  // LV_USE_SCALE

#ifdef USE_LVGL_GRADIENT
/**
 *
 * @param dsc The gradient descriptor containing the color stops
 * @param pos The current position to calculate the color for
 * @return The color for the given position
 */

lv_color_t lv_grad_calculate_color(const lv_grad_dsc_t *dsc, int32_t pos) {
  if (dsc->stops_count == 0)
    return lv_color_black();
  if (dsc->stops_count == 1 || pos <= dsc->stops[0].frac)
    return dsc->stops[0].color;
  if (pos >= dsc->stops[dsc->stops_count - 1].frac)
    return dsc->stops[dsc->stops_count - 1].color;
  int i = 1;
  while (i < dsc->stops_count && dsc->stops[i].frac < pos)
    i++;
  auto *stop1 = &dsc->stops[i - 1];
  auto *stop2 = &dsc->stops[i];
  int32_t range = stop2->frac - stop1->frac;
  int32_t offset = pos - stop1->frac;
  return lv_color_mix(stop2->color, stop1->color, range == 0 ? 0 : (offset * 255) / range);
}
#endif  // USE_LVGL_GRADIENT

lv_point_t LvglComponent::get_touch_relative_to_obj(lv_obj_t *obj) {
  auto *indev = lv_indev_get_act();
  if (indev == nullptr) {
    return {INT32_MAX, INT32_MAX};
  }
  lv_point_t point;
  lv_indev_get_point(indev, &point);
  lv_area_t coords;
  lv_obj_get_coords(obj, &coords);
  point.x -= coords.x1;
  point.y -= coords.y1;
  return point;
}

static void lv_container_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj) {
  LV_TRACE_OBJ_CREATE("begin");
  LV_UNUSED(class_p);
}

// Container class. Name is based on LVGL naming convention but upper case to keep ESPHome clang-tidy happy
const lv_obj_class_t LV_CONTAINER_CLASS = {
    .base_class = &lv_obj_class,
    .constructor_cb = lv_container_constructor,
    .name = "lv_container",
};

lv_obj_t *lv_container_create(lv_obj_t *parent) {
  lv_obj_t *obj = lv_obj_class_create_obj(&LV_CONTAINER_CLASS, parent);
  lv_obj_class_init_obj(obj);
  return obj;
}

}  // namespace esphome::lvgl

lv_result_t lv_mem_test_core() { return LV_RESULT_OK; }

void lv_mem_init() {}

void lv_mem_deinit() {}

#if defined(USE_HOST) || defined(USE_RP2040) || defined(USE_ESP8266)
void *lv_malloc_core(size_t size) {
  auto *ptr = malloc(size);  // NOLINT
  if (ptr == nullptr) {
    ESP_LOGE(esphome::lvgl::TAG, "Failed to allocate %zu bytes", size);
  }
  return ptr;
}
void lv_free_core(void *ptr) { return free(ptr); }                            // NOLINT
void *lv_realloc_core(void *ptr, size_t size) { return realloc(ptr, size); }  // NOLINT

void lv_mem_monitor_core(lv_mem_monitor_t *mon_p) { memset(mon_p, 0, sizeof(lv_mem_monitor_t)); }

#endif
#ifdef USE_ESP32
static unsigned cap_bits = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;  // NOLINT

void lv_mem_monitor_core(lv_mem_monitor_t *mon_p) {
  multi_heap_info_t heap_info;
  heap_caps_get_info(&heap_info, cap_bits);
  mon_p->total_size = heap_info.total_allocated_bytes + heap_info.total_free_bytes;
  mon_p->free_size = heap_info.total_free_bytes;
  mon_p->max_used = heap_info.total_allocated_bytes;
  mon_p->free_biggest_size = heap_info.largest_free_block;
  mon_p->used_cnt = heap_info.allocated_blocks;
  mon_p->free_cnt = heap_info.free_blocks;
  mon_p->used_pct = heap_info.allocated_blocks * 100 / (heap_info.allocated_blocks + heap_info.free_blocks);
  mon_p->frag_pct = 0;
}

void *lv_malloc_core(size_t size) {
  void *ptr;
  // Use 64-byte alignment for optimal ESP32 PSRAM/cache performance.
  // Note: LV_DRAW_BUF_ALIGN is set to 4 to avoid LVGL warnings from
  // internal stack/static buffers, but heap allocations use 64-byte alignment.
  constexpr size_t lvgl_alignment = 64;

  // BUGFIX: Don't modify global cap_bits - use local variable
  unsigned caps = cap_bits;

  // Try PSRAM first
  ptr = heap_caps_aligned_alloc(lvgl_alignment, size, caps);
  if (ptr == nullptr) {
    // Fallback to internal RAM if PSRAM allocation fails
    caps = MALLOC_CAP_8BIT;
    ptr = heap_caps_aligned_alloc(lvgl_alignment, size, caps);
  }

  if (ptr == nullptr) {
    ESP_LOGE(esphome::lvgl::TAG, "Failed to allocate %zu bytes (64-byte aligned)", size);
    return nullptr;
  }

  // Log only very large buffers (>1MB) for debugging
  if (size > 1000000) {
    ESP_LOGI(esphome::lvgl::TAG, "Large buffer allocated: %zu bytes at %p", size, ptr);
  }

  return ptr;
}

void lv_free_core(void *ptr) {
  ESP_LOGV(esphome::lvgl::TAG, "free %p", ptr);
  if (ptr == nullptr)
    return;
  heap_caps_free(ptr);
}

void *lv_realloc_core(void *ptr, size_t size) {
  ESP_LOGV(esphome::lvgl::TAG, "realloc %p: %zu", ptr, size);

  if (ptr == nullptr)
    return lv_malloc_core(size);
  if (size == 0) {
    lv_free_core(ptr);
    return nullptr;
  }

  // CRITICAL: heap_caps_realloc does NOT preserve 64-byte alignment!
  // We must allocate a new aligned buffer and copy the data
  void *new_ptr = lv_malloc_core(size);
  if (new_ptr == nullptr)
    return nullptr;

  // Copy data to new buffer using heap_caps_get_allocated_size for safe bounds
  size_t old_size = heap_caps_get_allocated_size(ptr);
  memcpy(new_ptr, ptr, (size < old_size) ? size : old_size);
  lv_free_core(ptr);

  return new_ptr;
}
#endif
