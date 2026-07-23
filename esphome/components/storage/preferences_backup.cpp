#include "preferences_backup.h"

#if defined(USE_STORAGE_PREFERENCES) && defined(USE_ESP32)

#include <cctype>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "storage.h"

#include <esp_rom_crc.h>

#include <span>
#include <vector>

#include "esphome/core/string_ref.h"

#include "esphome/core/entity_base.h"
#include "esphome/core/helpers.h"

// Codecs compile against the REAL component restore structs — field access by
// name, layouts stay the compiler's problem, sizeof gates every decode.
#ifdef USE_SENSOR
// Only for App.get_sensors() — nothing is decoded from a sensor, see the sweep.
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_FAN
#include "esphome/components/fan/fan.h"
#endif
#ifdef USE_COVER
#include "esphome/components/cover/cover.h"
#endif
#ifdef USE_VALVE
#include "esphome/components/valve/valve.h"
#endif
#ifdef USE_LIGHT
#include "esphome/components/light/light_state.h"
#endif
#ifdef USE_CLIMATE
#include "esphome/components/climate/climate.h"
#endif
#ifdef USE_DATETIME_DATE
#include "esphome/components/datetime/date_entity.h"
#endif
#ifdef USE_DATETIME_TIME
#include "esphome/components/datetime/time_entity.h"
#endif
#ifdef USE_DATETIME_DATETIME
#include "esphome/components/datetime/datetime_entity.h"
#endif

#include "esphome/components/json/json_util.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "esphome/core/preferences.h"

#include <nvs.h>

namespace esphome::storage {

static const char *const TAG = "storage.preferences";

// The namespace every ESPHome preference lives in (see esp32/preferences.cpp).
static constexpr const char *NVS_NAMESPACE = "esphome";
static constexpr size_t MAX_BLOB_LEN = 4096;  // NVS blob hard limit is well below this
static constexpr const char *HEX_PREFIX = "hex:";

// SINGLE coupling point to the core's preference-key recipe.
// get_preference_hash() is deprecated in favor of make_entity_preference<T>()
// because upstream plans a v2 key migration (esphome/backlog#85) and carries a
// known collision problem (different names sanitizing to the same object_id).
// We need the raw key for NVS<->name mapping, so we keep exactly ONE silenced
// call here: when the core migrates, this one function follows the new recipe
// and every codec/sweep path inherits it.
static uint32_t entity_pref_key(esphome::EntityBase *e, uint32_t version) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  return e->get_preference_hash() ^ version;
#pragma GCC diagnostic pop
}

struct RuntimeEntry {
  uint32_t key;
  std::string name;  // owned: sweep names come from stack buffers
  EntityKind kind;
  uint16_t aux;                          // STRING: SZ incl. length byte
  esphome::EntityBase *entity{nullptr};  // for pointer-based selection
};

static std::vector<RuntimeEntry> &runtime_registry() {
  static std::vector<RuntimeEntry> reg;  // function-local: safe init order
  return reg;
}

#ifdef USE_TEXT
static void register_text_pref_impl(esphome::EntityBase *entity, const char *name, uint32_t min_len, uint32_t max_len,
                                    const char *pattern) {
  // template_text does NOT use make_entity_preference: its key adds trait
  // salts on top of the base hash — replicated 1:1 from
  // template/text/template_text.cpp (traits come from the live object via
  // codegen, so the salt inputs are always the real ones).
  uint32_t key = entity_pref_key(entity, 0);
  key += min_len << 2;
  key += max_len << 4;
  key += fnv1_hash(pattern) << 6;
  for (const auto &r : runtime_registry()) {
    if (r.key == key) {
      if (r.entity != entity) {
        ESP_LOGW(TAG, "Preference key collision: '%s' and '%s' share key %" PRIu32 " — only the first is exported",
                 r.name.c_str(), name, key);
      }
      return;
    }
  }
  runtime_registry().push_back({key, name, EntityKind::STRING, static_cast<uint16_t>(max_len + 1), entity});
}
#endif  // USE_TEXT

// Runtime safety net: codegen registration depends on the Python-side
// declaration parents of each entity class, which are not reliable across
// esphome versions (media_player_ns.class_("MediaPlayer") has none at all in
// some trees). This sweep walks the App entity lists ONCE, lazily, and
// registers anything the baked calls missed — naming then only depends on
// the running firmware itself.
static void register_if_missing(esphome::EntityBase *e, uint32_t version, EntityKind kind, uint16_t aux = 0) {
  const uint32_t key = entity_pref_key(e, version);
  for (const auto &r : runtime_registry()) {
    if (r.key == key) {
      if (r.entity != e) {
        char nbuf[OBJECT_ID_MAX_LEN];
        const StringRef nid = e->get_object_id_to(std::span<char, OBJECT_ID_MAX_LEN>(nbuf));
        ESP_LOGW(TAG, "Preference key collision: '%s' and '%s' share key %" PRIu32 " — only the first is exported",
                 r.name.c_str(), nid.c_str(), key);
      }
      return;
    }
  }
  char buf[OBJECT_ID_MAX_LEN];
  const StringRef oid = e->get_object_id_to(std::span<char, OBJECT_ID_MAX_LEN>(buf));
  runtime_registry().emplace_back(RuntimeEntry{key, std::string(oid.c_str(), oid.size()), kind, aux, e});
}

void register_entity_pref(esphome::EntityBase *entity, EntityKind kind) {
  if (entity != nullptr)
    register_if_missing(entity, 0, kind);
}

void register_key_pref(uint32_t key, const char *name, EntityKind kind) {
  for (const auto &r : runtime_registry()) {
    if (r.key == key)
      return;  // an entity already claimed it; entities win, they can be selected by id
  }
  // No entity pointer: an action's `preferences:` filter selects by object, so these are only
  // ever part of an unrestricted export -- which is what a component-owned key should be.
  runtime_registry().emplace_back(RuntimeEntry{key, std::string(name), kind, 0, nullptr});
}

static void sweep_app_entities() {
  static bool done = false;
  if (done)
    return;
  done = true;
#ifdef USE_SWITCH
  for (auto *e : App.get_switches())
    register_if_missing(e, 0, EntityKind::BOOL);
#endif
#ifdef USE_NUMBER
  for (auto *e : App.get_numbers())
    register_if_missing(e, 0, EntityKind::FLOAT);
#endif
#ifdef USE_SELECT
  for (auto *e : App.get_selects())
    register_if_missing(e, 0, EntityKind::SELECT_INDEX);
#endif
#ifdef USE_MEDIA_PLAYER
  for (auto *e : App.get_media_players())
    register_if_missing(e, 0, EntityKind::MEDIA_VOLUME);
#endif
#ifdef USE_SENSOR
  // Named, but not rendered: what a restoring sensor stores is its platform's business and the
  // platforms disagree -- integration and total_daily_energy keep a float, duty_time a uint32_t,
  // rotary_encoder an int32_t, all four bytes and indistinguishable at runtime (no RTTI). RAW
  // still turns "2817293318=hex:..." into "<the sensor's id>=hex:...", which is the difference
  // between a line nobody can place and one that names itself.
  for (auto *e : App.get_sensors())
    register_if_missing(e, 0, EntityKind::RAW);
#endif
#ifdef USE_FAN
  for (auto *e : App.get_fans())
    register_if_missing(e, 0x71700ABB, EntityKind::FAN);
#endif
#ifdef USE_CLIMATE
  for (auto *e : App.get_climates())
    register_if_missing(e, 0x848EA6AD, EntityKind::CLIMATE);
#endif
#ifdef USE_LIGHT
  for (auto *e : App.get_lights())
    register_if_missing(e, 0, EntityKind::LIGHT);
#endif
#ifdef USE_COVER
  for (auto *e : App.get_covers())
    register_if_missing(e, 0, EntityKind::COVER);
#endif
#ifdef USE_VALVE
  for (auto *e : App.get_valves())
    register_if_missing(e, 0, EntityKind::VALVE);
#endif
#ifdef USE_LOCK
  for (auto *e : App.get_locks())
    register_if_missing(e, 0, EntityKind::RAW);
#endif
#ifdef USE_DATETIME_DATE
  for (auto *e : App.get_dates())
    register_if_missing(e, 194434030, EntityKind::DATE);
#endif
#ifdef USE_DATETIME_TIME
  for (auto *e : App.get_times())
    register_if_missing(e, 194434060, EntityKind::TIME);
#endif
#ifdef USE_DATETIME_DATETIME
  for (auto *e : App.get_datetimes())
    register_if_missing(e, 194434090, EntityKind::DATETIME);
#endif
#ifdef USE_TEXT
  for (auto *e : App.get_texts()) {
    // trait-salted key — same recipe as the baked text registration; the
    // impl copies the name into its owned entry
    char buf[OBJECT_ID_MAX_LEN];
    const StringRef oid = e->get_object_id_to(std::span<char, OBJECT_ID_MAX_LEN>(buf));
    register_text_pref_impl(e, oid.c_str(), e->traits.get_min_length(), e->traits.get_max_length(),
                            e->traits.get_pattern_c_str());
  }
#endif
}

