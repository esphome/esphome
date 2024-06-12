#include "esphome/core/defines.h"
#ifdef USE_LVGL
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/hal.h"
#include "lvgl_hal.h"

namespace esphome {
namespace lvgl {}  // namespace lvgl
}  // namespace esphome
static const char *const TAG = "lvgl";
size_t lv_millis(void) { return esphome::millis(); }

#if defined(USE_HOST) || defined(USE_RP2040) || defined(USE_ESP8266)
void *lv_custom_mem_alloc(size_t size) {
  auto *ptr = malloc(size);  // NOLINT
  if (ptr == nullptr) {
    esphome::esph_log_e(TAG, "Failed to allocate %zu bytes", size);
  }
  return ptr;
}
void lv_custom_mem_free(void *ptr) { return free(ptr); }                            // NOLINT
void *lv_custom_mem_realloc(void *ptr, size_t size) { return realloc(ptr, size); }  // NOLINT
#else
static unsigned cap_bits = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;  // NOLINT

void *lv_custom_mem_alloc(size_t size) {
  void *ptr;
  ptr = heap_caps_malloc(size, cap_bits);
  if (ptr == nullptr) {
    cap_bits = MALLOC_CAP_8BIT;
    ptr = heap_caps_malloc(size, cap_bits);
  }
  if (ptr == nullptr) {
    esphome::esph_log_e(TAG, "Failed to allocate %zu bytes", size);
    return nullptr;
  }
  esphome::esph_log_v(TAG, "allocate %zu - > %p", size, ptr);
  return ptr;
}

void lv_custom_mem_free(void *ptr) {
  esphome::esph_log_v(TAG, "free %p", ptr);
  if (ptr == nullptr)
    return;
  heap_caps_free(ptr);
}

void *lv_custom_mem_realloc(void *ptr, size_t size) {
  esphome::esph_log_v(TAG, "realloc %p: %zu", ptr, size);
  return heap_caps_realloc(ptr, size, cap_bits);
}
#endif

#endif
