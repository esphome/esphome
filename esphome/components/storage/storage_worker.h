#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "storage.h"

// Compiled only when at least one path-based (filesystem/network) storage driver is
// configured; a raw-only node (small memory ICs) never pays for it. See
// request_storage_worker() in __init__.py for how this define is set.
#ifdef USE_STORAGE_WORKER

#include <atomic>
#include <functional>
#include <memory>

// The define is derived purely from drivers' task_safe flags in codegen; the platform
// condition lives here so a task-safe driver on a non-FreeRTOS target degrades to
// loop-sliced instead of failing to compile.
#if defined(USE_ESP32) && defined(USE_STORAGE_WORKER_TASK)
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#endif

namespace esphome::storage {

using CompletionCallback = std::function<void(storage::StorageError)>;

// Maximum path length copied into each pooled request. Paths are copied (not referenced) at
// submit time, since the caller's pointers must not be assumed to outlive submission -- the
// request may still be pending when the calling code returns. Longer paths are rejected with
// StorageError::INVALID_ARGS.
static constexpr size_t STORAGE_WORKER_MAX_PATH = STORAGE_PATH_MAX;

enum class RequestOp : uint8_t {
  COPY,
  MOVE,
  // Raw-device transfers (image read/flash). The device side is byte-addressed RawStorage,
  // the other side a regular file -- chunked by the engine like any transfer: no whole-image
  // RAM buffer, no blocking-size ceiling, progress and job id for free. A device write's
  // preparatory erase is sliced per pass (see run_raw_chunk_) so a chip-scale erase never
  // freezes the main loop in one piece.
  RAW_READ_TO_FILE,
  RAW_WRITE_FROM_FILE,
  // Device-only: the sliced erase alone, then done. Same freeze-avoidance rationale.
  RAW_ERASE,
  // Device-vs-file verify with no write: reads the device range back and compares it against a
  // file, verify_passes times. The read-back-and-compare phase is the same one a write's own
  // verify uses, entered directly from the pre-phase (no write step). Uses the file as the src
  // side like RAW_WRITE_FROM_FILE does.
  RAW_VERIFY_FILE,
  // Whole directory tree, walked by the engine itself: list, mkdir, copy each file, and for a
  // move remove each source entry as it is drained. Deliberately here and not in the caller:
  // on task-safe media the worker task then owns the entire operation start to finish, exactly
  // like a single file does. A caller that walks the tree itself can only step forward when
  // its own loop() runs, which makes the transfer depend on the main loop being scheduled --
  // it is not the caller's business, and nothing asks the main loop to run on their behalf.
  COPY_TREE,
  MOVE_TREE,
  // Filesystem format: a single blocking control-plane call (f_mkfs and the like). Run as a
  // one-shot on the worker task for task-safe media (main loop free, watchdog-safe) or from a
  // loop() step otherwise. Moves no bytes and opens no handles.
  FORMAT,
};

// Where a request currently stands. Transitions:
//   FREE -> PENDING (submit) -> RUNNING (engine picks it up) -> DONE (engine finishes)
//     -> FREE (main loop delivers the completion callback and releases the slot)
// RUNNING requests can also be marked CANCELLED, either by the engine's own per-chunk
// cancellation check or, synchronously, by on_storage_unregistered_() when a storage they use
// is removed: a PENDING request is finished immediately (no I/O to unwind); a RUNNING request
// on the loop-sliced engine is drained in place (run_chunk_() is called directly, right there
// in the hotplug callback, closing any open handles before the driver's unmount() proceeds); a
// RUNNING request on the worker task has its state set to CANCELLED and is then waited on
// (yielding, bounded by a timeout) until the task observes it and reaches DONE. Either way,
// the goal is the same: by the time on_storage_unregistered_() returns, no data-plane call
// into the removed storage is still in flight. See on_storage_unregistered_()'s own comment
// for the full drain sequence and its timeout behavior.
// Both engines must keep calling run_chunk_() until the state is DONE, not just while it's
// RUNNING -- CANCELLED is only ever acted on inside run_chunk_()'s own entry check, so a loop
// that stops as soon as the state leaves RUNNING would never actually observe/handle it.
enum class RequestState : uint8_t {
  FREE,
  PENDING,
  RUNNING,
  CANCELLED,
  DONE,
};

// Coarse phase of a raw transfer, surfaced so a status line can say what a running job is
// doing (erase -> write -> verify). NONE for transfers that have no such phases (plain
// file copy/read) or that are not running.
enum class TransferPhase : uint8_t {
  NONE,
  ERASE,
  WRITE,
  VERIFY,
};

// Opaque transfer-job handle: (generation << 8) | slot_index. max_pending is capped at 16 so
// 8 bits of slot index are plenty; 0 is the invalid handle (generations start at 1).
using TransferJob = uint32_t;
static constexpr TransferJob INVALID_TRANSFER_JOB = 0;

// The encoding lives here rather than being spelled out at each call site, so that a handle
// built by a submit funnel and one taken apart by get_transfer_status() cannot drift apart.
inline TransferJob make_transfer_job(uint32_t generation, size_t slot) {
  return (generation << 8) | static_cast<uint32_t>(slot & 0xFF);
}
inline uint32_t transfer_job_generation(TransferJob job) { return job >> 8; }
inline size_t transfer_job_slot(TransferJob job) { return job & 0xFF; }

// Bounded "<root>[/<sub>][/<name>]" join for the tree walk; false on truncation. In the
// header because it is pure string work with an edge case worth testing on its own: a walk
// that silently truncated a path would copy into the wrong place rather than fail.
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

// 64-bit progress counter for TransferRequest, atomic only when it has to be. With the
// background worker task the counters are written from the task while the main loop reads
// them concurrently -- that build uses std::atomic<uint64_t>. Every other build runs the
// loop-sliced engine exclusively, so all access is main-loop-only and a plain field is
// sufficient; this matters because 64-bit atomics are not lock-free on several supported
// cores and GCC lowers them to __atomic_*_8 libcalls that toolchains without libatomic
// (e.g. arm-none-eabi as used for LibreTiny/bk72xx) fail to link. The plain variant mirrors
// the tiny slice of the std::atomic interface the worker actually uses (load/store/assign)
// so every call site compiles unchanged against either type.
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

// Snapshot of a transfer's externally observable state -- see get_transfer_status().
struct TransferStatus {
  // A short label for the file currently in flight (basename, truncated -- this feeds a
  // status line, not a path column). Empty when nothing is in flight: rename fast path,
  // a finished job, the gaps between a tree's files.
  static constexpr size_t FILE_LABEL_LEN = 40;