static const RuntimeEntry *runtime_by_key(uint32_t key, esphome::EntityBase *const *allowed, size_t allowed_count) {
  for (const auto &e : runtime_registry()) {
    if (e.key != key)
      continue;
    if (allowed == nullptr)
      return &e;
    for (size_t i = 0; i < allowed_count; i++) {
      if (e.entity == allowed[i])
        return &e;
    }
    return nullptr;  // known, but filtered out by the action's selection
  }
  return nullptr;
}

static const RuntimeEntry *runtime_by_name(const char *token, size_t len, esphome::EntityBase *const *allowed,
                                           size_t allowed_count) {
  for (const auto &e : runtime_registry()) {
    if (e.name.size() != len || memcmp(e.name.data(), token, len) != 0)
      continue;
    if (allowed == nullptr)
      return &e;
    for (size_t i = 0; i < allowed_count; i++) {
      if (e.entity == allowed[i])
        return &e;
    }
    return nullptr;
  }
  return nullptr;
}

static const PrefSelection *find_by_key(uint32_t key, const PrefSelection *sel, size_t count) {
  for (size_t i = 0; i < count; i++) {
    if (sel[i].key == key)
      return &sel[i];
  }
  return nullptr;
}

static const PrefSelection *find_by_name(const char *token, size_t len, const PrefSelection *sel, size_t count) {
  for (size_t i = 0; i < count; i++) {
    if (strlen(sel[i].name) == len && memcmp(sel[i].name, token, len) == 0)
      return &sel[i];
  }
  return nullptr;
}

static size_t scalar_size(PrefType t) {
  switch (t) {
    case PrefType::BOOL:
    case PrefType::I8:
    case PrefType::U8:
      return 1;
    case PrefType::I16:
    case PrefType::U16:
      return 2;
    case PrefType::I32:
    case PrefType::U32:
    case PrefType::F32:
      return 4;
    case PrefType::F64:
      return 8;
    default:
      return 0;
  }
}

static void append_hex(std::string &out, const uint8_t *data, size_t len) {
  static const char *const HEX = "0123456789abcdef";
  for (size_t i = 0; i < len; i++) {
    out += HEX[data[i] >> 4];
    out += HEX[data[i] & 0x0F];
  }
}

static bool parse_hex(const char *s, size_t len, uint8_t *out, size_t *out_len) {
  if (len % 2 != 0 || len / 2 > MAX_BLOB_LEN)
    return false;
  auto nib = [](char c) -> int {
    if (c >= '0' && c <= '9')
      return c - '0';
    c |= 0x20;
    if (c >= 'a' && c <= 'f')
      return c - 'a' + 10;
    return -1;
  };
  for (size_t i = 0; i < len; i += 2) {
    int hi = nib(s[i]), lo = nib(s[i + 1]);
    if (hi < 0 || lo < 0)
      return false;
    out[i / 2] = (hi << 4) | lo;
  }
  *out_len = len / 2;
  return true;
}

// ---- scalar element <-> text (little-endian raw bytes, ESP32 native) ----

static void encode_scalar(std::string &out, PrefType t, const uint8_t *p) {
  char buf[32];
  switch (t) {
    case PrefType::BOOL:
      out += (*p != 0) ? "true" : "false";
      return;
    case PrefType::I8: {
      int8_t v;
      memcpy(&v, p, 1);
      snprintf(buf, sizeof(buf), "%d", (int) v);
      break;
    }
    case PrefType::U8:
      snprintf(buf, sizeof(buf), "%u", (unsigned) *p);
      break;
    case PrefType::I16: {
      int16_t v;
      memcpy(&v, p, 2);
      snprintf(buf, sizeof(buf), "%d", (int) v);
      break;
    }
    case PrefType::U16: {
      uint16_t v;
      memcpy(&v, p, 2);
      snprintf(buf, sizeof(buf), "%u", (unsigned) v);
      break;
    }
    case PrefType::I32: {
      int32_t v;
      memcpy(&v, p, 4);
      snprintf(buf, sizeof(buf), "%" PRId32, v);
      break;
    }
    case PrefType::U32: {
      uint32_t v;
      memcpy(&v, p, 4);
      snprintf(buf, sizeof(buf), "%" PRIu32, v);
      break;
    }
    case PrefType::F32: {
      float v;
      memcpy(&v, p, 4);
      snprintf(buf, sizeof(buf), "%.9g", (double) v);
      break;
    }
    case PrefType::F64: {
      double v;
      memcpy(&v, p, 8);
      snprintf(buf, sizeof(buf), "%.17g", v);
      break;
    }
    default:
      return;
  }
  out += buf;
}

static bool decode_scalar(const char *s, size_t len, PrefType t, uint8_t *p) {
  char buf[48];
  if (len == 0 || len >= sizeof(buf))
    return false;
  memcpy(buf, s, len);
  buf[len] = '\0';
  char *end = nullptr;
  switch (t) {
    case PrefType::BOOL: {
      bool v;
      if (strcmp(buf, "true") == 0 || strcmp(buf, "1") == 0) {
        v = true;
      } else if (strcmp(buf, "false") == 0 || strcmp(buf, "0") == 0) {
        v = false;
      } else {
        return false;
      }
      uint8_t b = v ? 1 : 0;
      memcpy(p, &b, 1);
      return true;
    }
    case PrefType::F32: {
      float v = strtof(buf, &end);
      if (end == nullptr || *end != '\0')
        return false;
      memcpy(p, &v, 4);
      return true;
    }
    case PrefType::F64: {
      double v = strtod(buf, &end);
      if (end == nullptr || *end != '\0')
        return false;
      memcpy(p, &v, 8);
      return true;
    }
    default: {
      int64_t v = strtoll(buf, &end, 10);
      if (end == nullptr || *end != '\0')
        return false;
      int64_t iv = v;
      memcpy(p, &iv, scalar_size(t));  // little-endian truncation = native narrowing
      return true;
    }
  }
}

// ---- whole-value <-> text ----

// Renders a typed blob into `out`. Falls back to "hex:<...>" when the blob
// does not match the expected layout (stale entry from an older config).
static void encode_value(std::string &out, const PrefSelection &s, const uint8_t *blob, size_t len) {
  const size_t es = scalar_size(s.type);
  if (s.type == PrefType::STRING) {
    // length-prefixed char[SZ]: blob[0] = size, then the bytes
    if (len >= 1 && len <= MAX_BLOB_LEN && blob[0] < len) {
      const char *str = reinterpret_cast<const char *>(blob + 1);
      const size_t slen = blob[0];
      // newlines would break the line-based kv format — hex-fall back
      if (memchr(str, '\n', slen) == nullptr && memchr(str, '\r', slen) == nullptr) {
        out.append(str, slen);
        return;
      }
    }
  } else if (es != 0 && len == es * s.count) {
    for (uint16_t i = 0; i < s.count; i++) {
      if (i != 0)
        out += ',';
      encode_scalar(out, s.type, blob + i * es);
    }
    return;
  }
  out += HEX_PREFIX;
  append_hex(out, blob, len);
}

