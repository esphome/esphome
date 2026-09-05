#pragma once

// EXPERIMENTAL: API may change without following the breaking-changes policy.

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "storage.h"

// Compiled only when a path-based (filesystem/network) driver is configured; set from codegen
// (request_storage_worker() in __init__.py). A raw-only node never pays for it.
#ifdef USE_STORAGE_WORKER

#include <atomic>
#include <functional>
#include <memory>

// Platform condition here (not codegen) so a task-safe driver on a non-FreeRTOS target degrades to
// loop-sliced instead of failing to compile.
#if defined(USE_ESP32) && defined(USE_STORAGE_WORKER_TASK)
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#endif

namespace esphome::storage {

using CompletionCallback = std::function<void(storage::StorageError)>;

// Max path length copied into each pooled request. Paths are copied at submit time (caller pointers
// may not outlive submission); longer paths are rejected with STORAGE_ERROR_INVALID_ARGS.
static constexpr size_t STORAGE_WORKER_MAX_PATH = STORAGE_PATH_MAX;

enum class RequestOp : uint8_t {
  COPY,
  MOVE,
  // Raw device <-> file transfers, chunked like any transfer (no whole-image RAM buffer). A write's
  // preparatory erase is sliced per pass (run_raw_chunk_) so a chip-scale erase never blocks.
  RAW_READ_TO_FILE,
  RAW_WRITE_FROM_FILE,
  RAW_ERASE,  // sliced erase only
  // Device-vs-file compare, verify_passes times, no write. Same read-back phase a write's verify
  // uses; file is the src side like RAW_WRITE_FROM_FILE.
  RAW_VERIFY_FILE,
  // Whole tree walked by the engine (list/mkdir/copy, and remove each source entry for a move) so
  // task-safe media run it start to finish on the worker task, not stepped by the caller's loop().
  COPY_TREE,
  MOVE_TREE,
  // Filesystem format: one blocking control-plane call (f_mkfs etc.), run on the worker task for
  // task-safe media else loop-sliced. Moves no bytes, opens no handles.
  FORMAT,
  // Mountable-device mount, same shape as FORMAT. Target rides in dst_storage; run resolves it back
  // to MountableStorage via as_mountable().
  MOUNT,
};

// Transitions: FREE -> PENDING (submit) -> RUNNING -> DONE -> FREE (callback delivered on main loop).
// A RUNNING request may be CANCELLED by the per-chunk check or on_storage_unregistered_() (drain in
// .cpp). Both engines call run_chunk_() until DONE: CANCELLED is acted on only in the entry check.
enum class RequestState : uint8_t {
  FREE,
  PENDING,
  RUNNING,
  CANCELLED,
  DONE,
};

// Coarse phase of a raw transfer for status lines (erase -> write -> verify). NONE for transfers
// without phases (plain copy/read) or not running.
enum class TransferPhase : uint8_t {
  NONE,
  ERASE,
  WRITE,
  VERIFY,
};

// Opaque job handle: (generation << 8) | slot_index. max_pending capped at 16, so 8 bits of slot
// suffice; 0 is invalid (generations start at 1).
using TransferJob = uint32_t;
static constexpr TransferJob INVALID_TRANSFER_JOB = 0;

// Encoding kept here so a handle built by a submit funnel and one decoded by get_transfer_status()
// cannot drift apart.
inline TransferJob make_transfer_job(uint32_t generation, size_t slot) {
  return (generation << 8) | static_cast<uint32_t>(slot & 0xFF);
}
inline uint32_t transfer_job_generation(TransferJob job) { return job >> 8; }
inline size_t transfer_job_slot(TransferJob job) { return job & 0xFF; }

// Bounded "<root>[/<sub>][/<name>]" join for the tree walk; false on truncation. Pure string work,
// header-inlined and unit-tested: a silent truncation would copy into the wrong place.
inline bool join_walk_path(char *out, size_t out_size, const char *root, const char *sub, const char *name) {
  int n;
  if (name != nullptr) {
    n = (sub[0] != '\0') ? snprintf(out, out_size, "%s/%s/%s", root, sub, name)
                         : snprintf(out, out_size, "%s/%s", root, name);
  } else {
    n = (sub[0] != '\0') ? snprintf(out, out_size, "%s/%s", root, sub) : snprintf(out, out_size, "%s", root);
  }
  return n > 0 && static_cast<size_t>(n) < out_size;
}

// 64-bit progress counter, atomic only where required. Task builds write on the task and read on the
// main loop (std::atomic<uint64_t>); else main-loop-only, where a plain field avoids __atomic_*_8
// libcalls that fail to link without libatomic (arm-none-eabi). Plain variant mirrors load/store/assign.
#if defined(USE_ESP32) && defined(USE_STORAGE_WORKER_TASK)
using ProgressCounter = std::atomic<uint64_t>;
#else
struct ProgressCounter {
  uint64_t value{0};
  uint64_t load() const { return this->value; }
  void store(uint64_t v) { this->value = v; }
  ProgressCounter &operator=(uint64_t v) {
    this->value = v;
    return *this;
  }
};
#endif

// Snapshot of a transfer's observable state -- see get_transfer_status().
struct TransferStatus {
  // Short basename label for the file in flight (truncated); empty when nothing is in flight.
  static constexpr size_t FILE_LABEL_LEN = 40;