  RequestState state{RequestState::FREE};
  storage::StorageError result{storage::StorageError::OK};
  uint64_t bytes_done{0};
  uint64_t bytes_total{0};  // 0 = unknown (indeterminate progress)
  // Coarse phase of a raw write/verify job (erase/write/verify), plus how far a multi-pass
  // verify has progressed. verify_passes is the configured pass count; verify_pass is the
  // 1-based pass currently running (0 outside the verify phase). Both 0 for non-raw work.
  TransferPhase phase{TransferPhase::NONE};
  uint8_t verify_pass{0};
  uint8_t verify_passes{0};
  // Per-file progress of the file in flight. For a single-file transfer these mirror
  // bytes_done/bytes_total; for a tree they are what bytes_total cannot be -- the tree's
  // total is unknown without walking it twice, but the current file's is one cheap stat.
  uint64_t file_done{0};
  uint64_t file_total{0};
  char file[FILE_LABEL_LEN]{};
};

// One pooled transfer request. Fixed-size, no heap allocation -- the pool is a FixedVector of
// these, sized exactly to max_pending at codegen. Path buffers and the callback are only valid
// while state != FREE.
// Position inside a directory tree, allocated only for COPY_TREE/MOVE_TREE. Kept out of
// TransferRequest so a pool sized for plain file transfers does not carry it per slot.
struct TreeWalk {
  // Same bound as the blocking walks in storage.cpp: a tree this walk creates has to stay
  // within what copy()/remove_recursive() can handle afterwards, so both refuse at the same
  // nesting. One index slot per level, root level included -- hence the + 1.
  static constexpr size_t MAX_DEPTH = STORAGE_MAX_RECURSION_DEPTH + 1;

  char src_root[STORAGE_WORKER_MAX_PATH]{};
  char dst_root[STORAGE_WORKER_MAX_PATH]{};
  // Current position below the roots, "" at the top. src_path/dst_path always hold the file
  // being copied right now, so the chunk loop below needs no special-casing for trees.
  char sub[STORAGE_WORKER_MAX_PATH]{};
  uint16_t index_stack[MAX_DEPTH]{};
  uint8_t depth{0};
  bool file_in_flight{false};
  bool root_created{false};
  uint32_t files_done{0};
  // Bytes of the files already finished; the one in flight adds its own offset on top, so
  // progress counts the whole tree rather than restarting with every file.
  uint64_t bytes_base{0};
};

struct TransferRequest {
  std::atomic<RequestState> state{RequestState::FREE};

  RequestOp op{RequestOp::COPY};
  storage::PathStorage *src_storage{nullptr};
  storage::PathStorage *dst_storage{nullptr};
  char src_path[STORAGE_WORKER_MAX_PATH]{};
  char dst_path[STORAGE_WORKER_MAX_PATH]{};

  CompletionCallback callback;
  storage::StorageError result{storage::StorageError::OK};

