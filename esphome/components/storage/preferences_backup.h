#pragma once

#include "esphome/core/defines.h"
// Preferences backup/restore is an ESP32-only storage capability: it talks to
// the NVS namespace ESPHome preferences live in. Codegen defines
// USE_STORAGE_PREFERENCES when one of the actions is used.
#if defined(USE_STORAGE_PREFERENCES) && defined(USE_ESP32)

#include <cstddef>
#include <cstdint>

namespace esphome::storage {

// Value type of a selected preference, baked by codegen from the global's
// YAML `type:`. Blobs are the raw bytes of T (strings: length-prefixed
// char[SZ], see globals_component.h) — the tag is what makes them readable.
enum class PrefType : uint8_t {
  HEX = 0,  // unknown/unsupported type — hex round-trip fallback
  BOOL,
  I8,
  U8,
  I16,
  U16,
  I32,
  U32,
  F32,
  F64,
  STRING,  // length-prefixed char[count] (count == SZ)
};

struct PrefSelection {
  const char *name;
  uint32_t key;
  PrefType type;
  // scalar: 1; array T[N]: N; STRING: SZ (buffer size incl. length byte)
  uint16_t count;
};

// The selection table provides names and types. Codegen ALWAYS bakes it: from
// the action's `preferences:` list when given (restrict == true: only those
// entries round-trip), otherwise from every restore_value global in the
// config (restrict == false: the whole namespace round-trips, table entries
// render readable, unknown keys — entity states etc. — fall back to hex).
bool preferences_export_to_storage(const char *path, const char *format, const PrefSelection *selection,
                                   size_t count, bool restrict_to_selection);
bool preferences_import_from_storage(const char *path, const char *format, bool reboot,
                                     const PrefSelection *selection, size_t count, bool restrict_to_selection);

}  // namespace esphome::storage

#endif  // USE_STORAGE_PREFERENCES && USE_ESP32
