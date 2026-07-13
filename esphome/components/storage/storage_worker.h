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
// submit time, since the caller's pointers must not be assumed to outlive submission — the
// request may still be pending when the calling code returns. Longer paths are rejected with
// StorageError::INVALID_ARGS.
static constexpr size_t STORAGE_WORKER_MAX_PATH = 256;

enum class RequestOp : uint8_t {
  COPY,
  MOVE,
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
// RUNNING — CANCELLED is only ever acted on inside run_chunk_()'s own entry check, so a loop
// that stops as soon as the state leaves RUNNING would never actually observe/handle it.
enum class RequestState : uint8_t {
  FREE,
  PENDING,
  RUNNING,
  CANCELLED,
  DONE,
};

// Opaque transfer-job handle: (generation << 8) | slot_index. max_pending is capped at 16 so
// 8 bits of slot index are plenty; 0 is the invalid handle (generations start at 1).
using TransferJob = uint32_t;
static constexpr TransferJob INVALID_TRANSFER_JOB = 0;

// Snapshot of a transfer's externally observable state — see get_transfer_status().
struct TransferStatus {
  RequestState state{RequestState::FREE};
  storage::StorageError result{storage::StorageError::OK};
  uint64_t bytes_done{0};
  uint64_t bytes_total{0};  // 0 = unknown (indeterminate progress)
};

// One pooled transfer request. Fixed-size, no heap allocation — the pool is a FixedVector of
// these, sized exactly to max_pending at codegen. Path buffers and the callback are only valid
// while state != FREE.
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
  storage::RamBuffer chunk_buf;
  size_t chunk_size{0};
  bool src_is_fs{false};
  bool dst_is_fs{false};
  bool handles_open{false};

  // Set once loop()'s dispatch has already logged that this request cannot proceed because it
  // overlaps a slot stuck RUNNING/CANCELLED past the drain timeout (see
  // on_storage_unregistered_()'s timeout comment) — avoids re-logging every loop() iteration
  // for as long as the stuck slot remains. Reset when the request is freed.
  bool stuck_warned{false};

  // Externally observable progress (see get_transfer_status()): bytes_done is advanced by
  // run_chunk_() on whichever engine runs the transfer (possibly the worker task) while the
  // main loop may query concurrently — hence atomics. bytes_total is stat()ed once at
  // transfer start; 0 means unknown (e.g. rename fast path, stat failure).
  std::atomic<uint64_t> bytes_done{0};
  std::atomic<uint64_t> bytes_total{0};
  // Job-handle generation: bumped on every slot claim so a recycled slot invalidates stale
  // TransferJob handles (never 0 — 0 is reserved for the invalid job). Main-loop-only.
  uint32_t generation{0};
};

// ===========================================================================================
// Streaming request types — chunked read/write against externally-driven data (e.g. an HTTP
// request body being uploaded, or an HTTP response being streamed out), as opposed to
// async_copy()/async_move() below which read/write both ends themselves via the storage::
// interface. A separate pool/state machine from TransferRequest/run_chunk_() rather than a
// third RequestOp: those assume the worker itself pulls each chunk (read() then write()) in
// one run_chunk_() call; here the caller supplies (WRITE) or wants delivered (READ) exactly
// one chunk at a time, on its own schedule (e.g. once per handleUpload() invocation), so the
// two shapes fundamentally differ in what drives progress. Same worker, same background task
// and transparency contract as async_copy()/async_move() though — see StorageWorker below.
enum class StreamOp : uint8_t {
  WRITE,  // caller pushes chunks in; storage receives them (e.g. HTTP upload -> file)
  READ,   // caller requests chunks out; storage supplies them (e.g. file -> HTTP download)
};

// Where a stream currently stands. Transitions:
//   FREE -> OPENING (begin_write()/begin_read()) -> IDLE (open() done, callback delivered)
//     -> WRITING/READING (write_chunk()/read_chunk() in flight) -> IDLE (chunk done, callback
//     delivered) -> ... (repeat) ... -> CLOSING (end_write()/end_read()) -> DONE -> FREE
// Unlike TransferRequest, IDLE can persist indefinitely between chunks — there's no
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
};

// Opaque handle returned to the caller — stable for the stream's lifetime (begin_* through
// end_*/DONE). Callers must not dereference; it only identifies a pool slot.
struct StreamHandle {
  size_t index;
};

