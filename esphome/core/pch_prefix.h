#pragma once
// The curated core headers the precompiled header holds. Guarded because
// PlatformIO force-includes this into C and assembly compiles too.
#ifdef __cplusplus
#include "esphome/core/application.h"
#include "esphome/core/automation.h"
#endif
