#include "string_ref.h"

namespace esphome {

#ifdef USE_JSON

// NOLINTNEXTLINE(readability-identifier-naming)
void convertToJson(const StringRef &src, JsonVariant dst) { dst.set(src.c_str()); }

#endif  // USE_JSON

}  // namespace esphome
