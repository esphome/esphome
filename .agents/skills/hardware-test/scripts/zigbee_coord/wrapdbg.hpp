#pragma once
// Print who calls nvs_erase_all and esp_restart (Zigbee leave handling). Link with
// -Wl,--wrap=nvs_erase_all,--wrap=esp_restart; esp_rom_printf is synchronous, so the line survives the reset.
#include "esp_rom_sys.h"
#include "nvs.h"
extern "C" {
esp_err_t __real_nvs_erase_all(nvs_handle_t h);
esp_err_t __wrap_nvs_erase_all(nvs_handle_t h) {
  esp_rom_printf("DBGWRAP nvs_erase_all ra=%p\n", __builtin_return_address(0));
  return __real_nvs_erase_all(h);
}
void __real_esp_restart(void);
void __wrap_esp_restart(void) {
  esp_rom_printf("DBGWRAP esp_restart ra=%p\n", __builtin_return_address(0));
  __real_esp_restart();
}
}