// Parses a typed value text back into a blob. Accepts the "hex:" fallback for
// every type.
static bool decode_value(const char *s, size_t len, const PrefSelection &sel, uint8_t *blob, size_t *blob_len) {
  if (len >= strlen(HEX_PREFIX) && memcmp(s, HEX_PREFIX, strlen(HEX_PREFIX)) == 0) {
    return parse_hex(s + strlen(HEX_PREFIX), len - strlen(HEX_PREFIX), blob, blob_len);
  }
  const size_t es = scalar_size(sel.type);
  if (sel.type == PrefType::STRING) {
    if (sel.count == 0 || len >= sel.count)  // SZ includes the length byte
      return false;
    blob[0] = static_cast<uint8_t>(len);
    memcpy(blob + 1, s, len);
    // globals compare/save the full SZ buffer; zero the tail for determinism
    memset(blob + 1 + len, 0, sel.count - 1 - len);
    *blob_len = sel.count;
    return true;
  }
  if (es == 0)
    return false;
  size_t idx = 0, start = 0;
  for (size_t i = 0; i <= len; i++) {
    if (i == len || s[i] == ',') {
      if (idx >= sel.count)
        return false;
      if (!decode_scalar(s + start, i - start, sel.type, blob + idx * es))
        return false;
      idx++;
      start = i + 1;
    }
  }
  if (idx != sel.count)
    return false;
  *blob_len = es * sel.count;
  return true;
}

// ---- entity value codecs (real component structs) ----

[[maybe_unused]] static void kv_field(std::string &out, const char *k, const char *v, bool &first) {
  if (!first)
    out += ',';
  first = false;
  out += k;
  out += ':';
  out += v;
}
[[maybe_unused]] static void kv_field_f(std::string &out, const char *k, float v, bool &first) {
  char b[24];
  snprintf(b, sizeof(b), "%.9g", (double) v);
  kv_field(out, k, b, first);
}
[[maybe_unused]] static void kv_field_i(std::string &out, const char *k, int32_t v, bool &first) {
  char b[16];
  snprintf(b, sizeof(b), "%" PRId32, v);
  kv_field(out, k, b, first);
}
[[maybe_unused]] static void kv_field_b(std::string &out, const char *k, bool v, bool &first) {
  kv_field(out, k, v ? "true" : "false", first);
}

// Tiny "{k:v,k:v}" reader shared by all struct decoders.
struct FieldReader {
  const char *p;
  const char *end;
  bool ok{true};
  bool get(const char *key, char *val, size_t val_size) {
    // fields may arrive in any order; scan from the start each time
    const char *s = this->p;
    size_t klen = strlen(key);
    while (s < this->end) {
      const char *colon = static_cast<const char *>(memchr(s, ':', this->end - s));
      if (colon == nullptr)
        break;
      const char *comma = static_cast<const char *>(memchr(colon, ',', this->end - colon));
      const char *vend = comma != nullptr ? comma : this->end;
      if (static_cast<size_t>(colon - s) == klen && memcmp(s, key, klen) == 0) {
        size_t vlen = vend - (colon + 1);
        if (vlen >= val_size)
          return false;
        memcpy(val, colon + 1, vlen);
        val[vlen] = '\0';
        return true;
      }
      s = comma != nullptr ? comma + 1 : this->end;
    }
    return false;
  }
  bool f(const char *key, float &out) {
    char b[32];
    if (!this->get(key, b, sizeof(b)))
      return false;
    char *e = nullptr;
    out = strtof(b, &e);
    return e != nullptr && *e == '\0';
  }
  bool i(const char *key, int32_t &out) {
    char b[24];
    if (!this->get(key, b, sizeof(b)))
      return false;
    char *e = nullptr;
    out = strtol(b, &e, 10);
    return e != nullptr && *e == '\0';
  }
  bool b(const char *key, bool &out) {
    char v[8];
    if (!this->get(key, v, sizeof(v)))
      return false;
    if (strcmp(v, "true") == 0 || strcmp(v, "1") == 0) {
      out = true;
      return true;
    }
    if (strcmp(v, "false") == 0 || strcmp(v, "0") == 0) {
      out = false;
      return true;
    }
    return false;
  }
};

