#ifdef USE_HOST

#include "core.h"

#include "esphome/core/application.h"
#include "preferences.h"

#include <climits>
#include <csignal>
#include <cstdlib>
#include <string>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

#ifdef __linux__
#include <unistd.h>
#endif

namespace {
volatile sig_atomic_t s_signal_received = 0;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
void signal_handler(int signal) { s_signal_received = signal; }

char **s_argv = nullptr;               // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
std::string *s_exe_path = nullptr;     // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
std::string *s_reexec_path = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

std::string resolve_exe_path(const char *argv0) {
#ifdef __linux__
  char buf[PATH_MAX];
  ssize_t len = ::readlink("/proc/self/exe", buf, sizeof(buf) - 1);
  if (len > 0) {
    buf[len] = '\0';
    return std::string(buf);
  }
#endif
#ifdef __APPLE__
  char buf[PATH_MAX];
  uint32_t size = sizeof(buf);
  if (_NSGetExecutablePath(buf, &size) == 0) {
    char real[PATH_MAX];
    if (::realpath(buf, real) != nullptr)
      return std::string(real);
    return std::string(buf);
  }
#endif
  if (argv0 == nullptr)
    return {};
  char real[PATH_MAX];
  if (::realpath(argv0, real) != nullptr)
    return std::string(real);
  return std::string(argv0);
}
}  // namespace

namespace esphome::host {

char **get_argv() { return s_argv; }

const std::string &get_exe_path() {
  static const std::string empty;
  return s_exe_path != nullptr ? *s_exe_path : empty;
}

void arm_reexec(const std::string &path) {
  if (s_reexec_path != nullptr)
    *s_reexec_path = path;
}

const char *get_reexec_path() {
  if (s_reexec_path == nullptr || s_reexec_path->empty())
    return nullptr;
  return s_reexec_path->c_str();
}

}  // namespace esphome::host

// HAL functions live in hal.cpp.

void setup();
void loop();
int main(int argc, char **argv) {
  s_argv = argv;
  static std::string exe_path = resolve_exe_path(argc > 0 ? argv[0] : nullptr);
  s_exe_path = &exe_path;
  static std::string reexec_path;
  s_reexec_path = &reexec_path;

  // Install signal handlers for graceful shutdown (flushes preferences to disk)
  std::signal(SIGINT, signal_handler);
  std::signal(SIGTERM, signal_handler);

  esphome::host::setup_preferences();
  setup();
  while (s_signal_received == 0) {
    loop();
  }
  esphome::App.run_safe_shutdown_hooks();
  return 0;
}

#endif  // USE_HOST
