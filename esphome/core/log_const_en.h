#pragma once

#include "defines.h"

#ifdef USE_ESP8266
#define ESP_LOG_MSG_COMM_FAIL "Communication failed"
#define ESP_LOG_MSG_COMM_FAIL_FOR "Communication failed for '%s'"
#else
constexpr const char *const ESP_LOG_MSG_COMM_FAIL = "Communication failed";
constexpr const char *const ESP_LOG_MSG_COMM_FAIL_FOR = "Communication failed for '%s'";
#endif
