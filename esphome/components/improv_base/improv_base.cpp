#include "improv_base.h"

#include <cstring>
#include "esphome/components/network/util.h"
#include "esphome/core/application.h"
#include "esphome/core/defines.h"

namespace esphome {
namespace improv_base {

#if defined(USE_ESP32_IMPROV_NEXT_URL) || defined(USE_IMPROV_SERIAL_NEXT_URL)
static constexpr const char DEVICE_NAME_PLACEHOLDER[] = "{{device_name}}";
static constexpr size_t DEVICE_NAME_PLACEHOLDER_LEN = sizeof(DEVICE_NAME_PLACEHOLDER) - 1;
static constexpr const char IP_ADDRESS_PLACEHOLDER[] = "{{ip_address}}";
static constexpr size_t IP_ADDRESS_PLACEHOLDER_LEN = sizeof(IP_ADDRESS_PLACEHOLDER) - 1;

/// Copy src to dest, returning pointer past last written char. Stops at end or if src is null.
static char *copy_to_buffer(char *dest, const char *end, const char *src) {
  if (src == nullptr) {
    return dest;
  }
  while (*src != '\0' && dest < end) {
    *dest++ = *src++;
  }
  return dest;
}

size_t ImprovBase::get_formatted_next_url_(char *buffer, size_t buffer_size) {
  if (this->next_url_ == nullptr || buffer_size == 0) {
    if (buffer_size > 0) {
      buffer[0] = '\0';
    }
    return 0;
  }

  // Get IP address once for replacement
  const char *ip_str = nullptr;
  char ip_buffer[network::IP_ADDRESS_BUFFER_SIZE];
  for (auto &ip : network::get_ip_addresses()) {
    if (ip.is_ip4()) {
      ip.str_to(ip_buffer);
      ip_str = ip_buffer;
      break;
    }
  }

  const char *device_name = App.get_name().c_str();
  char *out = buffer;
  const char *end = buffer + buffer_size - 1;

  // Note: {{esphome_version}} is replaced at code generation time in Python
  for (const char *p = this->next_url_; *p != '\0' && out < end;) {
    if (strncmp(p, DEVICE_NAME_PLACEHOLDER, DEVICE_NAME_PLACEHOLDER_LEN) == 0) {
      out = copy_to_buffer(out, end, device_name);
      p += DEVICE_NAME_PLACEHOLDER_LEN;
    } else if (ip_str != nullptr && strncmp(p, IP_ADDRESS_PLACEHOLDER, IP_ADDRESS_PLACEHOLDER_LEN) == 0) {
      out = copy_to_buffer(out, end, ip_str);
      p += IP_ADDRESS_PLACEHOLDER_LEN;
    } else {
      *out++ = *p++;
    }
  }
  *out = '\0';
  return out - buffer;
}
#endif

}  // namespace improv_base
}  // namespace esphome
