#include "preferences_backup.h"

#if defined(USE_STORAGE_PREFERENCES) && defined(USE_ESP32)

#include <cctype>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "storage.h"

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
      long long v = strtoll(buf, &end, 10);
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
static size_t collect_entries(nvs_handle_t handle, const PrefSelection *sel, size_t count, EmitFn &&emit) {
  size_t n = 0;
  NvsEntry e;
  if (count > 0) {
    for (size_t i = 0; i < count; i++) {
      if (nvs_read_entry(handle, sel[i].key, e)) {
        emit(e, &sel[i]);
        n++;
      } else {
        ESP_LOGW(TAG, "Preference '%s' has no stored value yet — skipped", sel[i].name);
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
    unsigned long key = strtoul(info.key, &end, 10);
    if (end != nullptr && *end == '\0' && nvs_read_entry(handle, static_cast<uint32_t>(key), e)) {
      emit(e, nullptr);
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

bool preferences_export_to_storage(const char *path, const char *format, const PrefSelection *sel, size_t count) {
  const bool as_json = strcmp(format, "json") == 0;
  if (!as_json && strcmp(format, "kv") != 0) {
    ESP_LOGE(TAG, "Unsupported format '%s'", format);
    return false;
  }
  const char *rel = nullptr;
  PathStorage *ps = resolve_file_target(path, &rel);
  if (ps == nullptr)
    return false;

  // Flush pending preference writes so NVS reflects the current state.
  global_preferences->sync();

  nvs_handle_t handle;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "nvs_open failed: %s", esp_err_to_name(err));
    return false;
  }

  std::string out;
  size_t exported = 0;
  if (as_json) {
    auto buf = json::build_json([&](JsonObject root) {
      root["version"] = 1;
      JsonObject prefs = root["preferences"].to<JsonObject>();
      exported = collect_entries(handle, sel, count, [&](const NvsEntry &e, const PrefSelection *s) {
        char key_str[16];
        snprintf(key_str, sizeof(key_str), "%" PRIu32, e.key);
        std::string value;
        if (s != nullptr) {
          encode_value(value, *s, e.blob, e.len);
        } else {
          value = HEX_PREFIX;
          append_hex(value, e.blob, e.len);
        }
        // Values as strings uniformly (incl. "hex:..."): keeps import
        // parsing symmetric and avoids float round-trip surprises.
        prefs[s != nullptr ? s->name : key_str] = value;
      });
    });
    out.assign(buf.begin(), buf.end());
  } else {
    out += "# ESPHome preferences export (kv v1)\n";
    out += "# <global id or numeric NVS key>=<typed value or hex:...>\n";
    exported = collect_entries(handle, sel, count, [&](const NvsEntry &e, const PrefSelection *s) {
      if (s != nullptr) {
        out += s->name;
        out += '=';
        encode_value(out, *s, e.blob, e.len);
      } else {
        char key_str[16];
        snprintf(key_str, sizeof(key_str), "%" PRIu32, e.key);
        out += key_str;
        out += '=';
        out += HEX_PREFIX;
        append_hex(out, e.blob, e.len);
      }
      out += '\n';
    });
  }
  nvs_close(handle);

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
                       const PrefSelection *sel, size_t count, size_t &imported, size_t &skipped) {
  const PrefSelection *s = find_by_name(name, name_len, sel, count);
  uint32_t key;
  if (s != nullptr) {
    key = s->key;
  } else {
    char buf[16];
    if (name_len == 0 || name_len >= sizeof(buf)) {
      skipped++;
      return true;
    }
    memcpy(buf, name, name_len);
    buf[name_len] = '\0';
    char *end = nullptr;
    unsigned long v = strtoul(buf, &end, 10);
    if (end == nullptr || *end != '\0' || (count > 0 && find_by_key(v, sel, count) == nullptr)) {
      skipped++;  // unknown name, or filtered out by the configured selection
      return true;
    }
    key = static_cast<uint32_t>(v);
    s = find_by_key(key, sel, count);
  }

  uint8_t blob[MAX_BLOB_LEN];
  size_t blob_len = 0;
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
                                     size_t count) {
  const bool as_json = strcmp(format, "json") == 0;
  if (!as_json && strcmp(format, "kv") != 0) {
    ESP_LOGE(TAG, "Unsupported format '%s'", format);
    return false;
  }
  const char *rel = nullptr;
  PathStorage *ps = resolve_file_target(path, &rel);
  if (ps == nullptr)
    return false;

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
    ok = json::parse_json(buf.data(), size, [&](JsonObject root) -> bool {
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
                        imported, skipped))
          return false;
      }
      return true;
    });
  } else {
    const char *data = reinterpret_cast<const char *>(buf.data());
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
      ok = import_one(handle, line, eq - line, eq + 1, line_len - (eq + 1 - line), sel, count, imported, skipped);
    }
  }
  if (ok && (err = nvs_commit(handle)) != ESP_OK) {
    ESP_LOGE(TAG, "nvs_commit failed: %s", esp_err_to_name(err));
    ok = false;
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
