#pragma once
#ifdef USE_HOST

#include <string>

namespace esphome::host {

/// Returns argv as captured by main(). Stable for the life of the process.
/// Used by host OTA re-exec to preserve the original arguments.
char **get_argv();

/// Returns the absolute path to the running executable. Resolved once at
/// startup using /proc/self/exe (Linux), _NSGetExecutablePath (macOS), or
/// argv[0] as a fallback. Empty if resolution failed.
const std::string &get_exe_path();

/// Arm a pending re-exec. The next call to `arch_restart()` (typically via
/// `App::safe_reboot()`) will execv `path` with the original argv instead
/// of exit(0). Used by the host OTA backend after a successful firmware
/// swap. Pass an empty string to disarm.
void arm_reexec(const std::string &path);

/// Returns the armed re-exec path, or nullptr if no re-exec is armed.
const char *get_reexec_path();

}  // namespace esphome::host

#endif  // USE_HOST