// Renders a runtime entry's blob readable; returns false to hex-fall back
// (unknown kind, or blob size does not match the compiled struct — stale).
static bool encode_entity_value(std::string &out, const RuntimeEntry &re, const uint8_t *blob, size_t len) {
  switch (re.kind) {
    case EntityKind::BOOL:
      if (len != sizeof(bool))
        return false;
      out += (*blob != 0) ? "true" : "false";
      return true;
    case EntityKind::FLOAT: {
      if (len != sizeof(float))
        return false;
      float v;
      memcpy(&v, blob, sizeof(v));
      char b[24];
      snprintf(b, sizeof(b), "%.9g", (double) v);
      out += b;
      return true;
    }
    case EntityKind::U32: {
      if (len != sizeof(uint32_t))
        return false;
      uint32_t v;
      memcpy(&v, blob, sizeof(v));
      char b[16];
      snprintf(b, sizeof(b), "%" PRIu32, v);
      out += b;
      return true;
    }
    case EntityKind::I32: {
      if (len != sizeof(int32_t))
        return false;
      int32_t v;
      memcpy(&v, blob, sizeof(v));
      char b[16];
      snprintf(b, sizeof(b), "%" PRId32, v);
      out += b;
      return true;
    }
    case EntityKind::STRING: {
      // length-prefixed char[SZ]; same layout as string globals
      if (len < 1 || blob[0] >= len)
        return false;
      const char *s = reinterpret_cast<const char *>(blob + 1);
      if (memchr(s, '\n', blob[0]) != nullptr || memchr(s, '\r', blob[0]) != nullptr)
        return false;
      out.append(s, blob[0]);
      return true;
    }
#ifdef USE_FAN
    case EntityKind::FAN: {
      if (len != sizeof(fan::FanRestoreState))
        return false;
      fan::FanRestoreState st;
      memcpy(&st, blob, sizeof(st));
      bool first = true;
      out += '{';
      kv_field_b(out, "state", st.state, first);
      kv_field_i(out, "speed", st.speed, first);
      kv_field_b(out, "oscillating", st.oscillating, first);
      kv_field_i(out, "direction", static_cast<int32_t>(st.direction), first);
      kv_field_i(out, "preset_mode", st.preset_mode, first);
      out += '}';
      return true;
    }
#endif
#ifdef USE_COVER
    case EntityKind::COVER: {
      if (len != sizeof(cover::CoverRestoreState))
        return false;
      cover::CoverRestoreState st;
      memcpy(&st, blob, sizeof(st));
      bool first = true;
      out += '{';
      kv_field_f(out, "position", st.position, first);
      kv_field_f(out, "tilt", st.tilt, first);
      out += '}';
      return true;
    }
#endif
#ifdef USE_VALVE
    case EntityKind::VALVE: {
      if (len != sizeof(valve::ValveRestoreState))
        return false;
      valve::ValveRestoreState st;
      memcpy(&st, blob, sizeof(st));
      bool first = true;
      out += '{';
      kv_field_f(out, "position", st.position, first);
      out += '}';
      return true;
    }
#endif
#ifdef USE_LIGHT
    case EntityKind::LIGHT: {
      if (len != sizeof(light::LightStateRTCState))
        return false;
      light::LightStateRTCState st;
      memcpy(&st, blob, sizeof(st));
      bool first = true;
      out += '{';
      kv_field_b(out, "state", st.state, first);
      kv_field_f(out, "brightness", st.brightness, first);
      kv_field_f(out, "color_brightness", st.color_brightness, first);
      kv_field_f(out, "red", st.red, first);
      kv_field_f(out, "green", st.green, first);
      kv_field_f(out, "blue", st.blue, first);
      kv_field_f(out, "white", st.white, first);
      kv_field_f(out, "color_temp", st.color_temp, first);
      kv_field_f(out, "cold_white", st.cold_white, first);
      kv_field_f(out, "warm_white", st.warm_white, first);
      kv_field_i(out, "effect", st.effect, first);
      kv_field_i(out, "color_mode", static_cast<int32_t>(st.color_mode), first);
      out += '}';
      return true;
    }
#endif
#ifdef USE_CLIMATE
    case EntityKind::CLIMATE: {
      if (len != sizeof(climate::ClimateDeviceRestoreState))
        return false;
      climate::ClimateDeviceRestoreState st;
      memcpy(&st, blob, sizeof(st));
      bool first = true;
      out += '{';
      kv_field_i(out, "mode", static_cast<int32_t>(st.mode), first);
      kv_field_b(out, "uses_custom_fan_mode", st.uses_custom_fan_mode, first);
      kv_field_i(out, "fan_mode", st.uses_custom_fan_mode ? st.custom_fan_mode : static_cast<int32_t>(st.fan_mode),
                 first);
      kv_field_b(out, "uses_custom_preset", st.uses_custom_preset, first);
      kv_field_i(out, "preset", st.uses_custom_preset ? st.custom_preset : static_cast<int32_t>(st.preset), first);
      kv_field_i(out, "swing_mode", static_cast<int32_t>(st.swing_mode), first);
      // two-point control shares the union — export both words, they alias
      kv_field_f(out, "target_temperature_low", st.target_temperature_low, first);
      kv_field_f(out, "target_temperature_high", st.target_temperature_high, first);
      kv_field_f(out, "target_humidity", st.target_humidity, first);
      out += '}';
      return true;
    }
#endif
    case EntityKind::SELECT_INDEX: {
      // template select restores its option index as size_t
      if (len != sizeof(size_t))
        return false;
      size_t v;
      memcpy(&v, blob, sizeof(v));
      char b[16];
      snprintf(b, sizeof(b), "%zu", v);
      out += b;
      return true;
    }
    case EntityKind::MEDIA_VOLUME: {
      // {float volume; bool is_muted} — defined identically (and trivially)
      // in speaker/media_player/speaker_media_player.h and
      // speaker_source/speaker_source_media_player.h; both are platform
      // headers we cannot include generically, so this mirrors the layout
      // with the usual sizeof gate (mismatch -> hex fallback).
      struct MediaVolumeState {
        float volume;
        bool is_muted;
      } st;
      if (len != sizeof(st))
        return false;
      memcpy(&st, blob, sizeof(st));
      bool first = true;
      out += '{';
      kv_field_f(out, "volume", st.volume, first);
      kv_field_b(out, "is_muted", st.is_muted, first);
      out += '}';
      return true;
    }
#ifdef USE_DATETIME_DATE
    case EntityKind::DATE: {
      if (len != sizeof(datetime::DateEntityRestoreState))
        return false;
      datetime::DateEntityRestoreState st;
      memcpy(&st, blob, sizeof(st));
      bool first = true;
      out += '{';
      kv_field_i(out, "year", st.year, first);
      kv_field_i(out, "month", st.month, first);
      kv_field_i(out, "day", st.day, first);
      out += '}';
      return true;
    }
#endif
#ifdef USE_DATETIME_TIME
    case EntityKind::TIME: {
      if (len != sizeof(datetime::TimeEntityRestoreState))
        return false;
      datetime::TimeEntityRestoreState st;
      memcpy(&st, blob, sizeof(st));
      bool first = true;
      out += '{';
      kv_field_i(out, "hour", st.hour, first);
      kv_field_i(out, "minute", st.minute, first);
      kv_field_i(out, "second", st.second, first);
      out += '}';
      return true;
    }
#endif
#ifdef USE_DATETIME_DATETIME
    case EntityKind::DATETIME: {
      if (len != sizeof(datetime::DateTimeEntityRestoreState))
        return false;
      datetime::DateTimeEntityRestoreState st;
      memcpy(&st, blob, sizeof(st));
      bool first = true;
      out += '{';
      kv_field_i(out, "year", st.year, first);
      kv_field_i(out, "month", st.month, first);
      kv_field_i(out, "day", st.day, first);
      kv_field_i(out, "hour", st.hour, first);
      kv_field_i(out, "minute", st.minute, first);
      kv_field_i(out, "second", st.second, first);
      out += '}';
      return true;
    }
#endif
    default:
      return false;
  }
}

