#pragma once
#include <atomic>
#include <esp_ipc.h>
#include <esp_intr_alloc.h>
#include "esphome/core/log.h"
// Targeted linker faults, enabled only by the teardown bench fixture.
inline std::atomic<int> ow_fault_mode{0};
inline std::atomic<void *> ow_fault_target{nullptr};
inline std::atomic<intr_handle_t> ow_failed_handle{nullptr};
inline std::atomic<bool> ow_free_called{false}, ow_handle_retained{false};
extern "C" esp_err_t __real_esp_intr_free(intr_handle_t);
extern "C" esp_err_t __real_esp_ipc_call_blocking(uint32_t, esp_ipc_func_t, void *);
extern "C" esp_err_t __wrap_esp_intr_free(intr_handle_t handle) {
  if (ow_fault_mode.load() == 1) {
    ow_failed_handle.store(handle);
    return ESP_ERR_INVALID_STATE;
  }
  ow_free_called.store(true);
  ow_handle_retained.store(handle == ow_failed_handle.load());
  return __real_esp_intr_free(handle);
}
extern "C" esp_err_t __wrap_esp_ipc_call_blocking(uint32_t core, esp_ipc_func_t function, void *arg) {
  if (ow_fault_mode.load() == 2 && arg == ow_fault_target.load())
    return ESP_ERR_TIMEOUT;
  return __real_esp_ipc_call_blocking(core, function, arg);
}
