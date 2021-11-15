#include "improv.h"

namespace improv {

ImprovCommand parse_improv_data(const std::vector<uint8_t> &data, bool check_checksum) {
  return parse_improv_data(data.data(), data.size(), check_checksum);
}

ImprovCommand parse_improv_data(const uint8_t *data, size_t length, bool check_checksum) {
  ImprovCommand improv_command;
  Command command = (Command) data[0];
  uint8_t data_length = data[1];

  if (data_length != length - 2 - check_checksum) {
    improv_command.command = UNKNOWN;
    return improv_command;
  }

  if (check_checksum) {
    uint8_t checksum = data[length - 1];

    uint32_t calculated_checksum = 0;
    for (uint8_t i = 0; i < length - 1; i++) {
      calculated_checksum += data[i];
    }

    if ((uint8_t) calculated_checksum != checksum) {
      improv_command.command = BAD_CHECKSUM;
      return improv_command;
    }
  }

  if (command == WIFI_SETTINGS) {
    uint8_t ssid_length = data[2];
    uint8_t ssid_start = 3;
    size_t ssid_end = ssid_start + ssid_length;

    uint8_t pass_length = data[ssid_end];
    size_t pass_start = ssid_end + 1;
    size_t pass_end = pass_start + pass_length;

    std::string ssid(data + ssid_start, data + ssid_end);
    std::string password(data + pass_start, data + pass_end);
    return {.command = command, .ssid = ssid, .password = password};
  }

  improv_command.command = command;
  return improv_command;
}

std::vector<uint8_t> build_rpc_response(Command command, const std::vector<std::string> &datum, bool add_checksum) {
  std::vector<uint8_t> out;
  uint32_t length = 0;
  out.push_back(command);
  for (auto str : datum) {
    uint8_t len = str.length();
    length += len;
    out.push_back(len);
    out.insert(out.end(), str.begin(), str.end());
  }
  out.insert(out.begin() + 1, length);

  if (add_checksum) {
    uint32_t calculated_checksum = 0;

    for (uint8_t byte : out) {
      calculated_checksum += byte;
    }
    out.push_back(calculated_checksum);
  }
  return out;
}

#ifdef ARDUINO
std::vector<uint8_t> build_rpc_response(Command command, const std::vector<String> &datum, bool add_checksum) {
  std::vector<uint8_t> out;
  uint32_t length = 0;
  out.push_back(command);
  for (auto str : datum) {
    uint8_t len = str.length();
    length += len;
    out.push_back(len);
    out.insert(out.end(), str.begin(), str.end());
  }
  out.insert(out.begin() + 1, length);

  if (add_checksum) {
    uint32_t calculated_checksum = 0;

    for (uint8_t byte : out) {
      calculated_checksum += byte;
    }
    out.push_back(calculated_checksum);
  }
  return out;
}
#endif  // ARDUINO

}  // namespace improv