  RequestState state{RequestState::FREE};
  storage::StorageError result{storage::StorageError::STORAGE_ERROR_OK};
  uint64_t bytes_done{0};
  uint64_t bytes_total{0};  // 0 = unknown (indeterminate progress)
  // Coarse raw phase plus multi-pass verify progress. verify_passes is the configured count,
  // verify_pass the 1-based pass running (0 outside verify). Both 0 for non-raw work.
  TransferPhase phase{TransferPhase::NONE};
  uint8_t verify_pass{0};
  uint8_t verify_passes{0};
  // Per-file progress. Mirrors bytes_done/total for a single file; for a tree it is what the tree
  // total cannot be (unknown without a double walk) -- one cheap stat of the current file.
  uint64_t file_done{0};
  uint64_t file_total{0};
  char file[FILE_LABEL_LEN]{};
};

// Tree-walk position, allocated only for COPY_TREE/MOVE_TREE. Kept out of TransferRequest so a pool
// sized for plain transfers does not carry it per slot.
struct TreeWalk {
  // Same bound as copy()/remove_recursive() in storage.cpp so a tree this walk creates stays within
  // what they can handle afterward. One index slot per level, root included -- hence + 1.
  static constexpr size_t MAX_DEPTH = STORAGE_MAX_RECURSION_DEPTH + 1;

  char src_root[STORAGE_WORKER_MAX_PATH]{};
  char dst_root[STORAGE_WORKER_MAX_PATH]{};
  // Position below the roots, "" at the top. src_path/dst_path always hold the file being copied
  // now, so the chunk loop needs no special-casing for trees.
  char sub[STORAGE_WORKER_MAX_PATH]{};
  uint16_t index_stack[MAX_DEPTH]{};
  uint8_t depth{0};
  bool file_in_flight{false};
  bool root_created{false};
  uint32_t files_done{0};
  // Bytes of finished files; the one in flight adds its offset on top, so progress spans the whole
  // tree rather than restarting per file.
  uint64_t bytes_base{0};
};

// Allocated via RAMAllocator (PSRAM-capable), so destroyed and freed the same way.
struct TreeWalkDeleter {
  void operator()(TreeWalk *ptr) const {
    if (ptr != nullptr) {
      ptr->~TreeWalk();
      RAMAllocator<TreeWalk>().deallocate(ptr, 1);
    }
  }
};

struct TransferRequest {
  std::atomic<RequestState> state{RequestState::FREE};

