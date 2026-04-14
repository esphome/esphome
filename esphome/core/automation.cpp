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
  // Use NRVO-friendly constructors so libstdc++'s existing _M_construct helpers
  // (already present in most firmwares) are reused, avoiding net growth from
  // pulling in a distinct assign/_M_replace_cold set of out-of-line symbols.
  switch (type) {
    case TSS_VALUE:
      return *reinterpret_cast<const std::string *>(storage);
    case TSS_STATIC_STRING:
      return std::string(storage);
#ifdef USE_ESP8266
    case TSS_FLASH_STRING: {
      size_t len = strlen_P(storage);
      std::string result(len, '\0');
      memcpy_P(&result[0], storage, len);
      return result;
    }
#endif
    case TSS_NONE:
    default:
      return {};
  }
}

}  // namespace esphome::detail
