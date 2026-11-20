#pragma once
#include <esp_mac.h>
// Linker-injected SDK failure for this bench fixture only; production has no test branch.
extern "C" esp_err_t __wrap_esp_read_mac(uint8_t *, esp_mac_type_t) { return ESP_ERR_INVALID_STATE; }