  RequestOp op{RequestOp::COPY};
  storage::PathStorage *src_storage{nullptr};
  storage::PathStorage *dst_storage{nullptr};
  // FORMAT target only: may be Raw/KeyValue storage (not a PathStorage), so it cannot ride in
  // dst_storage. Read only on the op == FORMAT path; stale on non-format slots, never consulted there.
  storage::Storage *format_target{nullptr};
  char src_path[STORAGE_WORKER_MAX_PATH]{};
  char dst_path[STORAGE_WORKER_MAX_PATH]{};

  CompletionCallback callback;
  storage::StorageError result{storage::StorageError::STORAGE_ERROR_OK};

  // Transfer progress, kept here so the loop-sliced engine can resume across loop() iterations and
  // the same fields serve the task path unchanged.
  storage::FileHandle *src_handle{nullptr};
  storage::FileHandle *dst_handle{nullptr};
  uint64_t offset{0};
  bool src_is_fs{false};
  bool dst_is_fs{false};
  bool handles_open{false};

  // Set once dispatch has logged that this request is blocked behind a slot stuck past the drain
  // timeout; avoids re-logging every loop(). Reset when the request is freed.
  bool stuck_warned{false};
  // Submission timestamp: bounds how long NOT_READY from a network storage counts as "connecting"
  // (run_chunk_) before it becomes the final answer.
  uint32_t submitted_ms{0};
  // Set once at submission and never moved (unlike submitted_ms, which check_stalled_ refreshes
  // while deferred): the backstop that times a request out even if the slot ahead never clears.
  uint32_t queued_ms{0};
  bool waiting_logged{false};
  // File API is a pure HTTP -> storage translator: all pre-phase driver I/O (stat, overwrite clear,
  // tree-vs-file decision) runs HERE, in the engine that owns the storages, on the first pass.
  bool overwrite{false};
  bool pre_phase_done{false};
  // Stall watchdog (check_stalled_): refreshed whenever the request moves; one that stops is
  // finished with TIMEOUT so its storages (and everything blocked behind them) come free.
  uint32_t last_progress_ms{0};
  uint64_t progress_mark{0};
  // Why the CANCELLED entry check finishes the request: only the running engine may finish a RUNNING
  // request; others mark it CANCELLED. NOT_READY for hotplug, TIMEOUT for the stall watchdog.
  storage::StorageError cancel_result{storage::StorageError::STORAGE_ERROR_NOT_READY};
  // Raw ops only. raw_address is the transfer window offset; the erase cursor walks
  // [raw_erase_pos, raw_erase_end) one geometry step per pass. The device side is raw_device,
  // deliberately part of overlaps_active_'s contention checks.
  storage::RawStorage *raw_device{nullptr};
  uint64_t raw_address{0};
  uint64_t raw_erase_pos{0};
  uint64_t raw_erase_end{0};
  // Opt out of the whole-chip fast path: true walks a full-span erase block by block (the loop
  // engine's path) instead of the single blocking erase(0, capacity). Consulted only in run_raw_chunk_.
  bool force_sliced_erase{false};
  // Verify phase for RAW_WRITE_FROM_FILE. verify_passes = read-back-and-compare count after the write
  // (0 = none); verify_pass_done counts finished passes; verifying flags the read-back phase. Reuses
  // chunk_buf: device into buffer, then file in small slices with memcmp -- no second buffer.
  uint8_t verify_passes{0};
  uint8_t verify_pass_done{0};
  std::atomic<bool> verifying{false};
  // Set around a blocking whole-chip erase(0, capacity), which busy-waits tens of seconds without
  // advancing bytes_done; check_stalled_ skips a RUNNING request while set. Task-only, bounded by the
  // driver's wait_ready. std::atomic<bool> is lock-free everywhere (unlike the 64-bit counters).
  std::atomic<bool> blocking_erase_active{false};
  // Set when the task dequeues this request. The stall watchdog skips a task-dispatched request still
  // queued (RUNNING but not started), so a long transfer ahead does not time it out before it runs.
  std::atomic<bool> task_started_{false};
  // Set by wait_for_network_ready_() when a chunk stayed RUNNING waiting for a network storage; the
  // task loop paces its retry on it. Reset at the top of each run_chunk_().
  bool network_waiting{false};

