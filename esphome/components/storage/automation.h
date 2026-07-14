#pragma once

#include "storage.h"
#if defined(USE_STORAGE_PREFERENCES) && defined(USE_ESP32)
#include "preferences_backup.h"  // PrefSelection — file scope, NOT inside the namespace below
#endif
#include "esphome/core/alloc_helpers.h"
#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"

#include <string>
#include <vector>

namespace esphome::storage {

// These actions are globally available on every node that loads the storage component
// (which every storage device driver AUTO_LOADs) — no per-component preparation needed,
// analogous to how web_server sorting groups work for all components. Paths are full VFS
// paths; routing to the right device happens via StorageRegistry::resolve_path().

// Logging helpers implemented in automation.cpp (log macros must stay out of headers).
void warn_invalid_bool(const std::string &s);
void warn_invalid_number(const std::string &s);

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
      warn_invalid_bool(s);
    }
  } else if constexpr (std::is_arithmetic_v<T>) {
    auto v = parse_number<T>(s);
    if (v.has_value()) {
      g->value() = *v;
    } else {
      warn_invalid_number(s);
    }
  } else {
    static_assert(std::is_same_v<T, std::string>, "Unsupported global type for storage.file_read to_global");
  }
}

// ---------------------------------------------------------------------------
// Extraction pipeline for storage.file_read (implementation in automation.cpp)
// ---------------------------------------------------------------------------

enum class ExtractStepType : uint8_t {
  LINE,   // pick the Nth line (1-based)
  SPLIT,  // split at a separator string, pick the Nth element (0-based)
  JSON,   // resolve a '/'-separated pointer (object keys, array indices) in a JSON document
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

// Whitespace trim (no equivalent in core helpers).
std::string extract_trim(const std::string &s);

// Applies one step; returns false (with a warning) on structural failure — line/element out
// of range, key not found, regex not matching. An empty extraction result is not a failure.
bool apply_extract_step(const ExtractStep &step, std::string &buf);

// Non-template workers for the actions below — all error logging lives in the .cpp.
void perform_mount(MountableStorage *target, bool mount);
void perform_file_copy(const std::string &from, const std::string &to, bool is_move);
void perform_file_delete(const std::string &path, bool recursive);
bool check_file_exists(const std::string &path);
void perform_file_write(const std::string &path, std::string content, bool append, bool newline);
bool perform_file_read(const std::string &path, const std::vector<ExtractStep> &steps, std::string &out);

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
    perform_file_write(this->path_.value(x...), this->content_.value(x...), this->append_, this->newline_);
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
    std::string value;
    if (!perform_file_read(this->path_.value(x...), this->steps_, value))
      return;  // already logged; global untouched, no trigger
    if (this->setter_)
      this->setter_(value);
    this->value_trigger_.trigger(value);
  }

 protected:
  std::vector<ExtractStep> steps_;
  std::function<void(const std::string &)> setter_;
  Trigger<std::string> value_trigger_;
};

// ---------------------------------------------------------------------------
// storage.file_copy / storage.file_move (move doubles as rename — see .cpp)
// ---------------------------------------------------------------------------

template<typename... Ts> class FileCopyAction : public Action<Ts...> {
 public:
  explicit FileCopyAction(bool is_move) : is_move_(is_move) {}

  TEMPLATABLE_VALUE(std::string, from)
  TEMPLATABLE_VALUE(std::string, to)

  void play(const Ts &...x) override {
    perform_file_copy(this->from_.value(x...), this->to_.value(x...), this->is_move_);
  }

 protected:
  bool is_move_;
};

// ---------------------------------------------------------------------------
// storage.file_delete
// ---------------------------------------------------------------------------

template<typename... Ts> class FileDeleteAction : public Action<Ts...> {
 public:
  TEMPLATABLE_VALUE(std::string, path)
  void set_recursive(bool recursive) { this->recursive_ = recursive; }

  void play(const Ts &...x) override { perform_file_delete(this->path_.value(x...), this->recursive_); }

 protected:
  bool recursive_{false};
};

// ---------------------------------------------------------------------------
// storage.file_exists condition
// ---------------------------------------------------------------------------

template<typename... Ts> class FileExistsCondition : public Condition<Ts...> {
 public:
  TEMPLATABLE_VALUE(std::string, path)

  bool check(const Ts &...x) override { return check_file_exists(this->path_.value(x...)); }
};

// ---------------------------------------------------------------------------
// storage.mount / storage.unmount — target must opt in via MountableStorage
// (validated at YAML time through the codegen class hierarchy)
// ---------------------------------------------------------------------------

template<typename... Ts> class MountAction : public Action<Ts...> {
 public:
  explicit MountAction(MountableStorage *target, bool mount) : target_(target), mount_(mount) {}

  void play(const Ts &...x) override { perform_mount(this->target_, this->mount_); }

 protected:
  MountableStorage *target_;
  bool mount_;
};


#if defined(USE_STORAGE_PREFERENCES) && defined(USE_ESP32)
// storage.export_preferences / storage.import_preferences — see
// preferences_backup.h. The selection table (name/key/type/count) is
// codegen-baked per action instance from its optional `preferences:` list;
// empty selection = all preferences (hex round-trip, types unknown).
template<typename... Ts> class ExportPreferencesAction : public Action<Ts...> {
 public:
  TEMPLATABLE_VALUE(std::string, path)
  void set_format(const char *format) { this->format_ = format; }
  void set_selection(const PrefSelection *selection, size_t count) {
    this->selection_ = selection;
    this->count_ = count;
  }

  void play(Ts... x) override {
    const std::string path = this->path_.value(x...);
    preferences_export_to_storage(path.c_str(), this->format_, this->selection_, this->count_);
  }

 protected:
  const char *format_{"kv"};
  const PrefSelection *selection_{nullptr};
  size_t count_{0};
};

template<typename... Ts> class ImportPreferencesAction : public Action<Ts...> {
 public:
  TEMPLATABLE_VALUE(std::string, path)
  void set_format(const char *format) { this->format_ = format; }
  void set_reboot(bool reboot) { this->reboot_ = reboot; }
  void set_selection(const PrefSelection *selection, size_t count) {
    this->selection_ = selection;
    this->count_ = count;
  }

  void play(Ts... x) override {
    const std::string path = this->path_.value(x...);
    preferences_import_from_storage(path.c_str(), this->format_, this->reboot_, this->selection_, this->count_);
  }

 protected:
  const char *format_{"kv"};
  bool reboot_{false};
  const PrefSelection *selection_{nullptr};
  size_t count_{0};
};
#endif  // USE_STORAGE_PREFERENCES && USE_ESP32

}  // namespace esphome::storage