  // Transfer progress, kept here so the loop-sliced engine can resume across loop()
  // iterations and so the same fields serve the worker task path unchanged.
  storage::FileHandle *src_handle{nullptr};
  storage::FileHandle *dst_handle{nullptr};
  uint64_t offset{0};
  bool src_is_fs{false};
  bool dst_is_fs{false};
  bool handles_open{false};

  // Set once loop()'s dispatch has already logged that this request cannot proceed because it
  // overlaps a slot stuck RUNNING/CANCELLED past the drain timeout (see
  // on_storage_unregistered_()'s timeout comment) -- avoids re-logging every loop() iteration
  // for as long as the stuck slot remains. Reset when the request is freed.
  bool stuck_warned{false};
  // Submission timestamp: bounds how long NOT_READY from a network storage is treated as
  // "still connecting" (see run_chunk_) before it becomes the honest final answer.
  uint32_t submitted_ms{0};
  bool waiting_logged{false};
  // Architecture contract: the file API is a pure HTTP -> storage-interface translator. All
  // driver I/O that used to run in the HTTP handler's pre-phase (source/destination stat,
  // overwrite clearing, the tree-vs-file decision) happens HERE, inside the engine that owns
  // the storages -- executed once on the request's first pass.
  bool overwrite{false};
  bool pre_phase_done{false};
  // Stall watchdog (check_stalled_): refreshed whenever the request demonstrably moves; a
  // request that stops moving is finished with TIMEOUT so its storages -- and everything
  // overlap-blocked behind them -- come free again.
  uint32_t last_progress_ms{0};
  uint64_t progress_mark{0};
  // Result the CANCELLED entry check finishes the request with. Ownership contract: only
  // the engine that runs a request (loop-sliced engine or worker task) may finish a RUNNING
  // request -- everyone else (hotplug drain, stall watchdog) marks it CANCELLED and lets the
  // owning engine's next run_chunk_() pass close handles and set DONE. This field carries
  // WHY it was cancelled: NOT_READY for hotplug, TIMEOUT for the stall watchdog.
  storage::StorageError cancel_result{storage::StorageError::NOT_READY};
  // Raw ops only. raw_address is the device offset of the transfer window; the erase cursor
  // walks [raw_erase_pos, raw_erase_end) one geometry-sized step per pass before any bytes
  // move. The file side lives in src/dst_storage + src/dst_path as usual; the device side is
  // this pointer, deliberately part of overlaps_active_'s contention checks.
  storage::RawStorage *raw_device{nullptr};
  uint64_t raw_address{0};
  uint64_t raw_erase_pos{0};
  uint64_t raw_erase_end{0};
  // Opt-out from the whole-chip fast path: when true, a full-span erase that would otherwise
  // take the single blocking erase(0, capacity) on the task instead walks block by block (the
  // same path the loop-sliced engine always uses). Default false = chip erase when eligible.
  // Set by the caller (raw_erase action / raw HTTP api); only consulted in run_raw_chunk_.
  bool force_sliced_erase{false};
  // Verify phase for RAW_WRITE_FROM_FILE. verify_passes is how many times the written range is
  // read back and compared against the source file after the write completes (0 = no verify).
  // verify_pass_done counts finished passes; verifying flags that the chunk loop is in the
  // read-back-and-compare phase rather than the write phase. Uses the same chunk_buf: each chunk
  // reads the device into the buffer, then reads the file in small slices and memcmp's them, so
  // no second full buffer is allocated.
  uint8_t verify_passes{0};
  uint8_t verify_pass_done{0};
  std::atomic<bool> verifying{false};
  // Set by the worker task right before a blocking whole-chip erase(0, capacity) and cleared
  // right after. A chip erase busy-waits for its full duration (tens of seconds) without
  // advancing bytes_done, so the stall watchdog (check_stalled_, main loop) would otherwise
  // time it out; it skips a RUNNING request while this is set. Only ever true on the worker
  // task (the loop-sliced engine never does a whole-chip erase), so it is bounded by the
  // driver's own wait_ready timeout -- no risk of blinding the watchdog indefinitely.
  // std::atomic<bool> is lock-free on every supported core (unlike the 64-bit counters), so
  // it needs no conditional-atomic dance; the flag is written on the task and read on the
  // main loop.
  std::atomic<bool> blocking_erase_active{false};
  // Set by wait_for_network_ready_() when a chunk stayed RUNNING waiting for a network
  // storage to come up; the worker-task loop paces its retry on it (loop-sliced paces
  // naturally via update()). Reset at the top of each run_chunk_().
  bool network_waiting{false};