// Parses a readable entity value back into a blob; false = not parseable.
static bool decode_entity_value(const char *s, size_t len, const RuntimeEntry &re, uint8_t *blob, size_t *blob_len) {
  switch (re.kind) {
    case EntityKind::BOOL: {
      bool v;
      if (len == 4 && memcmp(s, "true", 4) == 0) {
        v = true;
      } else if (len == 5 && memcmp(s, "false", 5) == 0) {
        v = false;
      } else if (len == 1 && (s[0] == '0' || s[0] == '1')) {
        v = s[0] == '1';
      } else {
        return false;
      }
      memcpy(blob, &v, sizeof(v));
      *blob_len = sizeof(bool);
      return true;
    }
    case EntityKind::FLOAT: {
      char b[32];
      if (len == 0 || len >= sizeof(b))
        return false;
      memcpy(b, s, len);
      b[len] = '\0';
      char *e = nullptr;
      float v = strtof(b, &e);
      if (e == nullptr || *e != '\0')
        return false;
      memcpy(blob, &v, sizeof(v));
      *blob_len = sizeof(float);
      return true;
    }
    case EntityKind::SELECT_INDEX: {
      char b[16];
      if (len == 0 || len >= sizeof(b))
        return false;
      memcpy(b, s, len);
      b[len] = '\0';
      char *e = nullptr;
      size_t v = static_cast<size_t>(strtoul(b, &e, 10));
      if (e == nullptr || *e != '\0')
        return false;
      memcpy(blob, &v, sizeof(v));
      *blob_len = sizeof(size_t);
      return true;
    }
    case EntityKind::U32: {
      char b[16];
      if (len == 0 || len >= sizeof(b))
        return false;
      memcpy(b, s, len);
      b[len] = '\0';
      char *e = nullptr;
      uint32_t v = static_cast<uint32_t>(strtoul(b, &e, 10));
      if (e == nullptr || *e != '\0')
        return false;
      memcpy(blob, &v, sizeof(v));
      *blob_len = sizeof(v);
      return true;
    }
    case EntityKind::I32: {
      char b[16];
      if (len == 0 || len >= sizeof(b))
        return false;
      memcpy(b, s, len);
      b[len] = '\0';
      char *e = nullptr;
      int32_t v = static_cast<int32_t>(strtol(b, &e, 10));
      if (e == nullptr || *e != '\0')
        return false;
      memcpy(blob, &v, sizeof(v));
      *blob_len = sizeof(v);
      return true;
    }
    case EntityKind::STRING: {
      if (re.aux == 0 || len >= re.aux)
        return false;
      blob[0] = static_cast<uint8_t>(len);
      memcpy(blob + 1, s, len);
      memset(blob + 1 + len, 0, re.aux - 1 - len);
      *blob_len = re.aux;
      return true;
    }
    default:
      break;
  }
  // struct kinds: expect "{...}"
  if (len < 2 || s[0] != '{' || s[len - 1] != '}')
    return false;
  FieldReader r{s + 1, s + len - 1};
  int32_t li;
  switch (re.kind) {
#ifdef USE_FAN
    case EntityKind::FAN: {
      fan::FanRestoreState st{};
      if (!r.b("state", st.state) || !r.i("speed", li))
        return false;
      st.speed = static_cast<int>(li);
      if (!r.b("oscillating", st.oscillating) || !r.i("direction", li))
        return false;
      st.direction = static_cast<fan::FanDirection>(li);
      if (!r.i("preset_mode", li))
        return false;
      st.preset_mode = static_cast<uint8_t>(li);
      memcpy(blob, &st, sizeof(st));
      *blob_len = sizeof(st);
      return true;
    }
#endif
#ifdef USE_COVER
    case EntityKind::COVER: {
      cover::CoverRestoreState st{};
      // packed struct: fields cannot bind to float& — go through locals
      float pos, tilt;
      if (!r.f("position", pos) || !r.f("tilt", tilt))
        return false;
      st.position = pos;
      st.tilt = tilt;
      memcpy(blob, &st, sizeof(st));
      *blob_len = sizeof(st);
      return true;
    }
#endif
#ifdef USE_VALVE
    case EntityKind::VALVE: {
      valve::ValveRestoreState st{};
      float pos;  // packed struct — see cover above
      if (!r.f("position", pos))
        return false;
      st.position = pos;
      memcpy(blob, &st, sizeof(st));
      *blob_len = sizeof(st);
      return true;
    }
#endif
#ifdef USE_LIGHT
    case EntityKind::LIGHT: {
      light::LightStateRTCState st{};
      if (!r.b("state", st.state) || !r.f("brightness", st.brightness) ||
          !r.f("color_brightness", st.color_brightness) || !r.f("red", st.red) || !r.f("green", st.green) ||
          !r.f("blue", st.blue) || !r.f("white", st.white) || !r.f("color_temp", st.color_temp) ||
          !r.f("cold_white", st.cold_white) || !r.f("warm_white", st.warm_white) || !r.i("effect", li))
        return false;
      st.effect = static_cast<uint32_t>(li);
      if (!r.i("color_mode", li))
        return false;
      st.color_mode = static_cast<light::ColorMode>(li);
      memcpy(blob, &st, sizeof(st));
      *blob_len = sizeof(st);
      return true;
    }
#endif
#ifdef USE_CLIMATE
    case EntityKind::CLIMATE: {
      climate::ClimateDeviceRestoreState st{};
      if (!r.i("mode", li))
        return false;
      st.mode = static_cast<climate::ClimateMode>(li);
      if (!r.b("uses_custom_fan_mode", st.uses_custom_fan_mode) || !r.i("fan_mode", li))
        return false;
      if (st.uses_custom_fan_mode) {
        st.custom_fan_mode = static_cast<uint8_t>(li);
      } else {
        st.fan_mode = static_cast<climate::ClimateFanMode>(li);
      }
      if (!r.b("uses_custom_preset", st.uses_custom_preset) || !r.i("preset", li))
        return false;
      if (st.uses_custom_preset) {
        st.custom_preset = static_cast<uint8_t>(li);
      } else {
        st.preset = static_cast<climate::ClimatePreset>(li);
      }
      if (!r.i("swing_mode", li))
        return false;
      st.swing_mode = static_cast<climate::ClimateSwingMode>(li);
      if (!r.f("target_temperature_low", st.target_temperature_low) ||
          !r.f("target_temperature_high", st.target_temperature_high) || !r.f("target_humidity", st.target_humidity))
        return false;
      memcpy(blob, &st, sizeof(st));
      *blob_len = sizeof(st);
      return true;
    }
#endif
    case EntityKind::MEDIA_VOLUME: {
      struct MediaVolumeState {
        float volume;
        bool is_muted;
      } st{};
      if (!r.f("volume", st.volume) || !r.b("is_muted", st.is_muted))
        return false;
      memcpy(blob, &st, sizeof(st));
      *blob_len = sizeof(st);
      return true;
    }
#ifdef USE_DATETIME_DATE
    case EntityKind::DATE: {
      datetime::DateEntityRestoreState st{};
      if (!r.i("year", li))
        return false;
      st.year = static_cast<uint16_t>(li);
      if (!r.i("month", li))
        return false;
      st.month = static_cast<uint8_t>(li);
      if (!r.i("day", li))
        return false;
      st.day = static_cast<uint8_t>(li);
      memcpy(blob, &st, sizeof(st));
      *blob_len = sizeof(st);
      return true;
    }
#endif
#ifdef USE_DATETIME_TIME
    case EntityKind::TIME: {
      datetime::TimeEntityRestoreState st{};
      if (!r.i("hour", li))
        return false;
      st.hour = static_cast<uint8_t>(li);
      if (!r.i("minute", li))
        return false;
      st.minute = static_cast<uint8_t>(li);
      if (!r.i("second", li))
        return false;
      st.second = static_cast<uint8_t>(li);
      memcpy(blob, &st, sizeof(st));
      *blob_len = sizeof(st);
      return true;
    }
#endif
#ifdef USE_DATETIME_DATETIME
    case EntityKind::DATETIME: {
      datetime::DateTimeEntityRestoreState st{};
      if (!r.i("year", li))
        return false;
      st.year = static_cast<uint16_t>(li);
      if (!r.i("month", li))
        return false;
      st.month = static_cast<uint8_t>(li);
      if (!r.i("day", li))
        return false;
      st.day = static_cast<uint8_t>(li);
      if (!r.i("hour", li))
        return false;
      st.hour = static_cast<uint8_t>(li);
      if (!r.i("minute", li))
        return false;
      st.minute = static_cast<uint8_t>(li);
      if (!r.i("second", li))
        return false;
      st.second = static_cast<uint8_t>(li);
      memcpy(blob, &st, sizeof(st));
      *blob_len = sizeof(st);
      return true;
    }
#endif
    default:
      return false;
  }
}

// ---- shared NVS plumbing ----

struct NvsEntry {
  uint32_t key;
  uint8_t blob[MAX_BLOB_LEN];
  size_t len;
};

static bool nvs_read_entry(nvs_handle_t handle, uint32_t key, NvsEntry &e) {
  char key_str[16];
  snprintf(key_str, sizeof(key_str), "%" PRIu32, key);
  e.key = key;
  e.len = 0;
  size_t len = 0;
  if (nvs_get_blob(handle, key_str, nullptr, &len) != ESP_OK || len == 0 || len > MAX_BLOB_LEN)
    return false;
  if (nvs_get_blob(handle, key_str, e.blob, &len) != ESP_OK)
    return false;
  e.len = len;
  return true;
}

template<typename EmitFn>
static size_t collect_entries(nvs_handle_t handle, const PrefSelection *sel, size_t count, bool restrict_to_selection,
                              esphome::EntityBase *const *selected_entities, size_t selected_entity_count,
                              EmitFn &&emit) {
  size_t n = 0;
  NvsEntry e;
  if (restrict_to_selection) {
    for (size_t i = 0; i < count; i++) {
      if (nvs_read_entry(handle, sel[i].key, e)) {
        emit(e, &sel[i]);
        n++;
      } else {
        ESP_LOGW(TAG, "Preference '%s' has no stored value yet — skipped", sel[i].name);
      }
    }
    // Selected ENTITY preferences: resolve by object pointer via the sweep.
    for (size_t i = 0; i < selected_entity_count; i++) {
      const RuntimeEntry *re = nullptr;
      for (const auto &entry : runtime_registry()) {
        if (entry.entity == selected_entities[i]) {
          re = &entry;
          break;
        }
      }
      if (re == nullptr) {
        ESP_LOGW(TAG, "Selected entity #%zu stores no known preference — skipped", i);
        continue;
      }
      if (nvs_read_entry(handle, re->key, e)) {
        emit(e, nullptr);  // emit resolves the name via runtime_by_key
        n++;
      } else {
        ESP_LOGW(TAG, "Entity preference '%s' has no stored value yet — skipped", re->name.c_str());
      }
    }
    return n;
  }
  nvs_iterator_t it = nullptr;
  esp_err_t err = nvs_entry_find(NVS_DEFAULT_PART_NAME, NVS_NAMESPACE, NVS_TYPE_BLOB, &it);
  while (err == ESP_OK && it != nullptr) {
    nvs_entry_info_t info;
    nvs_entry_info(it, &info);
    char *end = nullptr;
    uint32_t key = static_cast<uint32_t>(strtoul(info.key, &end, 10));
    if (end != nullptr && *end == '\0' && nvs_read_entry(handle, static_cast<uint32_t>(key), e)) {
      // Unrestricted mode still knows names/types for everything codegen
      // could see (all restore_value globals) — render those readable.
      emit(e, find_by_key(static_cast<uint32_t>(key), sel, count));
      n++;
    }
    err = nvs_entry_next(&it);
  }
  nvs_release_iterator(it);
  return n;
}

