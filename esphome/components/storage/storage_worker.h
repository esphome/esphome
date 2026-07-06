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
};

// Asynchronous, chunked file copy/move on top of the storage:: interface (storage.h itself is
// unmodified by this component). See the design note in storage.h's control-/data-plane
// contract: STORAGE_CAP_IO_TASK_SAFE storages get offloaded to a background FreeRTOS task on
// platforms that have one; everything else — including all non-ESP32 platforms — runs in
// loop-sliced mode, processing one chunk of the head-of-line request per main loop iteration.
// The public API is identical either way; callers cannot tell the difference except in timing.
// Requests that share a storage instance never run concurrently across the two engines — see
// overlaps_active_() below — since the interface requires all data-plane calls on a given
// instance to be externally serialized.
class StorageWorker : public Component {
 public:
  void set_task_stack_size(uint32_t size) { this->task_stack_size_ = size; }
  void set_task_priority(uint8_t priority) { this->task_priority_ = priority; }
  void set_max_pending(size_t count) { this->max_pending_ = count; }

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
                                   const char *dst_path, CompletionCallback &&on_done);
  storage::StorageError async_move(storage::PathStorage *src, const char *src_path, storage::PathStorage *dst,
                                   const char *dst_path, CompletionCallback &&on_done);

 protected:
  storage::StorageError submit_(RequestOp op, storage::PathStorage *src, const char *src_path,
                                storage::PathStorage *dst, const char *dst_path, CompletionCallback &&on_done);

  // Lazily creates the request pool (and, where applicable, the background task) on the first
  // submit_() call. A path-based driver that links in the worker but never actually issues an
  // async transfer never pays for either — setup() itself only subscribes to the hotplug
  // callback. Idempotent: subsequent calls are a no-op once started_ is set.
  void ensure_started_();

  // True if every storage involved may have its data-plane calls run off the main loop.
  bool is_task_safe_(const TransferRequest &req) const;

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

  // Hotplug: synchronously cancels and drains every request touching an unregistered storage
  // (as either src or dst) before returning, so the driver can proceed to unmount/tear down
  // the storage immediately afterward with no in-flight data-plane calls left against it. See
  // the .cpp for the full per-engine drain sequence.
  void on_storage_unregistered_(storage::Storage *s);

  FixedVector<TransferRequest> pool_;
  uint32_t task_stack_size_{8192};
  uint8_t task_priority_{1};
  size_t max_pending_{4};

  // Set by ensure_started_() the first time it runs; guards the pool/task creation so it only
  // ever happens once, on the first submit_() call.
  bool started_{false};

  // Index of the request currently owned by the loop-sliced engine (SIZE_MAX = none in
  // flight). Requests run strictly one at a time in FIFO submission order in this mode.
  size_t loop_active_index_{SIZE_MAX};

#if defined(USE_ESP32) && defined(USE_STORAGE_WORKER_TASK)
  // Set once the worker task has been created; guards whether new task-safe requests get
  // enqueued to the task vs. picked up by the loop-sliced engine.
  bool task_running_{false};
  TaskHandle_t task_handle_{nullptr};
  QueueHandle_t task_queue_{nullptr};  // holds size_t pool indices

  static void task_fn(void *arg);
  void task_loop_();
#endif
};

extern StorageWorker *global_storage_worker;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome::storage

#endif  // USE_STORAGE_WORKER