  // Externally observable progress (see get_transfer_status()): bytes_done is advanced by
  // run_chunk_() on whichever engine runs the transfer (possibly the worker task) while the
  // main loop may query concurrently -- atomic in task builds only, see ProgressCounter above.
  // bytes_total is stat()ed once at transfer start; 0 means unknown -- the rename fast path, a
  // failed stat, and every tree op, whose total nobody knows without walking it twice. For a
  // tree bytes_done counts the whole thing, not the file in flight.
  ProgressCounter bytes_done{};
  ProgressCounter bytes_total{};
  // Progress of the file currently in flight, same concurrency rules as the pair above.
  // Equal to bytes_done/bytes_total for a single-file transfer; for a tree this is the part
  // bytes_total cannot provide (see the comment there) -- file_total is stat()ed when the
  // walk opens each file, file_done tracks its offset and both drop to 0 between files.
  ProgressCounter file_done{};
  ProgressCounter file_total{};
  // Job-handle generation: bumped on every slot claim so a recycled slot invalidates stale
  // TransferJob handles (never 0 -- 0 is reserved for the invalid job). Main-loop-only.
  uint32_t generation{0};

  // Set for COPY_TREE/MOVE_TREE only; owns the walk position for the duration of the request.
  std::unique_ptr<TreeWalk> tree;
};

// ===========================================================================================
// Streaming request types -- chunked read/write against externally-driven data (e.g. an HTTP
// request body being uploaded, or an HTTP response being streamed out), as opposed to
// async_copy()/async_move() below which read/write both ends themselves via the storage::
// interface. A separate pool/state machine from TransferRequest/run_chunk_() rather than a
// third RequestOp: those assume the worker itself pulls each chunk (read() then write()) in
// one run_chunk_() call; here the caller supplies (WRITE) or wants delivered (READ) exactly
// one chunk at a time, on its own schedule (e.g. once per handleUpload() invocation), so the
// two shapes fundamentally differ in what drives progress. Same worker, same background task
// and transparency contract as async_copy()/async_move() though -- see StorageWorker below.
enum class StreamOp : uint8_t {
  WRITE,  // caller pushes chunks in; storage receives them (e.g. HTTP upload -> file)
  READ,   // caller requests chunks out; storage supplies them (e.g. file -> HTTP download)
};

// Where a stream currently stands. Transitions:
//   FREE -> OPENING (begin_write()/begin_read()) -> IDLE (open() done, callback delivered)
//     -> WRITING/READING (write_chunk()/read_chunk() in flight) -> IDLE (chunk done, callback
//     delivered) -> ... (repeat) ... -> CLOSING (end_write()/end_read()) -> DONE -> FREE
// Unlike TransferRequest, IDLE can persist indefinitely between chunks -- there's no
// auto-advance, so a stream sits IDLE for as long as the caller takes to supply/consume the
// next chunk (e.g. waiting on the network for the next HTTP upload chunk).
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

// Opaque handle returned to the caller -- valid for the stream's lifetime (begin_* through
// end_*/DONE) and not a moment longer. Callers must not dereference it.
//
// The generation is what makes "not a moment longer" enforceable rather than a rule to
// remember: it is bumped every time a slot is claimed, so a handle held past end_* stops
// matching once someone else takes the slot, and the call is refused instead of writing into
// a stranger's file. TransferJob does the same thing for the same reason.
struct StreamHandle {
  size_t index;
  uint32_t generation;
};

// One pooled stream. Fixed-size, no heap allocation -- pool is a FixedVector sized to
// max_streams at codegen (separate limit from max_pending, since streams are typically much
// longer-lived than a single copy/move and a node doing e.g. one upload at a time needs very
// few slots).
struct StreamRequest {
  std::atomic<StreamState> state{StreamState::FREE};

  StreamOp op{StreamOp::WRITE};
  storage::PathStorage *storage{nullptr};
  char path[STORAGE_WORKER_MAX_PATH]{};
  bool is_fs{false};

  CompletionCallback callback;  // set per-call (open/chunk/close); invoked once, then cleared
  storage::StorageError result{storage::StorageError::OK};

  storage::FileHandle *handle{nullptr};  // FilesystemStorage only; unused for NetworkStorage
  uint64_t offset{0};                    // NetworkStorage read_chunk()/write_chunk() position

  // Stall watchdog: streams are driven by an external client (HTTP upload/download); when
  // that client vanishes mid-flight the slot would otherwise stay claimed forever.
  uint32_t last_activity_ms{0};

  // Set by write_chunk()/read_chunk() for the in-flight chunk; cleared once delivered.
  const uint8_t *pending_write_data{nullptr};
  size_t pending_len{0};
  uint8_t *pending_read_buf{nullptr};
  size_t *bytes_transferred_out{nullptr};  // read_chunk() writes the actual count here

  // seek()/tell(): target + mode for a SEEKING step; tell() writes the position here.
  int64_t seek_target{0};
  storage::SeekMode seek_mode{storage::SeekMode::SET};
  uint64_t *tell_out{nullptr};

  // Bumped on every slot claim so a StreamHandle from a finished stream stops matching once
  // the slot is reused (never 0 -- that is the unclaimed value). Main-loop-only.
  uint32_t generation{0};