// One pooled stream. Fixed-size, no heap allocation — pool is a FixedVector sized to
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

  // Set by write_chunk()/read_chunk() for the in-flight chunk; cleared once delivered.
  const uint8_t *pending_write_data{nullptr};
  size_t pending_len{0};
  uint8_t *pending_read_buf{nullptr};
  size_t *bytes_transferred_out{nullptr};  // read_chunk() writes the actual count here

  // Set by dispatch_stream_step_() when a step was queued for the loop-sliced engine rather
  // than run immediately — loop() picks it up and runs it there, so the callback always fires
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
// background FreeRTOS task on platforms that have one; everything else — including all
// non-ESP32 platforms — runs in loop-sliced mode, processing one chunk/step per main loop
// iteration. The public API is identical either way; callers cannot tell the difference except
// in timing. Requests/streams that share a storage instance never run concurrently across the
// two engines — see overlaps_active_() below — since the interface requires all data-plane
// calls on a given instance to be externally serialized.
//
// Two independent pools live here: `pool_` (TransferRequest, for async_copy()/async_move())
// and `stream_pool_` (StreamRequest, for begin_write()/write_chunk()/.../begin_read()/
// read_chunk()/...). They're separate state machines — TransferRequest auto-advances through
// its own chunks via run_chunk_(), StreamRequest only ever advances when the caller supplies
// the next chunk via run_stream_step_() — but share one task_stack_size_/task_priority_, one
// background task, and one task_queue_ (tagged by QueueEntry::kind) rather than paying for two
// of each.
class StorageWorker : public Component {
 public:
  void set_task_stack_size(uint32_t size) { this->task_stack_size_ = size; }
  void set_task_priority(uint8_t priority) { this->task_priority_ = priority; }
  void set_max_pending(size_t count) { this->max_pending_ = count; }
  void set_max_streams(size_t count) { this->max_streams_ = count; }

  void setup() override;
  void loop() override;
  // DATA, not AFTER_CONNECTION: the worker has no networking dependency of its own (NFS/SMB
  // storages are accessed through the storage:: interface, not directly), so there's no reason
  // to delay pool/task creation until after Wi-Fi/API come up. StorageRegistry (BUS) is
  // guaranteed to exist by DATA, and setting up this early means async_copy()/async_move()
  // work correctly even if called from another component's setup() (as long as that
  // component's own setup_priority is lower than DATA).
  float get_setup_priority() const override { return setup_priority::DATA; }

  // Submits an async copy/move. Returns StorageError::OK once the request is queued (the
  // callback will be invoked exactly once, later, always on the main loop) — or an error
  // immediately if the request could not be queued, in which case the callback is NOT
  // invoked. Rejections: StorageError::NOT_READY (request pool full — this is backpressure,
  // not a frozen-enum addition, see the PR notes) or StorageError::INVALID_ARGS (a path
  // exceeds STORAGE_WORKER_MAX_PATH).
  storage::StorageError async_copy(storage::PathStorage *src, const char *src_path, storage::PathStorage *dst,
                                   const char *dst_path, CompletionCallback &&on_done, TransferJob *job_out = nullptr);
  storage::StorageError async_move(storage::PathStorage *src, const char *src_path, storage::PathStorage *dst,
                                   const char *dst_path, CompletionCallback &&on_done, TransferJob *job_out = nullptr);

  // Snapshot of a transfer's externally observable state, for progress bars / job-status
  // endpoints. Main-loop-only (like all control-plane calls). Returns false when the job
  // handle is unknown or expired: a slot is recycled by the pool after its completion
  // callback ran, so the DONE snapshot is only observable until then — consumers that need
  // the final result reliably must capture it in the completion callback (an HTTP job
  // endpoint caches it web-side) and use polling only for progress.
  bool get_transfer_status(TransferJob job, TransferStatus *out) const;

  // True if any active (PENDING/RUNNING/CANCELLED) transfer references `storage` as source or
  // destination. Main-loop-only, like all control-plane queries. Used e.g. by a removable
  // device to defer its unmount until no in-flight job still touches it.
  bool is_busy_with(const storage::Storage *storage) const;

