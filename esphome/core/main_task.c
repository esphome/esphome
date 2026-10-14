#include "esphome/core/main_task.h"

#if defined(USE_ESP32) || defined(USE_LIBRETINY)
TaskHandle_t esphome_main_task_handle = NULL;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
#endif