  // Set by dispatch_stream_step_() when a step was queued for the loop-sliced engine rather
  // than run immediately -- loop() picks it up and runs it there, so the callback always fires
  // from loop(), never reentrantly from inside the caller's own begin_*/write_chunk/
  // read_chunk/end_* call. Not used on the task path, which runs the step directly off the
  // shared queue.
  bool pending_step_{false};
};

// Tags a background-task queue entry so the single shared task can dispatch to either engine's
// per-slot step function. Queue holds these instead of a bare index now that both
// TransferRequest and StreamRequest share one task/queue.
enum class QueueEntryKind : uint8_t {
  TRANSFER,
  STREAM,
};

struct QueueEntry {
  QueueEntryKind kind;
  size_t index;
};

// Asynchronous, chunked file copy/move/read/write on top of the storage:: interface (storage.h
// itself is unmodified by this component). See the design note in storage.h's control-/data-
// plane contract: STORAGE_CAP_IO_TASK_SAFE storages get offloaded to a single shared
// background FreeRTOS task on platforms that have one; everything else -- including all
// non-ESP32 platforms -- runs in loop-sliced mode, processing one chunk/step per main loop
// iteration. The public API is identical either way; callers cannot tell the difference except
// in timing. Requests/streams that share a storage instance never run concurrently across the
// two engines -- see overlaps_active_() below -- since the interface requires all data-plane
// calls on a given instance to be externally serialized.
//
// Two independent pools live here: `pool_` (TransferRequest, for async_copy()/async_move())
// and `stream_pool_` (StreamRequest, for begin_write()/write_chunk()/.../begin_read()/
// read_chunk()/...). They're separate state machines -- TransferRequest auto-advances through
// its own chunks via run_chunk_(), StreamRequest only ever advances when the caller supplies
// the next chunk via run_stream_step_() -- but share one task_stack_size_/task_priority_, one
// background task, and one task_queue_ (tagged by QueueEntry::kind) rather than paying for two
// of each.
class StorageWorker : public PollingComponent {
 public:
  // update_interval defaults to 5 ms (codegen sets it via set_update_interval); the engine is
  // driven by PollingComponent's scheduler interval, i.e. a scheduler item serviced every
  // fired tick (Phase A) -- the same mechanism PN532/sensors use -- not the gated component
  // loop(). start_poller()/stop_poller() (below) arm and disarm that interval with work.
  StorageWorker() : PollingComponent(5) {}
  void set_task_stack_size(uint32_t size) { this->task_stack_size_ = size; }
  void set_task_priority(uint8_t priority) { this->task_priority_ = priority; }
  void set_max_pending(size_t count) { this->max_pending_ = count; }
  void set_max_streams(size_t count) { this->max_streams_ = count; }

  void setup() override;
  // Reports the resolved streaming/copy chunk size, its RAM placement (internal vs PSRAM)
  // and DMA-capability, and the platform those were chosen for -- so the buffer policy is
  // visible in the boot log without reading defines.
  void dump_config() override;
  // The engine: advances the loop-sliced transfer/stream slots one time-budgeted batch per
  // call. Called by PollingComponent's scheduler interval (started with the first pending
  // work via start_poller() at the submit funnels, stopped again by update() itself once
  // every slot and stream is FREE). No custom driver: the scheduler runs this on Phase A of
  // every fired tick, independent of the gated component loop().
  void update() override;
  // DATA, not AFTER_CONNECTION: the worker has no networking dependency of its own (NFS/SMB
  // storages are accessed through the storage:: interface, not directly), so there's no reason
  // to delay pool/task creation until after Wi-Fi/API come up. StorageRegistry (BUS) is
  // guaranteed to exist by DATA, and setting up this early means async_copy()/async_move()
  // work correctly even if called from another component's setup() (as long as that
  // component's own setup_priority is lower than DATA).
  float get_setup_priority() const override { return setup_priority::DATA; }

  // Submits an async copy/move. Returns StorageError::OK once the request is queued (the
  // callback will be invoked exactly once, later, always on the main loop) -- or an error
  // immediately if the request could not be queued, in which case the callback is NOT
  // invoked. Rejections: StorageError::NOT_READY (request pool full -- this is backpressure,
  // not a frozen-enum addition, see the PR notes) or StorageError::INVALID_ARGS (a path
  // exceeds STORAGE_WORKER_MAX_PATH).
  // overwrite=false answers ALREADY_EXISTS for an occupied destination; true clears it first
  // (recursively for trees) -- inside the worker, never in a caller's context. The worker also
  // decides tree-vs-file itself by stat()ing the source: callers no longer pre-classify.
  storage::StorageError async_copy(storage::PathStorage *src, const char *src_path, storage::PathStorage *dst,
                                   const char *dst_path, CompletionCallback &&on_done, TransferJob *job_out = nullptr,
                                   bool overwrite = false);
  storage::StorageError async_move(storage::PathStorage *src, const char *src_path, storage::PathStorage *dst,
                                   const char *dst_path, CompletionCallback &&on_done, TransferJob *job_out = nullptr,
                                   bool overwrite = false);

