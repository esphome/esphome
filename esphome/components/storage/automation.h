#pragma once

#include "storage.h"
#include "esphome/core/automation.h"
#include "esphome/core/alloc_helpers.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <string>
#include <vector>

#ifdef USE_STORAGE_REGEX_EXTRACT
// <regex> costs significant flash (~50-100 kB) and stack; it is only compiled in when a
// config actually uses a `regex:` extraction step (codegen sets the define).
#include <regex>
#endif

namespace esphome::storage {

static const char *const AUTOMATION_TAG = "storage.automation";

// These actions are globally available on every node that loads the storage component
// (which every storage device driver AUTO_LOADs) — no per-component preparation needed,
// analogous to how web_server sorting groups work for all components. Paths are full VFS
// paths; routing to the right device happens via StorageRegistry::resolve_path().

// Assign an extracted string to a global variable of any supported type. The value type is
// deduced from the global's value() so one helper covers GlobalsComponent<T>,
// RestoringGlobalsComponent<T> and the string variants alike.
template<typename GlobT> void assign_from_string(GlobT *g, const std::string &s) {
  using T = std::decay_t<decltype(g->value())>;
  if constexpr (std::is_same_v<T, std::string>) {
    g->value() = s;
  } else if constexpr (std::is_same_v<T, bool>) {
    if (str_equals_case_insensitive(s, "true") || str_equals_case_insensitive(s, "on") || s == "1") {
      g->value() = true;
    } else if (str_equals_case_insensitive(s, "false") || str_equals_case_insensitive(s, "off") || s == "0") {
      g->value() = false;
    } else {
      ESP_LOGW(AUTOMATION_TAG, "file_read: '%s' is not a valid bool; global unchanged", s.c_str());
    }
  } else if constexpr (std::is_arithmetic_v<T>) {
    auto v = parse_number<T>(s);
    if (v.has_value()) {
      g->value() = *v;
    } else {
      ESP_LOGW(AUTOMATION_TAG, "file_read: '%s' is not a valid number; global unchanged", s.c_str());
    }
  } else {
    static_assert(std::is_same_v<T, std::string>, "Unsupported global type for storage.file_read to_global");
  }
}

// ---------------------------------------------------------------------------
// Extraction pipeline for storage.file_read
// ---------------------------------------------------------------------------

// Local whitespace trim (no equivalent in core helpers).
inline std::string extract_trim(const std::string &s) {
  size_t b = s.find_first_not_of(" \t\r\n");
  if (b == std::string::npos)
    return "";
  size_t e = s.find_last_not_of(" \t\r\n");
  return s.substr(b, e - b + 1);
}

enum class ExtractStepType : uint8_t {
  LINE,   // pick the Nth line (1-based)
  SPLIT,  // split at a separator string, pick the Nth element (0-based)
  KEY,    // find the first line starting with "<key><separator>", yield the remainder
  TRIM,   // strip leading/trailing whitespace
  REGEX,  // regex_search, yield the given capture group
};

struct ExtractStep {
  ExtractStepType type;
  std::string arg;  // SPLIT: separator, KEY: key, REGEX: pattern
  std::string sep;  // KEY: separator
  int index{0};     // LINE: line number, SPLIT: element index, REGEX: capture group
};

// Applies one step; returns false (with a warning) on structural failure — line/element out
// of range, key not found, regex not matching. An empty extraction result is not a failure.
inline bool apply_extract_step(const ExtractStep &step, std::string &buf) {
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
      ESP_LOGW(AUTOMATION_TAG, "file_read: line %d not found (%d lines)", step.index, current - 1);
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
      ESP_LOGW(AUTOMATION_TAG, "file_read: split element %d not found", step.index);
      return false;
    }
    case ExtractStepType::KEY: {
      const std::string needle = step.arg + step.sep;
      size_t start = 0;
      while (start <= buf.size()) {
        size_t end = buf.find('\n', start);
        if (end == std::string::npos)
          end = buf.size();
        std::string line = buf.substr(start, end - start);
        std::string trimmed = extract_trim(line);
        if (trimmed.rfind(needle, 0) == 0) {
          std::string value = trimmed.substr(needle.size());
          if (!value.empty() && value.back() == '\r')
            value.pop_back();
          buf = std::move(value);
          return true;
        }
        start = end + 1;
      }
      ESP_LOGW(AUTOMATION_TAG, "file_read: key '%s' not found", step.arg.c_str());
      return false;
    }
    case ExtractStepType::TRIM:
      buf = extract_trim(buf);
      return true;
    case ExtractStepType::REGEX: {
#ifdef USE_STORAGE_REGEX_EXTRACT
      // Pattern syntax was validated at config time; construct once per step invocation.
      // ECMAScript grammar (std::regex default).
      std::regex re(step.arg);
      std::smatch m;
      if (!std::regex_search(buf, m, re)) {
        ESP_LOGW(AUTOMATION_TAG, "file_read: regex '%s' did not match", step.arg.c_str());
        return false;
      }
      int group = step.index;
      if (group >= static_cast<int>(m.size())) {
        ESP_LOGW(AUTOMATION_TAG, "file_read: regex group %d does not exist (%zu groups)", group, m.size() - 1);
        return false;
      }
      buf = m[group].str();
      return true;
#else
      ESP_LOGW(AUTOMATION_TAG, "file_read: regex step configured but not compiled in");
      return false;
#endif
    }
  }
  return false;
}

