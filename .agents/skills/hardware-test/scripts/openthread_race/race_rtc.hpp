#pragma once
// Marks a mode 3 race as fired in RTC memory, which survives a panic or watchdog reset, without a flash write in
// the timing-sensitive log callback. race-auto.yaml reads and clears it on boot.
#include <cstdint>
#include <esp_attr.h>

static constexpr uint32_t RACE_RTC_FIRED = 0x52414345;  // "RACE"; RTC memory holds garbage after power on
static RTC_NOINIT_ATTR uint32_t race_rtc_fired;
