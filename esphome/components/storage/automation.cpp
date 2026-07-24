#include "automation.h"

#ifdef USE_STORAGE_JSON_EXTRACT
#include "esphome/components/json/json_util.h"
#endif

#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include <cinttypes>

#ifdef USE_STORAGE_REGEX_EXTRACT
// <regex> costs significant flash (~50-100 kB) and stack; it is only compiled in when a
// config actually uses a `regex:` extraction step (codegen sets the define).
#include <regex>
#endif

namespace esphome::storage {

static const char *const TAG = "storage.automation";

void warn_invalid_bool(const std::string &s) {
  ESP_LOGV(TAG, "file_read: '%s' is not a valid bool; global unchanged", s.c_str());
}
void warn_invalid_number(const std::string &s) {
  ESP_LOGV(TAG, "file_read: '%s' is not a valid number; global unchanged", s.c_str());
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
      ESP_LOGV(TAG, "file_read: line %d not found (%d lines)", step.index, current - 1);
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
      ESP_LOGV(TAG, "file_read: split element %d not found", step.index);
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
      ESP_LOGV(TAG, "file_read: key '%s' not found", step.arg.c_str());
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
        ESP_LOGV(TAG, "extract json: invalid JSON document");
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
              ESP_LOGV(TAG, "extract json: '%s' is not an array index", token.c_str());
              return false;
            }
            node = node.as<JsonArrayConst>()[idx];
          } else {
            node = node[token.c_str()];
          }
          if (node.isNull()) {
            ESP_LOGV(TAG, "extract json: path element '%s' not found", token.c_str());
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
        serializeJson(node, serialized);
        buf = std::move(serialized);
      }
      return true;
#else
      return false;  // step cannot be configured without the define -- defensive
