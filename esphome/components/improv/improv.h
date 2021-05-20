#pragma once

#include <stdint.h>
#include <string>

namespace improv {

static const char *SERVICE_UUID = "00467768-6228-2272-4663-277478268000";
static const char *STATUS_UUID = "00467768-6228-2272-4663-277478268001";
static const char *ERROR_UUID = "00467768-6228-2272-4663-277478268002";
static const char *RPC_UUID = "00467768-6228-2272-4663-277478268003";
static const char *RESPONSE_UUID = "00467768-6228-2272-4663-277478268004";

enum Error : uint8_t {
  ERROR_NONE = 0x00,
  ERROR_INVALID_RPC = 0x01,
  ERROR_UNKNOWN_RPC = 0x02,
  ERROR_UNABLE_TO_CONNECT = 0x03,
  ERROR_NOT_ACTIVATED = 0x04,
  ERROR_UNKNOWN = 0xFF,
};

enum State : uint8_t {
  STATE_STOPPED = 0x00,
  STATE_AWAITING_ACTIVATION = 0x01,
  STATE_ACTIVATED = 0x02,
  STATE_PROVISIONING = 0x03,
  STATE_PROVISIONED = 0x04,
};

enum Command : uint8_t {
  UNKNOWN = 0x00,
  WIFI_SETTINGS = 0x01,
  IDENTIFY = 0x02,
  BAD_CHECKSUM = 0xFF,
};

struct ImprovCommand {
  Command command;
  std::string ssid;
  std::string password;
};

ImprovCommand parse_improv_data(const uint8_t *data, uint8_t length);

}  // namespace improv
