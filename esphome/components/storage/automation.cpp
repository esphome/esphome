#include "automation.h"

#ifdef USE_STORAGE_JSON_EXTRACT
// ArduinoJson is only compiled in when a config actually uses a `json:` extraction
// step (codegen sets the define; the schema requires the json component).
#include "esphome/components/json/json_util.h"
#endif

#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include <cinttypes>
#include <cstring>

#ifdef USE_STORAGE_REGEX_EXTRACT
// <regex> costs significant flash (~50-100 kB) and stack; it is only compiled in when a
// config actually uses a `regex:` extraction step (codegen sets the define).
#include <regex>
#endif

namespace esphome::storage {

static const char *const TAG = "storage.automation";

void warn_invalid_bool(const std::string &s) {
  ESP_LOGW(TAG, "file_read: '%s' is not a valid bool; global unchanged", s.c_str());
}
void warn_invalid_number(const std::string &s) {
  ESP_LOGW(TAG, "file_read: '%s' is not a valid number; global unchanged", s.c_str());
}

std::string extract_trim(const std::string &s) {
  size_t b = s.find_first_not_of(" \t\r\n");
  if (b == std::string::npos)
    return "";
  size_t e = s.find_last_not_of(" \t\r\n");
  return s.substr(b, e - b + 1);
}

bool apply_extract_step(const ExtractStep &step, std::string &buf) {
  switch (step.type) {
    case ExtractStepType::LINE: {
      int current = 1;
      size_t start = 0;
      while (start <= buf.size()) {
        size_t end = buf.find('\n', start);
        if (end == std::string::npos)
          end = buf.size();
        if (current == step.index) {
          std::string line = buf.substr(start, end - start);
          if (!line.empty() && line.back() == '\r')
            line.pop_back();
          buf = std::move(line);
          return true;
        }
        current++;
        start = end + 1;
      }
      ESP_LOGW(TAG, "file_read: line %d not found (%d lines)", step.index, current - 1);
      return false;
    }
    case ExtractStepType::SPLIT: {
      int current = 0;
      size_t start = 0;
      while (true) {
        size_t end = buf.find(step.arg, start);
        if (current == step.index) {
          buf = buf.substr(start, (end == std::string::npos ? buf.size() : end) - start);
          return true;
        }
        if (end == std::string::npos)
          break;
        current++;
        start = end + step.arg.size();
      }
      ESP_LOGW(TAG, "file_read: split element %d not found", step.index);
      return false;
    }
    case ExtractStepType::KEY: {
      const std::string needle = step.arg + step.sep;
      size_t start = 0;
      while (start <= buf.size()) {
        size_t end = buf.find('\n', start);
        if (end == std::string::npos)
          end = buf.size();
        std::string trimmed = extract_trim(buf.substr(start, end - start));
        if (trimmed.starts_with(needle)) {
          buf = trimmed.substr(needle.size());
          return true;
        }
        start = end + 1;
      }
      ESP_LOGW(TAG, "file_read: key '%s' not found", step.arg.c_str());
      return false;
    }
    case ExtractStepType::TRIM:
      buf = extract_trim(buf);
      return true;
    case ExtractStepType::JSON: {
#ifdef USE_STORAGE_JSON_EXTRACT
      // '/'-separated pointer: object keys and array indices ("a/b/0").
      // Scalars yield their string form; objects/arrays yield serialized
      // JSON so further steps (or nested json steps) can keep working on it.
      JsonDocument doc = json::parse_json(reinterpret_cast<const uint8_t *>(buf.data()), buf.size());
      JsonVariantConst node = doc.as<JsonVariantConst>();
      if (node.isNull()) {
        ESP_LOGW(TAG, "extract json: invalid JSON document");
        return false;
      }
      const std::string &ptr = step.arg;
      size_t start = 0;
      while (start <= ptr.size() && !ptr.empty()) {
        size_t sep = ptr.find('/', start);
        std::string token = ptr.substr(start, sep == std::string::npos ? std::string::npos : sep - start);
        if (!token.empty()) {
          if (node.is<JsonArrayConst>()) {
            char *end = nullptr;
            uint64_t idx = strtoul(token.c_str(), &end, 10);
            if (end == nullptr || *end != '\0') {
              ESP_LOGW(TAG, "extract json: '%s' is not an array index", token.c_str());
              return false;
            }
            node = node.as<JsonArrayConst>()[idx];
          } else {
            node = node[token.c_str()];
          }
          if (node.isNull()) {
            ESP_LOGW(TAG, "extract json: path element '%s' not found", token.c_str());
            return false;
          }
        }
        if (sep == std::string::npos)
          break;
        start = sep + 1;
      }
      if (node.is<const char *>()) {
        buf = node.as<const char *>();  // unquoted string scalar
      } else {
        std::string serialized;
        if (serializeJson(node, serialized) == 0) {
          ESP_LOGW(TAG, "extract json: serialization produced no output");
          return false;
        }
        buf = std::move(serialized);
      }
      return true;
#else
      ESP_LOGE(TAG, "file_read: json step configured but not compiled in");
      return false;  // step cannot be configured without the define -- defensive
#endif  // USE_STORAGE_JSON_EXTRACT
    }
    case ExtractStepType::REGEX: {
#ifdef USE_STORAGE_REGEX_EXTRACT
      // Pattern was compiled once at step construction (see the ExtractStep constructor); this
      // path never recompiles per play().
      std::smatch m;
      if (!std::regex_search(buf, m, step.compiled_re)) {
        ESP_LOGW(TAG, "file_read: regex '%s' did not match", step.arg.c_str());
        return false;
      }
      int group = step.index;
      if (group >= static_cast<int>(m.size())) {
        ESP_LOGW(TAG, "file_read: regex group %d does not exist (%zu groups)", group, m.size() - 1);
        return false;
      }
      buf = m[group].str();
      return true;
#else
      ESP_LOGE(TAG, "file_read: regex step configured but not compiled in");
      return false;
#endif
    }
  }
  return false;
}

// True while the background worker task may be doing I/O on this storage concurrently. The
// synchronous paths below run blocking helpers on the main loop; doing that against a medium a
// task transfer is streaming would put two threads into one volume -- the corruption class the
// worker's cross-engine serialization exists to prevent -- so they refuse instead. Loop-sliced
// worker jobs, PENDING jobs and idle-open streams do not count (same thread / no I/O in flight).
static bool worker_task_busy(const Storage *s) {
#ifdef USE_STORAGE_WORKER
  return global_storage_worker != nullptr && global_storage_worker->has_active_task_io(s);
#else
  (void) s;
  return false;
#endif
}

StorageError perform_file_write(const std::string &path, std::string content, bool append, bool newline) {
  const char *op = append ? "append" : "write";
  if (newline)
    content += '\n';

  if (global_storage_registry == nullptr) {
    ESP_LOGE(TAG, "file_%s: no storage registry", op);
    return StorageError::STORAGE_ERROR_NOT_READY;
  }
  const char *rel = nullptr;
  PathStorage *ps = global_storage_registry->resolve_path(path.c_str(), &rel);
  if (ps == nullptr) {
    ESP_LOGE(TAG, "file_%s: no storage mounted for '%s'", op, path.c_str());
    return StorageError::STORAGE_ERROR_NOT_FOUND;
  }
  if (worker_task_busy(ps)) {
    ESP_LOGE(TAG, "file_%s: '%s' is busy with a background transfer -- refusing blocking I/O", op, path.c_str());
    return StorageError::STORAGE_ERROR_NOT_READY;
  }

  StorageError err;
  if (!append) {
    // PathStorage-level helper -- works on FILESYSTEM and NETWORK storages alike.
    err = write_file(ps, rel, reinterpret_cast<const uint8_t *>(content.data()), content.size());
  } else {
    // Same, via the interface's append helper (filesystem: native APPEND open; network:
    // stat-for-size + offset write_chunk). The blocking-size limit now covers append too.
    err = append_file(ps, rel, reinterpret_cast<const uint8_t *>(content.data()), content.size());
  }

  if (err != StorageError::STORAGE_ERROR_OK) {
    ESP_LOGE(TAG, "file_%s: writing '%s' failed (%s)", op, path.c_str(), error_to_string(err));
  }
  return err;
}

bool perform_file_read(const std::string &path, const FixedVector<ExtractStep> &steps, std::string &out,
                       std::string &error) {
  if (global_storage_registry == nullptr) {
    ESP_LOGE(TAG, "file_read: no storage registry");
    error = "no storage registry";
    return false;
  }
  const char *rel = nullptr;
  PathStorage *ps = global_storage_registry->resolve_path(path.c_str(), &rel);
  if (ps == nullptr) {
    ESP_LOGE(TAG, "file_read: no storage mounted for '%s'", path.c_str());
    error = "no storage mounted for '" + path + "'";
    return false;
  }
  if (worker_task_busy(ps)) {
    ESP_LOGE(TAG, "file_read: '%s' is busy with a background transfer -- refusing blocking I/O", path.c_str());
    error = "storage is busy with a background transfer";
    return false;
  }

  RamBuffer buf;
  size_t size = 0;
  // PathStorage-level helper -- works on FILESYSTEM and NETWORK storages alike.
  StorageError err = read_file(ps, rel, buf, &size);
  if (err != StorageError::STORAGE_ERROR_OK) {
    // Error path leaves any configured global untouched and does not fire on_value.
    ESP_LOGE(TAG, "file_read: reading '%s' failed (%s)", path.c_str(), error_to_string(err));
    error = error_to_string(err);
    return false;
  }

  // read_file() hands back a null buffer for a zero-byte file; assign(nullptr, 0) is UB the
  // standard does not permit (trips _GLIBCXX_ASSERTIONS / UBSan), so guard the empty case.
  if (size > 0) {
    out.assign(reinterpret_cast<const char *>(buf.get()), size);
  } else {
    out.clear();
  }
  // The content now lives in `out`; free the full-file buffer before the extraction loop so peak heap
  // stays ~2x the file (the copy plus a step's working string) rather than 3x. Matters on ESP8266/LibreTiny.
  buf.reset();
  // 1-based index so a failing step is nameable to the user; not a StorageError (an extract miss
  // is a content outcome, not a medium fault), so it rides the on_error text channel instead.
  size_t step_index = 0;
  for (const auto &step : steps) {
    step_index++;
    if (!apply_extract_step(step, out)) {
      // step already logged; global untouched, on_value not fired -- report which one.
      char msg[48];
      snprintf(msg, sizeof(msg), "extract step %d did not match", static_cast<int>(step_index));
      error = msg;
      return false;
    }
  }
  return true;
}

#ifdef USE_STORAGE_RAW_ACTIONS

// Every raw action asks the device what it is before touching it -- capacity and geometry come
// from the driver (see RawGeometry), never from an assumption about the medium.
static StorageError raw_preflight(RawStorage *device, const char *op, uint64_t address, uint64_t size,
                                  RawGeometry *geo) {
  device->get_raw_geometry(geo);
  if (geo->capacity == 0) {
    ESP_LOGE(TAG, "raw_%s: device reports no capacity", op);
    return StorageError::STORAGE_ERROR_NOT_READY;  // no medium yet, not a bad argument
  }
  if (address >= geo->capacity || size > geo->capacity - address) {
    ESP_LOGE(TAG, "raw_%s: 0x%08" PRIX32 " + %" PRIu32 " exceeds the device capacity %" PRIu32, op, (uint32_t) address,
             (uint32_t) size, (uint32_t) geo->capacity);
    return StorageError::STORAGE_ERROR_INVALID_ARGS;
  }
  return StorageError::STORAGE_ERROR_OK;
}

// Same guard rail the blocking file helpers use: these actions run on the main loop.
static StorageError raw_size_allowed(const char *op, uint64_t size) {
  if (global_storage_registry == nullptr) {
    ESP_LOGE(TAG, "raw_%s: no storage registry", op);
    return StorageError::STORAGE_ERROR_NOT_READY;  // not "unlimited" -- the guard is simply unavailable
  }
  uint64_t limit = global_storage_registry->get_max_blocking_transfer_size();
  if (limit != 0 && size > limit) {
    ESP_LOGE(TAG, "raw_%s: %" PRIu32 " bytes exceeds max_blocking_transfer_size (%" PRIu32 ")", op, (uint32_t) size,
             (uint32_t) limit);
    return StorageError::STORAGE_ERROR_TRANSFER_TOO_LARGE;
  }
  return StorageError::STORAGE_ERROR_OK;
}

// Erases the sector range covering [address, address+len) -- expanding to sector bounds, which
// is what makes this destructive to neighbours and therefore opt-in.
static StorageError raw_erase_for_write(RawStorage *device, const RawGeometry &geo, uint64_t address, size_t len) {
  if (geo.erase_sector == 0) {
    ESP_LOGE(TAG, "raw_write: erase_first requested but this device has no erase");
    return StorageError::STORAGE_ERROR_NOT_SUPPORTED;
  }
  uint64_t start = address - (address % geo.erase_sector);
  uint64_t end = address + len;
  if ((end % geo.erase_sector) != 0)
    end += geo.erase_sector - (end % geo.erase_sector);
  // Rounding up can leave the device behind when its capacity is not a whole number of erase
  // sectors. Asking a driver to erase past its own end is not something to find out about from
  // whatever it happens to return -- say so here, the way the preferences export does when its
  // rounded erase would leave the region it was given.
  if (end > geo.capacity) {
    ESP_LOGE(TAG,
             "raw_write: erase_first would have to erase up to %" PRIu32 " to cover this write, past the device's "
             "%" PRIu32 " bytes -- this device's last sector is partial",
             (uint32_t) end, (uint32_t) geo.capacity);
    return StorageError::STORAGE_ERROR_INVALID_ARGS;
  }
  ESP_LOGD(TAG, "raw_write: erasing 0x%08" PRIX32 " + %" PRIu32 " before writing", (uint32_t) start,
           (uint32_t) (end - start));
  StorageError err = device->erase(start, static_cast<size_t>(end - start));
  if (err != StorageError::STORAGE_ERROR_OK) {
    ESP_LOGE(TAG, "raw_write: erase failed (%s)", error_to_string(err));
    return err;
  }
  return StorageError::STORAGE_ERROR_OK;
}

// Reads the range into an already-sized buffer, honoring the partial-read contract.
static StorageError raw_read_into(RawStorage *device, uint64_t address, uint8_t *buf, size_t size, size_t *done_out) {
  size_t done = 0;
  while (done < size) {
    size_t got = 0;
    StorageError err = device->read(address + done, buf + done, size - done, &got);
    if (err != StorageError::STORAGE_ERROR_OK) {
      ESP_LOGE(TAG, "raw_read: failed at 0x%08" PRIX32 " (%s)", (uint32_t) (address + done), error_to_string(err));
      return err;  // propagate the driver's verdict instead of a generic READ_ERROR
    }
    if (got == 0)
      break;  // end of medium before the requested size
    done += got;
  }
  *done_out = done;
  if (done < size) {
    // A sized (whole-object) read: a short count means the medium ended before `size`, so the
    // read the caller expects cannot be fulfilled. Report it rather than handing back a
    // truncated buffer that looks complete (the primitive read()'s short-is-EOF contract is for
    // streaming callers, not this one).
    ESP_LOGE(TAG, "raw_read: short read at 0x%08" PRIX32 " (%u of %u bytes)", (uint32_t) address, (unsigned) done,
             (unsigned) size);
    return StorageError::STORAGE_ERROR_READ_ERROR;  // the medium ended before the requested size
  }
  return StorageError::STORAGE_ERROR_OK;
}

StorageError perform_raw_read(RawStorage *device, uint64_t address, size_t size, std::vector<uint8_t> &out) {
  if (size == 0) {
    ESP_LOGE(TAG, "raw_read: refusing a zero-length request");
    return StorageError::STORAGE_ERROR_INVALID_ARGS;  // a raw read of 0 bytes is meaningless (unlike an empty file)
  }
  RawGeometry geo;
  StorageError pf = raw_preflight(device, "read", address, size, &geo);
  if (pf != StorageError::STORAGE_ERROR_OK)
    return pf;
  StorageError sz = raw_size_allowed("read", size);
  if (sz != StorageError::STORAGE_ERROR_OK)
    return sz;
  if (worker_task_busy(device)) {
    ESP_LOGE(TAG, "raw_read: device is busy with a background transfer -- refusing blocking I/O");
    return StorageError::STORAGE_ERROR_NOT_READY;
  }
  // std::vector::resize() cannot report a failed allocation in an exceptions-free build -- it
  // aborts. Ask the nothrow allocator first (null on failure) and hand the block straight back:
  // nothing else allocates between here and the resize below, so it reuses the same memory. Probe
  // the INTERNAL heap, since resize() allocates through operator new (internal) -- a PSRAM-first
  // probe could succeed where the resize then aborts. read_file() sidesteps this with a RAMAllocator
  // buffer; this path must end with a std::vector because that is what the trigger takes.
  if (size > out.capacity()) {
    RAMAllocator<uint8_t> allocator(RAMAllocator<uint8_t>::ALLOC_INTERNAL);
    uint8_t *probe = allocator.allocate(size);
    if (probe == nullptr) {
      ESP_LOGE(TAG, "raw_read: cannot allocate %" PRIu32 " bytes", (uint32_t) size);
      return StorageError::STORAGE_ERROR_NO_SPACE;
    }
    allocator.deallocate(probe, size);
  }
  out.resize(size);
  size_t done = 0;
  StorageError rerr = raw_read_into(device, address, out.data(), size, &done);
  if (rerr != StorageError::STORAGE_ERROR_OK) {
    out.clear();
    return rerr;
  }
  out.resize(done);
  return StorageError::STORAGE_ERROR_OK;
}

StorageError perform_raw_read_to_file(RawStorage *device, uint64_t address, uint64_t size, const std::string &path) {
  if (worker_task_busy(device)) {
    ESP_LOGE(TAG, "raw_read: device is busy with a background transfer -- refusing blocking I/O");
    return StorageError::STORAGE_ERROR_NOT_READY;
  }
  RawGeometry geo;
  device->get_raw_geometry(&geo);
  if (size == 0)  // "to the end of the device"
    size = geo.capacity > address ? geo.capacity - address : 0;
  ESP_LOGI(TAG, "Transfer started: read 0x%08" PRIX32 " + %" PRIu32 " -> '%s'", (uint32_t) address, (uint32_t) size,
           path.c_str());
  StorageError pf = raw_preflight(device, "read", address, size, &geo);
  if (pf != StorageError::STORAGE_ERROR_OK)
    return pf;
  StorageError sz = raw_size_allowed("read", size);
  if (sz != StorageError::STORAGE_ERROR_OK)
    return sz;
  if (global_storage_registry == nullptr) {
    ESP_LOGE(TAG, "raw_read: no storage registry");
    return StorageError::STORAGE_ERROR_NOT_READY;
  }
  const char *rel = nullptr;
  PathStorage *ps = global_storage_registry->resolve_path(path.c_str(), &rel);
  if (ps == nullptr) {
    ESP_LOGE(TAG, "raw_read: no storage mounted for '%s'", path.c_str());
    return StorageError::STORAGE_ERROR_NOT_FOUND;
  }
  if (worker_task_busy(ps)) {
    ESP_LOGE(TAG, "raw_read: '%s' is busy with a background transfer -- refusing blocking I/O", path.c_str());
    return StorageError::STORAGE_ERROR_NOT_READY;
  }

  if constexpr (sizeof(size_t) < sizeof(uint64_t)) {
    if (size > static_cast<uint64_t>(SIZE_MAX)) {
      ESP_LOGE(TAG, "raw_read: requested size exceeds this target's address space");
      return StorageError::STORAGE_ERROR_NO_SPACE;
    }
  }
  auto buf_size = static_cast<size_t>(size);
  uint8_t *raw = RAMAllocator<uint8_t>().allocate(buf_size);
  if (raw == nullptr) {
    ESP_LOGE(TAG, "raw_read: cannot allocate %" PRIu32 " bytes", (uint32_t) buf_size);
    return StorageError::STORAGE_ERROR_NO_SPACE;
  }
  RamBuffer buf(raw, RamBufferDeleter{buf_size});
  size_t done = 0;
  StorageError rerr = raw_read_into(device, address, buf.get(), buf_size, &done);
  if (rerr != StorageError::STORAGE_ERROR_OK)
    return rerr;

  StorageError err = write_file(ps, rel, buf.get(), done);
  if (err != StorageError::STORAGE_ERROR_OK) {
    ESP_LOGE(TAG, "raw_read: writing '%s' failed (%s)", path.c_str(), error_to_string(err));
    return err;
  }
  ESP_LOGI(TAG, "Transfer done: read 0x%08" PRIX32 " + %" PRIu32 " -> '%s'", (uint32_t) address, (uint32_t) done,
           path.c_str());
  return StorageError::STORAGE_ERROR_OK;
}

StorageError perform_raw_write(RawStorage *device, uint64_t address, const uint8_t *data, size_t len,
                               bool erase_first) {
  if (len == 0) {
    ESP_LOGE(TAG, "raw_write: refusing a zero-length request");
    return StorageError::STORAGE_ERROR_INVALID_ARGS;  // a raw write of 0 bytes is meaningless (unlike an empty file)
  }
  RawGeometry geo;
  StorageError pf = raw_preflight(device, "write", address, len, &geo);
  if (pf != StorageError::STORAGE_ERROR_OK)
    return pf;
  StorageError sz = raw_size_allowed("write", len);
  if (sz != StorageError::STORAGE_ERROR_OK)
    return sz;
  if (worker_task_busy(device)) {
    ESP_LOGE(TAG, "raw_write: device is busy with a background transfer -- refusing blocking I/O");
    return StorageError::STORAGE_ERROR_NOT_READY;
  }
  if (erase_first) {
    StorageError eerr = raw_erase_for_write(device, geo, address, len);
    if (eerr != StorageError::STORAGE_ERROR_OK)
      return eerr;  // NOT_SUPPORTED / INVALID_ARGS / the driver's erase error, not a blanket WRITE_ERROR
  }

  size_t done = 0;
  while (done < len) {
    size_t written = 0;
    StorageError err = device->write(address + done, data + done, len - done, &written);
    if (err != StorageError::STORAGE_ERROR_OK) {
      ESP_LOGE(TAG, "raw_write: failed at 0x%08" PRIX32 " (%s)", (uint32_t) (address + done), error_to_string(err));
      return err;
    }
    if (written == 0) {
      ESP_LOGE(TAG, "raw_write: device stopped accepting data at 0x%08" PRIX32, (uint32_t) (address + done));
      return StorageError::STORAGE_ERROR_WRITE_ERROR;
    }
    done += written;
  }
  ESP_LOGD(TAG, "raw_write: %" PRIu32 " bytes at 0x%08" PRIX32, (uint32_t) len, (uint32_t) address);
  return StorageError::STORAGE_ERROR_OK;
}

StorageError perform_raw_write_from_file(RawStorage *device, uint64_t address, const std::string &path,
                                         bool erase_first) {
  if (worker_task_busy(device)) {
    ESP_LOGE(TAG, "raw_write: device is busy with a background transfer -- refusing blocking I/O");
    return StorageError::STORAGE_ERROR_NOT_READY;
  }
  ESP_LOGI(TAG, "Transfer started: write '%s' -> 0x%08" PRIX32, path.c_str(), (uint32_t) address);
  if (global_storage_registry == nullptr) {
    ESP_LOGE(TAG, "raw_write: no storage registry");
    return StorageError::STORAGE_ERROR_NOT_READY;
  }
  const char *rel = nullptr;
  PathStorage *ps = global_storage_registry->resolve_path(path.c_str(), &rel);
  if (ps == nullptr) {
    ESP_LOGE(TAG, "raw_write: no storage mounted for '%s'", path.c_str());
    return StorageError::STORAGE_ERROR_NOT_FOUND;
  }
  if (worker_task_busy(ps)) {
    ESP_LOGE(TAG, "raw_write: '%s' is busy with a background transfer -- refusing blocking I/O", path.c_str());
    return StorageError::STORAGE_ERROR_NOT_READY;
  }
  RamBuffer buf;
  size_t size = 0;
  StorageError err = read_file(ps, rel, buf, &size);
  if (err != StorageError::STORAGE_ERROR_OK) {
    ESP_LOGE(TAG, "raw_write: reading '%s' failed (%s)", path.c_str(), error_to_string(err));
    return err;
  }
  StorageError werr = perform_raw_write(device, address, buf.get(), size, erase_first);
  if (werr == StorageError::STORAGE_ERROR_OK) {
    ESP_LOGI(TAG, "Transfer done: write '%s' (%" PRIu32 " bytes) -> 0x%08" PRIX32, path.c_str(), (uint32_t) size,
             (uint32_t) address);
  }
  return werr;
}

StorageError perform_raw_erase(RawStorage *device, uint64_t address, uint64_t size, bool all) {
  RawGeometry geo;
  device->get_raw_geometry(&geo);
  if (all) {
    address = 0;
    size = geo.capacity;
  }
  if (size == 0) {
    ESP_LOGE(TAG, "raw_erase: refusing a zero-length request");
    return StorageError::STORAGE_ERROR_INVALID_ARGS;  // a raw erase of 0 bytes is meaningless (unlike an empty file)
  }
  StorageError pf = raw_preflight(device, "erase", address, size, &geo);
  if (pf != StorageError::STORAGE_ERROR_OK)
    return pf;
  if (worker_task_busy(device)) {
    ESP_LOGE(TAG, "raw_erase: device is busy with a background transfer -- refusing blocking I/O");
    return StorageError::STORAGE_ERROR_NOT_READY;
  }
  // No alignment massaging here: erase() rejects an unaligned range on purpose (it would take
  // the neighbouring data with it), and silently rounding would defeat that.
  StorageError err = device->erase(address, static_cast<size_t>(size));
  if (err != StorageError::STORAGE_ERROR_OK) {
    ESP_LOGE(TAG, "raw_erase: 0x%08" PRIX32 " + %" PRIu32 " failed (%s)", (uint32_t) address, (uint32_t) size,
             error_to_string(err));
    return err;
  }
  ESP_LOGD(TAG, "raw_erase: 0x%08" PRIX32 " + %" PRIu32 " done", (uint32_t) address, (uint32_t) size);
  return StorageError::STORAGE_ERROR_OK;
}

// --- async raw helpers: submit to the worker, stream, fire on_complete (error text) ---------

// Shared completion glue for the async raw actions: log on failure, fire the trigger once with
// the error text (empty = success).
static void raw_fire(Trigger<std::string> *on_complete, const char *op, StorageError result) {
  if (result != StorageError::STORAGE_ERROR_OK) {
    ESP_LOGE(TAG, "raw_%s failed (%s)", op, error_to_string(result));
  } else {
    ESP_LOGI(TAG, "Transfer done: raw %s", op);
  }
  if (on_complete != nullptr)
    on_complete->trigger(result == StorageError::STORAGE_ERROR_OK ? std::string() : std::string(error_to_string(result)));
}
// Report a pre-submission failure and fire the trigger once so it always fires exactly once.
static void raw_fail(Trigger<std::string> *on_complete, const char *op, const std::string &msg) {
  ESP_LOGE(TAG, "raw_%s: %s", op, msg.c_str());
  if (on_complete != nullptr)
    on_complete->trigger(msg);
}

void perform_raw_read_to_file_async(RawStorage *device, uint64_t address, uint64_t size, const std::string &path,
                                    Trigger<std::string> *on_complete) {
#ifdef USE_STORAGE_WORKER
  if (global_storage_worker != nullptr) {
    RawGeometry geo;
    if (size == 0) {  // "to the end of the device"
      device->get_raw_geometry(&geo);
      size = geo.capacity > address ? geo.capacity - address : 0;
    }
    // The sync twin (perform_raw_read_to_file) runs raw_preflight() here; the worker path skipped it
    // and submitted an out-of-range address as a zero-length "success". Reject it the same way.
    StorageError pf = raw_preflight(device, "read", address, size, &geo);
    if (pf != StorageError::STORAGE_ERROR_OK) {
      raw_fail(on_complete, "read", error_to_string(pf));
      return;
    }
    if (global_storage_registry == nullptr) {
      raw_fail(on_complete, "read", "no storage registry");
      return;
    }
    const char *rel = nullptr;
    PathStorage *ps = global_storage_registry->resolve_path(path.c_str(), &rel);
    if (ps == nullptr) {
      raw_fail(on_complete, "read", std::string("no storage mounted for '") + path + "'");
      return;
    }
    ESP_LOGI(TAG, "Transfer started: read 0x%08" PRIX32 " + %" PRIu32 " -> '%s'", (uint32_t) address, (uint32_t) size,
             path.c_str());
    StorageError err = global_storage_worker->async_raw_read(
        device, address, size, ps, rel, [on_complete](StorageError r) { raw_fire(on_complete, "read", r); }, nullptr,
        /*overwrite=*/true);
    if (err != StorageError::STORAGE_ERROR_OK)
      raw_fail(on_complete, "read", std::string("could not queue (") + error_to_string(err) + ")");
    return;
  }
#endif
  StorageError ferr = perform_raw_read_to_file(device, address, size, path);
  if (on_complete != nullptr)
    on_complete->trigger(ferr == StorageError::STORAGE_ERROR_OK ? std::string() : std::string(error_to_string(ferr)));
}

void perform_raw_write_from_file_async(RawStorage *device, uint64_t address, const std::string &path, bool erase_first,
                                       Trigger<std::string> *on_complete) {
#ifdef USE_STORAGE_WORKER
  if (global_storage_worker != nullptr) {
    if (global_storage_registry == nullptr) {
      raw_fail(on_complete, "write", "no storage registry");
      return;
    }
    const char *rel = nullptr;
    PathStorage *ps = global_storage_registry->resolve_path(path.c_str(), &rel);
    if (ps == nullptr) {
      raw_fail(on_complete, "write", std::string("no storage mounted for '") + path + "'");
      return;
    }
    // stat() below runs on the main loop; every other helper that touches a path storage there
    // refuses while the worker task owns it, or the two race on the same driver. Do the same.
    if (worker_task_busy(ps)) {
      raw_fail(on_complete, "write", "source storage is busy with a background transfer");
      return;
    }
    // The sync twin (perform_raw_write_from_file -> perform_raw_write) rejects a zero-length write
    // and preflights the address against the device geometry before touching it; the worker path did
    // neither. from_file needs a path driver and every path driver requests the worker, so the sync
    // twin is unreachable in practice and this is the only path a real build takes -- do both checks
    // here. The stat gives the length to preflight with; if the stat itself fails, fall through and
    // let the worker report the real error (a missing source, etc.).
    FileStat st{};
    const bool have_stat = ps->stat(rel, &st) == StorageError::STORAGE_ERROR_OK && !st.is_dir;
    if (have_stat && st.size == 0) {
      raw_fail(on_complete, "write", error_to_string(StorageError::STORAGE_ERROR_INVALID_ARGS));
      return;
    }
    if (have_stat) {
      RawGeometry geo;
      StorageError pf = raw_preflight(device, "write", address, st.size, &geo);
      if (pf != StorageError::STORAGE_ERROR_OK) {
        raw_fail(on_complete, "write", error_to_string(pf));
        return;
      }
    }
    ESP_LOGI(TAG, "Transfer started: write '%s' -> 0x%08" PRIX32, path.c_str(), (uint32_t) address);
    StorageError err = global_storage_worker->async_raw_write(
        ps, rel, device, address, erase_first, [on_complete](StorageError r) { raw_fire(on_complete, "write", r); });
    if (err != StorageError::STORAGE_ERROR_OK)
      raw_fail(on_complete, "write", std::string("could not queue (") + error_to_string(err) + ")");
    return;
  }
#endif
  StorageError ferr = perform_raw_write_from_file(device, address, path, erase_first);
  if (on_complete != nullptr)
    on_complete->trigger(ferr == StorageError::STORAGE_ERROR_OK ? std::string() : std::string(error_to_string(ferr)));
}

void perform_raw_erase_async(RawStorage *device, uint64_t address, uint64_t size, bool all,
                             Trigger<std::string> *on_complete, bool force_sliced) {
  RawGeometry geo;
  device->get_raw_geometry(&geo);
  if (all) {
    address = 0;
    size = geo.capacity;
  }
  // The sync twin (perform_raw_erase) rejects these before touching the device; the worker path must
  // too, or a zero-length or out-of-range erase (e.g. all:true when get_raw_geometry() reports
  // capacity 0 because the probe failed) submits and fires on_complete with the empty success string.
  if (size == 0) {
    raw_fail(on_complete, "erase", error_to_string(StorageError::STORAGE_ERROR_INVALID_ARGS));
    return;
  }
  StorageError pf = raw_preflight(device, "erase", address, size, &geo);
  if (pf != StorageError::STORAGE_ERROR_OK) {
    raw_fail(on_complete, "erase", error_to_string(pf));
    return;
  }
#ifdef USE_STORAGE_WORKER
  if (global_storage_worker != nullptr) {
    StorageError err = global_storage_worker->async_raw_erase(
        device, address, size, [on_complete](StorageError r) { raw_fire(on_complete, "erase", r); }, nullptr,
        force_sliced);
    if (err != StorageError::STORAGE_ERROR_OK)
      raw_fail(on_complete, "erase", std::string("could not queue (") + error_to_string(err) + ")");
    return;
  }
#endif
  StorageError err = perform_raw_erase(device, address, size, /*all=*/false);  // address/size already resolved above
  if (on_complete != nullptr)
    on_complete->trigger(err == StorageError::STORAGE_ERROR_OK ? std::string() : std::string(error_to_string(err)));
}
#endif  // USE_STORAGE_RAW_ACTIONS

StorageError perform_file_copy(const std::string &from, const std::string &to, bool is_move) {
  const char *op = is_move ? "move" : "copy";
  ESP_LOGI(TAG, "Transfer started: %s '%s' -> '%s'", op, from.c_str(), to.c_str());
  if (global_storage_registry == nullptr) {
    ESP_LOGE(TAG, "file_%s: no storage registry", op);
    return StorageError::STORAGE_ERROR_NOT_READY;
  }
  const char *src_rel = nullptr;
  const char *dst_rel = nullptr;
  PathStorage *src = global_storage_registry->resolve_path(from.c_str(), &src_rel);
  PathStorage *dst = global_storage_registry->resolve_path(to.c_str(), &dst_rel);
  if (src == nullptr || dst == nullptr) {
    ESP_LOGE(TAG, "file_%s: no storage mounted for '%s'", op, src == nullptr ? from.c_str() : to.c_str());
    return StorageError::STORAGE_ERROR_NOT_FOUND;
  }
  // Same guard as every other blocking helper here: refuse if the worker task is streaming either
  // volume, so an external main-loop caller cannot put two threads into one storage at once.
  if (worker_task_busy(src) || worker_task_busy(dst)) {
    ESP_LOGE(TAG, "file_%s: storage is busy with a background transfer -- refusing blocking I/O", op);
    return StorageError::STORAGE_ERROR_NOT_READY;
  }
  // move() internally takes the same-storage rename() fast path and only falls back to
  // copy+delete across devices -- so this action doubles as a rename action. Both helpers are
  // PathStorage-level (filesystem and network alike), take a directory as readily as a file
  // (recursively, source decides), and honor max_blocking_transfer_size per file.
  StorageError err = is_move ? move(src, src_rel, dst, dst_rel) : copy(src, src_rel, dst, dst_rel);
  if (err != StorageError::STORAGE_ERROR_OK) {
    ESP_LOGE(TAG, "file_%s: '%s' -> '%s' failed (%s)", op, from.c_str(), to.c_str(), error_to_string(err));
    return err;
  }
  ESP_LOGI(TAG, "Transfer done: %s '%s' -> '%s'", op, from.c_str(), to.c_str());
  return StorageError::STORAGE_ERROR_OK;
}

void perform_file_copy_async(const std::string &from, const std::string &to, bool is_move,
                             Trigger<std::string> *on_complete) {
  const char *op = is_move ? "move" : "copy";
  ESP_LOGI(TAG, "Transfer started: %s '%s' -> '%s'", op, from.c_str(), to.c_str());

  // Helper: report a synchronous (pre-submission) failure -- log it and fire the trigger with
  // the message so an automation can react. Reused for every early-out below.
  auto fail = [&](const std::string &msg) {
    ESP_LOGE(TAG, "file_%s: %s", op, msg.c_str());
    if (on_complete != nullptr)
      on_complete->trigger(msg);
  };

  if (global_storage_registry == nullptr) {
    fail("no storage registry");
    return;
  }
  const char *src_rel = nullptr;
  const char *dst_rel = nullptr;
  PathStorage *src = global_storage_registry->resolve_path(from.c_str(), &src_rel);
  PathStorage *dst = global_storage_registry->resolve_path(to.c_str(), &dst_rel);
  if (src == nullptr || dst == nullptr) {
    fail(std::string("no storage mounted for '") + (src == nullptr ? from : to) + "'");
    return;
  }

  // Same-storage guards the blocking twin (storage::copy) enforces before it truncates the
  // destination. The worker COPY/MOVE pre-phase does not, so from == to would open the destination
  // for write, read EOF, and fire on_complete with the empty success string while the source is
  // destroyed. Rejecting here, where src_rel/dst_rel are already resolved, needs no main-loop
  // stat(): dst strictly under src is never a valid same-storage transfer whether src is a file or
  // a directory, so the prefix test stands in for copy()'s is_dir-gated subtree check.
  if (src == dst) {
    size_t src_len = strlen(src_rel);
    if (strcmp(src_rel, dst_rel) == 0 || (strncmp(src_rel, dst_rel, src_len) == 0 && dst_rel[src_len] == '/')) {
      fail(std::string(op) + ": destination is the source or lies within it");
      return;
    }
  }

#ifdef USE_STORAGE_WORKER
  if (global_storage_worker != nullptr) {
    // Overwrite: the action's historical semantics were "just do it" (the blocking helpers
    // truncate/replace), so keep that -- pass overwrite = true for parity.
    // Capture only the pointer and the bool so the closure fits std::function's small-buffer and
    // avoids the per-play() heap allocation the raw variants already avoid. The paths were logged
    // at "Transfer started" above and the worker holds its own copies for its completion logs.
    auto on_done = [on_complete, is_move](StorageError result) {
      const char *op = is_move ? "move" : "copy";
      if (result != StorageError::STORAGE_ERROR_OK) {
        ESP_LOGE(TAG, "file_%s failed (%s)", op, error_to_string(result));
      } else {
        ESP_LOGI(TAG, "Transfer done: %s", op);
      }
      if (on_complete != nullptr)
        on_complete->trigger(result == StorageError::STORAGE_ERROR_OK ? std::string() : std::string(error_to_string(result)));
    };
    StorageError err = is_move ? global_storage_worker->async_move(src, src_rel, dst, dst_rel, std::move(on_done),
                                                                   nullptr, /*overwrite=*/true)
                               : global_storage_worker->async_copy(src, src_rel, dst, dst_rel, std::move(on_done),
                                                                   nullptr, /*overwrite=*/true);
    // Submission itself can fail (pool full -> NOT_READY, or bad args) before any callback is
    // scheduled -- report that synchronously so the trigger still fires exactly once.
    if (err != StorageError::STORAGE_ERROR_OK) {
      fail(std::string("could not queue (") + error_to_string(err) + ")");
    }
    return;
  }
#endif

  // No worker compiled in (raw-only node, or no path driver requested it): fall back to the
  // blocking helper. This can exceed the 30 ms loop budget for large transfers -- the async
  // path above is the norm; this is only the degenerate no-worker build.
  StorageError err = perform_file_copy(from, to, is_move);
  if (on_complete != nullptr)
    on_complete->trigger(err == StorageError::STORAGE_ERROR_OK ? std::string() : std::string(error_to_string(err)));
}

StorageError perform_file_delete(const std::string &path, bool recursive) {
  if (global_storage_registry == nullptr) {
    ESP_LOGE(TAG, "file_delete: no storage registry");
    return StorageError::STORAGE_ERROR_NOT_READY;
  }
  const char *rel = nullptr;
  PathStorage *ps = global_storage_registry->resolve_path(path.c_str(), &rel);
  if (ps == nullptr) {
    ESP_LOGE(TAG, "file_delete: no storage mounted for '%s'", path.c_str());
    return StorageError::STORAGE_ERROR_NOT_FOUND;
  }
  if (worker_task_busy(ps)) {
    ESP_LOGE(TAG, "file_delete: '%s' is busy with a background transfer -- refusing blocking I/O", path.c_str());
    return StorageError::STORAGE_ERROR_NOT_READY;
  }
  // remove() deletes files and empty directories; remove_recursive() walks subtrees.
  StorageError err = recursive ? remove_recursive(ps, rel) : ps->remove(rel);
  if (err != StorageError::STORAGE_ERROR_OK) {
    ESP_LOGE(TAG, "file_delete: '%s' failed (%s)", path.c_str(), error_to_string(err));
  }
  return err;
}

StorageError check_file_exists(const std::string &path) {
  if (global_storage_registry == nullptr) {
    ESP_LOGW(TAG, "file_exists: no storage registry; cannot check '%s'", path.c_str());
    return StorageError::STORAGE_ERROR_NOT_READY;
  }
  const char *rel = nullptr;
  PathStorage *ps = global_storage_registry->resolve_path(path.c_str(), &rel);
  if (ps == nullptr) {
    // Almost always a typo'd mount point in the config -- not a real "the file is absent".
    ESP_LOGW(TAG, "file_exists: no storage mounted for '%s'", path.c_str());
    return StorageError::STORAGE_ERROR_NOT_READY;
  }
  if (worker_task_busy(ps)) {
    ESP_LOGW(TAG, "file_exists: '%s' is busy with a background transfer", path.c_str());
    return StorageError::STORAGE_ERROR_NOT_READY;
  }
  StorageError err = StorageError::STORAGE_ERROR_OK;
  exists(ps, rel, &err);
  // OK = present, NOT_FOUND = a clean absent. Anything else is a not-ready/faulted medium: log it
  // here so the failure is never silent even for a caller (the bool condition) that cannot forward
  // it, and pass the code up so storage.stat can branch on it.
  if (err != StorageError::STORAGE_ERROR_OK && err != StorageError::STORAGE_ERROR_NOT_FOUND) {
    ESP_LOGE(TAG, "file_exists: checking '%s' failed (%s)", path.c_str(), error_to_string(err));
  }
  return err;
}

static void mount_fire(bool mount, StorageError result, Trigger<std::string> *on_complete) {
  if (result != StorageError::STORAGE_ERROR_OK) {
    ESP_LOGE(TAG, "%s failed (%s)", mount ? "mount" : "unmount", error_to_string(result));
  }
  if (on_complete != nullptr)
    on_complete->trigger(result == StorageError::STORAGE_ERROR_OK ? std::string() : std::string(error_to_string(result)));
}

void perform_mount(PathStorage *target, bool mount, Trigger<std::string> *on_complete) {
  MountableStorage *m = target->as_mountable();
  if (m == nullptr) {
    ESP_LOGE(TAG, "target is not mountable");
    if (on_complete != nullptr)
      on_complete->trigger(std::string("not mountable"));
    return;
  }
  // as_mountable() only means at least one op works; gate each op on its cap bit (USB is
  // unmount-only) so an unsupported op returns a uniform NOT_SUPPORTED, not driver-defined behaviour.
  const uint8_t need = mount ? MountableStorage::MOUNT_CAP_MOUNT : MountableStorage::MOUNT_CAP_UNMOUNT;
  if ((m->get_mount_caps() & need) == 0) {
    mount_fire(mount, StorageError::STORAGE_ERROR_NOT_SUPPORTED, on_complete);
    return;
  }
  if (mount) {
#ifdef USE_STORAGE_WORKER
    // Async-first, the perform_format_async() shape: the worker routes the blocking
    // resolve/connect/probe work to its task when the driver reports task-safety and the
    // platform has one, and loop-slices it otherwise -- capabilities decide, not the caller.
    if (global_storage_worker != nullptr) {
      StorageError err = global_storage_worker->async_mount(
          target, [on_complete](StorageError r) { mount_fire(true, r, on_complete); }, nullptr);
      if (err != StorageError::STORAGE_ERROR_OK)
        mount_fire(true, err, on_complete);  // could not queue -- report inline
      return;
    }
#endif
    mount_fire(true, m->mount(), on_complete);
    return;
  }
  // Unmount stays synchronous by design: drivers quiesce the worker inside unmount(), and the
  // quiesce drain is owned by the main loop -- running it on the worker task would deadlock
  // the drain against itself.
  mount_fire(false, m->unmount(), on_complete);
}

static void format_fire(Trigger<std::string> *on_complete, StorageError result) {
  if (result == StorageError::STORAGE_ERROR_NOT_SUPPORTED) {
    ESP_LOGE(TAG, "format is not supported by this filesystem");
  } else if (result != StorageError::STORAGE_ERROR_OK) {
    ESP_LOGE(TAG, "format failed (%s)", error_to_string(result));
  } else {
    ESP_LOGI(TAG, "filesystem formatted");
  }
  if (on_complete != nullptr)
    on_complete->trigger(result == StorageError::STORAGE_ERROR_OK ? std::string() : std::string(error_to_string(result)));
}

void perform_format_async(Storage *target, Trigger<std::string> *on_complete) {
#ifdef USE_STORAGE_WORKER
  if (global_storage_worker != nullptr) {
    ESP_LOGI(TAG, "Formatting filesystem...");
    StorageError err = global_storage_worker->async_format(
        target, [on_complete](StorageError r) { format_fire(on_complete, r); }, nullptr);
    if (err != StorageError::STORAGE_ERROR_OK)
      format_fire(on_complete, err);  // could not queue -- report inline
    return;
  }
#endif
  // No worker in this build: the one blocking call runs on the main loop, bounded by the
  // driver's format() (see the blocking contract in automation.h), not by the automation.
  format_fire(on_complete, target->format());
}

}  // namespace esphome::storage
