#pragma once

// The few FreeRTOS names the runtime and esphome/core/main_task.h need on the host.
using TaskHandle_t = void *;
using BaseType_t = int;
using TickType_t = unsigned;
static constexpr BaseType_t pdTRUE = 1;
#define configTASK_NOTIFICATION_ARRAY_ENTRIES 2  // NOLINT: main_task.h tests it in an #if
