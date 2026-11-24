#pragma once
extern "C" {
#include <user_interface.h>
}
static uint32_t repro_scan_done_count = 0;
static void repro_scan_done(void *arg, STATUS status) { repro_scan_done_count++; }