  // Observable progress (get_transfer_status()): bytes_done advanced on whichever engine runs it, read
  // concurrently by the main loop (atomic in task builds only). bytes_total stat()ed once, 0 = unknown
  // (rename fast path, failed stat, any tree); for a tree bytes_done counts the whole thing.
  ProgressCounter bytes_done{};
  ProgressCounter bytes_total{};
  // Progress of the file in flight, same concurrency as above. Equals bytes_done/total for a single
  // file; for a tree it is what bytes_total cannot provide -- file_total stat()ed per file, both 0
  // between files.
  ProgressCounter file_done{};
  ProgressCounter file_total{};
  // Bumped on every slot claim so a recycled slot invalidates stale TransferJob handles (never 0).
  // Main-loop-only.
  uint32_t generation{0};

  // COPY_TREE/MOVE_TREE only; owns the walk position for the request's duration.
  std::unique_ptr<TreeWalk, TreeWalkDeleter> tree;
};

// Streaming requests -- chunked read/write against externally-driven data (HTTP upload in, response
// out), vs async_copy()/move() which drive both ends. Separate pool/state machine: the caller
// supplies (WRITE) or consumes (READ) one chunk at a time. Same worker/task/transparency contract.
enum class StreamOp : uint8_t {
  WRITE,  // caller pushes chunks in; storage receives them (e.g. HTTP upload -> file)
  READ,   // caller requests chunks out; storage supplies them (e.g. file -> HTTP download)
};

// Transitions: FREE -> OPENING -> IDLE -> WRITING/READING -> IDLE -> ... -> CLOSING -> DONE -> FREE.
// Unlike TransferRequest, IDLE can persist indefinitely between chunks -- no auto-advance, the stream
// waits for the caller to supply/consume the next chunk.
enum class StreamState : uint8_t {
  FREE,
  OPENING,
  IDLE,
  WRITING,
  READING,
  CLOSING,
  CANCELLED,
  DONE,
  // Cursor control between chunks -- IDLE-only single steps, like READING.
  SEEKING,
  TELLING,
};

// Opaque handle, valid only for the stream's lifetime (begin_* through end_*/DONE). The generation
// makes that enforceable: bumped on every slot claim, so a handle held past end_* stops matching once
// the slot is reused and the call is refused. TransferJob does the same.
struct StreamHandle {
  size_t index;
  uint32_t generation;
};

// One pooled stream. Fixed-size slot, never allocates (bar the std::function callback). Pool is a
// FixedVector sized to max_streams at codegen (separate from max_pending: streams are longer-lived).
struct StreamRequest {
  std::atomic<StreamState> state{StreamState::FREE};

  StreamOp op{StreamOp::WRITE};
  storage::PathStorage *storage{nullptr};
  char path[STORAGE_WORKER_MAX_PATH]{};
  bool is_fs{false};

  CompletionCallback callback;  // set per-call (open/chunk/close); invoked once, then cleared
  storage::StorageError result{storage::StorageError::STORAGE_ERROR_OK};

  storage::FileHandle *handle{nullptr};  // FilesystemStorage only; unused for NetworkStorage
  uint64_t offset{0};                    // NetworkStorage read_chunk()/write_chunk() position

  // Stall watchdog: an external client drives the stream; if it vanishes mid-flight the slot would
  // otherwise stay claimed forever.
  uint32_t last_activity_ms{0};

  // Set by write_chunk()/read_chunk() for the in-flight chunk; cleared once delivered.
  const uint8_t *pending_write_data{nullptr};
  size_t pending_len{0};
  uint8_t *pending_read_buf{nullptr};
  size_t *bytes_transferred_out{nullptr};  // read_chunk() writes the actual count here

