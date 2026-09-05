#pragma once
// Curated precompiled-header prefix for backends that force-include it via
// build_src_flags (the pch script folds it into the .gch). Guarded because
// build_src_flags also reaches C and assembly src edges.
#ifdef __cplusplus
#include "esphome/core/application.h"
#include "esphome/core/automation.h"
#endif
