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
  ERROR_BAD_CREDENTIALS = 0x03,
  ERROR_NOT_ACTIVATED = 0x04,
  ERROR_UNKNOWN = 0xFF,
};

enum State : uint8_t {
  STATE_NONE = 0,
  STATE_STOPPED,
  STATE_STARTED,
  STATE_ACTIVATED,
  STATE_RECEIVED,
  STATE_SAVED,
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
