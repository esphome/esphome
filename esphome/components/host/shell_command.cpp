#ifdef USE_HOST

#if !(defined(__linux__) || defined(__APPLE__))
#error This Host shell command implementation is not supported on this host OS
#endif

#include "shell_command.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <map>
#include <string>
#include <future>
#include <cstring>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <vector>
#include <unistd.h>

// POSIX defines `environ` as a global variable containing the process environment.
// It is not declared in a standard header on all platforms, so we declare it here.
extern char **environ;

namespace esphome::host {

static const char *const TAG = "host.shell";

static bool create_pipe(int fds[2]) {
  if (pipe(fds) != 0) {
    return false;
  }
  // Make the descriptors close-on-exec
  fcntl(fds[0], F_SETFD, FD_CLOEXEC);
  fcntl(fds[1], F_SETFD, FD_CLOEXEC);
  return true;
}

static std::map<std::string, std::string> current_environment() {
  std::map<std::string, std::string> env_map;
  for (char **env = environ; env != nullptr && *env != nullptr; ++env) {
    const std::string entry(*env);
    auto separator = entry.find('=');
    if (separator == std::string::npos) {
      continue;
    }
    env_map[entry.substr(0, separator)] = entry.substr(separator + 1);
  }
  return env_map;
}

ShellCommandResult execute_host_command(const std::string &command, const ShellCommandOptions &options) {
  ShellCommandResult result{};

  int stdout_pipe[2];
  int stderr_pipe[2];
  if (!create_pipe(stdout_pipe) || !create_pipe(stderr_pipe)) {
    ESP_LOGE(TAG, "Creating pipes failed: errno=%d", errno);
    return result;
  }

  pid_t pid = fork();
  if (pid == -1) {
    ESP_LOGE(TAG, "Fork failed: errno=%d", errno);
    close(stdout_pipe[0]);
    close(stdout_pipe[1]);
    close(stderr_pipe[0]);
    close(stderr_pipe[1]);
    return result;
  }

  if (pid == 0) {
    std::string shell = options.shell.empty() ? "/bin/sh" : options.shell;

    auto env_map = current_environment();
    for (const auto &kv : options.environment) {
      env_map[kv.first] = kv.second;
    }

    std::string env_log;
    for (const auto &kv : options.environment) {
      if (!env_log.empty()) {
        env_log.append(", ");
      }
      env_log.append(kv.first);
      env_log.append("=");
      env_log.append(kv.second);
    }

    std::vector<char *> envp;
    // strdup is used to allocate memory that remains valid after the map is out of scope.
    // There is no memory leak since the process memory is replaced by execle.
    // if execle fails, we immediately exit anyway
    envp.reserve(env_map.size() + 1);
    for (const auto &[k, v] : env_map) {
      envp.push_back(strdup((k + "=" + v).c_str()));
    }
    envp.push_back(nullptr);

    ESP_LOGD(TAG, "Executing command with shell '%s' and %zu custom env vars: %s", shell.c_str(),
             options.environment.size(), command.c_str());
    if (!env_log.empty()) {
      ESP_LOGD(TAG, "Custom environment variables from YAML: %s", env_log.c_str());
    }

    if (dup2(stdout_pipe[1], STDOUT_FILENO) == -1 || dup2(stderr_pipe[1], STDERR_FILENO) == -1) {
      _exit(127);
    }
    close(stdout_pipe[0]);
    close(stdout_pipe[1]);
    close(stderr_pipe[0]);
    close(stderr_pipe[1]);
    execle(shell.c_str(), shell.c_str(), "-c", command.c_str(), nullptr, envp.data());
    _exit(127);
  }

  close(stdout_pipe[1]);
  close(stderr_pipe[1]);

  bool stdout_open = true;
  bool stderr_open = true;
  std::array<char, 4096> buffer{};

  while (stdout_open || stderr_open) {
    fd_set read_fds;
    FD_ZERO(&read_fds);
    int max_fd = -1;
    if (stdout_open) {
      FD_SET(stdout_pipe[0], &read_fds);
      max_fd = std::max(max_fd, stdout_pipe[0]);
    }
    if (stderr_open) {
      FD_SET(stderr_pipe[0], &read_fds);
      max_fd = std::max(max_fd, stderr_pipe[0]);
    }

    int ready = select(max_fd + 1, &read_fds, nullptr, nullptr, nullptr);
    if (ready == -1) {
      if (errno == EINTR) {
        continue;
      }
      ESP_LOGW(TAG, "select() failed: errno=%d", errno);
      break;
    }

    if (stdout_open && FD_ISSET(stdout_pipe[0], &read_fds)) {
      ssize_t count = ::read(stdout_pipe[0], buffer.data(), buffer.size());
      if (count > 0) {
        result.stdout_output.append(buffer.data(), static_cast<size_t>(count));
      } else if (count == 0) {
        stdout_open = false;
      } else if (errno != EINTR) {
        ESP_LOGW(TAG, "Reading stdout failed: errno=%d", errno);
        stdout_open = false;
      }
    }

    if (stderr_open && FD_ISSET(stderr_pipe[0], &read_fds)) {
      ssize_t count = ::read(stderr_pipe[0], buffer.data(), buffer.size());
      if (count > 0) {
        result.stderr_output.append(buffer.data(), static_cast<size_t>(count));
      } else if (count == 0) {
        stderr_open = false;
      } else if (errno != EINTR) {
        ESP_LOGW(TAG, "Reading stderr failed: errno=%d", errno);
        stderr_open = false;
      }
    }
  }

  close(stdout_pipe[0]);
  close(stderr_pipe[0]);

  int status = 0;
  if (waitpid(pid, &status, 0) == -1) {
    ESP_LOGE(TAG, "waitpid failed: errno=%d", errno);
    result.exit_code = -1;
    return result;
  }

  if (WIFEXITED(status)) {
    result.exit_code = WEXITSTATUS(status);
  } else if (WIFSIGNALED(status)) {
    result.exit_code = 128 + WTERMSIG(status);
  } else {
    result.exit_code = -1;
  }

  ESP_LOGD(TAG, "Command stdout:\n%s", result.stdout_output.c_str());
  if (!result.stderr_output.empty()) {
    ESP_LOGE(TAG, "Command stderr:\n%s", result.stderr_output.c_str());
  }
  ESP_LOGD(TAG, "Command finished with exit code %d", result.exit_code);

  return result;
}

std::future<ShellCommandResult> execute_host_command_async(const std::string &command,
                                                           const ShellCommandOptions &options) {
  return std::async(std::launch::async, [command, options]() { return execute_host_command(command, options); });
}

ShellCommandResult execute_command(const std::vector<std::string> &args, const ShellCommandOptions &options) {
  ShellCommandResult result{};

  if (args.empty()) {
    ESP_LOGE(TAG, "execute_command requires at least one argument");
    return result;
  }

  int stdout_pipe[2];
  int stderr_pipe[2];
  if (!create_pipe(stdout_pipe) || !create_pipe(stderr_pipe)) {
    ESP_LOGE(TAG, "Creating pipes failed: errno=%d", errno);
    return result;
  }

  pid_t pid = fork();
  if (pid == -1) {
    ESP_LOGE(TAG, "Fork failed: errno=%d", errno);
    close(stdout_pipe[0]);
    close(stdout_pipe[1]);
    close(stderr_pipe[0]);
    close(stderr_pipe[1]);
    return result;
  }

  if (pid == 0) {
    std::string shell = options.shell.empty() ? "/bin/sh" : options.shell;

    auto env_map = current_environment();
    for (const auto &kv : options.environment) {
      env_map[kv.first] = kv.second;
    }

    std::string env_log;
    for (const auto &kv : options.environment) {
      if (!env_log.empty()) {
        env_log.append(", ");
      }
      env_log.append(kv.first);
      env_log.append("=");
      env_log.append(kv.second);
    }

    std::vector<char *> envp;
    // strdup is used to allocate memory that remains valid after the map is out of scope.
    // There is no memory leak since the process memory is replaced by execve/execle.
    // if the exec* call fails, we immediately exit anyway
    envp.reserve(env_map.size() + 1);
    for (const auto &[k, v] : env_map) {
      envp.push_back(strdup((k + "=" + v).c_str()));
    }
    envp.push_back(nullptr);

    std::string command_desc;
    for (size_t i = 0; i < args.size(); i++) {
      if (i != 0) {
        command_desc.append(" ");
      }
      command_desc.append(args[i]);
    }

    ESP_LOGD(TAG, "Executing command (%s) with %zu custom env vars: %s",
             options.use_shell ? "via shell" : "direct execve", options.environment.size(), command_desc.c_str());
    if (!env_log.empty()) {
      ESP_LOGD(TAG, "Custom environment variables from YAML: %s", env_log.c_str());
    }

    if (dup2(stdout_pipe[1], STDOUT_FILENO) == -1 || dup2(stderr_pipe[1], STDERR_FILENO) == -1) {
      _exit(127);
    }
    close(stdout_pipe[0]);
    close(stdout_pipe[1]);
    close(stderr_pipe[0]);
    close(stderr_pipe[1]);

    if (options.use_shell) {
      execle(shell.c_str(), shell.c_str(), "-c", command_desc.c_str(), nullptr, envp.data());
    } else {
      std::vector<char *> argv;
      argv.reserve(args.size() + 1);
      for (const auto &arg : args) {
        argv.push_back(const_cast<char *>(arg.c_str()));
      }
      argv.push_back(nullptr);
      execve(args.front().c_str(), argv.data(), envp.data());
    }
    _exit(127);
  }

  close(stdout_pipe[1]);
  close(stderr_pipe[1]);

  bool stdout_open = true;
  bool stderr_open = true;
  std::array<char, 4096> buffer{};

  while (stdout_open || stderr_open) {
    fd_set read_fds;
    FD_ZERO(&read_fds);
    int max_fd = -1;
    if (stdout_open) {
      FD_SET(stdout_pipe[0], &read_fds);
      max_fd = std::max(max_fd, stdout_pipe[0]);
    }
    if (stderr_open) {
      FD_SET(stderr_pipe[0], &read_fds);
      max_fd = std::max(max_fd, stderr_pipe[0]);
    }

    int ready = select(max_fd + 1, &read_fds, nullptr, nullptr, nullptr);
    if (ready == -1) {
      if (errno == EINTR) {
        continue;
      }
      ESP_LOGW(TAG, "select() failed: errno=%d", errno);
      break;
    }

    if (stdout_open && FD_ISSET(stdout_pipe[0], &read_fds)) {
      ssize_t count = ::read(stdout_pipe[0], buffer.data(), buffer.size());
      if (count > 0) {
        result.stdout_output.append(buffer.data(), static_cast<size_t>(count));
      } else if (count == 0) {
        stdout_open = false;
      } else if (errno != EINTR) {
        ESP_LOGW(TAG, "Reading stdout failed: errno=%d", errno);
        stdout_open = false;
      }
    }

    if (stderr_open && FD_ISSET(stderr_pipe[0], &read_fds)) {
      ssize_t count = ::read(stderr_pipe[0], buffer.data(), buffer.size());
      if (count > 0) {
        result.stderr_output.append(buffer.data(), static_cast<size_t>(count));
      } else if (count == 0) {
        stderr_open = false;
      } else if (errno != EINTR) {
        ESP_LOGW(TAG, "Reading stderr failed: errno=%d", errno);
        stderr_open = false;
      }
    }
  }

  close(stdout_pipe[0]);
  close(stderr_pipe[0]);

  int status = 0;
  if (waitpid(pid, &status, 0) == -1) {
    ESP_LOGE(TAG, "waitpid failed: errno=%d", errno);
    result.exit_code = -1;
    return result;
  }

  if (WIFEXITED(status)) {
    result.exit_code = WEXITSTATUS(status);
  } else if (WIFSIGNALED(status)) {
    result.exit_code = 128 + WTERMSIG(status);
  } else {
    result.exit_code = -1;
  }

  ESP_LOGD(TAG, "Command stdout:\n%s", result.stdout_output.c_str());
  if (!result.stderr_output.empty()) {
    ESP_LOGE(TAG, "Command stderr:\n%s", result.stderr_output.c_str());
  }
  ESP_LOGD(TAG, "Command finished with exit code %d", result.exit_code);

  return result;
}

std::future<ShellCommandResult> execute_command_async(const std::vector<std::string> &args,
                                                      const ShellCommandOptions &options) {
  return std::async(std::launch::async, [args, options]() { return execute_command(args, options); });
}

}  // namespace esphome::host

#endif  // USE_HOST
