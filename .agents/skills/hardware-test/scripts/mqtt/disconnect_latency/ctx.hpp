#pragma once
// Test only: names the task (or ESP8266 sys/cont context) an automation runs in.
#if defined(USE_ESP8266)
#include <Arduino.h>
extern "C" bool can_yield();
inline const char *mq_ctx() { return can_yield() ? "cont" : "sys"; }
#elif defined(USE_LIBRETINY)
#include <FreeRTOS.h>
#include <task.h>
inline const char *mq_ctx() { return pcTaskGetName(nullptr); }
#else
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
inline const char *mq_ctx() { return pcTaskGetName(nullptr); }
#endif