  // Opens `path` for writing (create/truncate, like OpenMode::WRITE) and returns a handle
  // immediately if a slot was available — StorageError::NOT_READY (pool full) or
  // StorageError::INVALID_ARGS (path too long) otherwise, in which case on_open is NOT
  // invoked. on_open fires once, later, always on the main loop, once the underlying open()
  // has actually completed (task-safe storages: on the worker task's turn; else: next loop()).
  storage::StorageError begin_write(storage::PathStorage *storage, const char *path, StreamHandle *out_handle,
                                    CompletionCallback &&on_open);
  // Pushes exactly one chunk. `data` must remain valid until on_written fires — the worker
  // does not copy it (streams are expected to be large/frequent; copying would defeat the
  // point). Returns StorageError::OK once queued or an immediate error (handle unknown/not
  // IDLE) in which case on_written is NOT invoked.
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
  // *bytes_read is filled in before on_read is invoked; 0 means EOF (not an error — same
  // partial-read contract as storage.h's own read()/read_chunk()).
  storage::StorageError read_chunk(const StreamHandle &handle, uint8_t *buf, size_t len, size_t *bytes_read,
                                   CompletionCallback &&on_read);
  storage::StorageError end_read(const StreamHandle &handle, CompletionCallback &&on_closed);

 protected:
  storage::StorageError submit_(RequestOp op, storage::PathStorage *src, const char *src_path,
                                storage::PathStorage *dst, const char *dst_path, CompletionCallback &&on_done,
                                TransferJob *job_out = nullptr);

  // Lazily creates both pools (and, where applicable, the shared background task) on the first
  // submit_()/begin_write()/begin_read() call. A path-based driver that links in the worker but
  // never actually issues an async transfer or stream never pays for any of it — setup() itself
  // only subscribes to the hotplug callback. Idempotent: subsequent calls are a no-op once
  // started_ is set.
  void ensure_started_();

  // True if every storage involved may have its data-plane calls run off the main loop.
  bool is_task_safe_(const TransferRequest &req) const;
  bool is_task_safe_(const StreamRequest &req) const;

  // True if another request that is currently RUNNING or CANCELLED (i.e. still owned by an
  // engine) shares a storage instance with `candidate`. Used at both dispatch points to
  // uphold the interface's per-instance serialization guarantee across the two engines.
  // Called from the main loop only — a task request finishing concurrently is still seen
  // as RUNNING here, which merely delays dispatch by one loop iteration (safe/conservative).
  bool overlaps_active_(const TransferRequest &candidate) const;

  // Advances one request by exactly one chunk. Used by both the loop-sliced engine (called
  // from loop(), once per iteration) and the worker task (called in its own loop). Owns
  // opening/closing handles and the chunk buffer, offset tracking, cancellation/hotplug
  // checks, and the same-storage rename() fast path for MOVE. On completion (success, error,
  // or cancellation) sets req.result and req.state = RequestState::DONE; the caller (loop()
  // or the task) must not touch req again until the main loop has delivered the callback.
  void run_chunk_(TransferRequest &req);

  // Advances one stream by exactly the operation implied by its current state (open, one
  // write/read chunk, or close), then transitions to IDLE (more chunks expected) or DONE.
  // Unlike run_chunk_(), never decides what to do next on its own — state is set by the
  // caller's begin_*()/write_chunk()/read_chunk()/end_*() call, this just executes it once.
  void run_stream_step_(StreamRequest &req);

  // Hands `req` (pool index `index`) off to the shared task queue if task-safe, else marks it
  // pending for loop() to run on its next pass — never runs it inline from the caller's own
  // begin_*/write_chunk/read_chunk/end_* call frame. See run_stream_step_()'s doc above for
  // why streams need this rather than reusing TransferRequest's dispatch in submit_().
  void dispatch_stream_step_(StreamRequest &req, size_t index);

  // Hotplug: synchronously cancels and drains every request/stream touching an unregistered
  // storage before returning, so the driver can proceed to unmount/tear down the storage
  // immediately afterward with no in-flight data-plane calls left against it. See the .cpp for
  // the full per-engine drain sequence.
  void on_storage_unregistered_(storage::Storage *s);

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
  QueueHandle_t task_queue_{nullptr};  // holds QueueEntry — shared by both engines

  static void task_fn(void *arg);
  void task_loop_();
#endif
};

extern StorageWorker *global_storage_worker;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome::storage

#endif  // USE_STORAGE_WORKER
