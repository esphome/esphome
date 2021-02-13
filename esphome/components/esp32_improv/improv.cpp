#include "improv.h"

namespace improv {

ImprovCommand parse_improv_data(const uint8_t *data, uint8_t length) {
  Command command = (Command) data[0];
  uint8_t data_length = data[1];
  uint8_t checksum = data[length - 1];

  uint8_t calculated_checksum = 0;
  for (uint8_t i = 0; i < length - 1; i++) {
    calculated_checksum += data[i];
  }

  if (calculated_checksum != checksum) {
    return {.command = UNKNOWN};
  }

  if (command == WIFI_SETTINGS) {
    uint8_t ssid_length = data[2];
    uint8_t ssid_start = 3;
    uint8_t ssid_end = ssid_start + ssid_length - 1;

    uint8_t pass_length = data[ssid_end + 1];
    uint8_t pass_start = ssid_end + 2;
    uint8_t pass_end = pass_start + pass_length - 1;

    std::string ssid(*data + ssid_start, ssid_length);
    std::string password(*data + pass_start, pass_length);
    return {.command = command, .ssid = ssid, .password = password};
  }

  return {
      .command = command,
  };
}

}  // namespace improv