  // Same, for a whole directory tree. The engine walks it -- see RequestOp::COPY_TREE. The
  // destination root is created if missing; existing entries below it are overwritten.
  storage::StorageError async_copy_tree(storage::PathStorage *src, const char *src_path, storage::PathStorage *dst,
                                        const char *dst_path, CompletionCallback &&on_done,
                                        TransferJob *job_out = nullptr);
  storage::StorageError async_move_tree(storage::PathStorage *src, const char *src_path, storage::PathStorage *dst,
                                        const char *dst_path, CompletionCallback &&on_done,
                                        TransferJob *job_out = nullptr);

  // Filesystem format -- a single blocking control-plane call, routed through the engine so
  // it runs on the worker task for task-safe media (main loop free, watchdog-safe) instead of
  // blocking the caller. on_done fires once with the driver's result, like any transfer.
  storage::StorageError async_format(storage::FilesystemStorage *target, CompletionCallback &&on_done,
                                     TransferJob *job_out = nullptr);

  // Raw-device transfers -- the storage-interface home of what the raw HTTP API used to
  // hand-roll. async_raw_read streams device bytes [address, address+size) into a file
  // (overwrite semantics as async_copy); async_raw_write streams a file onto the device at
  // address, erasing the covered range first when erase_first is set (sector-aligned
  // address required, checked against geometry in the pre-phase).
  storage::StorageError async_raw_read(storage::RawStorage *device, uint64_t address, uint64_t size,
                                       storage::PathStorage *dst, const char *dst_path, CompletionCallback &&on_done,
                                       TransferJob *job_out = nullptr, bool overwrite = false);
  storage::StorageError async_raw_write(storage::PathStorage *src, const char *src_path, storage::RawStorage *device,
                                        uint64_t address, bool erase_first, CompletionCallback &&on_done,
                                        TransferJob *job_out = nullptr, uint8_t verify_passes = 0);
  // Device-vs-file verify with no write: compares [address, address+filesize) on the device
  // against the file, verify_passes times (at least 1). Same read-back-and-compare phase a
  // write's own verify uses.
  storage::StorageError async_raw_verify_file(storage::PathStorage *src, const char *src_path,
                                              storage::RawStorage *device, uint64_t address,
                                              CompletionCallback &&on_done, TransferJob *job_out = nullptr,
                                              uint8_t verify_passes = 1);
  // Media without any RAW_ERASE_* capability (EEPROM, FRAM -- overwrite in place, no erase
  // opcode) get a PSEUDO erase: the worker fills [address, address+size) with 0xFF via
  // chunked write(), sliced per pass like everything else. Byte-addressable, so no sector
  // alignment is demanded there; media with a real erase keep the aligned driver erase().
  storage::StorageError async_raw_erase(storage::RawStorage *device, uint64_t address, uint64_t size,
                                        CompletionCallback &&on_done, TransferJob *job_out = nullptr,
                                        bool force_sliced = false);

  // Snapshot of a transfer's externally observable state, for progress bars / job-status
  // endpoints. Main-loop-only (like all control-plane calls). Returns false when the job
  // handle is unknown or expired: a slot is recycled by the pool after its completion
  // callback ran, so the DONE snapshot is only observable until then -- consumers that need
  // the final result reliably must capture it in the completion callback (an HTTP job
  // endpoint caches it web-side) and use polling only for progress.
  bool get_transfer_status(TransferJob job, TransferStatus *out) const;

  // True if any active (PENDING/RUNNING/CANCELLED) transfer references `storage` as source or
  // destination, or any stream has it open (anything other than FREE/DONE). Main-loop-only,
  // like all control-plane queries. Used by a removable device to defer its unmount until no
  // in-flight job still touches it -- raw media never unmount (see RawStorage in storage.h),
  // so they never ask.
  bool is_busy_with(const storage::Storage *storage) const;

