#include "esphome/core/automation.h"
#include "esphome/core/defines.h"
#include <cstring>

#ifdef USE_ESP8266
#include "esphome/core/progmem.h"
#endif

namespace esphome::detail {

// Must match the enum order in TemplatableValue<std::string, X...>::type_.
// LAMBDA (2) and STATELESS_LAMBDA (3) are handled inline by the caller
// because they depend on the X... pack.
static constexpr uint8_t TSS_NONE = 0;
static constexpr uint8_t TSS_VALUE = 1;
static constexpr uint8_t TSS_STATIC_STRING = 4;
static constexpr uint8_t TSS_FLASH_STRING = 5;

std::string load_string_storage(uint8_t type, const char *storage) {
  // Build into one `result` via out-of-line std::string methods so each arm
  // avoids its own inline SSO-branch construction body.
  std::string result;
  switch (type) {
    case TSS_VALUE: {
      const auto *s = reinterpret_cast<const std::string *>(storage);
      result.assign(s->data(), s->size());
      break;
    }
    case TSS_STATIC_STRING:
      result.assign(storage, strlen(storage));
      break;
#ifdef USE_ESP8266
    case TSS_FLASH_STRING: {
      size_t len = strlen_P(storage);
      result.assign(len, '\0');
      memcpy_P(&result[0], storage, len);
      break;
    }
#endif
    case TSS_NONE:
    default:
      break;
  }
  return result;
}

}  // namespace esphome::detail
