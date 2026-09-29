#pragma once
#ifdef USE_HOST

#include <string>

namespace esphome::host {

/// argv captured by main(); stable for process lifetime.
char **get_argv();

/// Absolute path to running exe (resolved at startup); empty on failure.
const std::string &get_exe_path();

/// Arm an execv on the next arch_restart(). Pass empty to disarm.
void arm_reexec(const std::string &path);

/// Armed re-exec path, or nullptr.
const char *get_reexec_path();

}  // namespace esphome::host

#endif  // USE_HOST