  // seek()/tell(): target + mode for a SEEKING step; tell() writes the position here.
  int64_t seek_target{0};
  storage::SeekMode seek_mode{storage::SeekMode::SEEK_MODE_SET};
  uint64_t *tell_out{nullptr};

  // Bumped on every slot claim so a handle from a finished stream stops matching once reused
  // (never 0). Main-loop-only.
  uint32_t generation{0};

  // Set when a step was queued for the loop-sliced engine rather than run immediately, so the
  // callback always fires from loop(), never reentrantly from the caller's own call frame. Unused on
  // the task path.
  bool pending_step_{false};
  // Set when either engine starts this stream's queued step. The idle sweep skips a step still in the
  // task queue so a long transfer ahead is not mistaken for a vanished client.
  std::atomic<bool> step_started_{false};
};

// Tags a task-queue entry so the shared task dispatches to either engine's step function.
enum class QueueEntryKind : uint8_t {
  TRANSFER,
  STREAM,
};

struct QueueEntry {
  QueueEntryKind kind;
  size_t index;
};

// Asynchronous chunked copy/move/read/write over storage:: (unmodified). Task-safe storages run on
// one shared FreeRTOS task; everything else loop-sliced (one chunk/step per loop()). Two pools --
// pool_ (auto-advancing transfers) and stream_pool_ (caller-driven) -- share one task/queue/stack.
class StorageWorker : public PollingComponent {
 public:
  // update_interval defaults to 5 ms (codegen overrides); driven by PollingComponent's scheduler tick
  // (Phase A), not the gated component loop(). start_poller()/stop_poller() arm/disarm it with work.
  StorageWorker() : PollingComponent(5) {}
  void set_task_stack_size(uint32_t size) { this->task_stack_size_ = size; }
  void set_task_priority(uint8_t priority) { this->task_priority_ = priority; }
  void set_max_pending(size_t count) { this->max_pending_ = count; }
  void set_max_streams(size_t count) { this->max_streams_ = count; }

  void setup() override;
  // Logs the resolved chunk size, its RAM placement (internal/PSRAM) and DMA-capability, and the
  // target platform, so the buffer policy is visible in the boot log.
  void dump_config() override;
  // The engine: advances loop-sliced transfer/stream slots by one chunk, plus one completion sweep,
  // per call. Driven by the scheduler tick, started at the submit funnels (start_poller()) and stopped
  // by update() once every slot/stream is FREE.
  void update() override;
  // DATA, not AFTER_CONNECTION: no networking dependency of its own (NFS/SMB go through storage::), so
  // pool/task creation need not wait for Wi-Fi/API, and async_copy/move work from another component's
  // setup() below DATA.
  float get_setup_priority() const override { return setup_priority::DATA; }

  // Submits an async copy/move. Returns OK once queued (callback fires once, later, on the main loop)
  // or an immediate error, callback NOT invoked: NOT_READY (pool full) / INVALID_ARGS (path too long).
  // overwrite=false answers ALREADY_EXISTS; true clears first. Worker stat()s src for tree-vs-file.
  storage::StorageError async_copy(storage::PathStorage *src, const char *src_path, storage::PathStorage *dst,
                                   const char *dst_path, CompletionCallback &&on_done, TransferJob *job_out = nullptr,
                                   bool overwrite = false);
  storage::StorageError async_move(storage::PathStorage *src, const char *src_path, storage::PathStorage *dst,
                                   const char *dst_path, CompletionCallback &&on_done, TransferJob *job_out = nullptr,
                                   bool overwrite = false);

  // Whole directory tree (engine-walked, see RequestOp::COPY_TREE). Destination root created if
  // missing; existing entries below it are overwritten.
  storage::StorageError async_copy_tree(storage::PathStorage *src, const char *src_path, storage::PathStorage *dst,
                                        const char *dst_path, CompletionCallback &&on_done,
                                        TransferJob *job_out = nullptr);
  storage::StorageError async_move_tree(storage::PathStorage *src, const char *src_path, storage::PathStorage *dst,
                                        const char *dst_path, CompletionCallback &&on_done,
                                        TransferJob *job_out = nullptr);