static PathStorage *resolve_file_target(const char *path, const char **rel) {
  if (global_storage_registry == nullptr) {
    ESP_LOGE(TAG, "Storage registry not available");
    return nullptr;
  }
  PathStorage *ps = global_storage_registry->resolve_path(path, rel);
  if (ps == nullptr || *rel == nullptr || (*rel)[0] == '\0' || strcmp(*rel, "/") == 0) {
    ESP_LOGE(TAG, "'%s' is not a file path on a mounted storage", path);
    return nullptr;
  }
  return ps;
}

// ---- export ----

// ---------------------------------------------------------------------------
// Raw-device container
// ---------------------------------------------------------------------------
// Layout: header + entries, each entry {key u32, len u16, blob}. The blobs go in exactly as
// NVS holds them — component-private structs, no decoding, no rendering. The header is what
// lets import distinguish "an export starts here" from "whatever was here before".

constexpr uint32_t RAW_MAGIC = 0x57525045;  // 'EPRW'
constexpr uint8_t RAW_VERSION = 1;

struct RawHeader {
  uint32_t magic;
  uint8_t version;
  uint8_t reserved;
  uint16_t entries;
  uint32_t payload_len;
  uint32_t crc32;  // over the payload only
} __attribute__((packed));

static_assert(sizeof(RawHeader) == 16, "raw preferences header must stay 16 bytes");

static void append_u16(std::string &out, uint16_t v) {
  out.push_back(static_cast<char>(v & 0xFF));
  out.push_back(static_cast<char>(v >> 8));
}
static void append_u32(std::string &out, uint32_t v) {
  for (int i = 0; i < 4; i++)
    out.push_back(static_cast<char>((v >> (8 * i)) & 0xFF));
}

// Blocking chunked helpers — the RawStorage contract is "caller chunks and yields".
static bool raw_read_exact(RawStorage *device, uint64_t address, uint8_t *buf, size_t len) {
  size_t done = 0;
  while (done < len) {
    size_t got = 0;
    if (device->read(address + done, buf + done, len - done, &got) != StorageError::OK || got == 0)
      return false;
    done += got;
  }
  return true;
}

static bool raw_write_exact(RawStorage *device, uint64_t address, const uint8_t *buf, size_t len) {
  size_t done = 0;
  while (done < len) {
    size_t written = 0;
    if (device->write(address + done, buf + done, len - done, &written) != StorageError::OK || written == 0)
      return false;
    done += written;
    App.feed_wdt();
  }
  return true;
}

bool preferences_export_to_raw(RawStorage *device, uint64_t address, uint64_t window, const PrefSelection *sel,
                               size_t count, bool restrict_to_selection, esphome::EntityBase *const *selected_entities,
                               size_t selected_entity_count) {
  if (device == nullptr)
    return false;
  RawGeometry geo;
  device->get_raw_geometry(&geo);
  if (address >= geo.capacity) {
    ESP_LOGE(TAG, "Export address 0x%08" PRIX32 " is beyond the device (%" PRIu32 " bytes)", (uint32_t) address,
             (uint32_t) geo.capacity);
    return false;
  }

  sweep_app_entities();
  // Flush pending preference writes so NVS reflects the current state.
  global_preferences->sync();

  nvs_handle_t handle;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "nvs_open failed: %s", esp_err_to_name(err));
    return false;
  }

  // Budget checked while the payload grows, not after: the "nothing written" promise below is
  // about the medium, but the RAM is spent by then — and std::string has no way to report a
  // failed growth in an exceptions-free build, it aborts. Stop at the first entry that would
  // not fit and report it instead.
  const uint64_t budget = window > sizeof(RawHeader) ? window - sizeof(RawHeader) : 0;
  bool over_budget = false;
  std::string payload;
  size_t exported = collect_entries(handle, sel, count, restrict_to_selection, selected_entities, selected_entity_count,
                                    [&](const NvsEntry &e, const PrefSelection *s) {
                                      if (over_budget)
                                        return;
                                      if (payload.size() + 6 + e.len > budget) {
                                        over_budget = true;
                                        return;
                                      }
                                      append_u32(payload, e.key);
                                      append_u16(payload, static_cast<uint16_t>(e.len));
                                      payload.append(reinterpret_cast<const char *>(e.blob), e.len);
                                    });
  nvs_close(handle);
  if (over_budget) {
    ESP_LOGE(TAG, "Export does not fit the %" PRIu32 " bytes reserved at 0x%08" PRIX32 " — nothing written",
             (uint32_t) window, (uint32_t) address);
    return false;
  }

  RawHeader hdr{};
  hdr.magic = RAW_MAGIC;
  hdr.version = RAW_VERSION;
  hdr.entries = static_cast<uint16_t>(exported);
  hdr.payload_len = static_cast<uint32_t>(payload.size());
  hdr.crc32 = esp_rom_crc32_le(0, reinterpret_cast<const uint8_t *>(payload.data()), payload.size());

  const uint64_t total = sizeof(RawHeader) + payload.size();

  // Media that only clear bits on write need the covering sectors erased first. Rounding the
  // erase outward would take the region in front of us with it, so demand alignment instead of
  // guessing — and never erase past the window into the neighbouring region.
  if ((geo.caps & RAW_WRITE_NEEDS_ERASE) != 0) {
    if (geo.erase_sector == 0 || (address % geo.erase_sector) != 0) {
      ESP_LOGE(TAG, "Export address 0x%08" PRIX32 " must be aligned to this device's %" PRIu32 " byte sector size",
               (uint32_t) address, (uint32_t) geo.erase_sector);
      return false;
    }
    uint64_t erase_len = total;
    if ((erase_len % geo.erase_sector) != 0)
      erase_len += geo.erase_sector - (erase_len % geo.erase_sector);
    if (erase_len > window) {
      ESP_LOGE(TAG, "Erasing %" PRIu32 " bytes would reach past the %" PRIu32 " reserved here — nothing written",
               (uint32_t) erase_len, (uint32_t) window);
      return false;
    }
    StorageError eerr = device->erase(address, static_cast<size_t>(erase_len));
    if (eerr != StorageError::OK) {
      ESP_LOGE(TAG, "Erase before export failed (%s)", error_to_string(eerr));
      return false;
    }
  }

  if (!raw_write_exact(device, address, reinterpret_cast<const uint8_t *>(&hdr), sizeof(hdr)) ||
      !raw_write_exact(device, address + sizeof(hdr), reinterpret_cast<const uint8_t *>(payload.data()),
                       payload.size())) {
    ESP_LOGE(TAG, "Writing export to the device failed");
    return false;
  }
  ESP_LOGI(TAG, "Exported %zu preference(s), %" PRIu32 " bytes to 0x%08" PRIX32, exported, (uint32_t) total,
           (uint32_t) address);
  return true;
}

