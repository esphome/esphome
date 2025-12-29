#pragma once

#ifdef USE_HOST

#include <string>
#include <utility>
#include <vector>

namespace esphome {
namespace host {

struct ShellCommandResult {
  int exit_code{-1};
  std::string stdout_output;
  std::string stderr_output;
};

struct ShellCommandOptions {
  std::string shell{"/bin/sh"};
  std::vector<std::pair<std::string, std::string>> environment;
};

ShellCommandResult execute_shell_command(const std::string &command,
                                         const ShellCommandOptions &options = {});

}  // namespace host
}  // namespace esphome

#endif  // USE_HOST