  // Format and mount: one blocking control-plane call each, routed through the engine to run on the
  // worker task for task-safe media (main loop free) instead of blocking the caller. on_done fires
  // once with the driver's result.
  storage::StorageError async_mount(storage::PathStorage *target, CompletionCallback &&on_done,
                                    TransferJob *job_out = nullptr);
  storage::StorageError async_format(storage::Storage *target, CompletionCallback &&on_done,
                                     TransferJob *job_out = nullptr);

  // Raw-device transfers. async_raw_read streams device [address, address+size) into a file
  // (overwrite as async_copy); async_raw_write streams a file onto the device at address, erasing the
  // covered range first when erase_first is set (sector-aligned address checked in the pre-phase).
  storage::StorageError async_raw_read(storage::RawStorage *device, uint64_t address, uint64_t size,
                                       storage::PathStorage *dst, const char *dst_path, CompletionCallback &&on_done,
                                       TransferJob *job_out = nullptr, bool overwrite = false);
  storage::StorageError async_raw_write(storage::PathStorage *src, const char *src_path, storage::RawStorage *device,
                                        uint64_t address, bool erase_first, CompletionCallback &&on_done,
                                        TransferJob *job_out = nullptr, uint8_t verify_passes = 0);
  // Device-vs-file verify with no write: compares [address, address+filesize) against the file,
  // verify_passes times (>= 1). Same read-back phase a write's verify uses.
  storage::StorageError async_raw_verify_file(storage::PathStorage *src, const char *src_path,
                                              storage::RawStorage *device, uint64_t address,
                                              CompletionCallback &&on_done, TransferJob *job_out = nullptr,
                                              uint8_t verify_passes = 1);
  // Media without a RAW_ERASE_* capability (EEPROM/FRAM) get a PSEUDO erase: chunked 0xFF write() over
  // [address, address+size), no alignment demanded. Media with a real erase keep the aligned driver erase().
  storage::StorageError async_raw_erase(storage::RawStorage *device, uint64_t address, uint64_t size,
                                        CompletionCallback &&on_done, TransferJob *job_out = nullptr,
                                        bool force_sliced = false);

  // Snapshot for progress bars / job-status endpoints. Main-loop-only. False when the handle is
  // unknown or expired: a slot recycles after its callback ran, so the DONE snapshot is observable
  // only until then -- consumers needing the final result must capture it in the callback.
  bool get_transfer_status(TransferJob job, TransferStatus *out) const;

  // True if any active (PENDING/RUNNING/CANCELLED) transfer references `storage`, or any stream has it
  // open (not FREE/DONE). Main-loop-only. Used by a removable device to defer unmount until no job
  // touches it; raw media never unmount, so they never ask.
  bool is_busy_with(const storage::Storage *storage) const;

  // Contention twin of is_busy_with(): true while the BACKGROUND TASK may do I/O on `storage` -- a
  // task-owned RUNNING/CANCELLED transfer or a stream step on the task. PENDING, loop-sliced and IDLE
  // never count. Main-loop-only, false without the task. Running blocking I/O while true races the task.
  bool has_active_task_io(const storage::Storage *storage) const;

  // Opens `path` for writing (create/truncate) and returns a handle if a slot was free, else
  // NOT_READY (pool full) / INVALID_ARGS (path too long) with on_open NOT invoked. on_open fires once,
  // later, on the main loop, once open() has completed.
  storage::StorageError begin_write(storage::PathStorage *storage, const char *path, StreamHandle *out_handle,
                                    CompletionCallback &&on_open);
  // Pushes one chunk. `data` must stay valid until on_written fires -- not copied. Returns OK once
  // queued or an immediate error (handle unknown, not IDLE, previous completion undelivered), on_written
  // NOT invoked. Every per-chunk call sequences on its callback: wait for it or get NOT_READY.
  storage::StorageError write_chunk(const StreamHandle &handle, const uint8_t *data, size_t len,
                                    CompletionCallback &&on_written);
  // Closes the handle (closes the FileHandle for FilesystemStorage; data-plane no-op for
  // NetworkStorage). Always call once done, even after a chunk error, to release the slot.
  storage::StorageError end_write(const StreamHandle &handle, CompletionCallback &&on_closed);

