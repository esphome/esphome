#pragma once

#include <stdint.h>
#include <string>
#include "WString.h"
#include <vector>

namespace improv {

static const char *SERVICE_UUID = "00467768-6228-2272-4663-277478268000";
static const char *STATUS_UUID = "00467768-6228-2272-4663-277478268001";
static const char *ERROR_UUID = "00467768-6228-2272-4663-277478268002";
static const char *RPC_COMMAND_UUID = "00467768-6228-2272-4663-277478268003";
static const char *RPC_RESULT_UUID = "00467768-6228-2272-4663-277478268004";
static const char *CAPABILITIES_UUID = "00467768-6228-2272-4663-277478268005";

enum Error : uint8_t {
  ERROR_NONE = 0x00,
  ERROR_INVALID_RPC = 0x01,
  ERROR_UNKNOWN_RPC = 0x02,
  ERROR_UNABLE_TO_CONNECT = 0x03,
  ERROR_NOT_AUTHORIZED = 0x04,
  ERROR_UNKNOWN = 0xFF,
};

enum State : uint8_t {
  STATE_STOPPED = 0x00,
  STATE_AWAITING_AUTHORIZATION = 0x01,
  STATE_AUTHORIZED = 0x02,
  STATE_PROVISIONING = 0x03,
  STATE_PROVISIONED = 0x04,
};

enum Command : uint8_t {
  UNKNOWN = 0x00,
  WIFI_SETTINGS = 0x01,
  IDENTIFY = 0x02,
  BAD_CHECKSUM = 0xFF,
};

static const uint8_t CAPABILITY_IDENTIFY = 0x01;

struct ImprovCommand {
  Command command;
  std::string ssid;
  std::string password;
};

ImprovCommand parse_improv_data(const std::vector<uint8_t> &data);
ImprovCommand parse_improv_data(const uint8_t *data, size_t length);

std::vector<uint8_t> build_rpc_response(Command command, const std::vector<std::string> &datum);
std::vector<uint8_t> build_rpc_response(Command command, const std::vector<String> &datum);

}  // namespace improv
