#pragma once
// Marks a fired race in RTC memory, which survives a panic, a watchdog reset and a software reboot, without a flash
// write at the timing-sensitive moment of the race. race-auto.yaml reads and clears it on boot.
#include <cstdint>
#include <esp_attr.h>

static constexpr uint32_t RACE_RTC_FIRED = 0x52414345;   // "RACE"; RTC memory holds garbage after power on
static constexpr uint32_t RACE_RTC_REBOOT = 0x52424f54;  // "RBOT": a mode 2 race about to reboot
static RTC_NOINIT_ATTR uint32_t race_rtc_fired;
