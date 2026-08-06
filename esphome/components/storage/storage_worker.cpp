#include "storage_worker.h"
#include "esphome/core/log.h"
#include "esphome/core/wake.h"

#include <cstring>

#ifdef USE_STORAGE_WORKER

namespace esphome::storage {

static const char *const TAG = "storage_worker";

StorageWorker *global_storage_worker = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

void StorageWorker::setup() {
  // Pool and (on ESP32) task creation are deferred to the first submit_() call -- see
  // ensure_started_() -- so that a driver merely linking in the worker (because it's
  // path-based) but never actually issuing an async transfer pays no RAM/task cost at all.
  // Only the hotplug subscription happens here: it must be in place before any storage could
  // possibly be unregistered, and it is a single lambda capture, not an allocation of note.
  if (global_storage_registry != nullptr) {
    // Drain channel, not the unregister channel: a safe-eject quiesces without unregistering,
    // and the worker has to let go of the device in that case too.
    global_storage_registry->add_on_quiesce_callback([this](Storage *s) { this->on_storage_unregistered_(s); });
  } else {
    // The registry (BUS priority) is guaranteed to exist by DATA, this component's setup priority.
    // If it somehow does not, the drain contract cannot be honored -- fail loudly rather than let
    // in-flight I/O continue against an unmounted medium.
    ESP_LOGE(TAG, "storage registry unavailable -- cannot honor the drain contract");
    this->mark_failed();
  }
  // PollingComponent::call_setup() auto-starts the poller before setup(); stop it here so an
  // idle worker schedules nothing. The first submit funnel (start_poller()) arms it when work
  // actually arrives, and update() stops it again once everything is FREE.
  this->stop_poller();
}

void StorageWorker::dump_config() {
  // Fixed at compile time (STORAGE_COPY_CHUNK_SIZE and the variant flags) -- surfaces the
  // resolved buffer policy in the boot log. Loop path is always `chunk` bytes internal (20 ms
  // budget); the worker task on S3/P4 stages a 64 kB (P4) / 32 kB (S3) chunk in DMA-capable PSRAM.
  const size_t chunk = STORAGE_COPY_CHUNK_SIZE;
#if defined(USE_ESP32_VARIANT_ESP32S3)
  const char *platform = "ESP32-S3";
  const bool psram_dma_capable = true;
#elif defined(USE_ESP32_VARIANT_ESP32P4)
  const char *platform = "ESP32-P4";
  const bool psram_dma_capable = true;
#elif defined(USE_ESP32_VARIANT_ESP32)
  const char *platform = "ESP32";
  const bool psram_dma_capable = false;
#else
  const char *platform = "generic";
  const bool psram_dma_capable = false;
#endif
  ESP_LOGCONFIG(TAG, "Storage worker:");
  ESP_LOGCONFIG(TAG, "  Loop chunk: %u bytes (internal RAM, DMA-capable)", (unsigned) chunk);
  if (psram_dma_capable) {
#if defined(USE_ESP32_VARIANT_ESP32P4)
    const unsigned task_chunk = 65536;
#else
    const unsigned task_chunk = 32768;
#endif
    ESP_LOGCONFIG(TAG, "  Task chunk: %u bytes (PSRAM, DMA-capable)", task_chunk);
  } else {
    ESP_LOGCONFIG(TAG, "  Task chunk: %u bytes (internal RAM, DMA-capable)", (unsigned) chunk);
  }
  ESP_LOGCONFIG(TAG, "  Platform: %s (PSRAM-DMA %s)", platform, psram_dma_capable ? "yes" : "no");
}

void StorageWorker::ensure_started_() {
  if (this->started_)
    return;
  this->started_ = true;

  this->pool_.init(this->max_pending_);
  for (size_t i = 0; i < this->max_pending_; i++) {
    this->pool_.emplace_back();  // default-construct in place -- TransferRequest isn't movable
  }
  ESP_LOGD(TAG, "Request pool size: %zu", this->max_pending_);

  this->stream_pool_.init(this->max_streams_);
  for (size_t i = 0; i < this->max_streams_; i++) {
    this->stream_pool_.emplace_back();
  }
  ESP_LOGD(TAG, "Stream pool size: %zu", this->max_streams_);

  // The define is derived purely from drivers' task_safe flags in codegen; the platform
  // condition lives here so a task-safe driver on a non-FreeRTOS target degrades to
  // loop-sliced instead of failing to compile.
#if defined(USE_ESP32) && defined(USE_STORAGE_WORKER_TASK)
  // One shared queue sized for the worst case of both pools being simultaneously dispatched to
  // the task -- QueueEntry tags which pool/index each entry refers to (see task_loop_() below).
  this->task_queue_ = xQueueCreate(this->max_pending_ + this->max_streams_, sizeof(QueueEntry));
  if (this->task_queue_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create request queue -- falling back to loop-sliced mode only");
    return;
  }

  // ESP-IDF's xTaskCreate takes the stack size in bytes (unlike vanilla FreeRTOS, which uses
  // words) -- task_stack_size_ is passed through unconverted, same as other components' task
  // creation (see e.g. zigbee_esp32.cpp, usb_cdc_acm_esp32.cpp).
  BaseType_t ok = xTaskCreate(&StorageWorker::task_fn, "storage_worker", this->task_stack_size_, this,
                              this->task_priority_, &this->task_handle_);
  if (ok != pdPASS) {
    ESP_LOGE(TAG, "Failed to create worker task -- falling back to loop-sliced mode only");
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
  // format() touches exactly one storage (the target in dst_storage), no file side.
  if (req.op == RequestOp::FORMAT)
    return (req.dst_storage->get_capabilities() & StorageCaps::STORAGE_CAP_IO_TASK_SAFE) != 0;
  // Every storage this op touches must be task-safe, or a background task would race the main
  // loop on it. A copy/move touches src_storage AND dst_storage (both non-null, no raw_device).
  // A raw op touches raw_device AND exactly one path-storage side (the file; the other side is
  // null) -- so we OR the two path sides to pick the one that is set, and additionally require
  // the raw device's own capability.
  if (req.raw_device != nullptr) {
    const Storage *file_side = req.src_storage != nullptr ? req.src_storage : req.dst_storage;
    if (file_side == nullptr)  // raw_erase: device only, no file side
      return (req.raw_device->get_capabilities() & StorageCaps::STORAGE_CAP_IO_TASK_SAFE) != 0;
    return (req.raw_device->get_capabilities() & StorageCaps::STORAGE_CAP_IO_TASK_SAFE) != 0 &&
           (file_side->get_capabilities() & StorageCaps::STORAGE_CAP_IO_TASK_SAFE) != 0;
  }
  uint8_t src_caps = req.src_storage->get_capabilities();
  uint8_t dst_caps = req.dst_storage->get_capabilities();
  return (src_caps & StorageCaps::STORAGE_CAP_IO_TASK_SAFE) != 0 &&
         (dst_caps & StorageCaps::STORAGE_CAP_IO_TASK_SAFE) != 0;
#else
  return false;
#endif
}

// A stateless network storage answers NOT_READY while it is (re)connecting -- the very call
// that got the answer has just woken it (see NFSClient::wake_if_unmounted_()). Within this
// window the request simply tries again next pass instead of dying; after it, NOT_READY is
// the honest final answer (server truly gone).
static constexpr uint32_t NETWORK_READY_WINDOW_MS = 20000;

// Whether a request walks a directory tree. Read the op, not the presence of req.tree: the
// walk allocation stays on the slot after a tree finishes so the next one reuses it, so a
// non-null tree says nothing about what this request is.
static bool is_tree_op(RequestOp op) { return op == RequestOp::COPY_TREE || op == RequestOp::MOVE_TREE; }

// Gives `req` a zeroed walk, reusing the slot's existing allocation when there is one -- ~800
// bytes of allocate/free per tree operation is the churn this avoids. False if it cannot be had.
static bool ensure_tree(TransferRequest &req) {
  if (req.tree == nullptr) {
    req.tree = make_unique<TreeWalk>();
    return req.tree != nullptr;
  }
  *req.tree = TreeWalk{};
  return true;
}

bool StorageWorker::wait_for_network_ready_(TransferRequest &req, StorageError err, const Storage *side) {
  if (err != StorageError::NOT_READY || side == nullptr || side->get_storage_type() != StorageType::NETWORK)
    return false;
  if (millis() - req.submitted_ms > NETWORK_READY_WINDOW_MS)
    return false;
  if (!req.waiting_logged) {
    req.waiting_logged = true;
    ESP_LOGD(TAG, "Network storage not ready yet -- waiting for it to come up");
  }
  req.network_waiting = true;
  return true;  // stay RUNNING; the loop retries this request on the next pass
}

// A stream whose step is queued or running is calling into its storage right now. An IDLE one
// only holds a handle open, which the interface permits alongside other work -- so it does not
// contend, or an upload sitting between chunks would block every transfer on that storage.
static bool stream_step_active(StreamState state) {
  return state == StreamState::OPENING || state == StreamState::WRITING || state == StreamState::READING ||
         state == StreamState::SEEKING || state == StreamState::TELLING || state == StreamState::CLOSING ||
         state == StreamState::CANCELLED;
}

bool StorageWorker::overlaps_active_(const TransferRequest &candidate) const {
  for (const auto &req : this->pool_) {
    if (&req == &candidate)
      continue;
    RequestState state = req.state.load();
    if (state != RequestState::RUNNING && state != RequestState::CANCELLED)
      continue;
    // Null sides (raw ops carry only one path-storage side) must not match other nulls.
    if ((req.src_storage != nullptr &&
         (req.src_storage == candidate.src_storage || req.src_storage == candidate.dst_storage)) ||
        (req.dst_storage != nullptr &&
         (req.dst_storage == candidate.src_storage || req.dst_storage == candidate.dst_storage)) ||
        (req.raw_device != nullptr && req.raw_device == candidate.raw_device))
      return true;
  }
  // Streams drive the same storages through the same drivers -- see stream_overlaps_active_().
  for (const auto &sreq : this->stream_pool_) {
    if (sreq.storage == nullptr || !stream_step_active(sreq.state.load()))
      continue;
    if (sreq.storage == candidate.src_storage || sreq.storage == candidate.dst_storage)
      return true;
  }
  return false;
}

bool StorageWorker::stream_overlaps_active_(const StreamRequest &candidate, bool from_loop) const {
  if (candidate.storage == nullptr)
    return false;
  for (const auto &req : this->pool_) {
    RequestState state = req.state.load();
    if (state != RequestState::RUNNING && state != RequestState::CANCELLED)
      continue;
    if (req.src_storage == candidate.storage || req.dst_storage == candidate.storage)
      return true;
  }
  for (const auto &sreq : this->stream_pool_) {
    if (&sreq == &candidate || sreq.storage != candidate.storage)
      continue;
    if (from_loop && sreq.pending_step_)
      continue;  // queued for this same thread -- it runs after us, it does not race us
    if (stream_step_active(sreq.state.load()))
      return true;
  }
  return false;
}

bool StorageWorker::is_busy_with(const storage::Storage *storage) const {
  if (storage == nullptr)
    return false;
  // PENDING counts too: a submitted-but-not-yet-started job already "owns" the device from
  // the caller's point of view, so unmounting out from under it must wait.
  for (const auto &req : this->pool_) {
    RequestState state = req.state.load();
    if (state == RequestState::FREE || state == RequestState::DONE)
      continue;
    if (req.src_storage == storage || req.dst_storage == storage)
      return true;
  }
  // A stream counts whatever it is doing, IDLE included: unlike the contention question above,
  // this one asks whether the medium may be taken away -- and an idle stream still holds an
  // open handle on it.
  for (const auto &sreq : this->stream_pool_) {
    StreamState state = sreq.state.load();
    if (state == StreamState::FREE || state == StreamState::DONE)
      continue;
    if (sreq.storage == storage)
      return true;
  }
  return false;
}

StorageError StorageWorker::submit_(RequestOp op, PathStorage *src, const char *src_path, PathStorage *dst,
                                    const char *dst_path, CompletionCallback &&on_done, TransferJob *job_out,
                                    bool overwrite) {
  this->ensure_started_();

  if (strlen(src_path) >= STORAGE_WORKER_MAX_PATH || strlen(dst_path) >= STORAGE_WORKER_MAX_PATH)
    return StorageError::INVALID_ARGS;

  // Find a free slot. Backpressure: a full pool returns NOT_READY immediately, callback not
  // invoked -- this reuses the existing "device not ready to serve calls" error rather than
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

  // Work is pending from here on: start the poller (idempotent -- set_interval replaces any
  // existing one). The scheduler then services update() every update_interval until update()
  // stops the poller again once every slot and stream is FREE. This is the single funnel for
  // transfers submitted through submit_(); submit_raw_() and dispatch_stream_step_() have
  // their own start_poller() so every new job -- whatever its source -- arms the engine.
  this->start_poller();

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
  slot->submitted_ms = millis();
  slot->waiting_logged = false;
  slot->last_progress_ms = millis();
  slot->progress_mark = 0;
  slot->overwrite = overwrite;
  slot->pre_phase_done = false;
  slot->cancel_result = StorageError::NOT_READY;
  // A recycled slot may have carried a raw job before: clear the device pointer and cursors,
  // otherwise overlaps_active_() sees a stale raw_device on this plain copy/move and
  // serializes unrelated transfers against it -- up to blocking them indefinitely.
  slot->raw_device = nullptr;
  slot->raw_address = 0;
  slot->raw_erase_pos = 0;
  slot->raw_erase_end = 0;
  slot->force_sliced_erase = false;
  slot->verify_passes = 0;
  slot->verify_pass_done = 0;
  slot->verifying = false;
  slot->src_is_fs = src->get_storage_type() == StorageType::FILESYSTEM;
  slot->dst_is_fs = dst->get_storage_type() == StorageType::FILESYSTEM;
  // Tree ops keep their roots aside: src_path/dst_path are reused for the file currently in
  // flight, so everything below this point works on a tree exactly as it does on one file.
  if (op == RequestOp::COPY_TREE || op == RequestOp::MOVE_TREE) {
    if (!ensure_tree(*slot)) {
      slot->state = RequestState::FREE;
      return StorageError::NO_SPACE;  // same answer the chunk buffer gives when it cannot be had
    }
    strncpy(slot->tree->src_root, src_path, STORAGE_WORKER_MAX_PATH - 1);
    strncpy(slot->tree->dst_root, dst_path, STORAGE_WORKER_MAX_PATH - 1);
  }
  slot->bytes_done = 0;
  slot->bytes_total = 0;
  slot->file_done = 0;
  slot->file_total = 0;
  // Bump the generation on claim (skipping 0) so stale TransferJob handles from a previous
  // occupant of this slot stop resolving. Main loop only -- submissions never race each other.
  if (++slot->generation == 0)
    slot->generation = 1;
  if (job_out != nullptr) {
    *job_out = make_transfer_job(slot->generation, slot - this->pool_.begin());
  }

#if defined(USE_ESP32) && defined(USE_STORAGE_WORKER_TASK)
  // Skip task dispatch if another active (RUNNING/CANCELLED) request already shares a storage
  // with this one -- two engines must never call the same storage instance concurrently. A
  // task-safe request that loses this race simply degrades to the loop-sliced engine below
  // instead of being re-dispatched to the task once the conflict clears; simpler, and the
  // request still completes correctly, just via the other engine.
  if (this->is_task_safe_(*slot) && !this->overlaps_active_(*slot)) {
    QueueEntry entry{QueueEntryKind::TRANSFER, static_cast<size_t>(slot - this->pool_.begin())};
    slot->state = RequestState::RUNNING;
    if (xQueueSend(this->task_queue_, &entry, 0) == pdTRUE) {
      return StorageError::OK;
    }
    // Queue send failed despite a free slot (shouldn't normally happen since the queue and
    // pool are sized identically) -- fall through to loop-sliced handling instead of losing
    // the request.
    slot->state = RequestState::PENDING;
  }
#endif

  // Loop-sliced mode: leave the request PENDING; loop() picks up requests FIFO.
  return StorageError::OK;
}

StorageError StorageWorker::async_raw_read(RawStorage *device, uint64_t address, uint64_t size, PathStorage *dst,
                                           const char *dst_path, CompletionCallback &&on_done, TransferJob *job_out,
                                           bool overwrite) {
  if (device == nullptr || dst == nullptr || dst_path == nullptr || size == 0)
    return StorageError::INVALID_ARGS;
  return this->submit_raw_(RequestOp::RAW_READ_TO_FILE, device, address, size, dst, dst_path, false, overwrite,
                           std::move(on_done), job_out);
}

StorageError StorageWorker::async_raw_write(PathStorage *src, const char *src_path, RawStorage *device,
                                            uint64_t address, bool erase_first, CompletionCallback &&on_done,
                                            TransferJob *job_out, uint8_t verify_passes) {
  if (device == nullptr || src == nullptr || src_path == nullptr)
    return StorageError::INVALID_ARGS;
  return this->submit_raw_(RequestOp::RAW_WRITE_FROM_FILE, device, address, 0, src, src_path, erase_first, false,
                           std::move(on_done), job_out, false, verify_passes);
}

StorageError StorageWorker::async_raw_verify_file(PathStorage *src, const char *src_path, RawStorage *device,
                                                  uint64_t address, CompletionCallback &&on_done, TransferJob *job_out,
                                                  uint8_t verify_passes) {
  if (device == nullptr || src == nullptr || src_path == nullptr)
    return StorageError::INVALID_ARGS;
  // No erase, never a write -- the pre-phase enters the verify phase directly.
  return this->submit_raw_(RequestOp::RAW_VERIFY_FILE, device, address, 0, src, src_path, false, false,
                           std::move(on_done), job_out, false, verify_passes == 0 ? 1 : verify_passes);
}

StorageError StorageWorker::async_raw_erase(RawStorage *device, uint64_t address, uint64_t size,
                                            CompletionCallback &&on_done, TransferJob *job_out, bool force_sliced) {
  if (device == nullptr || size == 0)
    return StorageError::INVALID_ARGS;
  return this->submit_raw_(RequestOp::RAW_ERASE, device, address, size, nullptr, "", true, false, std::move(on_done),
                           job_out, force_sliced);
}

StorageError StorageWorker::async_format(FilesystemStorage *target, CompletionCallback &&on_done,
                                         TransferJob *job_out) {
  this->ensure_started_();
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
  slot->op = RequestOp::FORMAT;
  // The target rides in dst_storage so the registry check, overlaps_active_() and the
  // completion drain treat it like any transfer; src is null and no handle is ever opened.
  slot->src_storage = nullptr;
  slot->dst_storage = target;
  slot->src_path[0] = '\0';
  slot->dst_path[0] = '\0';
  slot->raw_device = nullptr;
  slot->raw_address = 0;
  slot->raw_erase_pos = 0;
  slot->raw_erase_end = 0;
  slot->force_sliced_erase = false;
  slot->verify_passes = 0;
  slot->verify_pass_done = 0;
  slot->verifying = false;
  slot->cancel_result = StorageError::NOT_READY;
  slot->callback = std::move(on_done);
  slot->result = StorageError::OK;
  slot->offset = 0;
  slot->src_handle = nullptr;
  slot->dst_handle = nullptr;
  slot->handles_open = false;
  slot->src_is_fs = false;
  slot->dst_is_fs = false;  // no handle to close -- keep close_handles() a no-op
  slot->pre_phase_done = false;
  slot->bytes_done = 0;
  slot->bytes_total = 0;
  slot->file_done = 0;
  slot->file_total = 0;
  slot->submitted_ms = millis();
  slot->waiting_logged = false;
  slot->last_progress_ms = millis();
  slot->progress_mark = 0;
  if (++slot->generation == 0)
    slot->generation = 1;
  if (job_out != nullptr)
    *job_out = make_transfer_job(slot->generation, slot - this->pool_.begin());

  // Arm the engine so the loop-sliced path promotes PENDING -> RUNNING and, on the task path, the
  // completion is delivered from update(); mirrors submit_()/submit_raw_().
  this->start_poller();

#if defined(USE_ESP32) && defined(USE_STORAGE_WORKER_TASK)
  // Task-safe target and nothing active already holds it -> run the single blocking format()
  // on the worker task, main loop free. Otherwise fall through to the loop-sliced engine,
  // which runs it from a loop() step (still one blocking call, but owned).
  if (this->is_task_safe_(*slot) && !this->overlaps_active_(*slot)) {
    QueueEntry entry{QueueEntryKind::TRANSFER, static_cast<size_t>(slot - this->pool_.begin())};
    slot->state = RequestState::RUNNING;
    if (xQueueSend(this->task_queue_, &entry, 0) == pdTRUE)
      return StorageError::OK;
    slot->state = RequestState::PENDING;
  }
#endif
  return StorageError::OK;
}

StorageError StorageWorker::submit_raw_(RequestOp op, RawStorage *device, uint64_t address, uint64_t size,
                                        PathStorage *file_side, const char *file_path, bool erase_first, bool overwrite,
                                        CompletionCallback &&on_done, TransferJob *job_out, bool force_sliced,
                                        uint8_t verify_passes) {
  this->ensure_started_();
  if (strlen(file_path) >= STORAGE_WORKER_MAX_PATH)
    return StorageError::INVALID_ARGS;
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
  // The file side rides in the regular storage/path fields so close_handles(), progress and
  // the completion drain treat a raw op exactly like any transfer. The unused side is null.
  const bool file_is_src = op == RequestOp::RAW_WRITE_FROM_FILE || op == RequestOp::RAW_VERIFY_FILE;
  slot->src_storage = file_is_src ? file_side : nullptr;
  slot->dst_storage = file_is_src ? nullptr : file_side;
  strncpy(file_is_src ? slot->src_path : slot->dst_path, file_path, STORAGE_WORKER_MAX_PATH - 1);
  (file_is_src ? slot->src_path : slot->dst_path)[STORAGE_WORKER_MAX_PATH - 1] = '\0';
  (file_is_src ? slot->dst_path : slot->src_path)[0] = '\0';
  slot->raw_device = device;
  slot->raw_address = address;
  slot->raw_erase_pos = 0;
  slot->raw_erase_end = erase_first ? 1 : 0;  // pre-phase converts to a real byte range
  slot->force_sliced_erase = force_sliced;
  slot->verify_passes = verify_passes;
  slot->verify_pass_done = 0;
  slot->verifying = false;
  slot->overwrite = overwrite;
  slot->pre_phase_done = false;
  slot->cancel_result = StorageError::NOT_READY;
  slot->callback = std::move(on_done);
  slot->result = StorageError::OK;
  slot->offset = 0;
  slot->src_handle = nullptr;
  slot->dst_handle = nullptr;
  slot->handles_open = false;
  slot->bytes_done = 0;
  slot->bytes_total = size;  // read: known now; write: pre-phase stats the file
  slot->file_done = 0;
  slot->file_total = size;
  slot->submitted_ms = millis();
  slot->waiting_logged = false;
  slot->last_progress_ms = millis();
  slot->progress_mark = 0;
  const bool file_fs = file_side != nullptr && file_side->get_storage_type() == StorageType::FILESYSTEM;
  slot->src_is_fs = file_fs && file_is_src;
  slot->dst_is_fs = file_fs && !file_is_src;
  if (++slot->generation == 0)
    slot->generation = 1;
  if (job_out != nullptr)
    *job_out = make_transfer_job(slot->generation, slot - this->pool_.begin());

  // Arm the engine before dispatch so a task-queued raw op still gets its completion delivered
  // from update(); mirrors submit_(). Fixes the early-return-before-start_poller() path.
  this->start_poller();

#if defined(USE_ESP32) && defined(USE_STORAGE_WORKER_TASK)
  // A raw device that declared itself alone on a hardware bus (assume_exclusive_bus) reports
  // STORAGE_CAP_IO_TASK_SAFE, so its blocking I/O may run on the worker task instead of the
  // main loop. Same contract as the copy path: only dispatch when every storage the op touches
  // is task-safe (is_task_safe_ checks raw_device + the file side) and nothing active already
  // holds one of them. Otherwise -- the common case of a shared-bus device -- fall through to the
  // loop-sliced engine below, exactly as before.
  if (this->is_task_safe_(*slot) && !this->overlaps_active_(*slot)) {
    QueueEntry entry{QueueEntryKind::TRANSFER, static_cast<size_t>(slot - this->pool_.begin())};
    slot->state = RequestState::RUNNING;
    if (xQueueSend(this->task_queue_, &entry, 0) == pdTRUE) {
      return StorageError::OK;
    }
    // Queue send failed despite a free slot -- fall through to loop-sliced rather than lose it.
    slot->state = RequestState::PENDING;
  }
#endif
  // Loop-sliced engine: raw devices without the task-safe opt-in are main-loop citizens
  // (esp_flash & friends), and this is also the fallback when task dispatch is skipped above.
  // (The poller is already armed above.)
  return StorageError::OK;
}

StorageError StorageWorker::async_copy(PathStorage *src, const char *src_path, PathStorage *dst, const char *dst_path,
                                       CompletionCallback &&on_done, TransferJob *job_out, bool overwrite) {
  return this->submit_(RequestOp::COPY, src, src_path, dst, dst_path, std::move(on_done), job_out, overwrite);
}

StorageError StorageWorker::async_move(PathStorage *src, const char *src_path, PathStorage *dst, const char *dst_path,
                                       CompletionCallback &&on_done, TransferJob *job_out, bool overwrite) {
  return this->submit_(RequestOp::MOVE, src, src_path, dst, dst_path, std::move(on_done), job_out, overwrite);
}

StorageError StorageWorker::async_copy_tree(PathStorage *src, const char *src_path, PathStorage *dst,
                                            const char *dst_path, CompletionCallback &&on_done, TransferJob *job_out) {
  return this->submit_(RequestOp::COPY_TREE, src, src_path, dst, dst_path, std::move(on_done), job_out);
}

StorageError StorageWorker::async_move_tree(PathStorage *src, const char *src_path, PathStorage *dst,
                                            const char *dst_path, CompletionCallback &&on_done, TransferJob *job_out) {
  return this->submit_(RequestOp::MOVE_TREE, src, src_path, dst, dst_path, std::move(on_done), job_out);
}

bool StorageWorker::get_transfer_status(TransferJob job, TransferStatus *out) const {
  if (job == INVALID_TRANSFER_JOB || out == nullptr)
    return false;
  size_t slot_index = transfer_job_slot(job);
  uint32_t generation = transfer_job_generation(job);
  // Lazy init: before the first submission the pool is empty and no job can exist.
  if (slot_index >= this->pool_.size())
    return false;
  const TransferRequest &req = this->pool_[slot_index];
  // A recycled slot bumped its generation on claim; a freed slot is FREE. Either way the
  // handle is expired -- the final DONE snapshot is only observable until loop() releases the
  // slot after the completion callback (see the header comment on capturing final results).
  if (req.generation != generation)
    return false;
  RequestState state = req.state.load();
  if (state == RequestState::FREE)
    return false;
  out->state = state;
  out->result = req.result;
  out->bytes_done = req.bytes_done.load();
  out->bytes_total = req.bytes_total.load();
  out->file_done = req.file_done.load();
  out->file_total = req.file_total.load();
  // Coarse phase for the status line. Only raw write/verify jobs have phases; a running job
  // is verifying if the chunk loop flagged it, still erasing while the erase cursor trails its
  // end, otherwise writing. verify_pass is 1-based within the verify phase.
  out->phase = TransferPhase::NONE;
  out->verify_passes = req.verify_passes;
  out->verify_pass = 0;
  if (state == RequestState::RUNNING) {
    // verifying is atomic; the erase cursors and req.op read just below can tear against the task
    // for one poll frame -- an accepted, bounded race (wrong phase label at worst, never data).
    if (req.verifying) {
      out->phase = TransferPhase::VERIFY;
      out->verify_pass = static_cast<uint8_t>(req.verify_pass_done + 1);
    } else if (req.raw_erase_pos < req.raw_erase_end) {
      out->phase = TransferPhase::ERASE;
    } else if (req.op == RequestOp::RAW_WRITE_FROM_FILE) {
      out->phase = TransferPhase::WRITE;
    }
  }
  // Label of the file in flight: for a tree src_path holds exactly that (tree_step_ reuses
  // the single-file fields); basename only and truncated -- this feeds a status line. The
  // worker task may be rewriting src_path between files while we copy; the counters above
  // are atomic, the name is not -- a torn read yields at worst one garbled label for one
  // poll frame, never out-of-bounds (copy is bounded, termination forced below).
  out->file[0] = '\0';
  if (state == RequestState::RUNNING && req.file_total.load() != 0) {
    const char *slash = strrchr(req.src_path, '/');
    strncpy(out->file, slash != nullptr ? slash + 1 : req.src_path, sizeof(out->file) - 1);
    out->file[sizeof(out->file) - 1] = '\0';
  }
  return true;
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
      // Not started yet -- finish it immediately, no partial I/O to unwind.
      req.result = StorageError::NOT_READY;
      req.state = RequestState::DONE;
    } else if (state == RequestState::RUNNING || state == RequestState::CANCELLED) {
      // CANCELLED is still owned by its engine (its next run_chunk_() closes handles and sets DONE),
      // so it must be drained like RUNNING -- not skipped, or the medium can unmount mid-call.
      if (i == this->loop_active_index_) {
        // Owned by the loop-sliced engine, which only ever advances from inside loop() -- we
        // are on the main loop right now too, so nothing else can touch this request
        // concurrently. Cancel and drain it in place: run_chunk_()'s entry check sees
        // CANCELLED and calls finish_request() immediately, closing any open handles before
        // this function returns -- i.e. before the driver's unmount() runs.
        req.cancel_result = StorageError::NOT_READY;
        req.state = RequestState::CANCELLED;
        this->run_chunk_(req, /*on_task=*/false);
        if (this->loop_active_index_ == i)
          this->loop_active_index_ = SIZE_MAX;
      } else {
#if defined(USE_ESP32) && defined(USE_STORAGE_WORKER_TASK)
        // Owned by the worker task (or, if it hasn't dequeued this index off task_queue_ yet,
        // about to be -- run_chunk_()'s entry check still sees CANCELLED as soon as the task
        // does pick it up, so no special-casing is needed for that race). Set CANCELLED and
        // wait for the task to actually reach DONE -- it can only observe the flag between
        // chunks, so this is bounded by at most one chunk's read+write (low single-digit ms
        // for SDMMC). Yield with vTaskDelay(1) rather than busy-spinning so the task (which
        // may run at the same or a lower priority) actually gets CPU time to reach that
        // check -- a tight spin here on a single-core target would starve it and deadlock
        // until the timeout below.
        req.cancel_result = StorageError::NOT_READY;
        req.state = RequestState::CANCELLED;
        uint32_t start = millis();
        while (req.state.load() != RequestState::DONE) {
          if (millis() - start > 500) {
            ESP_LOGE(TAG, "Timed out waiting for in-flight transfer to cancel -- the storage medium "
                          "is likely gone; proceeding with unmount anyway. Behavior from here is "
                          "undefined if the task is still touching it (e.g. still holding a file "
                          "handle) when it does eventually finish.");
            // Deliver the completion once, here, while the storage object is still valid: a
            // caller that chained work on it (the file-browser walker) would otherwise stall
            // forever. Then clear it -- if the task reaches DONE later, loop() must only free
            // the slot, never fire a second time into a context whose storage may be gone
            // post-unmount. The slot stays stuck (RUNNING/CANCELLED, never recycled) if the
            // task never finishes -- acceptable since the medium is dead anyway at that point.
            if (req.callback) {
              CompletionCallback cb = std::move(req.callback);
              req.callback = nullptr;
              cb(StorageError::NOT_READY);
            }
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

  // Same drain contract for streams: by the time this returns, no data-plane call into `s` is
  // still in flight and any handle it held is closed.
  for (auto &req : this->stream_pool_) {
    if (req.storage != s)
      continue;
    StreamState state = req.state.load();
    if (state == StreamState::FREE || state == StreamState::DONE)
      continue;
    if (state == StreamState::IDLE) {
      // No I/O in flight and no pending step queued -- finish immediately, no drain needed.
      if (req.is_fs && req.handle != nullptr)
        static_cast<FilesystemStorage *>(req.storage)->close(req.handle);
      req.result = StorageError::NOT_READY;
      req.state = StreamState::DONE;
      continue;
    }
    // A step (OPENING/WRITING/READING/CLOSING) is in flight or queued. Mark CANCELLED;
    // run_stream_step_()'s entry check sees it and closes/finishes immediately.
    req.state = StreamState::CANCELLED;
#if defined(USE_ESP32) && defined(USE_STORAGE_WORKER_TASK)
    // pending_step_ tells the two dispatch routes apart, and only the main loop ever writes
    // it: false means the step went to the worker task (queued or already running), so wait
    // (bounded) for the task to observe CANCELLED and reach DONE -- same rationale/timeout as
    // the TransferRequest drain above.
    if (!req.pending_step_) {
      uint32_t start = millis();
      while (req.state.load() != StreamState::DONE) {
        if (millis() - start > 500) {
          ESP_LOGE(TAG, "Timed out waiting for in-flight stream to cancel -- the storage medium is "
                        "likely gone; proceeding with unmount anyway.");
          // Deliver the completion once before discarding it (see the transfer drain above).
          if (req.callback) {
            CompletionCallback cb = std::move(req.callback);
            req.callback = nullptr;
            cb(StorageError::NOT_READY);
          }
          break;
        }
        vTaskDelay(1);
      }
      continue;
    }
#endif
    // Queued for the loop-sliced engine: run the (now-cancelled) step right here rather than
    // waiting for the next loop() pass -- we're on the main loop too (this callback only ever
    // fires from unregister_storage(), main-loop-only per StorageRegistry's contract), so
    // nothing else can be touching this slot. Its entry check closes the handle before this
    // function returns, which is what the drain contract promises.
    req.pending_step_ = false;
    this->run_stream_step_(req);
  }
}

void StorageWorker::deliver_completions_() {
  // Frees DONE slots and fires their callbacks. Runs regardless of which engine finished the
  // request, so this is the single place user callbacks are invoked -- always on the main
  // loop, per the public API's contract.
  for (auto &req : this->pool_) {
    if (req.state.load() == RequestState::DONE) {
#ifdef USE_STORAGE_CHANGE_FEED
      // Every transfer completes here, whoever submitted it -- YAML automations included --
      // so this is where the change feed learns about it, while the request's fields are
      // still intact. Whatever the result: a partially landed tree is still a change.
      // (Streaming requests are not fed: reads change nothing, and the write stream's
      // consumers note their own completion -- they know the path, this loop does not.)
      // build_path() writes mount point + relative path, which is longer than the relative
      // paths the request slots carry. STORAGE_WORKER_MAX_PATH would make it refuse near the
      // limit and drop the notification without a trace.
      char feed_path[STORAGE_VFS_PATH_MAX];
      if (global_storage_registry != nullptr && req.op == RequestOp::RAW_READ_TO_FILE && req.dst_storage != nullptr) {
        // A raw read landed (or failed to land -- partially is still a change) a file: the
        // feed learns about its parent directory exactly like any other transfer's.
        if (StorageRegistry::build_path(req.dst_storage, req.dst_path, feed_path, sizeof(feed_path))) {
          global_storage_registry->note_parent_changed(feed_path);
        } else {
          ESP_LOGW(TAG, "change feed skipped: path too long for USE_STORAGE_VFS_PATH_MAX ('%s')", req.dst_path);
        }
      } else if (global_storage_registry != nullptr && req.dst_storage != nullptr && req.src_storage != nullptr) {
        const bool is_tree = is_tree_op(req.op);
        const char *dst_rel = is_tree ? req.tree->dst_root : req.dst_path;
        if (StorageRegistry::build_path(req.dst_storage, dst_rel, feed_path, sizeof(feed_path))) {
          global_storage_registry->note_parent_changed(feed_path);
          if (is_tree) {
            // Merging into an existing (possibly open) directory changes its listing too.
            global_storage_registry->note_dir_changed(feed_path);
          }
        } else {
          ESP_LOGW(TAG, "change feed skipped: path too long for USE_STORAGE_VFS_PATH_MAX ('%s')", dst_rel);
        }
        if (req.op == RequestOp::MOVE || req.op == RequestOp::MOVE_TREE) {
          const char *src_rel = is_tree ? req.tree->src_root : req.src_path;
          if (StorageRegistry::build_path(req.src_storage, src_rel, feed_path, sizeof(feed_path))) {
            global_storage_registry->note_parent_changed(feed_path);
          } else {
            ESP_LOGW(TAG, "change feed skipped: path too long for USE_STORAGE_VFS_PATH_MAX ('%s')", src_rel);
          }
        }
      }
#endif
      CompletionCallback cb = std::move(req.callback);
      StorageError result = req.result;
      req.callback = nullptr;
      req.src_storage = nullptr;
      req.dst_storage = nullptr;
      req.raw_device = nullptr;
      req.stuck_warned = false;
      req.state = RequestState::FREE;
      if (cb)
        cb(result);
    }
  }
}

void StorageWorker::update() {
  // Nothing to do until the first async transfer is submitted. The poller is only running
  // because start_poller() was called at a submit funnel, so this is normally non-empty; the
  // early-out just covers a stray tick before ensure_started_() has built the pools.
  if (!this->started_)
    return;

  // The stall watchdog (see check_stalled_ below) -- once a second is plenty.
  uint32_t now_wd = millis();
  if (now_wd - this->last_stall_check_ms_ > 1000) {
    this->last_stall_check_ms_ = now_wd;
    this->check_stalled_();
  }

  this->deliver_completions_();

  if (this->loop_active_index_ == SIZE_MAX) {
    // Pick the next PENDING request, FIFO by pool position (pool order matches submission
    // order well enough in practice -- this component makes no stronger ordering promise).
    // Skip any request that overlaps a storage with something already RUNNING/CANCELLED (e.g.
    // on the worker task) -- it must wait its turn rather than run concurrently with it.
    for (size_t i = 0; i < this->pool_.size(); i++) {
      TransferRequest &candidate = this->pool_[i];
      if (candidate.state.load() != RequestState::PENDING)
        continue;
      if (this->overlaps_active_(candidate)) {
        // Known limitation: if the request it overlaps with is stuck (its owning engine never
        // reached DONE after a drain timeout -- see on_storage_unregistered_()), this request
        // waits forever and its callback never fires. There is no reclaim path for the stuck
        // slot itself: by the time a drain times out, the medium is presumed gone and the
        // engine that owns it may still be touching it, so freeing the slot out from under
        // that engine would be unsafe. Log once per request so this is at least diagnosable.
        if (!candidate.stuck_warned) {
          ESP_LOGW(TAG, "Request pending indefinitely -- blocked by another request on the same "
                        "storage that never completed (likely a stuck slot after a drain "
                        "timeout)");
          candidate.stuck_warned = true;
        }
        continue;
      }
      candidate.state = RequestState::RUNNING;
      // The stall clock starts NOW, not at submission: a request that legitimately waited
      // PENDING behind another transfer must get its full stall window once it actually runs.
      candidate.last_progress_ms = millis();
      this->loop_active_index_ = i;
      break;
    }
  }

  if (this->loop_active_index_ != SIZE_MAX) {
    TransferRequest &req = this->pool_[this->loop_active_index_];
    // Only advance a request this engine still owns (RUNNING, or CANCELLED -- which
    // run_chunk_()'s entry check drains). Anything else means it was finished and possibly
    // already freed from outside this section; running it would touch nulled storages.
    RequestState st = req.state.load();
    if (st == RequestState::RUNNING || st == RequestState::CANCELLED) {
      this->run_chunk_(req, /*on_task=*/false);
      st = req.state.load();
    }
    if (st != RequestState::RUNNING && st != RequestState::CANCELLED)
      this->loop_active_index_ = SIZE_MAX;
  }

  // Idle again? Then stop the poller -- the next submit funnel re-arms it.
  bool busy = this->loop_active_index_ != SIZE_MAX;
  if (!busy) {
    for (const auto &req : this->pool_) {
      if (req.state.load() != RequestState::FREE) {
        busy = true;
        break;
      }
    }
  }
  if (!busy) {
    for (const auto &req : this->stream_pool_) {
      if (req.state.load() != StreamState::FREE) {
        busy = true;
        break;
      }
    }
  }
  // Nothing left to do: stop the PollingComponent poller so an idle node schedules no worker
  // ticks at all. A subsequent submit (transfer, raw, or stream) calls start_poller() again.
  // Note the whole-pool/stream scan above must clear busy first -- stopping the poller while a
  // task-dispatched request is still in flight would drop the tick that delivers its
  // completion; the scan keeps the poller alive until that slot returns to FREE.
  if (!busy)
    this->stop_poller();

  // Streams: two jobs, both per-slot and independent of any other slot (no FIFO ordering
  // needed -- unlike TransferRequest's loop_active_index_, every stream's step is
  // self-contained, since nothing auto-advances without the caller supplying the next chunk).
  //  1. Run any step a loop-sliced dispatch queued but hasn't executed yet (see
  //     dispatch_stream_step_()'s loop-sliced branch -- it only flips req.pending_step_ rather
  //     than calling run_stream_step_() inline, so the callback always fires from here, never
  //     reentrantly from inside the caller's own begin_*/write_chunk/read_chunk/end_* call).
  //  2. Deliver completions for whichever streams finished a step (task or loop-sliced).
  for (auto &req : this->stream_pool_) {
    // Hold a queued step while a transfer (or another stream) drives the same storage -- the
    // slot stays non-FREE, so the poller keeps ticking and the step runs on a later pass.
    if (req.pending_step_ && !this->stream_overlaps_active_(req, /*from_loop=*/true)) {
      req.pending_step_ = false;
      this->run_stream_step_(req);
    }

    StreamState state = req.state.load();
    bool step_done = (state == StreamState::IDLE && req.callback) || state == StreamState::DONE;
    if (!step_done)
      continue;

    CompletionCallback cb = std::move(req.callback);
    StorageError result = req.result;
    req.callback = nullptr;
    bool freed = state == StreamState::DONE;
    if (freed) {
      req.storage = nullptr;
      req.state = StreamState::FREE;
    }
    if (cb)
      cb(result);
  }
}

#if defined(USE_ESP32) && defined(USE_STORAGE_WORKER_TASK)
void StorageWorker::task_fn(void *arg) { static_cast<StorageWorker *>(arg)->task_loop_(); }

void StorageWorker::task_loop_() {
  // No ESPHome facilities are touched from this task besides is_registered() (mutex-guarded)
  // and ESP_LOG (task-safe on IDF, kept to errors) -- see run_chunk_()'s cancellation check.
  // Deliberately NOT subscribed to the task watchdog: this task blocks on I/O by design, and
  // the chunked design here (like the synchronous copy() helper) already keeps any single
  // blocking call short.
  for (;;) {
    QueueEntry entry;
    if (xQueueReceive(this->task_queue_, &entry, portMAX_DELAY) != pdTRUE)
      continue;

    if (entry.kind == QueueEntryKind::TRANSFER) {
      TransferRequest &req = this->pool_[entry.index];
      // Loop until DONE, not just while RUNNING: if the hotplug handler flips the state to
      // CANCELLED between iterations, run_chunk_()'s entry check is what actually closes
      // handles, frees the chunk buffer, and transitions to DONE. Stopping on state != RUNNING
      // would exit before that ever happens, leaking the handles/buffer and leaving the slot
      // (and the pending callback) stuck forever.
      while (req.state.load() != RequestState::DONE) {
        this->run_chunk_(req, /*on_task=*/true);
        // A network storage that answered NOT_READY while reconnecting would otherwise be hammered
        // back-to-back for the whole NETWORK_READY_WINDOW_MS; pace the retry instead of spinning.
        if (req.network_waiting)
          vTaskDelay(pdMS_TO_TICKS(50));
      }
    } else {
      // STREAM: single step per queue entry, unlike TransferRequest above -- the caller submits
      // exactly one queue entry per write_chunk()/read_chunk()/begin_*()/end_*() call, so there
      // is no "loop until DONE" here; DONE only happens on end_write()/end_read()'s step.
      this->run_stream_step_(this->stream_pool_[entry.index]);
    }
    // DONE (or IDLE, for streams) reached, and this task is the producer: loop() delivers the
    // completion, but the component phase it runs in is gated (loop_interval_, high-frequency
    // request, or an explicit wake). Nothing else asks for it -- an HTTP request does not drive
    // it, it only reads. So wake it here, which is exactly what wake_loop_threadsafe() is for:
    // a background producer queued work for its component's loop() to drain.
    //
    // Without this the completion sits in the slot until the phase happens to come around.
    // A caller that polls for a result eventually sees it; a caller that *chains* on it -- the
    // file browser's directory walker submits file N+1 only once file N's completion has been
    // delivered -- stops dead after the first file.
    wake_loop_threadsafe();
  }
}
#endif

// Picks entry #target out of a directory listing; the walk re-lists per step rather than
// holding an open directory handle across calls, so nothing has to stay valid in between.
struct WalkEntryCtx {
  uint16_t target;
  uint16_t seen{0};
  bool found{false};
  FileStat entry{};
};

static bool walk_entry_cb(const FileStat *entry, void *ctx_raw) {
  auto *ctx = static_cast<WalkEntryCtx *>(ctx_raw);
  if (ctx->seen == ctx->target) {
    ctx->entry = *entry;
    ctx->found = true;
    return false;  // stop enumeration -- not an error per the list_dir contract
  }
  ctx->seen++;
  return true;
}

// Finishes a request: closes any open handles (best-effort -- a close failure only overrides
// the result if the transfer itself had otherwise succeeded, mirroring storage::copy()'s
// close-error propagation) and marks it DONE for loop()/task_loop_() to deliver.
// Closes whatever handles the request holds. A close failure only overrides an otherwise
// successful result, mirroring storage::copy()'s close-error propagation.
static void close_handles(TransferRequest &req, StorageError *result) {
  if (!req.handles_open)
    return;
  if (req.src_is_fs && req.src_handle != nullptr && req.src_storage != nullptr)
    static_cast<FilesystemStorage *>(req.src_storage)->close(req.src_handle);
  if (req.dst_is_fs && req.dst_handle != nullptr && req.dst_storage != nullptr) {
    StorageError close_err = static_cast<FilesystemStorage *>(req.dst_storage)->close(req.dst_handle);
    if (*result == StorageError::OK)
      *result = close_err;
  }
  req.src_handle = nullptr;
  req.dst_handle = nullptr;
}

static void finish_request(TransferRequest &req, StorageError result) {
  close_handles(req, &result);
  req.result = result;
  req.state = RequestState::DONE;
}

// Start (or restart) a read-back pass over the just-written range. Rewinds the source file to
// the beginning and resets the byte cursor; the chunk loop then re-reads the device and compares.
// Returns false (after finishing the request) only on a rewind error.
bool StorageWorker::begin_verify_pass_(TransferRequest &req) {
  if (req.src_is_fs) {
    // File-system source: seek the open handle back to the start.
    StorageError serr = static_cast<FilesystemStorage *>(req.src_storage)->seek(req.src_handle, 0);
    if (serr != StorageError::OK) {
      finish_request(req, serr);
      return false;
    }
  }
  // Network sources are read by absolute offset (read_chunk), so there is no handle to rewind.
  req.verifying = true;
  req.offset = 0;
  return true;
}

// One verify chunk: read the device span [raw_address+offset, +want) into the engine's chunk
// buffer, then read
// the same span from the source file in small stack-sized slices and memcmp each -- so the whole
// comparison needs only that one buffer plus a tiny stack buffer, no second large allocation.
// Returns false when it has finished the request (mismatch, I/O error, completed pass) or is
// waiting on the network; true to keep looping within the same call is not used (one chunk/call).
uint8_t *StorageWorker::chunk_buffer_(bool on_task, size_t *size_out) {
  RamBuffer &buf = on_task ? this->chunk_buf_task_ : this->chunk_buf_loop_;
  size_t &size = on_task ? this->chunk_size_task_ : this->chunk_size_loop_;
  if (buf.get() == nullptr) {
    size_t got = 0;
    buf = alloc_dma_capable(STORAGE_COPY_CHUNK_SIZE, on_task, &got);
    if (buf.get() == nullptr)
      return nullptr;
    size = got;
  }
  *size_out = size;
  return buf.get();
}

bool StorageWorker::verify_chunk_(TransferRequest &req, bool on_task) {
  const uint64_t total = req.bytes_total.load();
  size_t chunk_size = 0;
  uint8_t *chunk = this->chunk_buffer_(on_task, &chunk_size);
  if (chunk == nullptr) {
    finish_request(req, StorageError::NO_SPACE);
    return false;
  }
  size_t want = static_cast<size_t>(std::min<uint64_t>(chunk_size, total - req.offset));
  size_t got = 0;
  StorageError err = req.raw_device->read(req.raw_address + req.offset, chunk, want, &got);
  if (err != StorageError::OK) {
    finish_request(req, err);
    return false;
  }
  if (got == 0) {
    finish_request(req, StorageError::READ_ERROR);
    return false;
  }
  // Compare against the file in small slices reusing a stack buffer.
  uint8_t cmp[256];
  size_t compared = 0;
  const bool file_is_fs = req.src_is_fs;
  Storage *file_storage = req.src_storage;
  while (compared < got) {
    size_t slice = std::min<size_t>(sizeof(cmp), got - compared);
    size_t fgot = 0;
    if (file_is_fs) {
      err = static_cast<FilesystemStorage *>(file_storage)->read(req.src_handle, cmp, slice, &fgot);
    } else {
      err = static_cast<NetworkStorage *>(file_storage)
                ->read_chunk(req.src_path, cmp, req.offset + compared, slice, &fgot);
    }
    if (this->wait_for_network_ready_(req, err, file_storage))
      return false;
    if (err != StorageError::OK) {
      finish_request(req, err);
      return false;
    }
    if (fgot == 0) {
      finish_request(req, StorageError::READ_ERROR);  // file shrank underneath the verify
      return false;
    }
    if (memcmp(chunk + compared, cmp, fgot) != 0) {
      ESP_LOGW(TAG, "Verify FAILED: mismatch on pass %u/%u at byte %llu", req.verify_pass_done + 1, req.verify_passes,
               static_cast<unsigned long long>(req.offset + compared));
      finish_request(req, StorageError::VERIFY_MISMATCH);
      return false;
    }
    compared += fgot;
  }
  req.offset += got;
  req.bytes_done.store(req.offset);
  if (req.offset >= total) {
    // Pass complete. More passes queued -> rewind and go again; otherwise the write is verified.
    req.verify_pass_done++;
    req.verifying = false;  // the outer loop's offset>=total branch decides next pass vs done
  }
  return false;  // one chunk per call; the loop re-enters via the engine
}

// One step of a directory walk. Everything here is control plane -- list, mkdir, rmdir, remove --
// so it costs one call each and never blocks for long; the bytes go through run_chunk_()'s
// chunk loop exactly as they do for a single file. Returns false once the request is finished.
void StorageWorker::run_raw_chunk_(TransferRequest &req, bool on_task) {
  const bool to_file = req.op == RequestOp::RAW_READ_TO_FILE;
  PathStorage *file_storage = to_file ? req.dst_storage : req.src_storage;
  const char *file_path = to_file ? req.dst_path : req.src_path;
  const bool file_is_fs = to_file ? req.dst_is_fs : req.src_is_fs;

  if (req.op == RequestOp::RAW_ERASE && !req.pre_phase_done) {
    req.pre_phase_done = true;
    // bytes_total carries the requested erase length; align it up like the write path does.
    RawGeometry geo{};
    req.raw_device->get_raw_geometry(&geo);
    uint64_t len = req.bytes_total.load();
    const bool pseudo = (geo.caps & (RAW_ERASE_SECTOR | RAW_ERASE_BLOCK | RAW_ERASE_CHIP)) == 0;
    if (pseudo) {
      // Overwrite-in-place media (EEPROM, FRAM) have no erase opcode -- "erasing" them is a
      // chunked 0xFF fill via write() (see the step below). Byte-addressable, so only the
      // bounds are checked; no sector alignment exists to demand.
      if (len == 0 || req.raw_address >= geo.capacity || len > geo.capacity - req.raw_address) {
        finish_request(req, StorageError::INVALID_ARGS);
        return;
      }
      req.raw_erase_pos = req.raw_address;
      req.raw_erase_end = req.raw_address + len;
    } else {
      if (geo.erase_sector == 0 || (req.raw_address % geo.erase_sector) != 0 || req.raw_address >= geo.capacity ||
          len > geo.capacity - req.raw_address) {
        finish_request(req, StorageError::INVALID_ARGS);
        return;
      }
      if ((len % geo.erase_sector) != 0)
        len += geo.erase_sector - (len % geo.erase_sector);
      req.raw_erase_pos = req.raw_address;
      req.raw_erase_end = req.raw_address + len;
    }
  }
  if (req.op == RequestOp::RAW_ERASE) {
    if (req.raw_erase_pos >= req.raw_erase_end) {
      finish_request(req, StorageError::OK);
      return;
    }
    RawGeometry geo{};
    req.raw_device->get_raw_geometry(&geo);
    if ((geo.caps & (RAW_ERASE_SECTOR | RAW_ERASE_BLOCK | RAW_ERASE_CHIP)) == 0) {
      // Pseudo erase: fill one chunk of 0xFF per pass. Shares the platform-aware streaming
      // buffer policy (alloc_dma_capable) -- internal RAM under the cutoff, DMA-capable, halving
      // on pressure. The fill happens on every pass: the buffer belongs to the engine, not to
      // this request, so another transfer may have used it in between.
      size_t chunk_size = 0;
      uint8_t *chunk = this->chunk_buffer_(on_task, &chunk_size);
      if (chunk == nullptr) {
        finish_request(req, StorageError::NO_SPACE);
        return;
      }
      size_t step = static_cast<size_t>(std::min<uint64_t>(chunk_size, req.raw_erase_end - req.raw_erase_pos));
      memset(chunk, 0xFF, step);
      size_t written = 0;
      StorageError werr = req.raw_device->write(req.raw_erase_pos, chunk, step, &written);
      if (werr != StorageError::OK || written == 0) {
        finish_request(req, werr != StorageError::OK ? werr : StorageError::WRITE_ERROR);
        return;
      }
      // A partial write is not an error -- the next pass continues from where it stopped.
      req.raw_erase_pos += written;
      req.bytes_done.store(req.raw_erase_pos - req.raw_address);
      return;
    }
    uint64_t step = geo.erase_block != 0 ? geo.erase_block : geo.erase_sector;
    // Whole-device fast path: on the worker task, a full-span erase from offset 0 hands the
    // driver a single erase(0, capacity) so it can use the chip-erase opcode instead of walking
    // the device sector by sector. Same conditions and stall-watchdog shield as the erase step
    // in run_raw_chunk_'s write path -- kept here too because a standalone raw_erase never goes
    // through that path. force_sliced_erase opts out; off the task a chip-scale blocking erase
    // would trip the 5s watchdog, so it stays sliced there.
    const bool whole_device = on_task && !req.force_sliced_erase && req.raw_address == 0 && req.raw_erase_pos == 0 &&
                              req.raw_erase_end == geo.capacity;
    if (whole_device) {
      req.blocking_erase_active.store(true);
      StorageError eerr = req.raw_device->erase(0, static_cast<size_t>(geo.capacity));
      req.blocking_erase_active.store(false);
      if (eerr != StorageError::OK) {
        finish_request(req, eerr);
        return;
      }
      req.raw_erase_pos = req.raw_erase_end;
      req.bytes_done.store(req.raw_erase_pos - req.raw_address);
      return;
    }
    step = std::min<uint64_t>(step, req.raw_erase_end - req.raw_erase_pos);
    StorageError eerr = req.raw_device->erase(req.raw_erase_pos, static_cast<size_t>(step));
    if (eerr != StorageError::OK) {
      finish_request(req, eerr);
      return;
    }
    req.raw_erase_pos += step;
    req.bytes_done.store(req.raw_erase_pos - req.raw_address);
    return;
  }

  if (!req.pre_phase_done) {
    req.pre_phase_done = true;
    if (to_file) {
      // Same overwrite contract as async_copy: an occupied destination answers
      // ALREADY_EXISTS unless overwrite; a directory there is never replaced by an image.
      FileStat dst_st{};
      StorageError derr = file_storage->stat(file_path, &dst_st);
      if (this->wait_for_network_ready_(req, derr, file_storage)) {
        req.pre_phase_done = false;
        return;
      }
      if (derr == StorageError::OK) {
        if (!req.overwrite) {
          finish_request(req, StorageError::ALREADY_EXISTS);
          return;
        }
        if (dst_st.is_dir) {
          finish_request(req, StorageError::INVALID_ARGS);
          return;
        }
      }
    } else {
      FileStat src_st{};
      StorageError serr = file_storage->stat(file_path, &src_st);
      if (this->wait_for_network_ready_(req, serr, file_storage)) {
        req.pre_phase_done = false;
        return;
      }
      if (serr != StorageError::OK || src_st.is_dir) {
        finish_request(req, serr != StorageError::OK ? StorageError::NOT_FOUND : StorageError::INVALID_ARGS);
        return;
      }
      req.bytes_total.store(src_st.size);
      req.file_total.store(src_st.size);
      RawGeometry geo{};
      req.raw_device->get_raw_geometry(&geo);
      if (req.raw_address >= geo.capacity || src_st.size > geo.capacity - req.raw_address) {
        finish_request(req, StorageError::NO_SPACE);  // genuinely: does not fit on the device
        return;
      }
      if (req.raw_erase_end != 0) {  // erase requested -- turn the flag into the real range
        if (geo.erase_sector == 0 || (req.raw_address % geo.erase_sector) != 0) {
          finish_request(req, StorageError::INVALID_ARGS);
          return;
        }
        uint64_t len = src_st.size;
        if ((len % geo.erase_sector) != 0)
          len += geo.erase_sector - (len % geo.erase_sector);
        req.raw_erase_pos = req.raw_address;
        req.raw_erase_end = req.raw_address + len;
      }
      // Standalone verify: no write phase -- go straight into read-back-and-compare. At least one
      // pass; the file handle is opened by the chunk loop below (handles_open path) exactly as
      // the write path opens it, and begin/verify use the same src handle.
      if (req.op == RequestOp::RAW_VERIFY_FILE) {
        if (req.verify_passes == 0)
          req.verify_passes = 1;
        req.verifying = true;
      }
    }
  }

  // Sliced erase: one geometry-sized step per pass -- a chip-scale erase becomes many short
  // main-loop visits instead of one multi-second freeze.
  //
  // Whole-device erase strategy depends on which engine we are on:
  //
  //  - On the worker task (on_task), when the request covers the entire device from offset 0,
  //    hand the driver a single erase(0, capacity) so it can use the chip-erase opcode. That
  //    busy-waits for the full erase -- tens of seconds on a large NOR flash (see
  //    SPIFlash::erase_chip, wait_ready(60000)) -- which is fine here: the task is deliberately
  //    not on the 5s task watchdog (see task_loop_) and blocking on I/O is its whole purpose.
  //    The one hazard is the stall watchdog (check_stalled_, main loop), which keys off
  //    bytes_done and would time the request out at 30s since a single blocking call cannot
  //    advance progress mid-flight; blocking_erase_active tells it to leave this request alone
  //    for the duration. The flag is bounded by the driver's own wait_ready timeout.
  //
  //  - Otherwise (loop-sliced engine, or a partial-range erase) stay block-at-a-time: each pass
  //    erases one geometry-sized unit and returns, keeping every main-loop pass short. A chip
  //    erase here would freeze the loop long enough to trip the 5s watchdog, so it is never
  //    done off the task.
  if (!to_file && req.raw_erase_pos < req.raw_erase_end) {
    RawGeometry geo{};
    req.raw_device->get_raw_geometry(&geo);
    const bool whole_device = on_task && !req.force_sliced_erase && req.raw_address == 0 && req.raw_erase_pos == 0 &&
                              req.raw_erase_end == geo.capacity;
    if (whole_device) {
      // Single blocking chip erase. Shield it from the stall watchdog while it runs.
      req.blocking_erase_active.store(true);
      StorageError eerr = req.raw_device->erase(0, static_cast<size_t>(geo.capacity));
      req.blocking_erase_active.store(false);
      if (eerr != StorageError::OK) {
        finish_request(req, eerr);
        return;
      }
      req.raw_erase_pos = req.raw_erase_end;
      req.bytes_done.store(req.raw_erase_pos - req.raw_address);
      return;  // erase done; next pass starts moving bytes
    }
    // Not whole-device: do NOT erase the whole range up front. The erase is interleaved with
    // the write below -- each chunk erases only the sectors it is about to touch (raw_erase_pos
    // is the "erased up to" cursor). This shrinks the window between erasing a region and
    // filling it, and avoids a long erase-only phase that writes no bytes. Fall through.
  }

  // Platform-aware streaming buffer, owned by the engine (alloc_dma_capable): internal RAM
  // under the cutoff, DMA-capable, halving on pressure down to 4 KiB before giving up.
  size_t chunk_size = 0;
  uint8_t *chunk = this->chunk_buffer_(on_task, &chunk_size);
  if (chunk == nullptr) {
    finish_request(req, StorageError::NO_SPACE);
    return;
  }
  if (!req.handles_open) {
    if (file_is_fs) {
      auto *fs = static_cast<FilesystemStorage *>(file_storage);
      FileHandle *h = nullptr;
      StorageError oerr = fs->open(file_path, h, to_file ? OpenMode::WRITE : OpenMode::READ);
      if (this->wait_for_network_ready_(req, oerr, file_storage))
        return;
      if (oerr != StorageError::OK) {
        finish_request(req, oerr);
        return;
      }
      (to_file ? req.dst_handle : req.src_handle) = h;
    } else if (to_file) {
      // A raw read landing in a file on a NetworkStorage: no open() to truncate for it, and
      // write_chunk() only overwrites the range it is given, so a shorter image than the file
      // it replaces would leave the old tail behind.
      StorageError terr = static_cast<NetworkStorage *>(file_storage)->truncate(file_path, 0);
      if (this->wait_for_network_ready_(req, terr, file_storage))
        return;
      if (terr != StorageError::OK) {
        finish_request(req, terr);
        return;
      }
    }
    req.handles_open = true;
  }

  const uint64_t total = req.bytes_total.load();
  if (req.offset >= total) {
    // Write (or the current verify pass) reached the end. If verify passes remain, rewind and
    // read the whole written range back, comparing against the source file; otherwise done.
    if (!to_file && req.verify_passes > 0 && req.verify_pass_done < req.verify_passes) {
      if (!this->begin_verify_pass_(req))
        return;  // begin_verify_pass_ finished the request on error
      return;
    }
    // Overall verify result to the log (individual passes are not logged): all requested
    // passes read back and matched the source.
    if (!to_file && req.verify_passes > 0) {
      ESP_LOGI(TAG, "Verify OK: %u/%u pass(es) matched over %llu bytes", req.verify_pass_done, req.verify_passes,
               static_cast<unsigned long long>(total));
    }
    finish_request(req, StorageError::OK);
    return;
  }
  if (req.verifying) {
    // Verify phase: read one device chunk into the engine buffer, then read the same span from the file
    // in small slices and memcmp -- no second full buffer. A mismatch fails the request.
    if (!this->verify_chunk_(req, on_task))
      return;  // verify_chunk_ finished the request (mismatch or error) or is waiting on network
    return;
  }
  size_t want = static_cast<size_t>(std::min<uint64_t>(chunk_size, total - req.offset));
  size_t moved = 0;
  StorageError err = StorageError::OK;
  if (to_file) {
    err = req.raw_device->read(req.raw_address + req.offset, chunk, want, &moved);
    if (err == StorageError::OK && moved > 0) {
      // Write the chunk out fully -- write()/write_chunk() may return partial writes, which
      // are not errors (same retry-until-full loop the file-to-file chunk path uses).
      size_t total_written = 0;
      while (err == StorageError::OK && total_written < moved) {
        size_t written = 0;
        if (file_is_fs) {
          err = static_cast<FilesystemStorage *>(file_storage)
                    ->write(req.dst_handle, chunk + total_written, moved - total_written, &written);
        } else {
          err = static_cast<NetworkStorage *>(file_storage)
                    ->write_chunk(file_path, chunk + total_written, req.offset + total_written, moved - total_written,
                                  &written);
        }
        if (this->wait_for_network_ready_(req, err, file_storage))
          return;
        if (err == StorageError::OK && written == 0) {
          err = StorageError::WRITE_ERROR;  // no progress and no error -- a genuinely stuck sink
          break;
        }
        total_written += written;
      }
    }
  } else {
    size_t got = 0;
    if (file_is_fs) {
      err = static_cast<FilesystemStorage *>(file_storage)->read(req.src_handle, chunk, want, &got);
    } else {
      err = static_cast<NetworkStorage *>(file_storage)->read_chunk(file_path, chunk, req.offset, want, &got);
    }
    if (this->wait_for_network_ready_(req, err, file_storage))
      return;
    if (err == StorageError::OK) {
      if (got == 0) {
        finish_request(req, StorageError::READ_ERROR);  // file shrank underneath us
        return;
      }
      // Per-chunk erase: before writing this chunk, erase the sectors it will land in that are
      // not already erased. raw_erase_pos is the "erased up to" cursor (0 when no erase was
      // requested, so this is skipped). We erase up to the sector boundary at or above the end
      // of this chunk, but never past raw_erase_end, and only the part beyond raw_erase_pos --
      // so a sector shared by two chunks is erased exactly once, by the first chunk to reach it.
      if (req.raw_erase_pos < req.raw_erase_end) {
        RawGeometry geo{};
        req.raw_device->get_raw_geometry(&geo);
        uint64_t chunk_end = req.raw_address + req.offset + got;
        uint64_t erase_to = chunk_end;
        if (geo.erase_sector != 0 && (erase_to % geo.erase_sector) != 0)
          erase_to += geo.erase_sector - (erase_to % geo.erase_sector);  // round up to sector
        erase_to = std::min<uint64_t>(erase_to, req.raw_erase_end);
        while (req.raw_erase_pos < erase_to) {
          uint64_t step = geo.erase_block != 0 ? geo.erase_block : geo.erase_sector;
          step = std::min<uint64_t>(step, erase_to - req.raw_erase_pos);
          StorageError eerr = req.raw_device->erase(req.raw_erase_pos, static_cast<size_t>(step));
          if (eerr != StorageError::OK) {
            finish_request(req, eerr);
            return;
          }
          req.raw_erase_pos += step;
        }
      }
      err = req.raw_device->write(req.raw_address + req.offset, chunk, got, &moved);
    }
  }
  if (err != StorageError::OK || moved == 0) {
    finish_request(req, err != StorageError::OK ? err : StorageError::WRITE_ERROR);
    return;
  }
  req.offset += moved;
  req.bytes_done.store(req.offset);
  req.file_done.store(req.offset);
}

bool StorageWorker::tree_step_(TransferRequest &req) {
  // Both sides are set for every op that walks a tree -- raw transfers, which leave one of them
  // null, never get here. Saying so locally keeps the invariant checkable at this end too.
  if (req.src_storage == nullptr || req.dst_storage == nullptr) {
    finish_request(req, StorageError::INVALID_ARGS);
    return false;
  }
  TreeWalk &w = *req.tree;
  const bool is_move = req.op == RequestOp::MOVE_TREE;

  if (!w.root_created) {
    StorageError err = req.dst_storage->mkdir(w.dst_root);
    if (err != StorageError::OK && err != StorageError::ALREADY_EXISTS) {
      if (this->wait_for_network_ready_(req, err, req.dst_storage))
        return false;  // not finished -- retried next pass, the knock has woken the mount
      finish_request(req, err);
      return false;
    }
    w.root_created = true;
  }

  while (true) {
    char dir[STORAGE_WORKER_MAX_PATH];
    if (!join_walk_path(dir, sizeof(dir), w.src_root, w.sub, nullptr)) {
      finish_request(req, StorageError::INVALID_ARGS);
      return false;
    }

    WalkEntryCtx ctx{w.index_stack[w.depth]};
    StorageError err = req.src_storage->list_dir(dir, walk_entry_cb, &ctx);
    if (err == StorageError::NOT_READY && this->wait_for_network_ready_(req, err, req.src_storage))
      return false;
    if (err != StorageError::OK) {
      finish_request(req, err);
      return false;
    }

    if (!ctx.found) {
      // Drained. A move takes the emptied source directory with it.
      if (is_move) {
        err = req.src_storage->rmdir(dir);
        if (err != StorageError::OK) {
          finish_request(req, err);
          return false;
        }
      }
      if (w.depth == 0) {
        finish_request(req, StorageError::OK);
        return false;
      }
      char *slash = strrchr(w.sub, '/');
      if (slash != nullptr) {
        *slash = '\0';
      } else {
        w.sub[0] = '\0';
      }
      w.depth--;
      continue;
    }

    // Entry bookkeeping differs by mode: a copy leaves the source alone, so positions are
    // stable and the index counts up. A move removes each entry once it is done, so what is
    // left slides down and the next unprocessed entry is always #0 -- advancing would skip one.
    if (!is_move)
      w.index_stack[w.depth]++;

    if (ctx.entry.is_dir) {
      if (w.depth + 1 >= TreeWalk::MAX_DEPTH) {
        // Same answer the blocking walks give for the same reason (storage.cpp).
        finish_request(req, StorageError::INVALID_ARGS);
        return false;
      }
      char dst_dir[STORAGE_WORKER_MAX_PATH];
      if (!join_walk_path(dst_dir, sizeof(dst_dir), w.dst_root, w.sub, ctx.entry.name)) {
        finish_request(req, StorageError::INVALID_ARGS);
        return false;
      }
      err = req.dst_storage->mkdir(dst_dir);
      if (err != StorageError::OK && err != StorageError::ALREADY_EXISTS) {
        finish_request(req, err);
        return false;
      }
      size_t sub_len = strlen(w.sub);
      size_t name_len = strlen(ctx.entry.name);
      if (sub_len + name_len + 2 >= sizeof(w.sub)) {
        finish_request(req, StorageError::INVALID_ARGS);
        return false;
      }
      if (sub_len != 0)
        w.sub[sub_len++] = '/';
      memcpy(w.sub + sub_len, ctx.entry.name, name_len + 1);
      w.depth++;
      w.index_stack[w.depth] = 0;
      continue;
    }

    // A file: hand it to the chunk loop below by putting it where a single-file request keeps
    // its paths. Nothing else in run_chunk_() has to know it is part of a tree.
    if (!join_walk_path(req.src_path, sizeof(req.src_path), w.src_root, w.sub, ctx.entry.name) ||
        !join_walk_path(req.dst_path, sizeof(req.dst_path), w.dst_root, w.sub, ctx.entry.name)) {
      finish_request(req, StorageError::INVALID_ARGS);
      return false;
    }
    req.offset = 0;
    // Deliberately no bytes_total: it belongs to the request, and bytes_done counts the whole
    // tree, so a per-file total would have the two describe different things -- progress ran
    // past 100% for every file after the first. What a tree will weigh in total is unknown
    // without walking it twice, and 0 already means exactly that (see TransferRequest).
    w.file_in_flight = true;
    return true;
  }
}

// One stuck job must never wall off the queue: a RUNNING request holds its storages against
// every overlapping newcomer (overlaps_active_) and occupies the loop slice -- so a request
// that demonstrably stops moving is finished with TIMEOUT, freeing the slot, the storages,
// and everything blocked behind them. 'Moving' is measured, not assumed: a fingerprint over
// offset, byte/file progress and tree position -- big-but-progressing transfers never trip.
// PENDING gets a generous absolute cap as the second belt (its usual blocker is a stuck
// RUNNING one, which the first rule already clears). Streams are client-driven, so a
// vanished HTTP client is handled by the same sweep, via the exact contract the quiesce
// drain uses. Defined after finish_request() on purpose -- it calls it.
static constexpr uint32_t REQUEST_STALL_TIMEOUT_MS = 30000;
static constexpr uint32_t REQUEST_PENDING_CAP_MS = 120000;
static constexpr uint32_t STREAM_IDLE_TIMEOUT_MS = 30000;

void StorageWorker::check_stalled_() {
  const uint32_t now = millis();
  for (auto &req : this->pool_) {
    RequestState st = req.state.load(std::memory_order_acquire);
    if (st == RequestState::RUNNING) {
      // A whole-chip erase on the worker task blocks for its full duration without advancing
      // bytes_done, so the progress fingerprint below would flag it as stalled. Leave it alone
      // while it runs -- the erase is bounded by the driver's own wait_ready timeout, after
      // which run_chunk_ clears the flag and either finishes or reports the error.
      if (req.blocking_erase_active.load()) {
        req.last_progress_ms = now;  // keep the clock fresh so it is not timed out the instant it clears
        continue;
      }
      uint64_t mark = req.offset ^ (req.bytes_done.load() << 1) ^ (req.file_done.load() << 2);
      // Test the pointer as well, not just the op: this runs on the main loop against a
      // request the worker task may be running right now, and the pre-phase writes req.tree
      // and req.op as two separate plain stores. Observing the op without the walk is a
      // narrow window, but it is a null dereference when it happens.
      if (is_tree_op(req.op) && req.tree != nullptr)
        mark ^= (static_cast<uint64_t>(req.tree->files_done) << 32) ^ (static_cast<uint64_t>(req.tree->depth) << 56);
      if (mark != req.progress_mark) {
        req.progress_mark = mark;
        req.last_progress_ms = now;
      } else if (now - req.last_progress_ms > REQUEST_STALL_TIMEOUT_MS) {
        ESP_LOGW(TAG, "Transfer '%s' -> '%s' made no progress for %us - timing it out", req.src_path, req.dst_path,
                 static_cast<unsigned>(REQUEST_STALL_TIMEOUT_MS / 1000));
        // Ownership contract: a RUNNING request is owned by an engine (loop-sliced or worker
        // task) that is potentially inside a blocking driver call on its handles RIGHT NOW.
        // Finishing it here closed those handles under the engine's feet and -- worse -- the
        // completion sweep above then freed the slot in this very pass, while
        // loop_active_index_ still pointed at it: the next section ran run_chunk_() on a FREE
        // slot with nulled storages and the index never reset (FREE != DONE), permanently
        // wedging the loop engine. Every later loop-sliced job then sat PENDING at 0 bytes
        // and, with the pool full, submissions answered NOT_READY. Mark CANCELLED instead:
        // the owning engine's next chunk boundary observes it, closes its own handles and
        // finishes with the reason carried in cancel_result.
        req.cancel_result = StorageError::TIMEOUT;
        req.state = RequestState::CANCELLED;
      }
    } else if (st == RequestState::PENDING && now - req.submitted_ms > REQUEST_PENDING_CAP_MS) {
      ESP_LOGW(TAG, "Transfer '%s' -> '%s' pending for %us - timing it out", req.src_path, req.dst_path,
               static_cast<unsigned>(REQUEST_PENDING_CAP_MS / 1000));
      finish_request(req, StorageError::TIMEOUT);
    }
  }
  for (auto &sreq : this->stream_pool_) {
    StreamState sstate = sreq.state.load(std::memory_order_acquire);
    if (sstate == StreamState::FREE || sreq.last_activity_ms == 0 ||
        now - sreq.last_activity_ms <= STREAM_IDLE_TIMEOUT_MS)
      continue;
    ESP_LOGW(TAG, "Stream on '%s' idle for %us - abandoning it", sreq.path,
             static_cast<unsigned>(STREAM_IDLE_TIMEOUT_MS / 1000));
    if (sstate == StreamState::DONE) {
      // Finished but never collected -- the client is gone. Deliver the result once (the update()
      // sweep would have, had it run first) before reclaiming, so a waiter still learns the outcome.
      CompletionCallback cb = std::move(sreq.callback);
      sreq.callback = nullptr;
      sreq.state = StreamState::FREE;
      if (cb)
        cb(sreq.result);
    } else if (sstate == StreamState::IDLE) {
      // Same immediate-finish contract as the quiesce drain: no I/O in flight.
      if (sreq.is_fs && sreq.handle != nullptr)
        static_cast<FilesystemStorage *>(sreq.storage)->close(sreq.handle);
      sreq.handle = nullptr;
      sreq.result = StorageError::TIMEOUT;
      sreq.state = StreamState::DONE;
    } else {
      // A step is queued or running -- CANCELLED makes run_stream_step_ close and finish it.
      sreq.state = StreamState::CANCELLED;
    }
  }
}

void StorageWorker::run_chunk_(TransferRequest &req, bool on_task) {
  // Cancellation check, before doing any I/O this call. Whoever set CANCELLED (hotplug
  // drain or stall watchdog) put its reason into cancel_result -- this is the only place a
  // cancelled request is finished, always by the engine that owns it.
  if (req.state.load() == RequestState::CANCELLED) {
    finish_request(req, req.cancel_result);
    return;
  }
  req.network_waiting = false;  // set again below only if a side is still coming up
  if (global_storage_registry != nullptr &&
      ((req.src_storage != nullptr && !global_storage_registry->is_registered(req.src_storage)) ||
       (req.dst_storage != nullptr && !global_storage_registry->is_registered(req.dst_storage)))) {
    finish_request(req, StorageError::NOT_READY);
    return;
  }
  if (req.op == RequestOp::FORMAT) {
    // Single blocking control-plane op: one format() call, then done. On task-safe media this
    // runs on the worker task (main loop free); otherwise from a loop() step -- still one
    // blocking call, but owned, with the completion delivered exactly like any transfer.
    // async_format() always sets dst_storage; the guard keeps the static analyzer happy and
    // turns a caller bug into a clean error rather than a null dereference.
    if (req.dst_storage == nullptr) {
      finish_request(req, StorageError::INVALID_ARGS);
      return;
    }
    finish_request(req, static_cast<FilesystemStorage *>(req.dst_storage)->format());
    return;
  }
  if (req.op == RequestOp::RAW_READ_TO_FILE || req.op == RequestOp::RAW_WRITE_FROM_FILE ||
      req.op == RequestOp::RAW_ERASE || req.op == RequestOp::RAW_VERIFY_FILE) {
    this->run_raw_chunk_(req, on_task);
    return;
  }
  // Past the raw dispatch every op is path-to-path, so both sides are set -- the submit funnels
  // reject anything else. Everything below dereferences them without asking again.
  if (req.src_storage == nullptr || req.dst_storage == nullptr) {
    finish_request(req, StorageError::INVALID_ARGS);
    return;
  }

  // The relocated handler pre-phase, executed once inside the engine that owns the storages
  // (the file API is a pure translator and performs no driver I/O of its own): classify the
  // source, honor the overwrite contract, clear an occupied destination, and decide
  // tree-vs-file HERE by materializing the walk for a directory source.
  if (!req.pre_phase_done) {
    req.pre_phase_done = true;
    FileStat src_st{};
    StorageError serr = req.src_storage->stat(req.src_path, &src_st);
    if (this->wait_for_network_ready_(req, serr, req.src_storage)) {
      req.pre_phase_done = false;  // not decided yet -- redo the whole phase once ready
      return;
    }
    if (serr != StorageError::OK) {
      finish_request(req, StorageError::NOT_FOUND);
      return;
    }
    const bool src_is_dir = src_st.is_dir;
    const bool is_move_op = req.op == RequestOp::MOVE || req.op == RequestOp::MOVE_TREE;
    const bool same_storage_move = is_move_op && req.src_storage == req.dst_storage;
    // Explicit tree submissions (async_copy_tree()/async_move_tree(), e.g. automations) keep
    // their historical contract: an existing destination root is merged into, entries below
    // it overwritten by the walk itself. Only the self-classifying API enforces overwrite.
    const bool explicit_tree = is_tree_op(req.op);
    FileStat dst_st{};
    StorageError derr = req.dst_storage->stat(req.dst_path, &dst_st);
    if (this->wait_for_network_ready_(req, derr, req.dst_storage)) {
      req.pre_phase_done = false;
      return;
    }
    if (derr == StorageError::OK && !explicit_tree) {
      if (!req.overwrite) {
        finish_request(req, StorageError::ALREADY_EXISTS);
        return;
      }
      if (dst_st.is_dir != src_is_dir) {
        // Never trade a tree for a file or the other way round, no matter what was asked for.
        finish_request(req, StorageError::INVALID_ARGS);
        return;
      }
      // Clear the destination where the operation cannot replace it by itself -- same
      // reasoning the handler used to apply, now in the right context: a same-storage move
      // needs a free name for rename(); a directory copy was asked to REPLACE, and merging
      // would leave the old tree's files behind. A plain file write truncates by itself.
      if (same_storage_move || src_is_dir) {
        StorageError cerr =
            src_is_dir ? remove_recursive(req.dst_storage, req.dst_path) : req.dst_storage->remove(req.dst_path);
        if (cerr != StorageError::OK) {
          finish_request(req, cerr);
          return;
        }
      }
    }
    if (src_is_dir && !same_storage_move && !is_tree_op(req.op)) {
      // A directory source that rename() cannot take in one step: become a tree job.
      if (!ensure_tree(req)) {
        finish_request(req, StorageError::NO_SPACE);
        return;
      }
      strncpy(req.tree->src_root, req.src_path, STORAGE_WORKER_MAX_PATH - 1);
      strncpy(req.tree->dst_root, req.dst_path, STORAGE_WORKER_MAX_PATH - 1);
      req.op = req.op == RequestOp::MOVE ? RequestOp::MOVE_TREE : RequestOp::COPY_TREE;
    }
  }

  // A tree between files: take the next walk step. It either sets up the next file (and falls
  // through to the chunk loop below) or finishes the request -- no caller involved either way.
  if (is_tree_op(req.op) && !req.tree->file_in_flight) {
    if (!this->tree_step_(req))
      return;
  }

  // First call for this request: do the cheap same-storage rename() fast path for MOVE, or
  // open handles / size-check for everything else that needs the chunk loop.
  if (!req.handles_open) {
    if (req.op == RequestOp::MOVE && req.src_storage == req.dst_storage) {
      StorageError err = req.src_storage->rename(req.src_path, req.dst_path);
      // A refusal (an NFS export spanning file systems answers NOT_SUPPORTED) is the one error
      // worth redoing the long way: fall through to the chunk loop below, which copies and then
      // removes the source. Directories are not ours to salvage -- the loop moves file bytes, so
      // the caller's per-file walker has to take that one.
      if (this->wait_for_network_ready_(req, err, req.src_storage))
        return;
      bool salvageable = err == StorageError::NOT_SUPPORTED && global_storage_registry != nullptr &&
                         global_storage_registry->get_move_fallback_copy();
      if (salvageable) {
        FileStat st{};
        salvageable = req.src_storage->stat(req.src_path, &st) == StorageError::OK && !st.is_dir;
      }
      if (!salvageable) {
        finish_request(req, err);
        return;
      }
      ESP_LOGD(TAG, "rename refused for '%s' -- moving it as copy + remove instead", req.src_path);
    }

    // Progress total for get_transfer_status(): one cheap stat on the source. A failure here
    // is not fatal -- bytes_total stays 0, which consumers must treat as "unknown/indeterminate"
    // (the transfer itself still detects a truly missing source at open()/read time).
    {
      FileStat src_stat{};
      if (req.src_storage->stat(req.src_path, &src_stat) == StorageError::OK && !src_stat.is_dir) {
        // bytes_total belongs to the request as a whole and stays 0 for a tree (unknown
        // without walking it twice); the file in flight is one cheap stat either way.
        if (!is_tree_op(req.op))
          req.bytes_total.store(src_stat.size);
        req.file_total.store(src_stat.size);
      }
    }

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
        req.handles_open = true;  // src handle (if any) is open -- let finish_request() close it
        finish_request(req, err);
        return;
      }
    } else {
      // OpenMode::WRITE truncates for the filesystem branch above; write_chunk() addresses by
      // offset and never shortens, so a shorter source would leave the old tail behind.
      StorageError err = static_cast<NetworkStorage *>(req.dst_storage)->truncate(req.dst_path, 0);
      if (this->wait_for_network_ready_(req, err, req.dst_storage)) {
        // The source handle (if any) is already open but handles_open is still false, so a
        // retry would re-enter this block and reopen it, leaking one handle per retry until
        // the pool is exhausted. Close it here; the next pass reopens cleanly.
        if (req.src_is_fs && req.src_handle != nullptr) {
          static_cast<FilesystemStorage *>(req.src_storage)->close(req.src_handle);
          req.src_handle = nullptr;
        }
        return;
      }
      if (err != StorageError::OK) {
        req.handles_open = true;
        finish_request(req, err);
        return;
      }
    }
    req.handles_open = true;
  }

  // One chunk: read, then write it out fully (write_chunk/write can themselves return partial
  // writes -- loop here same as the synchronous copy() helper does). The buffer belongs to the
  // engine and only has to survive this call -- nothing carries over to the next one.
  size_t chunk_size = 0;
  uint8_t *chunk = this->chunk_buffer_(on_task, &chunk_size);
  if (chunk == nullptr) {
    finish_request(req, StorageError::NO_SPACE);
    return;
  }
  size_t bytes_read = 0;
  StorageError err;
  if (req.src_is_fs) {
    err = static_cast<FilesystemStorage *>(req.src_storage)->read(req.src_handle, chunk, chunk_size, &bytes_read);
  } else {
    err = static_cast<NetworkStorage *>(req.src_storage)
              ->read_chunk(req.src_path, chunk, req.offset, chunk_size, &bytes_read);
  }
  if (err != StorageError::OK) {
    if (this->wait_for_network_ready_(req, err, req.src_storage))
      return;
    finish_request(req, err);
    return;
  }
  if (bytes_read == 0) {
    // EOF of the file in flight.
    if (is_tree_op(req.op)) {
      // Close it before touching the entry itself, then let the walk pick the next one on the
      // next call. The chunk buffer stays -- the next file reuses it.
      close_handles(req, &err);
      req.handles_open = false;
      if (req.op == RequestOp::MOVE_TREE && err == StorageError::OK)
        err = req.src_storage->remove(req.src_path);
      if (err != StorageError::OK) {
        finish_request(req, err);
        return;
      }
      req.tree->bytes_base += req.offset;
      req.tree->files_done++;
      req.tree->file_in_flight = false;
      req.offset = 0;
      req.file_done.store(0);
      req.file_total.store(0);
      return;
    }
    if (req.op == RequestOp::MOVE) {
      // Cross-storage move: the copy succeeded, now remove the source. Per move()'s
      // documented semantics, if this remove fails the destination copy is kept rather than
      // rolled back -- the caller ends up with the file in both places, not neither.
      err = req.src_storage->remove(req.src_path);
    }
    finish_request(req, err);
    return;
  }

  size_t total_written = 0;
  while (total_written < bytes_read) {
    size_t bytes_written = 0;
    if (req.dst_is_fs) {
      err = static_cast<FilesystemStorage *>(req.dst_storage)
                ->write(req.dst_handle, chunk + total_written, bytes_read - total_written, &bytes_written);
    } else {
      err = static_cast<NetworkStorage *>(req.dst_storage)
                ->write_chunk(req.dst_path, chunk + total_written, req.offset + total_written,
                              bytes_read - total_written, &bytes_written);
    }
    if (err != StorageError::OK) {
      if (this->wait_for_network_ready_(req, err, req.dst_storage))
        return;
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
  // Progress for get_transfer_status(): offset equals bytes fully read AND written at this
  // point (the write loop above completed), so it doubles as bytes_done. Atomic store because
  // the main loop may snapshot progress while the worker task runs this transfer.
  req.bytes_done.store(is_tree_op(req.op) ? req.tree->bytes_base + req.offset : req.offset);
  req.file_done.store(req.offset);
  // Request stays RUNNING; the next call (next loop() iteration, or the task's own loop)
  // picks up at the new offset. No watchdog feed here by design -- see task_loop_()'s comment
  // for the task path; the loop-sliced path returns to the main loop's own feed_wdt() between
  // iterations same as any other component's loop().
}

// ===========================================================================================
// Streaming (begin_write/write_chunk/end_write, begin_read/read_chunk/end_read) -- shares this
// StorageWorker's pool machinery pattern (FixedVector, compare_exchange slot claim, same task/
// queue) but is its own independent state machine; see StreamRequest's comment in the header
// for why it isn't folded into TransferRequest/run_chunk_().

bool StorageWorker::is_task_safe_(const StreamRequest &req) const {
  (void) req;
#if defined(USE_ESP32) && defined(USE_STORAGE_WORKER_TASK)
  if (!this->task_running_)
    return false;
  return (req.storage->get_capabilities() & StorageCaps::STORAGE_CAP_IO_TASK_SAFE) != 0;
#else
  return false;
#endif
}

// Advances one stream by exactly the operation implied by its current state -- open, one
// write chunk, one read chunk, or close -- then transitions to IDLE (more chunks expected,
// open()/close() are one-shot so those always move straight past IDLE) or DONE. Unlike
// run_chunk_(), this never decides what to do next on its own: state is set by the caller's
// begin_*()/write_chunk()/read_chunk()/end_*() call, this function just executes it once.
static void run_stream_step(StreamRequest &req) {
  if (req.state.load() == StreamState::CANCELLED) {
    if (req.is_fs && req.handle != nullptr)
      static_cast<FilesystemStorage *>(req.storage)->close(req.handle);
    req.result = StorageError::NOT_READY;
    req.state = StreamState::DONE;
    return;
  }

  StreamState current = req.state.load();
  StorageError err = StorageError::OK;

  switch (current) {
    case StreamState::OPENING: {
      if (req.is_fs) {
        OpenMode mode = req.op == StreamOp::WRITE ? OpenMode::WRITE : OpenMode::READ;
        err = static_cast<FilesystemStorage *>(req.storage)->open(req.path, req.handle, mode);
      } else if (req.op == StreamOp::WRITE) {
        // Same reason as the copy path: a write stream to a NetworkStorage has no open() to
        // truncate for it, so an upload shorter than the file it replaces would leave the old
        // tail in place. A read stream changes nothing and needs none of this.
        err = static_cast<NetworkStorage *>(req.storage)->truncate(req.path, 0);
      }
      // NetworkStorage has no open() -- write_chunk()/read_chunk() below address by offset
      // directly, so OPENING is a no-op for it beyond the state transition itself.
      req.state = (err == StorageError::OK) ? StreamState::IDLE : StreamState::DONE;
      req.result = err;
      break;
    }

    case StreamState::WRITING: {
      size_t bytes_written = 0;
      if (req.is_fs) {
        err = static_cast<FilesystemStorage *>(req.storage)
                  ->write(req.handle, req.pending_write_data, req.pending_len, &bytes_written);
      } else {
        err = static_cast<NetworkStorage *>(req.storage)
                  ->write_chunk(req.path, req.pending_write_data, req.offset, req.pending_len, &bytes_written);
      }
      if (err == StorageError::OK && bytes_written < req.pending_len && bytes_written > 0) {
        // Partial write -- keep going within this same call rather than surfacing a short
        // write to the caller, mirroring storage::write_file()'s own retry-until-full loop.
        size_t total = bytes_written;
        while (err == StorageError::OK && total < req.pending_len) {
          size_t chunk_written = 0;
          if (req.is_fs) {
            err = static_cast<FilesystemStorage *>(req.storage)
                      ->write(req.handle, req.pending_write_data + total, req.pending_len - total, &chunk_written);
          } else {
            err = static_cast<NetworkStorage *>(req.storage)
                      ->write_chunk(req.path, req.pending_write_data + total, req.offset + total,
                                    req.pending_len - total, &chunk_written);
          }
          if (chunk_written == 0)
            break;
          total += chunk_written;
        }
        bytes_written = total;
      }
      if (err == StorageError::OK && bytes_written < req.pending_len)
        err = StorageError::WRITE_ERROR;  // 0 bytes accepted mid-stream -- treat as failure
      req.offset += bytes_written;
      req.pending_write_data = nullptr;
      req.pending_len = 0;
      req.result = err;
      req.state = StreamState::IDLE;
      break;
    }

    case StreamState::READING: {
      size_t bytes_read = 0;
      if (req.is_fs) {
        err = static_cast<FilesystemStorage *>(req.storage)
                  ->read(req.handle, req.pending_read_buf, req.pending_len, &bytes_read);
      } else {
        err = static_cast<NetworkStorage *>(req.storage)
                  ->read_chunk(req.path, req.pending_read_buf, req.offset, req.pending_len, &bytes_read);
      }
      if (err == StorageError::OK)
        req.offset += bytes_read;
      if (req.bytes_transferred_out != nullptr)
        *req.bytes_transferred_out = bytes_read;
      req.pending_read_buf = nullptr;
      req.pending_len = 0;
      req.result = err;
      req.state = StreamState::IDLE;
      break;
    }

    case StreamState::SEEKING: {
      if (req.is_fs) {
        // The driver's seek() handles SET/CUR/END natively; sync req.offset from it afterwards so
        // tell() and any later network-style bookkeeping agree.
        err = static_cast<FilesystemStorage *>(req.storage)->seek(req.handle, req.seek_target, req.seek_mode);
        if (err == StorageError::OK)
          err = static_cast<FilesystemStorage *>(req.storage)->tell(req.handle, &req.offset);
      } else {
        // NetworkStorage has no handle -- read_chunk()/write_chunk() address by req.offset, so a
        // seek is arithmetic on it. END needs the file size.
        int64_t base = 0;
        switch (req.seek_mode) {
          case storage::SeekMode::SET:
            base = 0;
            break;
          case storage::SeekMode::CUR:
            base = static_cast<int64_t>(req.offset);
            break;
          case storage::SeekMode::END: {
            uint64_t sz = 0;
            err = storage::file_size(req.storage, req.path, &sz);
            base = static_cast<int64_t>(sz);
            break;
          }
        }
        if (err == StorageError::OK) {
          const int64_t target = base + req.seek_target;
          if (target < 0) {
            err = StorageError::INVALID_ARGS;
          } else {
            req.offset = static_cast<uint64_t>(target);
          }
        }
      }
      req.result = err;
      req.state = StreamState::IDLE;
      break;
    }

    case StreamState::TELLING: {
      if (req.is_fs) {
        err = static_cast<FilesystemStorage *>(req.storage)->tell(req.handle, &req.offset);
      }
      // NetworkStorage: the tracked offset already IS the position.
      if (err == StorageError::OK && req.tell_out != nullptr)
        *req.tell_out = req.offset;
      req.tell_out = nullptr;
      req.result = err;
      req.state = StreamState::IDLE;
      break;
    }

    case StreamState::CLOSING: {
      if (req.is_fs && req.handle != nullptr)
        err = static_cast<FilesystemStorage *>(req.storage)->close(req.handle);
      req.handle = nullptr;
      req.result = err;
      req.state = StreamState::DONE;
      break;
    }

    default:
      // IDLE/FREE/DONE: nothing to do -- shouldn't be dispatched in these states.
      break;
  }
}

void StorageWorker::run_stream_step_(StreamRequest &req) {
  req.last_activity_ms = millis();  // the client is demonstrably still driving this stream
  run_stream_step(req);
}

static bool find_free_stream_slot(FixedVector<StreamRequest> &pool, size_t *out_index) {
  for (size_t i = 0; i < pool.size(); i++) {
    StreamState expected = StreamState::FREE;
    if (pool[i].state.compare_exchange_strong(expected, StreamState::OPENING)) {
      pool[i].last_activity_ms = millis();
      if (++pool[i].generation == 0)
        pool[i].generation = 1;  // 0 stays the never-claimed value
      *out_index = i;
      return true;
    }
  }
  return false;
}

// Dispatches one step: hands off to the shared task queue if task-safe, otherwise marks it
// pending for loop() to run on its next pass. Never runs the step inline from this call -- the
// callback must always fire from loop() (or the task), never reentrantly from inside the
// caller's own begin_*/write_chunk/read_chunk/end_* call frame.
void StorageWorker::dispatch_stream_step_(StreamRequest &req, size_t index) {
  // Arm the engine unconditionally: stream completions are delivered from update() even on the task
  // path, so the poller must run whether this step goes to the worker task or the loop-sliced engine.
  this->start_poller();
#if defined(USE_ESP32) && defined(USE_STORAGE_WORKER_TASK)
  if (this->is_task_safe_(req) && !this->stream_overlaps_active_(req, /*from_loop=*/false)) {
    QueueEntry entry{QueueEntryKind::STREAM, index};
    if (xQueueSend(this->task_queue_, &entry, 0) == pdTRUE)
      return;
  }
#endif
  // Loop-sliced fallback: mark pending so loop() runs it on its next pass rather than here
  // (streams share the transfer engine's driver -- see loop()'s tail). The poller is already
  // armed at the top of this function.
  req.pending_step_ = true;
}

// Resolves a caller's handle to its slot, or nullptr when the handle no longer refers to the
// stream it was issued for -- out of range, or the slot has been reclaimed since.
StreamRequest *StorageWorker::stream_for_handle_(const StreamHandle &handle) {
  if (handle.index >= this->stream_pool_.size())
    return nullptr;
  StreamRequest &req = this->stream_pool_[handle.index];
  if (req.generation != handle.generation)
    return nullptr;
  return &req;
}

StorageError StorageWorker::begin_write(PathStorage *storage, const char *path, StreamHandle *out_handle,
                                        CompletionCallback &&on_open) {
  this->ensure_started_();
  if (strlen(path) >= STORAGE_WORKER_MAX_PATH)
    return StorageError::INVALID_ARGS;

  size_t index;
  if (!find_free_stream_slot(this->stream_pool_, &index))
    return StorageError::NOT_READY;

  StreamRequest &req = this->stream_pool_[index];
  req.op = StreamOp::WRITE;
  req.storage = storage;
  strncpy(req.path, path, STORAGE_WORKER_MAX_PATH - 1);
  req.path[STORAGE_WORKER_MAX_PATH - 1] = '\0';
  req.is_fs = storage->get_storage_type() == StorageType::FILESYSTEM;
  req.handle = nullptr;
  req.offset = 0;
  req.callback = std::move(on_open);
  req.result = StorageError::OK;

  out_handle->index = index;
  out_handle->generation = req.generation;
  this->dispatch_stream_step_(req, index);
  return StorageError::OK;
}

StorageError StorageWorker::begin_read(PathStorage *storage, const char *path, StreamHandle *out_handle,
                                       CompletionCallback &&on_open) {
  this->ensure_started_();
  if (strlen(path) >= STORAGE_WORKER_MAX_PATH)
    return StorageError::INVALID_ARGS;

  size_t index;
  if (!find_free_stream_slot(this->stream_pool_, &index))
    return StorageError::NOT_READY;

  StreamRequest &req = this->stream_pool_[index];
  req.op = StreamOp::READ;
  req.storage = storage;
  strncpy(req.path, path, STORAGE_WORKER_MAX_PATH - 1);
  req.path[STORAGE_WORKER_MAX_PATH - 1] = '\0';
  req.is_fs = storage->get_storage_type() == StorageType::FILESYSTEM;
  req.handle = nullptr;
  req.offset = 0;
  req.callback = std::move(on_open);
  req.result = StorageError::OK;

  out_handle->index = index;
  out_handle->generation = req.generation;
  this->dispatch_stream_step_(req, index);
  return StorageError::OK;
}

StorageError StorageWorker::write_chunk(const StreamHandle &handle, const uint8_t *data, size_t len,
                                        CompletionCallback &&on_written) {
  StreamRequest *slot = this->stream_for_handle_(handle);
  if (slot == nullptr)
    return StorageError::INVALID_ARGS;
  StreamRequest &req = *slot;
  StreamState expected = StreamState::IDLE;
  if (!req.state.compare_exchange_strong(expected, StreamState::WRITING))
    return StorageError::NOT_READY;

  req.pending_write_data = data;
  req.pending_len = len;
  req.callback = std::move(on_written);

  this->dispatch_stream_step_(req, handle.index);
  return StorageError::OK;
}

StorageError StorageWorker::read_chunk(const StreamHandle &handle, uint8_t *buf, size_t len, size_t *bytes_read,
                                       CompletionCallback &&on_read) {
  StreamRequest *slot = this->stream_for_handle_(handle);
  if (slot == nullptr)
    return StorageError::INVALID_ARGS;
  StreamRequest &req = *slot;
  StreamState expected = StreamState::IDLE;
  if (!req.state.compare_exchange_strong(expected, StreamState::READING))
    return StorageError::NOT_READY;

  req.pending_read_buf = buf;
  req.pending_len = len;
  req.bytes_transferred_out = bytes_read;
  req.callback = std::move(on_read);

  this->dispatch_stream_step_(req, handle.index);
  return StorageError::OK;
}

StorageError StorageWorker::end_write(const StreamHandle &handle, CompletionCallback &&on_closed) {
  StreamRequest *slot = this->stream_for_handle_(handle);
  if (slot == nullptr)
    return StorageError::INVALID_ARGS;
  StreamRequest &req = *slot;
  StreamState expected = StreamState::IDLE;
  if (!req.state.compare_exchange_strong(expected, StreamState::CLOSING))
    return StorageError::NOT_READY;

  req.callback = std::move(on_closed);
  this->dispatch_stream_step_(req, handle.index);
  return StorageError::OK;
}

StorageError StorageWorker::end_read(const StreamHandle &handle, CompletionCallback &&on_closed) {
  return this->end_write(handle, std::move(on_closed));  // identical close path
}

StorageError StorageWorker::seek(const StreamHandle &handle, int64_t offset, storage::SeekMode mode,
                                 CompletionCallback &&on_seeked) {
  StreamRequest *slot = this->stream_for_handle_(handle);
  if (slot == nullptr)
    return StorageError::INVALID_ARGS;
  StreamRequest &req = *slot;
  StreamState expected = StreamState::IDLE;
  if (!req.state.compare_exchange_strong(expected, StreamState::SEEKING))
    return StorageError::NOT_READY;

  req.seek_target = offset;
  req.seek_mode = mode;
  req.callback = std::move(on_seeked);

  this->dispatch_stream_step_(req, handle.index);
  return StorageError::OK;
}

StorageError StorageWorker::tell(const StreamHandle &handle, uint64_t *position, CompletionCallback &&on_told) {
  StreamRequest *slot = this->stream_for_handle_(handle);
  if (slot == nullptr)
    return StorageError::INVALID_ARGS;
  StreamRequest &req = *slot;
  StreamState expected = StreamState::IDLE;
  if (!req.state.compare_exchange_strong(expected, StreamState::TELLING))
    return StorageError::NOT_READY;

  req.tell_out = position;
  req.callback = std::move(on_told);

  this->dispatch_stream_step_(req, handle.index);
  return StorageError::OK;
}

}  // namespace esphome::storage

#endif  // USE_STORAGE_WORKER