bool preferences_import_from_raw(RawStorage *device, uint64_t address, uint64_t window, bool reboot,
                                 const PrefSelection *sel, size_t count, bool restrict_to_selection,
                                 esphome::EntityBase *const *selected_entities, size_t selected_entity_count) {
  if (device == nullptr)
    return false;
  RawGeometry geo;
  device->get_raw_geometry(&geo);
  if (address + sizeof(RawHeader) > geo.capacity) {
    ESP_LOGE(TAG, "Import address 0x%08" PRIX32 " is beyond the device", (uint32_t) address);
    return false;
  }

  RawHeader hdr{};
  if (!raw_read_exact(device, address, reinterpret_cast<uint8_t *>(&hdr), sizeof(hdr))) {
    ESP_LOGE(TAG, "Reading the export header failed");
    return false;
  }
  if (hdr.magic != RAW_MAGIC) {
    ESP_LOGE(TAG, "No preferences export at 0x%08" PRIX32 " (magic 0x%08" PRIX32 ")", (uint32_t) address, hdr.magic);
    return false;
  }
  if (hdr.version != RAW_VERSION) {
    ESP_LOGE(TAG, "Export at 0x%08" PRIX32 " has version %u, this build reads version %u", (uint32_t) address,
             hdr.version, RAW_VERSION);
    return false;
  }
  if (sizeof(RawHeader) + static_cast<uint64_t>(hdr.payload_len) > window) {
    ESP_LOGE(TAG, "Export claims %" PRIu32 " bytes, more than the %" PRIu32 " reserved here — refusing",
             hdr.payload_len, (uint32_t) window);
    return false;
  }

  uint8_t *raw = RAMAllocator<uint8_t>().allocate(hdr.payload_len);
  if (raw == nullptr) {
    ESP_LOGE(TAG, "Cannot allocate %" PRIu32 " bytes for the import", hdr.payload_len);
    return false;
  }
  RamBuffer payload(raw, RamBufferDeleter{hdr.payload_len});
  if (!raw_read_exact(device, address + sizeof(RawHeader), payload.get(), hdr.payload_len)) {
    ESP_LOGE(TAG, "Reading the export payload failed");
    return false;
  }
  uint32_t crc = esp_rom_crc32_le(0, payload.get(), hdr.payload_len);
  if (crc != hdr.crc32) {
    ESP_LOGE(TAG, "Export at 0x%08" PRIX32 " is corrupt (CRC 0x%08" PRIX32 ", expected 0x%08" PRIX32 ")",
             (uint32_t) address, crc, hdr.crc32);
    return false;
  }

  sweep_app_entities();
  nvs_handle_t handle;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "nvs_open failed: %s", esp_err_to_name(err));
    return false;
  }

  size_t imported = 0, skipped = 0;
  bool ok = true;
  uint32_t pos = 0;
  for (uint16_t i = 0; i < hdr.entries && ok; i++) {
    if (pos + 6 > hdr.payload_len) {
      ESP_LOGE(TAG, "Export is truncated after %zu entries", imported + skipped);
      ok = false;
      break;
    }
    uint32_t key = payload[pos] | (payload[pos + 1] << 8) | (payload[pos + 2] << 16) | (payload[pos + 3] << 24);
    uint16_t len = payload[pos + 4] | (payload[pos + 5] << 8);
    pos += 6;
    if (pos + len > hdr.payload_len) {
      ESP_LOGE(TAG, "Export entry for key %" PRIu32 " runs past the payload", key);
      ok = false;
      break;
    }
    const uint8_t *blob = payload.get() + pos;
    pos += len;

    // Same filter rule as the path variants: an explicit selection restricts what round-trips,
    // an empty one lets the whole namespace through.
    if (restrict_to_selection && find_by_key(key, sel, count) == nullptr &&
        runtime_by_key(key, selected_entities, selected_entity_count) == nullptr) {
      skipped++;
      continue;
    }
    char key_str[16];
    snprintf(key_str, sizeof(key_str), "%" PRIu32, key);
    err = nvs_set_blob(handle, key_str, blob, len);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "nvs_set_blob('%s') failed: %s", key_str, esp_err_to_name(err));
      ok = false;
      break;
    }
    imported++;
  }
  if (ok) {
    err = nvs_commit(handle);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "nvs_commit failed: %s", esp_err_to_name(err));
      ok = false;
    }
  }
  nvs_close(handle);
  if (!ok)
    return false;

  ESP_LOGI(TAG, "Imported %zu preference(s), skipped %zu — values take effect after reboot", imported, skipped);
  if (reboot) {
    ESP_LOGI(TAG, "Rebooting to apply imported preferences");
    App.safe_reboot();
  }
  return true;
}

bool preferences_export_to_storage(const char *path, const char *format, const PrefSelection *sel, size_t count,
                                   bool restrict_to_selection, esphome::EntityBase *const *selected_entities,
                                   size_t selected_entity_count) {
  const bool as_json = strcmp(format, "json") == 0;
  if (!as_json && strcmp(format, "kv") != 0) {
    ESP_LOGE(TAG, "Unsupported format '%s'", format);
    return false;
  }
  const char *rel = nullptr;
  PathStorage *ps = resolve_file_target(path, &rel);
  if (ps == nullptr)
    return false;

  sweep_app_entities();
  // Flush pending preference writes so NVS reflects the current state.
  global_preferences->sync();

  nvs_handle_t handle;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "nvs_open failed: %s", esp_err_to_name(err));
    return false;
  }

  // Same reasoning as the raw export: max_blocking_transfer_size is what write_file() below
  // would reject the result by, but by then the whole rendering already sits in RAM, and a
  // std::string that cannot grow aborts rather than reporting it. Stop rendering once the
  // limit is reached. 0 means the check is off, per the option's contract.
  const uint64_t budget =
      global_storage_registry != nullptr ? global_storage_registry->get_max_blocking_transfer_size() : 0;
  bool over_budget = false;
  size_t json_bytes = 0;  // running estimate for the json branch, which cannot measure itself
  std::string out;
  size_t exported = 0;
  if (as_json) {
    auto buf = json::build_json([&](JsonObject root) {
      root["version"] = 1;
      JsonObject prefs = root["preferences"].to<JsonObject>();
      exported = collect_entries(
          handle, sel, count, restrict_to_selection, selected_entities, selected_entity_count,
          [&](const NvsEntry &e, const PrefSelection *s) {
            if (over_budget)
              return;
            char key_str[16];
            snprintf(key_str, sizeof(key_str), "%" PRIu32, e.key);
            std::string value;
            const RuntimeEntry *re =
                s == nullptr
                    ? runtime_by_key(e.key, restrict_to_selection ? selected_entities : nullptr, selected_entity_count)
                    : nullptr;
            if (s != nullptr) {
              encode_value(value, *s, e.blob, e.len);
            } else if (re == nullptr || !encode_entity_value(value, *re, e.blob, e.len)) {
              value = HEX_PREFIX;
              append_hex(value, e.blob, e.len);
            }
            const char *name = s != nullptr ? s->name : (re != nullptr ? re->name.c_str() : key_str);
            // The document is ArduinoJson's, so there is no size to read
            // back mid-build — track what this entry will render as
            // instead: "name":"value", quotes, colon and separator.
            json_bytes += strlen(name) + value.size() + 6;
            if (budget != 0 && json_bytes > budget) {
              over_budget = true;
              return;
            }
            prefs[name] = value;
          });
    });
    out.assign(buf.data(), buf.size());
  } else {
    out += "# ESPHome preferences export (kv v1)\n";
    out += "# <global id or numeric NVS key>=<typed value or hex:...>\n";
    exported = collect_entries(handle, sel, count, restrict_to_selection, selected_entities, selected_entity_count,
                               [&](const NvsEntry &e, const PrefSelection *s) {
                                 if (over_budget)
                                   return;
                                 if (budget != 0 && out.size() >= budget) {
                                   over_budget = true;
                                   return;
                                 }
                                 if (s != nullptr) {
                                   out += s->name;
                                   out += '=';
                                   encode_value(out, *s, e.blob, e.len);
                                 } else {
                                   const RuntimeEntry *re =
                                       runtime_by_key(e.key, restrict_to_selection ? selected_entities : nullptr,
                                                      selected_entity_count);
                                   if (re != nullptr) {
                                     out += re->name;
                                   } else {
                                     char key_str[16];
                                     snprintf(key_str, sizeof(key_str), "%" PRIu32, e.key);
                                     out += key_str;
                                   }
                                   out += '=';
                                   if (re == nullptr || !encode_entity_value(out, *re, e.blob, e.len)) {
                                     out += HEX_PREFIX;
                                     append_hex(out, e.blob, e.len);
                                   }
                                 }
                                 out += '\n';
                               });
  }
  nvs_close(handle);
  if (over_budget) {
    ESP_LOGE(TAG,
             "Export exceeds max_blocking_transfer_size (%" PRIu32 " bytes) — nothing written. Narrow it with the "
             "action's 'preferences:' filter, or raise the limit.",
             (uint32_t) budget);
    return false;
  }

  StorageError werr = write_file(ps, rel, reinterpret_cast<const uint8_t *>(out.data()), out.size());
  if (werr != StorageError::OK) {
    ESP_LOGE(TAG, "Writing export failed (%s)", error_to_string(werr));
    return false;
  }
  ESP_LOGI(TAG, "Exported %zu preference(s), %zu bytes to '%s'", exported, out.size(), path);
  return true;
}