  // Opens `path` for reading. Otherwise as begin_write().
  storage::StorageError begin_read(storage::PathStorage *storage, const char *path, StreamHandle *out_handle,
                                   CompletionCallback &&on_open);
  // Requests up to `len` bytes into `buf` (valid until on_read fires). *bytes_read filled before
  // on_read; 0 = EOF (not an error, same partial-read contract as storage.h read()).
  storage::StorageError read_chunk(const StreamHandle &handle, uint8_t *buf, size_t len, size_t *bytes_read,
                                   CompletionCallback &&on_read);
  storage::StorageError end_read(const StreamHandle &handle, CompletionCallback &&on_closed);

  // Repositions the cursor (SeekMode SET/CUR/END). Filesystem: seeks the handle; network: arithmetic
  // on the offset (END consults file_size()). on_seeked fires once on the main loop; stream must be
  // IDLE. NOT_READY if a step is in flight, INVALID_ARGS on unknown handle or negative result.
  storage::StorageError seek(const StreamHandle &handle, int64_t offset, storage::SeekMode mode,
                             CompletionCallback &&on_seeked);
  // Reports the cursor into *position before on_told. Same IDLE-only, main-loop-callback contract.
  storage::StorageError tell(const StreamHandle &handle, uint64_t *position, CompletionCallback &&on_told);

 protected:
  storage::StorageError submit_control_op_(RequestOp op, storage::Storage *target, CompletionCallback &&on_done,
                                           TransferJob *job_out);
  storage::StorageError submit_(RequestOp op, storage::PathStorage *src, const char *src_path,
                                storage::PathStorage *dst, const char *dst_path, CompletionCallback &&on_done,
                                TransferJob *job_out = nullptr, bool overwrite = false);

  // Lazily creates both pools (and the shared task where applicable) on the first submit/begin_*
  // call, so a driver that links the worker but never issues a transfer pays nothing. Idempotent.
  // Deliberate heap use after setup() (AGENTS.md #7): several kB at first submit, not boot.
  void ensure_started_();

  // True if every storage involved may have its data-plane calls run off the main loop.
  bool is_task_safe_(const TransferRequest &req) const;
  bool wait_for_network_ready_(TransferRequest &req, storage::StorageError err, const storage::Storage *side);
  // Verify phase for RAW_WRITE_FROM_FILE. begin_verify_pass_ rewinds the source, returning false
  // (request finished) on error. verify_chunk_ compares one device chunk against the file, finishing on
  // mismatch/error/completion or yielding on the network. Callers must return immediately after either.
  bool begin_verify_pass_(TransferRequest &req);
  void verify_chunk_(TransferRequest &req, bool on_task);
  storage::StorageError submit_raw_(RequestOp op, storage::RawStorage *device, uint64_t address, uint64_t size,
                                    storage::PathStorage *file_side, const char *file_path, bool erase_first,
                                    bool overwrite, CompletionCallback &&on_done, TransferJob *job_out,
                                    bool force_sliced = false, uint8_t verify_passes = 0);
  // on_task gates the whole-chip erase fast path to the worker task.
  void run_raw_chunk_(TransferRequest &req, bool on_task);
  // This engine's streaming buffer, allocated on first use. nullptr (and *size_out untouched) if
  // unavailable.
  uint8_t *chunk_buffer_(bool on_task, size_t *size_out);
  void check_stalled_();
  uint32_t last_stall_check_ms_{0};
  bool is_task_safe_(const StreamRequest &req) const;
  // Contention question overlaps_active_() answers, for a stream. from_loop: caller is loop(), about to
  // run this step -- another stream only queued for the loop does NOT contend (same thread runs them in
  // turn); at dispatch (from_loop=false) it does, so the candidate goes to the loop too.
  bool stream_overlaps_active_(const StreamRequest &candidate, bool from_loop) const;
  // The slot a handle refers to, or nullptr once it refers to nothing. Every handle entry point goes
  // through here.
  StreamRequest *stream_for_handle_(const StreamHandle &handle);