#endif  // USE_STORAGE_JSON_EXTRACT
    }
    case ExtractStepType::REGEX: {
#ifdef USE_STORAGE_REGEX_EXTRACT
      // Pattern syntax was validated at config time (ECMAScript grammar, std::regex default).
      std::regex re(step.arg);
      std::smatch m;
      if (!std::regex_search(buf, m, re)) {
        ESP_LOGV(TAG, "file_read: regex '%s' did not match", step.arg.c_str());
        return false;
      }
      int group = step.index;
      if (group >= static_cast<int>(m.size())) {
        ESP_LOGV(TAG, "file_read: regex group %d does not exist (%zu groups)", group, m.size() - 1);
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

// NOTE: A `json:` extraction step (JSON-pointer based) is planned as a separate follow-up PR --
// it pulls in the json component as a dependency, so it stays out of this baseline set.

void perform_file_write(const std::string &path, std::string content, bool append, bool newline) {
  const char *op = append ? "append" : "write";
  if (newline)
    content += '\n';

  if (global_storage_registry == nullptr) {
    ESP_LOGE(TAG, "file_%s: no storage registry", op);
    return;
  }
  const char *rel = nullptr;
  PathStorage *ps = global_storage_registry->resolve_path(path.c_str(), &rel);
  if (ps == nullptr) {
    ESP_LOGE(TAG, "file_%s: no storage mounted for '%s'", op, path.c_str());
    return;
  }

  StorageError err;
  if (!append) {
    // PathStorage-level helper -- works on FILESYSTEM and NETWORK storages alike.
    err = write_file(ps, rel, reinterpret_cast<const uint8_t *>(content.data()), content.size());
  } else if (ps->get_storage_type() == StorageType::FILESYSTEM) {
    // Filesystem append: native handle-based APPEND open.
    auto *fs = static_cast<FilesystemStorage *>(ps);
    FileHandle *handle = nullptr;
    err = fs->open(rel, handle, OpenMode::APPEND);
    if (err != StorageError::OK) {
      ESP_LOGE(TAG, "file_append: open '%s' failed (%s)", path.c_str(), error_to_string(err));
      return;
    }
    size_t written = 0;
    err = fs->write(handle, reinterpret_cast<const uint8_t *>(content.data()), content.size(), &written);
    // Close errors must surface: FATFS-backed drivers flush on close (see copy() contract).
    StorageError close_err = fs->close(handle);
    if (err == StorageError::OK)
      err = close_err;
    if (err == StorageError::OK && written != content.size())
      err = StorageError::WRITE_ERROR;
  } else {
    // Network append: the chunk API takes an explicit offset (NFS supports offset writes
    // natively), so appending is stat-for-size + one write_chunk at EOF -- O(1) RAM, no
    // read-modify-write. A missing file starts at offset 0 (created by the write). The
    // stat->write window is not atomic against other writers; acceptable for a single node
    // appending its own logs/values.
    auto *ns = static_cast<NetworkStorage *>(ps);
    uint64_t offset = 0;
    FileStat st{};
    err = ns->stat(rel, &st);
    if (err == StorageError::OK) {
      offset = st.size;
    } else if (err != StorageError::NOT_FOUND) {
      ESP_LOGE(TAG, "file_append: stat '%s' failed (%s)", path.c_str(), error_to_string(err));
      return;
    }
    size_t written = 0;
    err = ns->write_chunk(rel, reinterpret_cast<const uint8_t *>(content.data()), offset, content.size(), &written);
    if (err == StorageError::OK && written != content.size())
      err = StorageError::WRITE_ERROR;
  }

  if (err != StorageError::OK) {
    ESP_LOGE(TAG, "file_%s: writing '%s' failed (%s)", op, path.c_str(), error_to_string(err));
  }
}

bool perform_file_read(const std::string &path, const std::vector<ExtractStep> &steps, std::string &out) {
  if (global_storage_registry == nullptr) {
    ESP_LOGE(TAG, "file_read: no storage registry");
    return false;
  }
  const char *rel = nullptr;
  PathStorage *ps = global_storage_registry->resolve_path(path.c_str(), &rel);
  if (ps == nullptr) {
    ESP_LOGE(TAG, "file_read: no storage mounted for '%s'", path.c_str());
    return false;
  }

  RamBuffer buf;
  size_t size = 0;
  // PathStorage-level helper -- works on FILESYSTEM and NETWORK storages alike.
  StorageError err = read_file(ps, rel, buf, &size);
  if (err != StorageError::OK) {
    // Error path leaves any configured global untouched and does not fire on_value.
    ESP_LOGE(TAG, "file_read: reading '%s' failed (%s)", path.c_str(), error_to_string(err));
    return false;
  }

  out.assign(reinterpret_cast<const char *>(buf.get()), size);
  for (const auto &step : steps) {
    if (!apply_extract_step(step, out))
      return false;  // step already logged; global untouched, no trigger
  }
  return true;
}

#ifdef USE_STORAGE_RAW_ACTIONS

// Every raw action asks the device what it is before touching it -- capacity and geometry come
// from the driver (see RawGeometry), never from an assumption about the medium.
static bool raw_preflight(RawStorage *device, const char *op, uint64_t address, uint64_t size, RawGeometry *geo) {
  device->get_raw_geometry(geo);
  if (geo->capacity == 0) {
    ESP_LOGE(TAG, "raw_%s: device reports no capacity", op);
    return false;
  }
  if (address >= geo->capacity || size > geo->capacity - address) {
    ESP_LOGE(TAG, "raw_%s: 0x%08" PRIX32 " + %" PRIu32 " exceeds the device capacity %" PRIu32, op, (uint32_t) address,
             (uint32_t) size, (uint32_t) geo->capacity);
    return false;
  }
  return true;
}

// Same guard rail the blocking file helpers use: these actions run on the main loop.
static bool raw_size_allowed(const char *op, uint64_t size) {
  uint64_t limit = global_storage_registry != nullptr ? global_storage_registry->get_max_blocking_transfer_size() : 0;
  if (limit != 0 && size > limit) {
    ESP_LOGE(TAG, "raw_%s: %" PRIu32 " bytes exceeds max_blocking_transfer_size (%" PRIu32 ")", op, (uint32_t) size,
             (uint32_t) limit);
    return false;
  }
  return true;
}

// Erases the sector range covering [address, address+len) -- expanding to sector bounds, which
// is what makes this destructive to neighbours and therefore opt-in.
static bool raw_erase_for_write(RawStorage *device, const RawGeometry &geo, uint64_t address, size_t len) {
  if (geo.erase_sector == 0) {
    ESP_LOGE(TAG, "raw_write: erase_first requested but this device has no erase");
    return false;
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
    return false;
  }
  ESP_LOGD(TAG, "raw_write: erasing 0x%08" PRIX32 " + %" PRIu32 " before writing", (uint32_t) start,
           (uint32_t) (end - start));
  StorageError err = device->erase(start, static_cast<size_t>(end - start));
  if (err != StorageError::OK) {
    ESP_LOGE(TAG, "raw_write: erase failed (%s)", error_to_string(err));
    return false;
  }
  return true;
}

// Reads the range into an already-sized buffer, honoring the partial-read contract.
static bool raw_read_into(RawStorage *device, uint64_t address, uint8_t *buf, size_t size, size_t *done_out) {
  size_t done = 0;
  while (done < size) {
    size_t got = 0;
    StorageError err = device->read(address + done, buf + done, size - done, &got);
    if (err != StorageError::OK) {
      ESP_LOGE(TAG, "raw_read: failed at 0x%08" PRIX32 " (%s)", (uint32_t) (address + done), error_to_string(err));
      return false;
    }
    if (got == 0)
      break;  // end of medium (partial-read contract)
    done += got;
  }
  *done_out = done;
  return true;
}

bool perform_raw_read(RawStorage *device, uint64_t address, size_t size, std::vector<uint8_t> &out) {
  RawGeometry geo;
  if (!raw_preflight(device, "read", address, size, &geo) || !raw_size_allowed("read", size))
    return false;
  // std::vector::resize() has no way to report a failed allocation in an exceptions-free
  // build -- it aborts. Ask the nothrow allocator first, which answers with a null pointer, and
  // hand the block straight back: nothing else allocates between here and the resize below, so
  // it gets the same memory. read_file() avoids the question entirely by owning a RAMAllocator
  // buffer; this path has to end up with a std::vector because that is what the trigger takes.
  if (size > out.capacity()) {
    RAMAllocator<uint8_t> allocator;
    uint8_t *probe = allocator.allocate(size);
    if (probe == nullptr) {
      ESP_LOGE(TAG, "raw_read: cannot allocate %" PRIu32 " bytes", (uint32_t) size);
      return false;
    }
    allocator.deallocate(probe, size);
  }
  out.resize(size);
  size_t done = 0;
  if (!raw_read_into(device, address, out.data(), size, &done)) {
    out.clear();
    return false;
  }
  out.resize(done);
  return true;
}

bool perform_raw_read_to_file(RawStorage *device, uint64_t address, uint64_t size, const std::string &path) {
  RawGeometry geo;
  device->get_raw_geometry(&geo);
  if (size == 0)  // "to the end of the device"
    size = geo.capacity > address ? geo.capacity - address : 0;
  ESP_LOGI(TAG, "Transfer started: read 0x%08" PRIX32 " + %" PRIu32 " -> '%s'", (uint32_t) address, (uint32_t) size,
           path.c_str());
  if (!raw_preflight(device, "read", address, size, &geo) || !raw_size_allowed("read", size))
    return false;
  if (global_storage_registry == nullptr) {
    ESP_LOGE(TAG, "raw_read: no storage registry");
    return false;
  }
  const char *rel = nullptr;
  PathStorage *ps = global_storage_registry->resolve_path(path.c_str(), &rel);
  if (ps == nullptr) {
    ESP_LOGE(TAG, "raw_read: no storage mounted for '%s'", path.c_str());
    return false;
  }

  auto buf_size = static_cast<size_t>(size);
  uint8_t *raw = RAMAllocator<uint8_t>().allocate(buf_size);
  if (raw == nullptr) {
    ESP_LOGE(TAG, "raw_read: cannot allocate %" PRIu32 " bytes", (uint32_t) buf_size);
    return false;
  }
  RamBuffer buf(raw, RamBufferDeleter{buf_size});
  size_t done = 0;
  if (!raw_read_into(device, address, buf.get(), buf_size, &done))
    return false;

  StorageError err = write_file(ps, rel, buf.get(), done);
  if (err != StorageError::OK) {
    ESP_LOGE(TAG, "raw_read: writing '%s' failed (%s)", path.c_str(), error_to_string(err));
    return false;
  }
  ESP_LOGI(TAG, "Transfer done: read 0x%08" PRIX32 " + %" PRIu32 " -> '%s'", (uint32_t) address, (uint32_t) done,
           path.c_str());
  return true;
}

bool perform_raw_write(RawStorage *device, uint64_t address, const uint8_t *data, size_t len, bool erase_first) {
  if (len == 0)
    return true;
  RawGeometry geo;
  if (!raw_preflight(device, "write", address, len, &geo) || !raw_size_allowed("write", len))
    return false;
  if (erase_first && !raw_erase_for_write(device, geo, address, len))
    return false;

  size_t done = 0;
  while (done < len) {
    size_t written = 0;
    StorageError err = device->write(address + done, data + done, len - done, &written);
    if (err != StorageError::OK) {
      ESP_LOGE(TAG, "raw_write: failed at 0x%08" PRIX32 " (%s)", (uint32_t) (address + done), error_to_string(err));
      return false;
    }
    if (written == 0) {
      ESP_LOGE(TAG, "raw_write: device stopped accepting data at 0x%08" PRIX32, (uint32_t) (address + done));
      return false;
    }
    done += written;
  }
  ESP_LOGD(TAG, "raw_write: %" PRIu32 " bytes at 0x%08" PRIX32, (uint32_t) len, (uint32_t) address);
  return true;
}

bool perform_raw_write_from_file(RawStorage *device, uint64_t address, const std::string &path, bool erase_first) {
  ESP_LOGI(TAG, "Transfer started: write '%s' -> 0x%08" PRIX32, path.c_str(), (uint32_t) address);
  if (global_storage_registry == nullptr) {
    ESP_LOGE(TAG, "raw_write: no storage registry");
    return false;
  }
  const char *rel = nullptr;
  PathStorage *ps = global_storage_registry->resolve_path(path.c_str(), &rel);
  if (ps == nullptr) {
    ESP_LOGE(TAG, "raw_write: no storage mounted for '%s'", path.c_str());
    return false;
  }
  RamBuffer buf;
  size_t size = 0;
  StorageError err = read_file(ps, rel, buf, &size);
  if (err != StorageError::OK) {
    ESP_LOGE(TAG, "raw_write: reading '%s' failed (%s)", path.c_str(), error_to_string(err));
    return false;
  }
  bool ok = perform_raw_write(device, address, buf.get(), size, erase_first);
  if (ok) {
    ESP_LOGI(TAG, "Transfer done: write '%s' (%" PRIu32 " bytes) -> 0x%08" PRIX32, path.c_str(), (uint32_t) size,
             (uint32_t) address);
  }
  return ok;
}

void perform_raw_erase(RawStorage *device, uint64_t address, uint64_t size, bool all) {
  RawGeometry geo;
  device->get_raw_geometry(&geo);
  if (all) {
    address = 0;
    size = geo.capacity;
  }
  if (!raw_preflight(device, "erase", address, size, &geo))
    return;
  // No alignment massaging here: erase() rejects an unaligned range on purpose (it would take
  // the neighbouring data with it), and silently rounding would defeat that.
  StorageError err = device->erase(address, static_cast<size_t>(size));
  if (err != StorageError::OK) {
    ESP_LOGE(TAG, "raw_erase: 0x%08" PRIX32 " + %" PRIu32 " failed (%s)", (uint32_t) address, (uint32_t) size,
             error_to_string(err));
    return;
  }
  ESP_LOGD(TAG, "raw_erase: 0x%08" PRIX32 " + %" PRIu32 " done", (uint32_t) address, (uint32_t) size);
}

// --- async raw helpers: submit to the worker, stream, fire on_complete (error text) ---------

// Shared completion glue for the async raw actions: log on failure, fire the trigger once with
// the error text (empty = success).
static void raw_fire(Trigger<std::string> *on_complete, const char *op, StorageError result) {
  if (result != StorageError::OK) {
    ESP_LOGE(TAG, "raw_%s failed (%s)", op, error_to_string(result));
  } else {
    ESP_LOGI(TAG, "Transfer done: raw %s", op);
  }
  if (on_complete != nullptr)
    on_complete->trigger(result == StorageError::OK ? std::string() : std::string(error_to_string(result)));
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
    if (size == 0) {  // "to the end of the device"
      RawGeometry geo;
      device->get_raw_geometry(&geo);
      size = geo.capacity > address ? geo.capacity - address : 0;
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
    if (err != StorageError::OK)
      raw_fail(on_complete, "read", std::string("could not queue (") + error_to_string(err) + ")");
    return;
  }
#endif
  bool ok = perform_raw_read_to_file(device, address, size, path);
  if (on_complete != nullptr)
    on_complete->trigger(ok ? std::string() : std::string("read failed"));
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
    ESP_LOGI(TAG, "Transfer started: write '%s' -> 0x%08" PRIX32, path.c_str(), (uint32_t) address);
    StorageError err = global_storage_worker->async_raw_write(
        ps, rel, device, address, erase_first, [on_complete](StorageError r) { raw_fire(on_complete, "write", r); });
    if (err != StorageError::OK)
      raw_fail(on_complete, "write", std::string("could not queue (") + error_to_string(err) + ")");
    return;
  }
#endif
  bool ok = perform_raw_write_from_file(device, address, path, erase_first);
  if (on_complete != nullptr)
    on_complete->trigger(ok ? std::string() : std::string("write failed"));
}

void perform_raw_erase_async(RawStorage *device, uint64_t address, uint64_t size, bool all,
                             Trigger<std::string> *on_complete, bool force_sliced) {
  if (all) {
    RawGeometry geo;
    device->get_raw_geometry(&geo);
    address = 0;
    size = geo.capacity;
  }
#ifdef USE_STORAGE_WORKER
  if (global_storage_worker != nullptr) {
    StorageError err = global_storage_worker->async_raw_erase(
        device, address, size, [on_complete](StorageError r) { raw_fire(on_complete, "erase", r); }, nullptr,
        force_sliced);
    if (err != StorageError::OK)
      raw_fail(on_complete, "erase", std::string("could not queue (") + error_to_string(err) + ")");
    return;
  }
#endif
  perform_raw_erase(device, address, size, /*all=*/false);  // address/size already resolved above
  if (on_complete != nullptr)
    on_complete->trigger(std::string());  // best-effort: the sync helper only logs on failure
}
#endif  // USE_STORAGE_RAW_ACTIONS

StorageError perform_file_copy(const std::string &from, const std::string &to, bool is_move) {
  const char *op = is_move ? "move" : "copy";
  ESP_LOGI(TAG, "Transfer started: %s '%s' -> '%s'", op, from.c_str(), to.c_str());
  if (global_storage_registry == nullptr) {
    ESP_LOGE(TAG, "file_%s: no storage registry", op);
    return StorageError::NOT_READY;
  }
  const char *src_rel = nullptr;
  const char *dst_rel = nullptr;
  PathStorage *src = global_storage_registry->resolve_path(from.c_str(), &src_rel);
  PathStorage *dst = global_storage_registry->resolve_path(to.c_str(), &dst_rel);
  if (src == nullptr || dst == nullptr) {
    ESP_LOGE(TAG, "file_%s: no storage mounted for '%s'", op, src == nullptr ? from.c_str() : to.c_str());
    return StorageError::NOT_FOUND;
  }
  // move() internally takes the same-storage rename() fast path and only falls back to
  // copy+delete across devices -- so this action doubles as a rename action. Both helpers are
  // PathStorage-level (filesystem and network alike), take a directory as readily as a file
  // (recursively, source decides), and honor max_blocking_transfer_size per file.
  StorageError err = is_move ? move(src, src_rel, dst, dst_rel) : copy(src, src_rel, dst, dst_rel);
  if (err != StorageError::OK) {
    ESP_LOGE(TAG, "file_%s: '%s' -> '%s' failed (%s)", op, from.c_str(), to.c_str(), error_to_string(err));
    return err;
  }
  ESP_LOGI(TAG, "Transfer done: %s '%s' -> '%s'", op, from.c_str(), to.c_str());
  return StorageError::OK;
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

#ifdef USE_STORAGE_WORKER
  if (global_storage_worker != nullptr) {
    // Overwrite: the action's historical semantics were "just do it" (the blocking helpers
    // truncate/replace), so keep that -- pass overwrite = true for parity.
    auto on_done = [on_complete, from, to, is_move](StorageError result) {
      if (result != StorageError::OK) {
        ESP_LOGE(TAG, "file_%s: '%s' -> '%s' failed (%s)", is_move ? "move" : "copy", from.c_str(), to.c_str(),
                 error_to_string(result));
      } else {
        ESP_LOGI(TAG, "Transfer done: %s '%s' -> '%s'", is_move ? "move" : "copy", from.c_str(), to.c_str());
      }
      if (on_complete != nullptr)
        on_complete->trigger(result == StorageError::OK ? std::string() : std::string(error_to_string(result)));
    };
    StorageError err = is_move ? global_storage_worker->async_move(src, src_rel, dst, dst_rel, std::move(on_done),
                                                                   nullptr, /*overwrite=*/true)
                               : global_storage_worker->async_copy(src, src_rel, dst, dst_rel, std::move(on_done),
                                                                   nullptr, /*overwrite=*/true);
    // Submission itself can fail (pool full -> NOT_READY, or bad args) before any callback is
    // scheduled -- report that synchronously so the trigger still fires exactly once.
    if (err != StorageError::OK) {
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
    on_complete->trigger(err == StorageError::OK ? std::string() : std::string(error_to_string(err)));
}

void perform_file_delete(const std::string &path, bool recursive) {
  if (global_storage_registry == nullptr) {
    ESP_LOGE(TAG, "file_delete: no storage registry");
    return;
  }
  const char *rel = nullptr;
  PathStorage *ps = global_storage_registry->resolve_path(path.c_str(), &rel);
  if (ps == nullptr) {
    ESP_LOGE(TAG, "file_delete: no storage mounted for '%s'", path.c_str());
    return;
  }
  // remove() deletes files and empty directories; remove_recursive() walks subtrees.
  StorageError err = recursive ? remove_recursive(ps, rel) : ps->remove(rel);
  if (err != StorageError::OK) {
    ESP_LOGE(TAG, "file_delete: '%s' failed (%s)", path.c_str(), error_to_string(err));
  }
}

bool check_file_exists(const std::string &path) {
  if (global_storage_registry == nullptr)
    return false;
  const char *rel = nullptr;
  PathStorage *ps = global_storage_registry->resolve_path(path.c_str(), &rel);
  if (ps == nullptr)
    return false;
  StorageError err = StorageError::OK;
  bool found = exists(ps, rel, &err);
  // Only NOT_FOUND is a clean "no" -- surface anything else (unmounted/faulted medium) so a
  // transient failure is visible instead of silently reading as absence.
  if (!found && err != StorageError::NOT_FOUND && err != StorageError::OK) {
    ESP_LOGE(TAG, "file_exists: checking '%s' failed (%s)", path.c_str(), error_to_string(err));
  }
  return found;
}

void perform_mount(MountableStorage *target, bool mount) {
  StorageError err = mount ? target->mount() : target->unmount();
  if (err != StorageError::OK) {
    ESP_LOGE(TAG, "%s failed (%s)", mount ? "mount" : "unmount", error_to_string(err));
  }
}

}  // namespace esphome::storage
