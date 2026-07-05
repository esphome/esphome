#include "storage_worker.h"
#include "esphome/core/log.h"

#ifdef USE_STORAGE_WORKER

namespace esphome::storage {

static const char *const TAG = "storage_worker";

StorageWorker *global_storage_worker = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

void StorageWorker::setup() {
  // Pool and (on ESP32) task creation are deferred to the first submit_() call — see
  // ensure_started_() — so that a driver merely linking in the worker (because it's
  // path-based) but never actually issuing an async transfer pays no RAM/task cost at all.
  // Only the hotplug subscription happens here: it must be in place before any storage could
  // possibly be unregistered, and it is a single lambda capture, not an allocation of note.
  if (global_storage_registry != nullptr) {
    global_storage_registry->add_on_unregistered_callback([this](Storage *s) { this->on_storage_unregistered_(s); });
  }

  // loop() has nothing to do until the first async transfer is submitted (see
  // ensure_started_()/submit_() below, which re-enables it) — an idle empty-pool scan every
  // main loop iteration is pure overhead for a driver that links in the worker but never
  // actually calls async_copy()/async_move().
  this->disable_loop();
}

void StorageWorker::ensure_started_() {
  if (this->started_)
    return;
  this->started_ = true;
  this->enable_loop();

  this->pool_.init(this->max_pending_);
  for (size_t i = 0; i < this->max_pending_; i++) {
    this->pool_.emplace_back();  // default-construct in place — TransferRequest isn't movable
  }
  ESP_LOGCONFIG(TAG, "Request pool size: %zu", this->max_pending_);

  // The define is derived purely from drivers' task_safe flags in codegen; the platform
  // condition lives here so a task-safe driver on a non-FreeRTOS target degrades to
  // loop-sliced instead of failing to compile.
#if defined(USE_ESP32) && defined(USE_STORAGE_WORKER_TASK)
  this->task_queue_ = xQueueCreate(this->max_pending_, sizeof(size_t));
  if (this->task_queue_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create request queue — falling back to loop-sliced mode only");
    return;
  }

  // ESP-IDF's xTaskCreate takes the stack size in bytes (unlike vanilla FreeRTOS, which uses
  // words) — task_stack_size_ is passed through unconverted, same as other components' task
  // creation (see e.g. zigbee_esp32.cpp, usb_cdc_acm_esp32.cpp).
  BaseType_t ok = xTaskCreate(&StorageWorker::task_fn, "storage_worker", this->task_stack_size_, this,
                              this->task_priority_, &this->task_handle_);
  if (ok != pdPASS) {
    ESP_LOGE(TAG, "Failed to create worker task — falling back to loop-sliced mode only");
    this->task_handle_ = nullptr;
    vQueueDelete(this->task_queue_);
    this->task_queue_ = nullptr;
    return;
  }
  this->task_running_ = true;
#endif
}

bool StorageWorker::is_task_safe_(const TransferRequest &req) const {
  (void) req;  // unused on platforms without a background task (see the #else branch below)
#if defined(USE_ESP32) && defined(USE_STORAGE_WORKER_TASK)
  if (!this->task_running_)
    return false;
  uint8_t src_caps = req.src_storage->get_capabilities();
  uint8_t dst_caps = req.dst_storage->get_capabilities();
  return (src_caps & StorageCaps::STORAGE_CAP_IO_TASK_SAFE) != 0 &&
         (dst_caps & StorageCaps::STORAGE_CAP_IO_TASK_SAFE) != 0;
#else
  return false;
#endif
}

bool StorageWorker::overlaps_active_(const TransferRequest &candidate) const {
  for (const auto &req : this->pool_) {
    if (&req == &candidate)
      continue;
    RequestState state = req.state.load();
    if (state != RequestState::RUNNING && state != RequestState::CANCELLED)
      continue;
    if (req.src_storage == candidate.src_storage || req.src_storage == candidate.dst_storage ||
        req.dst_storage == candidate.src_storage || req.dst_storage == candidate.dst_storage)
      return true;
  }
  return false;
}

StorageError StorageWorker::submit_(RequestOp op, PathStorage *src, const char *src_path, PathStorage *dst,
                                    const char *dst_path, CompletionCallback &&on_done) {
  this->ensure_started_();

  if (strlen(src_path) >= STORAGE_WORKER_MAX_PATH || strlen(dst_path) >= STORAGE_WORKER_MAX_PATH)
    return StorageError::INVALID_ARGS;

  // Find a free slot. Backpressure: a full pool returns NOT_READY immediately, callback not
  // invoked — this reuses the existing "device not ready to serve calls" error rather than
  // adding a new value to the frozen StorageError enum (see the PR notes for the rationale).
  TransferRequest *slot = nullptr;
  for (auto &req : this->pool_) {
    RequestState expected = RequestState::FREE;
    if (req.state.compare_exchange_strong(expected, RequestState::PENDING)) {
      slot = &req;
      break;
    }
  }
  if (slot == nullptr)
    return StorageError::NOT_READY;

  slot->op = op;
  slot->src_storage = src;
  slot->dst_storage = dst;
  strncpy(slot->src_path, src_path, STORAGE_WORKER_MAX_PATH - 1);
  slot->src_path[STORAGE_WORKER_MAX_PATH - 1] = '\0';
  strncpy(slot->dst_path, dst_path, STORAGE_WORKER_MAX_PATH - 1);
  slot->dst_path[STORAGE_WORKER_MAX_PATH - 1] = '\0';
  slot->callback = std::move(on_done);
  slot->result = StorageError::OK;
  slot->offset = 0;
  slot->src_handle = nullptr;
  slot->dst_handle = nullptr;
  slot->handles_open = false;
  slot->chunk_buf.reset();
  slot->chunk_size = 0;
  slot->src_is_fs = src->get_storage_type() == StorageType::FILESYSTEM;
  slot->dst_is_fs = dst->get_storage_type() == StorageType::FILESYSTEM;

#if defined(USE_ESP32) && defined(USE_STORAGE_WORKER_TASK)
  // Skip task dispatch if another active (RUNNING/CANCELLED) request already shares a storage
  // with this one — two engines must never call the same storage instance concurrently. A
  // task-safe request that loses this race simply degrades to the loop-sliced engine below
  // instead of being re-dispatched to the task once the conflict clears; simpler, and the
  // request still completes correctly, just via the other engine.
  if (this->is_task_safe_(*slot) && !this->overlaps_active_(*slot)) {
    size_t index = slot - this->pool_.begin();
    slot->state = RequestState::RUNNING;
    if (xQueueSend(this->task_queue_, &index, 0) == pdTRUE) {
      return StorageError::OK;
    }
    // Queue send failed despite a free slot (shouldn't normally happen since the queue and
    // pool are sized identically) — fall through to loop-sliced handling instead of losing
    // the request.
    slot->state = RequestState::PENDING;
  }
#endif

  // Loop-sliced mode: leave the request PENDING; loop() picks up requests FIFO.
  return StorageError::OK;
}

StorageError StorageWorker::async_copy(PathStorage *src, const char *src_path, PathStorage *dst, const char *dst_path,
                                       CompletionCallback &&on_done) {
  return this->submit_(RequestOp::COPY, src, src_path, dst, dst_path, std::move(on_done));
}

StorageError StorageWorker::async_move(PathStorage *src, const char *src_path, PathStorage *dst, const char *dst_path,
                                       CompletionCallback &&on_done) {
  return this->submit_(RequestOp::MOVE, src, src_path, dst, dst_path, std::move(on_done));
}

void StorageWorker::on_storage_unregistered_(Storage *s) {
  // Synchronous cancel-and-drain: by the time this returns, no data-plane call into `s` is
  // still in flight, so the caller (unregister_storage(), on the main loop) can safely proceed
  // to unmount/tear down the driver right afterward.
  for (size_t i = 0; i < this->pool_.size(); i++) {
    TransferRequest &req = this->pool_[i];
    if (req.src_storage != s && req.dst_storage != s)
      continue;

    RequestState state = req.state.load();
    if (state == RequestState::PENDING) {
      // Not started yet — finish it immediately, no partial I/O to unwind.
      req.result = StorageError::NOT_READY;
      req.state = RequestState::DONE;
    } else if (state == RequestState::RUNNING) {
      if (i == this->loop_active_index_) {
        // Owned by the loop-sliced engine, which only ever advances from inside loop() — we
        // are on the main loop right now too, so nothing else can touch this request
        // concurrently. Cancel and drain it in place: run_chunk_()'s entry check sees
        // CANCELLED and calls finish_request() immediately, closing any open handles before
        // this function returns — i.e. before the driver's unmount() runs.
        req.state = RequestState::CANCELLED;
        this->run_chunk_(req);
        if (this->loop_active_index_ == i)
          this->loop_active_index_ = SIZE_MAX;
      } else {
#if defined(USE_ESP32) && defined(USE_STORAGE_WORKER_TASK)
        // Owned by the worker task (or, if it hasn't dequeued this index off task_queue_ yet,
        // about to be — run_chunk_()'s entry check still sees CANCELLED as soon as the task
        // does pick it up, so no special-casing is needed for that race). Set CANCELLED and
        // wait for the task to actually reach DONE — it can only observe the flag between
        // chunks, so this is bounded by at most one chunk's read+write (low single-digit ms
        // for SDMMC). Yield with vTaskDelay(1) rather than busy-spinning so the task (which
        // may run at the same or a lower priority) actually gets CPU time to reach that
        // check — a tight spin here on a single-core target would starve it and deadlock
        // until the timeout below.
        req.state = RequestState::CANCELLED;
        uint32_t start = millis();
        while (req.state.load() != RequestState::DONE) {
          if (millis() - start > 500) {
            ESP_LOGE(TAG, "Timed out waiting for in-flight transfer to cancel — the storage medium "
                          "is likely gone; proceeding with unmount anyway. Behavior from here is "
                          "undefined if the task is still touching it (e.g. still holding a file "
                          "handle) when it does eventually finish.");
            // Discard the callback now: if the task does reach DONE later, loop() must only
            // free the slot, not fire a completion into a context whose storage object may
            // no longer exist post-unmount. The slot stays stuck (RUNNING/CANCELLED, never
            // recycled) if the task never finishes — acceptable since the medium is dead
            // anyway at that point.
            req.callback = nullptr;
            break;
          }
          // pdMS_TO_TICKS(1) rounds down to 0 ticks at the default 100 Hz tick rate (10 ms per
          // tick), which would make this a pure yield with no actual delay. Passing 1 directly
          // requests one full tick (10 ms at the default rate) instead.
          vTaskDelay(1);
        }
#endif
      }
    }
  }
}

void StorageWorker::loop() {
  // Deliver completions and free slots. Runs regardless of which engine finished the
  // request, so this is the single place user callbacks are invoked — always on the main
  // loop, per the public API's contract.
  for (auto &req : this->pool_) {
    if (req.state.load() == RequestState::DONE) {
      CompletionCallback cb = std::move(req.callback);
      StorageError result = req.result;
      req.callback = nullptr;
      req.src_storage = nullptr;
      req.dst_storage = nullptr;
      req.stuck_warned = false;
      req.state = RequestState::FREE;
      if (cb)
        cb(result);
    }
  }

  if (this->loop_active_index_ == SIZE_MAX) {
    // Pick the next PENDING request, FIFO by pool position (pool order matches submission
    // order well enough in practice — this component makes no stronger ordering promise).
    // Skip any request that overlaps a storage with something already RUNNING/CANCELLED (e.g.
    // on the worker task) — it must wait its turn rather than run concurrently with it.
    for (size_t i = 0; i < this->pool_.size(); i++) {
      TransferRequest &candidate = this->pool_[i];
      if (candidate.state.load() != RequestState::PENDING)
        continue;
      if (this->overlaps_active_(candidate)) {
        // Known limitation: if the request it overlaps with is stuck (its owning engine never
        // reached DONE after a drain timeout — see on_storage_unregistered_()), this request
        // waits forever and its callback never fires. There is no reclaim path for the stuck
        // slot itself: by the time a drain times out, the medium is presumed gone and the
        // engine that owns it may still be touching it, so freeing the slot out from under
        // that engine would be unsafe. Log once per request so this is at least diagnosable.
        if (!candidate.stuck_warned) {
          ESP_LOGW(TAG, "Request pending indefinitely — blocked by another request on the same "
                        "storage that never completed (likely a stuck slot after a drain "
                        "timeout)");
          candidate.stuck_warned = true;
        }
        continue;
      }
      candidate.state = RequestState::RUNNING;
      this->loop_active_index_ = i;
      break;
    }
  }

  if (this->loop_active_index_ != SIZE_MAX) {
    TransferRequest &req = this->pool_[this->loop_active_index_];
    this->run_chunk_(req);
    if (req.state.load() == RequestState::DONE)
      this->loop_active_index_ = SIZE_MAX;
  }
}

#if defined(USE_ESP32) && defined(USE_STORAGE_WORKER_TASK)
void StorageWorker::task_fn(void *arg) { static_cast<StorageWorker *>(arg)->task_loop_(); }

void StorageWorker::task_loop_() {
  // No ESPHome facilities are touched from this task besides is_registered() (mutex-guarded)
  // and ESP_LOG (task-safe on IDF, kept to errors) — see run_chunk_()'s cancellation check.
  // Deliberately NOT subscribed to the task watchdog: this task blocks on I/O by design, and
  // the chunked design here (like the synchronous copy() helper) already keeps any single
  // blocking call short.
  for (;;) {
    size_t index;
    if (xQueueReceive(this->task_queue_, &index, portMAX_DELAY) != pdTRUE)
      continue;

    TransferRequest &req = this->pool_[index];
    // Loop until DONE, not just while RUNNING: if the hotplug handler flips the state to
    // CANCELLED between iterations, run_chunk_()'s entry check is what actually closes
    // handles, frees the chunk buffer, and transitions to DONE. Stopping on state != RUNNING
    // would exit before that ever happens, leaking the handles/buffer and leaving the slot
    // (and the pending callback) stuck forever.
    while (req.state.load() != RequestState::DONE) {
      this->run_chunk_(req);
    }
    // DONE reached; loop() on the main loop delivers the completion callback and frees the
    // slot.
  }
}
#endif

namespace {

// Finishes a request: closes any open handles (best-effort — a close failure only overrides
// the result if the transfer itself had otherwise succeeded, mirroring storage::copy()'s
// close-error propagation) and marks it DONE for loop()/task_loop_() to deliver.
void finish_request(TransferRequest &req, StorageError result) {
  if (req.handles_open) {
    if (req.src_is_fs && req.src_handle != nullptr)
      static_cast<FilesystemStorage *>(req.src_storage)->close(req.src_handle);
    if (req.dst_is_fs && req.dst_handle != nullptr) {
      StorageError close_err = static_cast<FilesystemStorage *>(req.dst_storage)->close(req.dst_handle);
      if (result == StorageError::OK)
        result = close_err;
    }
  }
  req.chunk_buf.reset();
  req.result = result;
  req.state = RequestState::DONE;
}

}  // namespace

void StorageWorker::run_chunk_(TransferRequest &req) {
  // Cancellation / hotplug check, before doing any I/O this call.
  if (req.state.load() == RequestState::CANCELLED) {
    finish_request(req, StorageError::NOT_READY);
    return;
  }
  if (global_storage_registry != nullptr && (!global_storage_registry->is_registered(req.src_storage) ||
                                             !global_storage_registry->is_registered(req.dst_storage))) {
    finish_request(req, StorageError::NOT_READY);
    return;
  }

  // First call for this request: do the cheap same-storage rename() fast path for MOVE, or
  // open handles / size-check for everything else that needs the chunk loop.
  if (!req.handles_open) {
    if (req.op == RequestOp::MOVE && req.src_storage == req.dst_storage) {
      StorageError err = req.src_storage->rename(req.src_path, req.dst_path);
      finish_request(req, err);
      return;
    }

    size_t chunk_size = STORAGE_COPY_CHUNK_SIZE;
    uint8_t *raw = nullptr;
    while (chunk_size >= 4096) {
      raw = RAMAllocator<uint8_t>(RAMAllocator<uint8_t>::PREFER_INTERNAL).allocate(chunk_size);
      if (raw != nullptr)
        break;
      chunk_size /= 2;
    }
    if (raw == nullptr) {
      finish_request(req, StorageError::NO_SPACE);
      return;
    }
    req.chunk_buf = RamBuffer(raw, RamBufferDeleter{chunk_size});
    req.chunk_size = chunk_size;

    if (req.src_is_fs) {
      StorageError err =
          static_cast<FilesystemStorage *>(req.src_storage)->open(req.src_path, req.src_handle, OpenMode::READ);
      if (err != StorageError::OK) {
        finish_request(req, err);
        return;
      }
    }
    if (req.dst_is_fs) {
      StorageError err =
          static_cast<FilesystemStorage *>(req.dst_storage)->open(req.dst_path, req.dst_handle, OpenMode::WRITE);
      if (err != StorageError::OK) {
        req.handles_open = true;  // src handle (if any) is open — let finish_request() close it
        finish_request(req, err);
        return;
      }
    }
    req.handles_open = true;
  }

  // One chunk: read, then write it out fully (write_chunk/write can themselves return partial
  // writes — loop here same as the synchronous copy() helper does).
  size_t bytes_read = 0;
  StorageError err;
  if (req.src_is_fs) {
    err = static_cast<FilesystemStorage *>(req.src_storage)
              ->read(req.src_handle, req.chunk_buf.get(), req.chunk_size, &bytes_read);
  } else {
    err = static_cast<NetworkStorage *>(req.src_storage)
              ->read_chunk(req.src_path, req.chunk_buf.get(), req.offset, req.chunk_size, &bytes_read);
  }
  if (err != StorageError::OK) {
    finish_request(req, err);
    return;
  }
  if (bytes_read == 0) {
    // EOF.
    if (req.op == RequestOp::MOVE) {
      // Cross-storage move: the copy succeeded, now remove the source. Per move()'s
      // documented semantics, if this remove fails the destination copy is kept rather than
      // rolled back — the caller ends up with the file in both places, not neither.
      err = req.src_storage->remove(req.src_path);
    }
    finish_request(req, err);
    return;
  }

  size_t total_written = 0;
  while (total_written < bytes_read) {
    size_t bytes_written = 0;
    if (req.dst_is_fs) {
      err =
          static_cast<FilesystemStorage *>(req.dst_storage)
              ->write(req.dst_handle, req.chunk_buf.get() + total_written, bytes_read - total_written, &bytes_written);
    } else {
      err = static_cast<NetworkStorage *>(req.dst_storage)
                ->write_chunk(req.dst_path, req.chunk_buf.get() + total_written, req.offset + total_written,
                              bytes_read - total_written, &bytes_written);
    }
    if (err != StorageError::OK) {
      finish_request(req, err);
      return;
    }
    if (bytes_written == 0) {
      finish_request(req, StorageError::WRITE_ERROR);
      return;
    }
    total_written += bytes_written;
  }

  req.offset += bytes_read;
  // Request stays RUNNING; the next call (next loop() iteration, or the task's own loop)
  // picks up at the new offset. No watchdog feed here by design — see task_loop_()'s comment
  // for the task path; the loop-sliced path returns to the main loop's own feed_wdt() between
  // iterations same as any other component's loop().
}

}  // namespace esphome::storage

#endif  // USE_STORAGE_WORKER
