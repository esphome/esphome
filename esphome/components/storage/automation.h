#pragma once

// WARNING: This component is EXPERIMENTAL. The API may change at any time
// without following the normal breaking changes policy. Use at your own risk.

// defines.h MUST be the first include: this header has define-gated action classes, and main.cpp
// placement-news them into sizeof()-sized static buffers -- a TU parsing these declarations with a
// different define state gets a different class size (ODR violation, boot crash). Including
// defines.h first resolves the gates from the generated defines, not from whatever an earlier
// include happened to set.
#include "esphome/core/defines.h"

#include "storage.h"
#ifdef USE_STORAGE_WORKER
#include "storage_worker.h"  // global_storage_worker -- async file/raw ops (fire-and-forget actions)
#endif
#include "esphome/core/alloc_helpers.h"
#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"

#include <optional>
#include <string>
#include <type_traits>
#include <vector>
#ifdef USE_STORAGE_REGEX_EXTRACT
#include <regex>  // ExtractStep::compiled_re -- REGEX patterns are compiled once, at construction
#endif

namespace esphome::storage {

// These actions are globally available on every node that loads the storage component
// (which every storage device driver AUTO_LOADs) -- no per-component preparation needed,
// analogous to how web_server sorting groups work for all components. Paths are full VFS
// paths; routing to the right device happens via StorageRegistry::resolve_path().

// Logging helpers implemented in automation.cpp (log macros stay out of headers). printf-style args
// from YAML flow verbatim through C varargs into the generated snprintf; a std::string there is UB
// (a non-POD through "..." renders garbage or corrupts memory, only warned by -Wformat). Normalizing
// every arg through this overload set means no config needs a manual .c_str(), and args that have
// one pass through unchanged.
inline const char *printf_arg(const std::string &s) { return s.c_str(); }
template<typename T> inline T printf_arg(T v) {
  static_assert(std::is_scalar_v<T>, "storage printf args must be scalars or strings; add .c_str() or convert first");
  return v;
}

void warn_invalid_bool(const std::string &s);
void warn_invalid_number(const std::string &s);

// Assign an extracted string to a global variable of any supported type. The value type is
// deduced from the global's value() so one helper covers GlobalsComponent<T>,
// RestoringGlobalsComponent<T> and the string variants alike.
// Returns true if the value was assigned, false if the text did not parse for the global's type
// (the global is then left untouched -- FileReadAction uses this to skip firing on_value).
template<typename GlobT> bool assign_from_string(GlobT *g, const std::string &s) {
  using T = std::decay_t<decltype(g->value())>;
  if constexpr (std::is_same_v<T, std::string>) {
    g->value() = s;
    return true;
  } else if constexpr (std::is_same_v<T, bool>) {
    if (str_equals_case_insensitive(s, "true") || str_equals_case_insensitive(s, "on") || s == "1") {
      g->value() = true;
      return true;
    } else if (str_equals_case_insensitive(s, "false") || str_equals_case_insensitive(s, "off") || s == "0") {
      g->value() = false;
      return true;
    }
    warn_invalid_bool(s);
    return false;
  } else if constexpr (std::is_arithmetic_v<T>) {
    static_assert(std::is_integral_v<T> || std::is_same_v<T, float>,
                  "storage.file_read to_global supports integer and float globals; double is not "
                  "supported (parse_number has no double overload)");
    auto v = parse_number<T>(s);
    if (v.has_value()) {
      g->value() = *v;
      return true;
    }
    warn_invalid_number(s);
    return false;
  } else {
    static_assert(std::is_same_v<T, std::string>, "Unsupported global type for storage.file_read to_global");
    return false;
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
  ExtractStep(ExtractStepType type, std::string arg, std::string sep, int index)
      : type(type), arg(std::move(arg)), sep(std::move(sep)), index(index) {
#ifdef USE_STORAGE_REGEX_EXTRACT
    // Config-time validation already guaranteed a std::regex-parseable ECMAScript pattern, so
    // compile it once here -- apply_extract_step() then never recompiles per play().
    if (this->type == ExtractStepType::REGEX)
      this->compiled_re = std::regex(this->arg);
#endif
  }
  ExtractStepType type;
  std::string arg;  // SPLIT: separator, KEY: key, REGEX: pattern
  std::string sep;  // KEY: separator
  int index{0};     // LINE: line number, SPLIT: element index, REGEX: capture group
#ifdef USE_STORAGE_REGEX_EXTRACT
  std::regex compiled_re;  // REGEX: pattern compiled once at construction (see the constructor)
#endif
};

// Whitespace trim (no equivalent in core helpers).
std::string extract_trim(const std::string &s);

// Applies one step; returns false (with a warning) on structural failure -- line/element out
// of range, key not found, regex not matching. An empty extraction result is not a failure.
bool apply_extract_step(const ExtractStep &step, std::string &buf);

// ===========================================================================
// Blocking contract for the storage actions (read before "fixing" an action to be async).
//
// Two deliberate kinds, split by WHERE the data lives: content already a RAM value stays
// synchronous; content that is (or becomes) a file streams through the async worker.
//
//   ASYNC (fire-and-forget + on_complete trigger; on the worker, never blocks the loop):
//     - file_copy / file_move          (file/tree <-> file/tree, streamed)
//     - raw_read  with to_file         (device -> file, streamed)
//     - raw_write with from_file       (file -> device, streamed)
//     - raw_erase                      (sliced one geometry step per pass)
//   Potentially large data; the worker streams them through one fixed chunk buffer (never a
//   whole-file/whole-image RAM buffer).
//
//   SYNCHRONOUS (small content already a RAM value; blocks only for that small write):
//     - file_write / file_append       (writes a std::string from the automation)
//     - raw_read  into on_value        (returns a std::vector -- bytes land in RAM)
//     - raw_write from inline data     (a flash/lambda byte array)
//     - file_delete / recursive delete (removes directory entries -- moves no bulk data)
//   Not routed through the worker: the payload is a small in-RAM value, nothing to stream, and a
//   worker job would add round-trips and a pool slot for no benefit. Blocking is bounded by the
//   payload size, which the author chose by constructing the value. The large-content counterpart
//   exists as a separate action (file_copy, or raw_write with from_file). On a slow NETWORK storage
//   even a small write can take a while (the round-trip, not the size) -- a property of the medium
//   the author picked, not a reason to go async. A genuinely non-blocking small write would want a
//   dedicated RAM->file worker job, not the stream API -- out of scope until a real need appears.
//   file_delete is synchronous for a second reason: it moves no bulk data (each step is a
//   directory-entry unlink/rmdir, short even over NFS) and synchronous is safer for a destructive
//   op. It fires on_complete (error text, empty = success): the delete is refused while the worker
//   task streams on the same volume, and a recursive delete is NOT rolled back on a mid-walk failure
//   -- so "gone before the next action" holds only when on_complete reports success. Sequence a
//   "recreate at the same path" from on_complete, not by ordering.
//   file_exists is a condition, not an action: while the worker task streams on the same volume it
//   returns false (a concurrent stat() would break per-instance serialization), so an existing file
//   reads as absent for that window, logged at WARN.
//
//   CONTROL-PLANE (moves no bulk data, but one driver call whose duration the medium bounds, not
//   the author):
//     - format          ASYNC. format() (f_mkfs etc.) is one blocking call that can run for seconds
//                       on a large card. Worker-routed because its bound is the medium's: a task-safe
//                       medium formats on the worker task (main loop free, watchdog-safe), otherwise
//                       a loop() step waits it out. Fires on_complete like the streaming actions.
//     - mount           ASYNC. One connect()/probe, bounded by the driver -- an NFS connect() to a
//                       dead server blocks for the socket timeout -- so worker-routed like format.
//                       play() returns before the mount completes and fires on_complete when the
//                       connect finishes. Sequence a dependent action from on_complete, not after.
//     - unmount         SYNCHRONOUS. One disconnect(). Stays synchronous: drivers quiesce the worker
//                       inside unmount() and that drain is owned by the main loop, so routing it to
//                       the worker task would deadlock the drain.
// ===========================================================================

// Non-template workers for the actions below -- all error logging lives in the .cpp.
void perform_mount(PathStorage *target, bool mount, Trigger<std::string> *on_complete);
void perform_format_async(Storage *target, Trigger<std::string> *on_complete);
// Returns the error so the no-worker fallback in perform_file_copy_async() can report it --
// on_complete's contract is "error text, empty = success", which a void return cannot honour.
// The raw helpers below already work this way.
StorageError perform_file_copy(const std::string &from, const std::string &to, bool is_move);
// Async variant used by FileCopyAction: submits to the worker (or, if the worker is not
// compiled in, runs the blocking helper and fires the trigger inline). `on_complete` receives
// the error text (empty = success) and may be nullptr.
void perform_file_copy_async(const std::string &from, const std::string &to, bool is_move,
                             Trigger<std::string> *on_complete);
StorageError perform_file_delete(const std::string &path, bool recursive);
StorageError check_file_exists(const std::string &path);
StorageError perform_file_write(const std::string &path, std::string content, bool append, bool newline);
bool perform_file_read(const std::string &path, const FixedVector<ExtractStep> &steps, std::string &out,
                       std::string &error);

// ---------------------------------------------------------------------------
// storage.file_write / storage.file_append
// ---------------------------------------------------------------------------

// SYNCHRONOUS by design -- see the blocking contract above. `content` is a std::string from
// the automation (a small in-RAM value), so there is nothing to stream: writing a big file is
// file_copy's / raw_write from_file's job, not this one.
template<typename... Ts> class FileWriteAction : public Action<Ts...> {
 public:
  explicit FileWriteAction(bool append) : append_(append) {}

  TEMPLATABLE_VALUE(std::string, path)
  TEMPLATABLE_VALUE(std::optional<std::string>, content)
  void set_newline(bool newline) { this->newline_ = newline; }

  Trigger<std::string> *get_complete_trigger() { return &this->complete_trigger_; }

  void play(const Ts &...x) override {
    std::optional<std::string> content = this->content_.value(x...);
    if (!content.has_value()) {
      // The format:/args: lambda could not render the line (encoding failure or over-long). Abort
      // before any open -- write_file() opens with OpenMode::WRITE and would truncate the target to
      // empty here -- and report the error instead of the empty "success" string.
      this->complete_trigger_.trigger(std::string("format failed"));
      return;
    }
    StorageError err = perform_file_write(this->path_.value(x...), std::move(*content), this->append_, this->newline_);
    this->complete_trigger_.trigger(err == StorageError::OK ? std::string() : std::string(error_to_string(err)));
  }

 protected:
  bool append_;
  bool newline_{false};
  Trigger<std::string> complete_trigger_;
};

// ---------------------------------------------------------------------------
// storage.file_read
// ---------------------------------------------------------------------------

template<typename... Ts> class FileReadAction : public Action<Ts...> {
 public:
  TEMPLATABLE_VALUE(std::string, path)

  // Capacity is known at codegen (the number of extract steps); steps_ is a FixedVector, so
  // this init() must run before the add_step() calls.
  void reserve_steps(size_t n) { this->steps_.init(n); }
  void add_step(ExtractStepType type, const std::string &arg, const std::string &sep, int index) {
    this->steps_.push_back(ExtractStep{type, arg, sep, index});
  }
  void set_global_setter(bool (*setter)(const std::string &)) { this->setter_ = setter; }
  Trigger<std::string> *get_value_trigger() { return &this->value_trigger_; }
  Trigger<std::string> *get_error_trigger() { return &this->error_trigger_; }

  void play(const Ts &...x) override {
    std::string value, error;
    if (!perform_file_read(this->path_.value(x...), this->steps_, value, error)) {
      // Read or an extract step failed: global untouched, on_value not fired -- surface the cause.
      this->error_trigger_.trigger(error);
      return;
    }
    // A parse failure inside the setter leaves the global untouched; skip on_value (no fresh value
    // arrived) and report why via on_error instead. The echoed value is bounded so a large read
    // cannot bloat the error string.
    if (this->setter_ && !this->setter_(value)) {
      std::string shown = value.size() > 32 ? value.substr(0, 32) + "..." : value;
      this->error_trigger_.trigger("read '" + shown + "' but it did not parse for the target global");
      return;
    }
    this->value_trigger_.trigger(value);
  }

 protected:
  FixedVector<ExtractStep> steps_;
  bool (*setter_)(const std::string &){nullptr};
  Trigger<std::string> value_trigger_;
  Trigger<std::string> error_trigger_;
};

// ---------------------------------------------------------------------------
// storage.file_copy / storage.file_move (move doubles as rename -- see .cpp)
// ---------------------------------------------------------------------------

// Fire-and-forget: play() submits the copy/move to the async worker and returns immediately, so the
// sequence continues without blocking the loop. on_complete fires later from the worker's completion
// callback (main loop) with the error text -- empty on success. A same-storage move still takes the
// rename() fast path in the worker's pre-phase. Falls back to the synchronous helper only when the
// worker was not compiled in (no path driver); that path blocks.
template<typename... Ts> class FileCopyAction : public Action<Ts...> {
 public:
  explicit FileCopyAction(bool is_move) : is_move_(is_move) {}

  TEMPLATABLE_VALUE(std::string, from)
  TEMPLATABLE_VALUE(std::string, to)

  Trigger<std::string> *get_complete_trigger() { return &this->complete_trigger_; }

  void play(const Ts &...x) override {
    perform_file_copy_async(this->from_.value(x...), this->to_.value(x...), this->is_move_, &this->complete_trigger_);
  }

 protected:
  bool is_move_;
  Trigger<std::string> complete_trigger_;
};

#ifdef USE_STORAGE_RAW_ACTIONS
// ---------------------------------------------------------------------------
// storage.raw_read / storage.raw_write / storage.raw_erase
// ---------------------------------------------------------------------------
// Address-based access to a RawStorage device (NOR flash, FRAM, EEPROM). What a medium accepts is
// its own business -- these helpers ask get_raw_geometry() instead of assuming flash semantics, and
// pass erase()'s verdict (NOT_SUPPORTED on erase-less media, INVALID_ARGS on an unaligned range)
// straight to the log.
//
// Blocking: see the contract block above. The file-based paths (raw_read to_file, raw_write
// from_file, raw_erase) run ASYNC on the worker; raw_read into on_value and raw_write from inline
// data are SYNCHRONOUS. The sync perform_raw_* helpers below back the small-content paths and the
// no-worker fallback; the perform_*_async ones back the rest.

// Reads [address, address+size) into `out`. Returns a non-OK StorageError (already logged) on any
// failure; `out` is left empty then, so a trigger never fires with half a result.
StorageError perform_raw_read(RawStorage *device, uint64_t address, size_t size, std::vector<uint8_t> &out);
// Same read, but written to a file on a mounted storage; the range is buffered in RAM as one block,
// not streamed. size == 0 means "to the end of the device".
StorageError perform_raw_read_to_file(RawStorage *device, uint64_t address, uint64_t size, const std::string &path);
// Writes `data` at `address`. erase_first erases the covering sectors beforehand -- required on
// media reporting RAW_WRITE_NEEDS_ERASE, and destructive to anything else sharing those sectors.
StorageError perform_raw_write(RawStorage *device, uint64_t address, const uint8_t *data, size_t len, bool erase_first);
StorageError perform_raw_write_from_file(RawStorage *device, uint64_t address, const std::string &path,
                                         bool erase_first);
// Erases [address, address+size), or the whole device when `all` is set. Returns the result so
// the async wrapper's no-worker fallback can propagate a failure to on_complete.
StorageError perform_raw_erase(RawStorage *device, uint64_t address, uint64_t size, bool all);

// Async variants used by the actions: submit to the worker (streaming, no whole-image RAM
// buffer) and fire `on_complete` (error text, empty = success) from the completion callback.
// Fall back to the blocking helpers only when the worker isn't compiled in. `on_complete` may
// be nullptr.
void perform_raw_read_to_file_async(RawStorage *device, uint64_t address, uint64_t size, const std::string &path,
                                    Trigger<std::string> *on_complete);
void perform_raw_write_from_file_async(RawStorage *device, uint64_t address, const std::string &path, bool erase_first,
                                       Trigger<std::string> *on_complete);
void perform_raw_erase_async(RawStorage *device, uint64_t address, uint64_t size, bool all,
                             Trigger<std::string> *on_complete, bool force_sliced = false);

template<typename... Ts> class RawReadAction : public Action<Ts...> {
 public:
  explicit RawReadAction(RawStorage *device) : device_(device) {}

  TEMPLATABLE_VALUE(uint32_t, address)
  TEMPLATABLE_VALUE(uint32_t, size)
  TEMPLATABLE_VALUE(std::string, to_file)

  void set_has_to_file(bool has_to_file) { this->has_to_file_ = has_to_file; }
  Trigger<std::vector<uint8_t>> *get_value_trigger() { return &this->value_trigger_; }
  Trigger<std::string> *get_complete_trigger() { return &this->complete_trigger_; }

  void play(const Ts &...x) override {
    const uint32_t address = this->address_.value(x...);
    const uint32_t size = this->size_.value(x...);
    if (this->has_to_file_) {
      // Streams device -> file on the worker (no whole-image RAM buffer); on_complete fires
      // with the error text (empty = success) when it lands.
      perform_raw_read_to_file_async(this->device_, address, size, this->to_file_.value(x...),
                                     &this->complete_trigger_);
      return;
    }
    // Read-into-variable: this returns the bytes in a std::vector (RAM), so it stays
    // synchronous and is meant for small reads. Large content should use to_file instead.
    std::vector<uint8_t> data;
    StorageError err = perform_raw_read(this->device_, address, size, data);
    if (err != StorageError::OK) {
      // Failure was invisible before -- on_value simply never fired. Report it on on_complete
      // (now allowed on the sync path too), matching the file actions.
      this->complete_trigger_.trigger(std::string(error_to_string(err)));
      return;
    }
    this->value_trigger_.trigger(data);
    this->complete_trigger_.trigger(std::string());  // success, empty error text
  }

 protected:
  RawStorage *device_;
  bool has_to_file_{false};
  Trigger<std::vector<uint8_t>> value_trigger_;
  Trigger<std::string> complete_trigger_;
};

template<typename... Ts> class RawWriteAction : public Action<Ts...> {
 public:
  explicit RawWriteAction(RawStorage *device) : device_(device) {}

  TEMPLATABLE_VALUE(uint32_t, address)
  TEMPLATABLE_VALUE(std::string, from_file)

  // Data sources, mirroring uart.write: a static array stays in flash, a lambda is called per
  // play(). from_file reads the file into RAM first (guard-railed by the transfer limit).
  void set_data_template(std::vector<uint8_t> (*func)(Ts...)) {
    this->code_.func = func;
    this->len_ = -1;  // sentinel: template mode
  }
  void set_data_static(const uint8_t *data, size_t len) {
    this->code_.data = data;
    this->len_ = static_cast<int>(len);
  }
  void set_has_from_file(bool has_from_file) { this->has_from_file_ = has_from_file; }
  void set_erase_first(bool erase_first) { this->erase_first_ = erase_first; }
  Trigger<std::string> *get_complete_trigger() { return &this->complete_trigger_; }

  void play(const Ts &...x) override {
    const uint32_t address = this->address_.value(x...);
    if (this->has_from_file_) {
      // Streams file -> device on the worker (no whole-file RAM buffer -- this used to read the
      // entire file into RAM first, which capped it at the transfer limit and could not do a
      // 20 MB image). on_complete fires with the error text (empty = success).
      perform_raw_write_from_file_async(this->device_, address, this->from_file_.value(x...), this->erase_first_,
                                        &this->complete_trigger_);
      return;
    }
    if (this->len_ >= 0) {
      StorageError err = perform_raw_write(this->device_, address, this->code_.data, static_cast<size_t>(this->len_),
                                           this->erase_first_);
      this->complete_trigger_.trigger(err == StorageError::OK ? std::string() : std::string(error_to_string(err)));
      return;
    }
    std::vector<uint8_t> data = (*this->code_.func)(x...);
    StorageError err = perform_raw_write(this->device_, address, data.data(), data.size(), this->erase_first_);
    this->complete_trigger_.trigger(err == StorageError::OK ? std::string() : std::string(error_to_string(err)));
  }

 protected:
  RawStorage *device_;
  union {
    const uint8_t *data;
    std::vector<uint8_t> (*func)(Ts...);
  } code_{nullptr};
  int len_{-1};
  bool has_from_file_{false};
  bool erase_first_{false};
  Trigger<std::string> complete_trigger_;
};

template<typename... Ts> class RawEraseAction : public Action<Ts...> {
 public:
  explicit RawEraseAction(RawStorage *device) : device_(device) {}

  TEMPLATABLE_VALUE(uint32_t, address)
  TEMPLATABLE_VALUE(uint32_t, size)

  void set_all(bool all) { this->all_ = all; }
  void set_force_sliced_erase(bool force_sliced) { this->force_sliced_erase_ = force_sliced; }
  Trigger<std::string> *get_complete_trigger() { return &this->complete_trigger_; }

  void play(const Ts &...x) override {
    // Sliced on the worker (one geometry step per pass) so a chip-scale erase never freezes
    // the loop; on_complete fires with the error text (empty = success). force_sliced_erase
    // suppresses the whole-chip fast path even where it would be eligible (see run_raw_chunk_).
    perform_raw_erase_async(this->device_, this->address_.value(x...), this->size_.value(x...), this->all_,
                            &this->complete_trigger_, this->force_sliced_erase_);
  }

 protected:
  RawStorage *device_;
  bool all_{false};
  bool force_sliced_erase_{false};
  Trigger<std::string> complete_trigger_;
};

#endif  // USE_STORAGE_RAW_ACTIONS

// ---------------------------------------------------------------------------
// storage.file_delete
// ---------------------------------------------------------------------------

template<typename... Ts> class FileDeleteAction : public Action<Ts...> {
 public:
  TEMPLATABLE_VALUE(std::string, path)
  void set_recursive(bool recursive) { this->recursive_ = recursive; }

  Trigger<std::string> *get_complete_trigger() { return &this->complete_trigger_; }

  void play(const Ts &...x) override {
    StorageError err = perform_file_delete(this->path_.value(x...), this->recursive_);
    this->complete_trigger_.trigger(err == StorageError::OK ? std::string() : std::string(error_to_string(err)));
  }

 protected:
  bool recursive_{false};
  Trigger<std::string> complete_trigger_;
};

// ---------------------------------------------------------------------------
// storage.file_exists condition
// ---------------------------------------------------------------------------

template<typename... Ts> class FileExistsCondition : public Condition<Ts...> {
 public:
  TEMPLATABLE_VALUE(std::string, path)

  bool check(const Ts &...x) override {
    // Only a clean NOT_FOUND means "absent". Any other non-OK is a not-ready or faulted medium, not
    // proof the file is gone, so read it as present (true): a guard-then-write must not overwrite a
    // file that is merely temporarily unavailable. Use storage.stat to branch on the error itself.
    return check_file_exists(this->path_.value(x...)) != StorageError::NOT_FOUND;
  }
};

// ---------------------------------------------------------------------------
// storage.stat action -- three-way existence with an explicit error path.
// A Condition can only answer yes/no, which cannot distinguish "absent" from "could not check".
// This action fires exactly one of on_exists / on_missing / on_error so an automation can tell a
// genuine absence from a not-ready/faulted medium instead of overwriting on the strength of an
// error. check_file_exists() also logs the failure cause, so on_error is additive, never the only
// trace.
// ---------------------------------------------------------------------------

template<typename... Ts> class FileStatAction : public Action<Ts...> {
 public:
  TEMPLATABLE_VALUE(std::string, path)

  Trigger<> *get_exists_trigger() { return &this->exists_trigger_; }
  Trigger<> *get_missing_trigger() { return &this->missing_trigger_; }
  Trigger<std::string> *get_error_trigger() { return &this->error_trigger_; }

  void play(const Ts &...x) override {
    StorageError err = check_file_exists(this->path_.value(x...));
    if (err == StorageError::OK) {
      this->exists_trigger_.trigger();
    } else if (err == StorageError::NOT_FOUND) {
      this->missing_trigger_.trigger();
    } else {
      this->error_trigger_.trigger(std::string(error_to_string(err)));
    }
  }

 protected:
  Trigger<> exists_trigger_;
  Trigger<> missing_trigger_;
  Trigger<std::string> error_trigger_;
};

// ---------------------------------------------------------------------------
// storage.mount / storage.unmount -- target must opt in via MountableStorage
// (validated at YAML time through the codegen class hierarchy)
// ---------------------------------------------------------------------------

template<typename... Ts> class MountAction : public Action<Ts...> {
 public:
  // Takes the PathStorage side (codegen passes the concrete driver, which is both): the worker
  // routing needs it, and perform_mount() derives MountableStorage via as_mountable() -- the
  // same target the worker's async_mount() expects.
  explicit MountAction(PathStorage *target, bool mount) : target_(target), mount_(mount) {}
  Trigger<std::string> *get_complete_trigger() { return &this->complete_trigger_; }

  // mount is fire-and-forget like format: play() submits and returns; on_complete fires (error
  // text, empty = success) when the connect/probe finishes. unmount runs synchronously and still
  // fires on_complete on return.
  void play(const Ts &...x) override { perform_mount(this->target_, this->mount_, &this->complete_trigger_); }

 protected:
  PathStorage *target_;
  bool mount_;
  Trigger<std::string> complete_trigger_;
};

template<typename... Ts> class FormatAction : public Action<Ts...> {
 public:
  explicit FormatAction(Storage *target) : target_(target) {}
  Trigger<std::string> *get_complete_trigger() { return &this->complete_trigger_; }

  // Fire-and-forget like the streaming actions: play() submits the worker job and returns;
  // the on_complete trigger fires (error text, empty = success) when the format finishes.
  void play(const Ts &...x) override { perform_format_async(this->target_, &this->complete_trigger_); }

 protected:
  Storage *target_;
  Trigger<std::string> complete_trigger_;
};

}  // namespace esphome::storage