// NOTE: A `json:` extraction step (JSON-pointer based) is planned as a separate follow-up PR —
// it pulls in the json component as a dependency, so it stays out of this baseline set.

// ---------------------------------------------------------------------------
// storage.file_write / storage.file_append
// ---------------------------------------------------------------------------

template<typename... Ts> class FileWriteAction : public Action<Ts...> {
 public:
  explicit FileWriteAction(bool append) : append_(append) {}

  TEMPLATABLE_VALUE(std::string, path)
  TEMPLATABLE_VALUE(std::string, content)
  void set_newline(bool newline) { this->newline_ = newline; }

  void play(const Ts &...x) override {
    std::string path = this->path_.value(x...);
    std::string content = this->content_.value(x...);
    if (this->newline_)
      content += '\n';

    if (global_storage_registry == nullptr) {
      ESP_LOGW(AUTOMATION_TAG, "file_%s: no storage registry", this->append_ ? "append" : "write");
      return;
    }
    const char *rel = nullptr;
    PathStorage *ps = global_storage_registry->resolve_path(path.c_str(), &rel);
    if (ps == nullptr) {
      ESP_LOGW(AUTOMATION_TAG, "file_%s: no storage mounted for '%s'", this->append_ ? "append" : "write",
               path.c_str());
      return;
    }
    // v1 supports filesystem-backed storages; NetworkStorage has no append semantics in its
    // chunk API yet (follow-up).
    if (ps->get_storage_type() != StorageType::FILESYSTEM) {
      ESP_LOGW(AUTOMATION_TAG, "file_%s: '%s' is not on a filesystem storage", this->append_ ? "append" : "write",
               path.c_str());
      return;
    }
    auto *fs = static_cast<FilesystemStorage *>(ps);

    FileHandle *handle = nullptr;
    StorageError err = fs->open(rel, handle, this->append_ ? OpenMode::APPEND : OpenMode::WRITE);
    if (err != StorageError::OK) {
      ESP_LOGW(AUTOMATION_TAG, "file_%s: open '%s' failed (%d)", this->append_ ? "append" : "write", path.c_str(),
               static_cast<int>(err));
      return;
    }
    size_t written = 0;
    err = fs->write(handle, reinterpret_cast<const uint8_t *>(content.data()), content.size(), &written);
    // Close errors must surface: FATFS-backed drivers flush on close (see copy() contract).
    StorageError close_err = fs->close(handle);
    if (err == StorageError::OK)
      err = close_err;
    if (err != StorageError::OK || written != content.size()) {
      ESP_LOGW(AUTOMATION_TAG, "file_%s: writing '%s' failed (%d, %zu/%zu bytes)", this->append_ ? "append" : "write",
               path.c_str(), static_cast<int>(err), written, content.size());
    }
  }

 protected:
  bool append_;
  bool newline_{false};
};

// ---------------------------------------------------------------------------
// storage.file_read
// ---------------------------------------------------------------------------

template<typename... Ts> class FileReadAction : public Action<Ts...> {
 public:
  TEMPLATABLE_VALUE(std::string, path)

  void add_step(ExtractStepType type, std::string arg, std::string sep, int index) {
    this->steps_.push_back(ExtractStep{type, std::move(arg), std::move(sep), index});
  }
  void set_global_setter(std::function<void(const std::string &)> setter) { this->setter_ = std::move(setter); }
  Trigger<std::string> *get_value_trigger() { return &this->value_trigger_; }

  void play(const Ts &...x) override {
    std::string path = this->path_.value(x...);
    if (global_storage_registry == nullptr) {
      ESP_LOGW(AUTOMATION_TAG, "file_read: no storage registry");
      return;
    }
    const char *rel = nullptr;
    PathStorage *ps = global_storage_registry->resolve_path(path.c_str(), &rel);
    if (ps == nullptr) {
      ESP_LOGW(AUTOMATION_TAG, "file_read: no storage mounted for '%s'", path.c_str());
      return;
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
      ESP_LOGW(AUTOMATION_TAG, "file_read: reading '%s' failed (%d)", path.c_str(), static_cast<int>(err));
      return;
    }

    std::string value(reinterpret_cast<const char *>(buf.get()), size);
    for (const auto &step : this->steps_) {
      if (!apply_extract_step(step, value))
        return;  // step already logged; global untouched, no trigger
    }

    if (this->setter_)
      this->setter_(value);
    this->value_trigger_.trigger(value);
  }

 protected:
  std::vector<ExtractStep> steps_;
  std::function<void(const std::string &)> setter_;
  Trigger<std::string> value_trigger_;
};

}  // namespace esphome::storage