  // Opens `path` for writing (create/truncate, like OpenMode::WRITE) and returns a handle
  // immediately if a slot was available -- StorageError::NOT_READY (pool full) or
  // StorageError::INVALID_ARGS (path too long) otherwise, in which case on_open is NOT
  // invoked. on_open fires once, later, always on the main loop, once the underlying open()
  // has actually completed (task-safe storages: on the worker task's turn; else: next loop()).
  storage::StorageError begin_write(storage::PathStorage *storage, const char *path, StreamHandle *out_handle,
                                    CompletionCallback &&on_open);
  // Pushes exactly one chunk. `data` must remain valid until on_written fires -- the worker
  // does not copy it (streams are expected to be large/frequent; copying would defeat the
  // point). Returns StorageError::OK once queued or an immediate error (handle unknown, not
  // IDLE, or the previous step's completion has not been delivered yet) in which case
  // on_written is NOT invoked. This applies to every per-chunk call below (read_chunk, end_*,
  // seek, tell): stream calls are sequenced by their completions -- wait for the callback
  // before issuing the next call, or NOT_READY is returned.
  storage::StorageError write_chunk(const StreamHandle &handle, const uint8_t *data, size_t len,
                                    CompletionCallback &&on_written);
  // Closes the handle (closes the underlying FileHandle for FilesystemStorage; a no-op data-
  // plane-wise for NetworkStorage, which has none). Always call this once done, even after a
  // write_chunk()/read_chunk() error, to release the pool slot.
  storage::StorageError end_write(const StreamHandle &handle, CompletionCallback &&on_closed);

  // Opens `path` for reading. Same semantics as begin_write() otherwise.
  storage::StorageError begin_read(storage::PathStorage *storage, const char *path, StreamHandle *out_handle,
                                   CompletionCallback &&on_open);
  // Requests up to `len` bytes into `buf` (must remain valid until on_read fires).
  // *bytes_read is filled in before on_read is invoked; 0 means EOF (not an error -- same
  // partial-read contract as storage.h's own read()/read_chunk()).
  storage::StorageError read_chunk(const StreamHandle &handle, uint8_t *buf, size_t len, size_t *bytes_read,
                                   CompletionCallback &&on_read);
  storage::StorageError end_read(const StreamHandle &handle, CompletionCallback &&on_closed);

  // Repositions the stream's cursor (SeekMode as in storage.h: SET/CUR/END). For a filesystem
  // stream this seeks the open handle; for a network stream it is arithmetic on the read/write
  // offset (END consults file_size()). on_seeked fires once, later, on the main loop; the stream
  // must be IDLE (between chunks). NOT_READY if a step is in flight or its completion is still
  // undelivered, INVALID_ARGS on an unknown handle or a resulting negative position.
  storage::StorageError seek(const StreamHandle &handle, int64_t offset, storage::SeekMode mode,
                             CompletionCallback &&on_seeked);
  // Reports the current cursor position into *position before on_told fires. Same IDLE-only,
  // main-loop-callback contract as the rest of the stream API.
  storage::StorageError tell(const StreamHandle &handle, uint64_t *position, CompletionCallback &&on_told);

 protected:
  storage::StorageError submit_(RequestOp op, storage::PathStorage *src, const char *src_path,
                                storage::PathStorage *dst, const char *dst_path, CompletionCallback &&on_done,
                                TransferJob *job_out = nullptr, bool overwrite = false);

  // Lazily creates both pools (and, where applicable, the shared background task) on the first
  // submit_()/begin_write()/begin_read() call. A path-based driver that links in the worker but
  // never actually issues an async transfer or stream never pays for any of it -- setup() itself
  // only subscribes to the hotplug callback. Idempotent: subsequent calls are a no-op once
  // started_ is set.
  void ensure_started_();

  // True if every storage involved may have its data-plane calls run off the main loop.
  bool is_task_safe_(const TransferRequest &req) const;
  bool wait_for_network_ready_(TransferRequest &req, storage::StorageError err, const storage::Storage *side);
  // Verify phase for RAW_WRITE_FROM_FILE. begin_verify_pass_ rewinds the source file and resets
  // the cursor to start a read-back pass; verify_chunk_ reads one device chunk and compares it
  // against the file. Both return false when they have finished the request (error/mismatch) or
  // are waiting on the network -- the caller must return immediately in that case.
  bool begin_verify_pass_(TransferRequest &req);
  bool verify_chunk_(TransferRequest &req, bool on_task);
  storage::StorageError submit_raw_(RequestOp op, storage::RawStorage *device, uint64_t address, uint64_t size,
                                    storage::PathStorage *file_side, const char *file_path, bool erase_first,
                                    bool overwrite, CompletionCallback &&on_done, TransferJob *job_out,
                                    bool force_sliced = false, uint8_t verify_passes = 0);
  // on_task carries the engine flag from run_chunk_ (see there): the whole-chip erase fast path
  // is only taken on the worker task.
  void run_raw_chunk_(TransferRequest &req, bool on_task);
  // This engine's streaming buffer, allocated on first use. Returns nullptr (and leaves
  // *size_out untouched) if it cannot be had.
  uint8_t *chunk_buffer_(bool on_task, size_t *size_out);
  void check_stalled_();
  uint32_t last_stall_check_ms_{0};
  bool is_task_safe_(const StreamRequest &req) const;
  // Same contention question overlaps_active_() answers for transfers, asked for a stream:
  // does anything else currently drive this storage's data plane? Keeps a stream step off the
  // task while a transfer holds the same storage on the loop engine (and the other way round),
  // which is what makes the interface's "calls on one instance are externally serialized"
  // contract hold across the two engines.
  // from_loop: the caller is loop(), about to run this step on the main loop. Another stream
  // that is itself only queued for the loop then does NOT contend -- this same thread runs them
  // one after another, and treating them as contending would leave two streams on one storage
  // holding each other forever. At dispatch time (from_loop=false) it does contend, so the
  // candidate goes to the loop as well instead of racing it from the task.
  bool stream_overlaps_active_(const StreamRequest &candidate, bool from_loop) const;
  // The slot a caller's handle refers to, or nullptr once it refers to nothing -- see
  // StreamHandle. Every entry point that takes a handle goes through here.
  StreamRequest *stream_for_handle_(const StreamHandle &handle);

