#include "automation.h"

#include "esphome/core/log.h"

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
        if (trimmed.rfind(needle, 0) == 0) {
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
    case ExtractStepType::REGEX: {
#ifdef USE_STORAGE_REGEX_EXTRACT
      // Pattern syntax was validated at config time (ECMAScript grammar, std::regex default).
      std::regex re(step.arg);
      std::smatch m;
      if (!std::regex_search(buf, m, re)) {
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
      ESP_LOGW(TAG, "file_read: regex step configured but not compiled in");
      return false;
#endif
    }
  }
  return false;
}

// NOTE: A `json:` extraction step (JSON-pointer based) is planned as a separate follow-up PR —
// it pulls in the json component as a dependency, so it stays out of this baseline set.

void perform_file_write(const std::string &path, std::string content, bool append, bool newline) {
  const char *op = append ? "append" : "write";
  if (newline)
    content += '\n';

  if (global_storage_registry == nullptr) {
    ESP_LOGW(TAG, "file_%s: no storage registry", op);
    return;
  }
  const char *rel = nullptr;
  PathStorage *ps = global_storage_registry->resolve_path(path.c_str(), &rel);
  if (ps == nullptr) {
    ESP_LOGW(TAG, "file_%s: no storage mounted for '%s'", op, path.c_str());
    return;
  }
  // v1 supports filesystem-backed storages; NetworkStorage has no append semantics in its
  // chunk API yet (follow-up).
  if (ps->get_storage_type() != StorageType::FILESYSTEM) {
    ESP_LOGW(TAG, "file_%s: '%s' is not on a filesystem storage", op, path.c_str());
    return;
  }
  auto *fs = static_cast<FilesystemStorage *>(ps);

  FileHandle *handle = nullptr;
  StorageError err = fs->open(rel, handle, append ? OpenMode::APPEND : OpenMode::WRITE);
  if (err != StorageError::OK) {
    ESP_LOGW(TAG, "file_%s: open '%s' failed (%s)", op, path.c_str(), error_to_string(err));
    return;
  }
  size_t written = 0;
  err = fs->write(handle, reinterpret_cast<const uint8_t *>(content.data()), content.size(), &written);
  // Close errors must surface: FATFS-backed drivers flush on close (see copy() contract).
  StorageError close_err = fs->close(handle);
  if (err == StorageError::OK)
    err = close_err;
  if (err != StorageError::OK || written != content.size()) {
    ESP_LOGW(TAG, "file_%s: writing '%s' failed (%s, %zu/%zu bytes)", op, path.c_str(), error_to_string(err), written,
             content.size());
  }
}

bool perform_file_read(const std::string &path, const std::vector<ExtractStep> &steps, std::string &out) {
  if (global_storage_registry == nullptr) {
    ESP_LOGW(TAG, "file_read: no storage registry");
    return false;
  }
  const char *rel = nullptr;
  PathStorage *ps = global_storage_registry->resolve_path(path.c_str(), &rel);
  if (ps == nullptr) {
    ESP_LOGW(TAG, "file_read: no storage mounted for '%s'", path.c_str());
    return false;
  }

  RamBuffer buf;
  size_t size = 0;
  StorageError err;
  if (ps->get_storage_type() == StorageType::FILESYSTEM) {
    err = read_file(static_cast<FilesystemStorage *>(ps), rel, buf, &size);
  } else {
    err = read_file(static_cast<NetworkStorage *>(ps), rel, buf, &size);
  }
  if (err != StorageError::OK) {
    // Error path leaves any configured global untouched and does not fire on_value.
    ESP_LOGW(TAG, "file_read: reading '%s' failed (%s)", path.c_str(), error_to_string(err));
    return false;
  }

  out.assign(reinterpret_cast<const char *>(buf.get()), size);
  for (const auto &step : steps) {
    if (!apply_extract_step(step, out))
      return false;  // step already logged; global untouched, no trigger
  }
  return true;
}

}  // namespace esphome::storage
