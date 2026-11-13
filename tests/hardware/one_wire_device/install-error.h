#pragma once
#include <atomic>
#include <esp_intr_alloc.h>
#include <esp_ipc.h>
#include <soc/interrupts.h>
inline std::atomic<int> ow_install_fault{0};
inline std::atomic<void *> ow_install_target{nullptr};
extern "C" esp_err_t __real_esp_intr_alloc(int, int, intr_handler_t, void *, intr_handle_t *);
extern "C" esp_err_t __wrap_esp_intr_alloc(int source, int flags, intr_handler_t handler, void *arg,
                                           intr_handle_t *handle) {
  if (ow_install_fault.load() == 1 && source == ETS_GPIO_INTR_SOURCE && arg == ow_install_target.load())
    return ESP_ERR_NOT_FOUND;
  return __real_esp_intr_alloc(source, flags, handler, arg, handle);
}
extern "C" esp_err_t __real_esp_ipc_call_blocking(uint32_t, esp_ipc_func_t, void *);
extern "C" esp_err_t __wrap_esp_ipc_call_blocking(uint32_t cpu, esp_ipc_func_t function, void *arg) {
  if (ow_install_fault.load() == 2 && arg == ow_install_target.load())
    return ESP_ERR_TIMEOUT;
  return __real_esp_ipc_call_blocking(cpu, function, arg);
}