  // True if another request that is currently RUNNING or CANCELLED (i.e. still owned by an
  // engine) shares a storage instance with `candidate`. Used at both dispatch points to
  // uphold the interface's per-instance serialization guarantee across the two engines.
  // Called from the main loop only -- a task request finishing concurrently is still seen
  // as RUNNING here, which merely delays dispatch by one loop iteration (safe/conservative).
  bool overlaps_active_(const TransferRequest &candidate) const;

  // Frees DONE slots and fires their completion callbacks (always on the main loop). Called
  // at the top of loop() and again after the chunk batch, so a completion produced inside
  // the current pass is delivered in the same pass instead of waiting for the next one.
  void deliver_completions_();

  // Advances one request by exactly one chunk. Used by both the loop-sliced engine (called
  // from loop(), once per iteration of its chunk batch) and the worker task (called in its
  // own loop). Owns
  // opening/closing handles and the chunk buffer, offset tracking, cancellation/hotplug
  // checks, and the same-storage rename() fast path for MOVE. On completion (success, error,
  // or cancellation) sets req.result and req.state = RequestState::DONE; the caller (loop()
  // or the task) must not touch req again until the main loop has delivered the callback.
  // on_task is true only when called from the worker task_loop_, false from the loop-sliced
  // engine. It gates the one operation that may block for a long time -- a whole-chip erase --
  // to the task, where a long block is safe (the task is deliberately not on the 5s watchdog).
  void run_chunk_(TransferRequest &req, bool on_task);
  // One step of a directory walk: pick the next entry, create a directory, or set up the next
  // file for the chunk loop. Returns false when the walk is over (request already finished).
  bool tree_step_(TransferRequest &req);

  // Advances one stream by exactly the operation implied by its current state (open, one
  // write/read chunk, or close), then transitions to IDLE (more chunks expected) or DONE.
  // Unlike run_chunk_(), never decides what to do next on its own -- state is set by the
  // caller's begin_*()/write_chunk()/read_chunk()/end_*() call, this just executes it once.
  void run_stream_step_(StreamRequest &req);

  // Hands `req` (pool index `index`) off to the shared task queue if task-safe, else marks it
  // pending for loop() to run on its next pass -- never runs it inline from the caller's own
  // begin_*/write_chunk/read_chunk/end_* call frame. See run_stream_step_()'s doc above for
  // why streams need this rather than reusing TransferRequest's dispatch in submit_().
  void dispatch_stream_step_(StreamRequest &req, size_t index);

  // Hotplug: synchronously cancels and drains every request/stream touching an unregistered
  // storage before returning, so the driver can proceed to unmount/tear down the storage
  // immediately afterward with no in-flight data-plane calls left against it. See the .cpp for
  // the full per-engine drain sequence.
  void on_storage_unregistered_(storage::Storage *s);

  // One streaming buffer per engine rather than one per request. A buffer's content never
  // outlives the run_chunk_() call that filled it, and neither engine runs two requests at
  // once, so requests can share. The two engines keep their own because the size and heap
  // follow the execution context (see alloc_dma_capable): the loop path stays small enough for
  // its 20 ms slice, the task path stages a larger DMA-capable PSRAM chunk. Allocated on first
  // use and kept afterwards -- a 16-64 kB allocate/free cycle per transfer is exactly the
  // fragmentation pattern to avoid.
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

  // Set by ensure_started_() the first time it runs; guards pool/task creation so it only ever
  // happens once, on the first submit_()/begin_write()/begin_read() call.
  bool started_{false};

  // Index of the request currently owned by the loop-sliced engine (SIZE_MAX = none in
  // flight). Requests run strictly one at a time in FIFO submission order in this mode.
  // Streams don't need an equivalent: each stream's step is self-contained (no auto-advance),
  // so there's no ordering to arbitrate between them.
  size_t loop_active_index_{SIZE_MAX};

#if defined(USE_ESP32) && defined(USE_STORAGE_WORKER_TASK)
  // Set once the worker task has been created; guards whether new task-safe requests get
  // enqueued to the task vs. picked up by the loop-sliced engine.
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
