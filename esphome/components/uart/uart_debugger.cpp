#include "esphome/core/defines.h"
#ifdef USE_UART_DEBUGGER

#include <vector>
#include "uart_debugger.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace uart {

static const char *const TAG = "uart_debug";

static const char *hexchars = "0123456789ABCDEF";

void UARTDebug::log_hex(UARTDirection direction, std::vector<uint8_t> bytes, uint8_t separator) {
  size_t len = bytes.size();
  size_t target_len = len * 3 - 1 + 4 /* prefix */;
  std::string res(target_len, ' ');
  if (direction == UART_DIRECTION_RX) {
    res[0] = res[1] = res[2] = '<';
  } else {
    res[0] = res[1] = res[2] = '>';
  } 
  for (size_t i = 0; i < len; i++) {
    res[4 + 3*i] = hexchars[(bytes[i] & 0xF0) >> 4];
    res[4 + 3*i + 1] = hexchars[(bytes[i] & 0x0F) >> 0];
    if (i != (len - 1)) {
      res[4 + 3*i + 2] = separator;
    }
  }
  ESP_LOGD(TAG, "%s", res.c_str());
}

void UARTDebug::log_string(UARTDirection direction, std::vector<uint8_t> bytes) {
  size_t len = bytes.size();
  size_t target_len = len + 4 /* prefix */ + 2 /* quotes */;
  for (int i = 0; i < len; i++) {
    if ((bytes[i] >= 7 && bytes[i] <= 13) || bytes[i] == 27 ||
        bytes[i] == 34 || bytes[i] == 39 || bytes[i] == 92) {
      target_len += 2;
    }
    else if (bytes[i] < 32 || bytes[i] > 126) {
      target_len += 4;
    }
  }
  std::string res(target_len, ' ');
  if (direction == UART_DIRECTION_RX) {
    res[0] = res[1] = res[2] = '<';
  } else {
    res[0] = res[1] = res[2] = '>';
  } 
  size_t pos = 4;
  res[pos++] = '"';
  for (size_t i = 0; i < len; i++) {
    if (bytes[i] == 7) { res[pos++] = '\\'; res[pos++] = 'a'; }
    else if (bytes[i] == 8) { res[pos++] = '\\'; res[pos++] = 'b'; }
    else if (bytes[i] == 9) { res[pos++] = '\\'; res[pos++] = 't'; }
    else if (bytes[i] == 10) { res[pos++] = '\\'; res[pos++] = 'n'; }
    else if (bytes[i] == 11) { res[pos++] = '\\'; res[pos++] = 'v'; }
    else if (bytes[i] == 12) { res[pos++] = '\\'; res[pos++] = 'f'; }
    else if (bytes[i] == 13) { res[pos++] = '\\'; res[pos++] = 'r'; }
    else if (bytes[i] == 27) { res[pos++] = '\\'; res[pos++] = 'e'; }
    else if (bytes[i] == 34) { res[pos++] = '\\'; res[pos++] = '"'; }
    else if (bytes[i] == 39) { res[pos++] = '\\'; res[pos++] = '\''; }
    else if (bytes[i] == 92) { res[pos++] = '\\'; res[pos++] = '\\'; }
    else if (bytes[i] < 32 || bytes[i] > 127) {
      res[pos++] = '\\';
      res[pos++] = 'x';
      res[pos++] = hexchars[(bytes[i] & 0xF0) >> 4];
      res[pos++] = hexchars[(bytes[i] & 0x0F) >> 0];
    }
    else {
      res[pos++] = bytes[i]; 
    }
  } 
  res[pos++] = '"';
  ESP_LOGD(TAG, "%s", res.c_str());
}

void UARTDebug::log_int(UARTDirection direction, std::vector<uint8_t> bytes, uint8_t separator) {
  std::string res;
  size_t len = bytes.size();
  if (direction == UART_DIRECTION_RX) { res += "<<< "; } else { res += ">>> "; }
  for (size_t i = 0; i < len; i++) {
    if (i > 0) { res += separator; }
    res += to_string(bytes[i]);
  } 
  ESP_LOGD(TAG, "%s", res.c_str());
}

}  // namespace uart
}  // namespace esphome
#endif