  // True if another RUNNING/CANCELLED request (still owned by an engine) shares a storage instance
  // with `candidate`. Checked at both dispatch points. Main-loop-only: a task request finishing
  // concurrently still reads RUNNING, delaying dispatch one iteration (conservative).
  bool overlaps_active_(const TransferRequest &candidate) const;

  // True if `req` operates on `s` on any side -- src/dst, a raw op's device, or a FORMAT's target
  // (format_target, op-gated). The single availability predicate shared by is_busy_with(),
  // has_active_task_io() and the unregister drain.
  bool request_touches_(const TransferRequest &req, const storage::Storage *s) const;

  // Frees DONE slots and fires their callbacks (main loop). Called at the top of update(), so a
  // completion from this tick's chunk is delivered next tick.
  void deliver_completions_();

  // Advances one request by one chunk, on either engine. Owns handle open/close, the chunk buffer,
  // offsets, cancellation/hotplug checks, and the same-storage rename() fast path for MOVE. Sets
  // req.result and state = DONE on completion; on_task gates the whole-chip erase to the task.
  void run_chunk_(TransferRequest &req, bool on_task);
  // One directory-walk step: next entry, mkdir, or set up the next file. False when the walk is over
  // (request already finished).
  bool tree_step_(TransferRequest &req);

  // Advances one stream by exactly the op its current state implies (open, one chunk, or close), then
  // IDLE or DONE. Unlike run_chunk_(), never decides the next step itself -- state is set by the
  // caller's call, this executes it once.
  void run_stream_step_(StreamRequest &req);

  // Queues `req` on the shared task if task-safe, else marks it pending for loop() -- never runs it
  // inline from the caller's own call frame (see run_stream_step_).
  void dispatch_stream_step_(StreamRequest &req, size_t index);

  // Hotplug: synchronously cancels and drains every request/stream touching an unregistered storage
  // before returning, so the driver can unmount immediately afterward. See the .cpp for the per-engine
  // drain.
  void on_storage_unregistered_(storage::Storage *s);

  // One streaming buffer per engine, not per request: content never outlives the run_chunk_() that
  // filled it and neither engine runs two at once. Separate per engine because size/heap follow context
  // (loop within its slice, task a larger DMA-capable PSRAM chunk). Allocated on first use and kept.
  storage::RamBuffer chunk_buf_loop_;
  storage::RamBuffer chunk_buf_task_;
  size_t chunk_size_loop_{0};
  size_t chunk_size_task_{0};

  FixedVector<TransferRequest> pool_;
  FixedVector<StreamRequest> stream_pool_;
  uint32_t task_stack_size_{8192};
  uint8_t task_priority_{1};
  size_t max_pending_{4};
  size_t max_streams_{2};

  // Set by ensure_started_() on its first run; guards one-time pool/task creation.
  bool started_{false};

  // Request currently owned by the loop-sliced engine (SIZE_MAX = none). Requests run one at a time in
  // FIFO order here. Streams need no equivalent: each step is self-contained.
  size_t loop_active_index_{SIZE_MAX};

#if defined(USE_ESP32) && defined(USE_STORAGE_WORKER_TASK)
  // Set once the worker task exists; gates whether new task-safe requests enqueue to the task or fall
  // to the loop engine.
  bool task_running_{false};
  TaskHandle_t task_handle_{nullptr};
  QueueHandle_t task_queue_{nullptr};  // holds QueueEntry -- shared by both engines

  static void task_fn(void *arg);
  void task_loop_();
#endif
};

extern StorageWorker *global_storage_worker;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome::storage

#endif  // USE_STORAGE_WORKER
