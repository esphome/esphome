#pragma once

#include "esphome/core/defines.h"
// Preferences backup/restore is an ESP32-only storage capability: it talks to
// the NVS namespace ESPHome preferences live in. Codegen defines
// USE_STORAGE_PREFERENCES when one of the actions is used.
#if defined(USE_STORAGE_PREFERENCES) && defined(USE_ESP32)

#include <cstddef>
#include <cstdint>

namespace esphome {
class EntityBase;  // fwd -- full definition pulled in by the .cpp only
}  // namespace esphome

namespace esphome::storage {

class RawStorage;  // fwd -- only ever used through the pointers below

// Value type of a selected preference, baked by codegen from the global's
// YAML `type:`. Blobs are the raw bytes of T (strings: length-prefixed
// char[SZ], see globals_component.h) -- the tag is what makes them readable.
enum class PrefType : uint8_t {
  HEX_FALLBACK = 0,  // unknown/unsupported type -- hex round-trip fallback
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
// render readable, unknown keys -- entity states etc. -- fall back to hex).
bool preferences_export_to_storage(const char *path, const char *format, const PrefSelection *selection, size_t count,
                                   bool restrict_to_selection, esphome::EntityBase *const *selected_entities,
                                   size_t selected_entity_count);
bool preferences_import_from_storage(const char *path, const char *format, bool reboot, const PrefSelection *selection,
                                     size_t count, bool restrict_to_selection,
                                     esphome::EntityBase *const *selected_entities, size_t selected_entity_count);

// Raw-device backup. Same selection semantics as the path variants above (empty list = the
// whole namespace), but the payload is the encoded blob exactly as NVS stores it: a raw medium
// is not read by a human, so there is nothing to render and no format to choose.
//
// Only the address is configured -- keeping regions apart is the user's business -- so the blob
// carries a header (magic, version, length, CRC32) and import refuses anything that does not
// check out instead of feeding leftovers into the preferences.
//
// `window` is the space codegen reserved for this action: from its address up to the next
// action's address on the same device, or the end of the device. An export that would not fit
// aborts without writing a single byte rather than trampling the neighbouring region -- the one
// case config-time size validation cannot catch, because an unrestricted selection grows with
// the app.
bool preferences_export_to_raw(RawStorage *device, uint64_t address, uint64_t window, const PrefSelection *selection,
                               size_t count, bool restrict_to_selection, esphome::EntityBase *const *selected_entities,
                               size_t selected_entity_count);
bool preferences_import_from_raw(RawStorage *device, uint64_t address, uint64_t window, bool reboot,
                                 const PrefSelection *selection, size_t count, bool restrict_to_selection,
                                 esphome::EntityBase *const *selected_entities, size_t selected_entity_count);

// ---- runtime entity name registry (baked registration calls) ----
// Codegen enumerates entity IDs in its coroutine and emits one registration
// call per restoring entity into setup(); the KEY comes from the entity
// object itself at runtime (get_preference_hash() ^ per-type version), so no
// hash recipe is replicated at codegen time. Values stay hex (component-
// private restore structs); this registry provides the NAMES.
// What the entity's restore blob IS -- codegen picks this from the declared
// class; the codecs in preferences_backup.cpp are compiled against the REAL
// component structs (sizeof-gated, hex fallback on mismatch), so no layout
// knowledge is duplicated here.
enum class EntityKind : uint8_t {
  RAW = 0,  // unknown restore layout: named, hex value
  BOOL,     // switch & friends
  FLOAT,    // number, integration, sprinkler, ...
  STRING,   // text (length-prefixed; aux = SZ incl. length byte)
  FAN,
  COVER,
  VALVE,
  LIGHT,
  CLIMATE,
  SELECT_INDEX,  // template select restores a size_t option index
  MEDIA_VOLUME,  // media players restore {float volume; bool is_muted}
  DATE,
  TIME,
  DATETIME,
  U32,  // sensors whose restore value is a uint32_t (duty_time)
  I32,  // sensors whose restore value is an int32_t (rotary_encoder)
};

// Codegen -> runtime bridge for entities the sweep cannot type by itself.
//
// The sweep works off App's entity lists, which say what an entity IS but not what it stores.
// That is enough everywhere the list already implies the layout (a cover stores
// CoverRestoreState), and not enough for sensors: integration and total_daily_energy keep a
// float, duty_time a uint32_t, rotary_encoder an int32_t -- four bytes each, indistinguishable
// at runtime in a build without RTTI. Codegen does know, because the platform is right there in
// the YAML, so it emits one of these per sensor it recognises. Registrations land before the
// sweep runs, and the sweep leaves a key that is already known alone -- so an unrecognised
// platform still falls back to the sweep's RAW entry, named but hex.
void register_entity_pref(esphome::EntityBase *entity, EntityKind kind);

// Same bridge for preferences that belong to no entity at all: safe_mode's boot counter and
// friends live under a constant of the owning component's own choosing, so the sweep -- which
// only ever walks entities -- cannot name them and they export as a bare number.
//
// Codegen emits the call with the owning component's SYMBOL, not its value, and only when that
// component is configured. The constant therefore stays where it belongs: rename or move it and
// the build says so, rather than this component quietly exporting a stale number.
void register_key_pref(uint32_t key, const char *name, EntityKind kind);

// Entity naming/typing is resolved ENTIRELY at runtime by the sweep in
// preferences_backup.cpp (App entity lists + per-type version constants);
// codegen contributes only the globals table and, for listed entity IDs,
// the object pointers handed to the actions.

}  // namespace esphome::storage

#endif  // USE_STORAGE_PREFERENCES && USE_ESP32
