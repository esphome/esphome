#pragma once

#ifdef USE_HOST

#include <string>
#include <utility>
#include <vector>
#include <future>


namespace esphome::host {

struct ShellCommandResult {
  int exit_code{-1};
  std::string stdout_output;
  std::string stderr_output;
};

struct ShellCommandOptions {
  std::string shell{"/bin/sh"};
  std::vector<std::pair<std::string, std::string>> environment;
  bool use_shell{HOST_SHELL_COMMAND_USE_SHELL_DEFAULT_VALUE};
};

ShellCommandResult execute_host_command(const std::string &command, const ShellCommandOptions &options = {});
ShellCommandResult execute_command(const std::vector<std::string> &args, const ShellCommandOptions &options = {});
std::future<ShellCommandResult> execute_host_command_async(const std::string &command,
                                                           const ShellCommandOptions &options = {});
std::future<ShellCommandResult> execute_command_async(const std::vector<std::string> &args,
                                                      const ShellCommandOptions &options = {});

}  // namespace esphome::host

#endif  // USE_HOST