// ---- import ----

// Writes one parsed name/value pair to NVS; shared by both formats.
static bool import_one(nvs_handle_t handle, const char *name, size_t name_len, const char *value, size_t value_len,
                       const PrefSelection *sel, size_t count, bool restrict_to_selection,
                       esphome::EntityBase *const *selected_entities, size_t selected_entity_count, size_t &imported,
                       size_t &skipped) {
  // One buffer for both paths below. They never overlap in time, but MAX_BLOB_LEN is 4 kB and
  // whether the compiler folds two of them into one stack slot is not something to rely on --
  // this runs on the main loop, whose stack is 8 kB by default.
  uint8_t blob[MAX_BLOB_LEN];
  size_t blob_len = 0;
  const PrefSelection *s = find_by_name(name, name_len, sel, count);
  uint32_t key;
  if (s != nullptr) {
    key = s->key;
  } else if (const RuntimeEntry *re = runtime_by_name(
                 name, name_len, restrict_to_selection ? selected_entities : nullptr, selected_entity_count)) {
    key = re->key;
    // typed parse first; hex: prefix (and stale-format hex) still accepted below
    if (value_len < strlen(HEX_PREFIX) || memcmp(value, HEX_PREFIX, strlen(HEX_PREFIX)) != 0) {
      if (decode_entity_value(value, value_len, *re, blob, &blob_len)) {
        char key_str[16];
        snprintf(key_str, sizeof(key_str), "%" PRIu32, key);
        esp_err_t err = nvs_set_blob(handle, key_str, blob, blob_len);
        if (err != ESP_OK) {
          ESP_LOGE(TAG, "nvs_set_blob('%s') failed: %s", key_str, esp_err_to_name(err));
          return false;
        }
        imported++;
        return true;
      }
    }
  } else {
    char buf[16];
    if (name_len == 0 || name_len >= sizeof(buf)) {
      skipped++;
      return true;
    }
    memcpy(buf, name, name_len);
    buf[name_len] = '\0';
    char *end = nullptr;
    uint32_t v = static_cast<uint32_t>(strtoul(buf, &end, 10));
    if (end == nullptr || *end != '\0' || (restrict_to_selection && find_by_key(v, sel, count) == nullptr)) {
      skipped++;  // unknown name, or filtered out by the configured selection
      return true;
    }
    key = static_cast<uint32_t>(v);
    s = find_by_key(key, sel, count);
  }

  blob_len = 0;
  bool ok;
  if (s != nullptr) {
    ok = decode_value(value, value_len, *s, blob, &blob_len);
  } else {
    // untyped numeric-key entry: hex only (with or without prefix)
    const char *hex = value;
    size_t hex_len = value_len;
    if (hex_len >= strlen(HEX_PREFIX) && memcmp(hex, HEX_PREFIX, strlen(HEX_PREFIX)) == 0) {
      hex += strlen(HEX_PREFIX);
      hex_len -= strlen(HEX_PREFIX);
    }
    ok = parse_hex(hex, hex_len, blob, &blob_len);
  }
  if (!ok) {
    ESP_LOGW(TAG, "Skipping malformed value for '%.*s'", (int) name_len, name);
    skipped++;
    return true;
  }

  char key_str[16];
  snprintf(key_str, sizeof(key_str), "%" PRIu32, key);
  esp_err_t err = nvs_set_blob(handle, key_str, blob, blob_len);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "nvs_set_blob('%s') failed: %s", key_str, esp_err_to_name(err));
    return false;
  }
  imported++;
  return true;
}

bool preferences_import_from_storage(const char *path, const char *format, bool reboot, const PrefSelection *sel,
                                     size_t count, bool restrict_to_selection,
                                     esphome::EntityBase *const *selected_entities, size_t selected_entity_count) {
  const bool as_json = strcmp(format, "json") == 0;
  if (!as_json && strcmp(format, "kv") != 0) {
    ESP_LOGE(TAG, "Unsupported format '%s'", format);
    return false;
  }
  const char *rel = nullptr;
  PathStorage *ps = resolve_file_target(path, &rel);
  if (ps == nullptr)
    return false;

  sweep_app_entities();

  RamBuffer buf;
  size_t size = 0;
  StorageError rerr = read_file(ps, rel, buf, &size);
  if (rerr != StorageError::OK) {
    ESP_LOGE(TAG, "Reading '%s' failed (%s)", path, error_to_string(rerr));
    return false;
  }

  nvs_handle_t handle;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "nvs_open failed: %s", esp_err_to_name(err));
    return false;
  }

  size_t imported = 0, skipped = 0;
  bool ok = true;
  if (as_json) {
    ok = json::parse_json(buf.get(), size, [&](JsonObject root) -> bool {
      JsonObject prefs = root["preferences"];
      if (prefs.isNull()) {
        ESP_LOGE(TAG, "JSON import: missing 'preferences' object");
        return false;
      }
      for (JsonPair kv : prefs) {
        const char *value = kv.value().as<const char *>();
        if (value == nullptr) {
          ESP_LOGW(TAG, "Skipping non-string value for '%s'", kv.key().c_str());
          skipped++;
          continue;
        }
        if (!import_one(handle, kv.key().c_str(), strlen(kv.key().c_str()), value, strlen(value), sel, count,
                        restrict_to_selection, selected_entities, selected_entity_count, imported, skipped))
          return false;
      }
      return true;
    });
  } else {
    const char *data = reinterpret_cast<const char *>(buf.get());
    size_t pos = 0;
    while (ok && pos < size) {
      size_t eol = pos;
      while (eol < size && data[eol] != '\n')
        eol++;
      size_t line_len = eol - pos;
      while (line_len > 0 && (data[pos + line_len - 1] == '\r' || data[pos + line_len - 1] == ' '))
        line_len--;
      const char *line = data + pos;
      pos = eol + 1;
      if (line_len == 0 || line[0] == '#')
        continue;
      const char *eq = static_cast<const char *>(memchr(line, '=', line_len));
      if (eq == nullptr) {
        ESP_LOGW(TAG, "Skipping malformed line (no '=')");
        skipped++;
        continue;
      }
      ok = import_one(handle, line, eq - line, eq + 1, line_len - (eq + 1 - line), sel, count, restrict_to_selection,
                      selected_entities, selected_entity_count, imported, skipped);
    }
  }
  if (ok) {
    err = nvs_commit(handle);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "nvs_commit failed: %s", esp_err_to_name(err));
      ok = false;
    }
  }
  nvs_close(handle);

  if (!ok)
    return false;
  ESP_LOGI(TAG, "Imported %zu preference(s), skipped %zu — values take effect after reboot", imported, skipped);
  if (reboot) {
    ESP_LOGI(TAG, "Rebooting to apply imported preferences");
    App.safe_reboot();
  }
  return true;
}

}  // namespace esphome::storage

#endif  // USE_STORAGE_PREFERENCES && USE_ESP32
