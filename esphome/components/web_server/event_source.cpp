#include "event_source.h"
#if defined(USE_WEBSERVER) && (defined(USE_ESP32) || defined(USE_ARDUINO))

#include <algorithm>
#include <cstring>
#include <iterator>

#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "web_server.h"

namespace esphome::web_server {

ESPHOME_LOG_TAG(TAG, "web_server");

// Only prepare_json_() may hold the JSON arena: every other frame in this file is capped below one
// arena. Measured at -Os on GCC 14; newer toolchains stay checked on purpose, older ones skip it.
#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 14 && defined(__OPTIMIZE_SIZE__)
#pragma GCC diagnostic error "-Wstack-usage=2048"
#endif

AsyncEventSource::~AsyncEventSource() {
  web_server_base::StreamIntakeGuard guard{this->pending_mutex_};
  for (auto *ses : {this->sessions_, this->pending_sessions_.load(std::memory_order_relaxed)}) {
    while (ses != nullptr) {
      auto *next = ses->next_;
      delete ses;  // NOLINT(cppcoreguidelines-owning-memory)
      ses = next;
    }
  }
}

void AsyncEventSource::handle_request(AsyncWebServerRequest *request, WebServer *ws) {
  // Network context: start the stream and park the session; the main loop does the rest.
#ifdef USE_ESP32
  // NOLINTNEXTLINE(cppcoreguidelines-owning-memory,clang-analyzer-cplusplus.NewDeleteLeaks)
  auto *rsp = new AsyncEventSourceResponse(ws);
  rsp->begin(request, SSE_STATUS, SSE_CONTENT_TYPE, SSE_HEADERS, SSE_FIRST_CHUNK);
  this->push_pending_(rsp);
#else
  // The session is created only once the head is queued and the connection is ours
  web_server_base::AsyncStreamResponse::accept(
      request, SSE_STATUS, SSE_CONTENT_TYPE, SSE_HEADERS, SSE_FIRST_CHUNK,
      {&AsyncEventSource::create_session, &AsyncEventSource::publish_session, this, ws});
#endif
}

void AsyncEventSource::push_pending_(AsyncEventSourceResponse *rsp) {
  {
    web_server_base::StreamIntakeGuard guard{this->pending_mutex_};
    rsp->next_ = this->pending_sessions_.load(std::memory_order_relaxed);
    this->pending_sessions_.store(rsp, std::memory_order_release);
  }
  rsp->web_server_->enable_loop_soon_any_context();
}

#ifndef USE_ESP32
web_server_base::AsyncStreamResponse *AsyncEventSource::create_session(void *ws) {
  // NOLINTNEXTLINE(cppcoreguidelines-owning-memory,clang-analyzer-cplusplus.NewDeleteLeaks)
  return new AsyncEventSourceResponse(static_cast<WebServer *>(ws));
}

void AsyncEventSource::publish_session(void *source, web_server_base::AsyncStreamResponse *stream) {
  static_cast<AsyncEventSource *>(source)->push_pending_(static_cast<AsyncEventSourceResponse *>(stream));
}
#endif

// clang-analyzer traces a false-positive leak path from loop() through
// adopt_pending_sessions_main_loop_() into start_session_main_loop_() and
// finally ArduinoJson. Suppress along the entire in-our-code call chain.
// NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks)
bool AsyncEventSource::loop() {
  // Fast path: one atomic load per tick. Slow path is out-of-line on connect.
  if (this->pending_sessions_.load(std::memory_order_acquire) != nullptr) {
    this->adopt_pending_sessions_main_loop_();
  }

  // The network side only marks a session dead; the main loop frees it, so nothing races the free
  for (AsyncEventSourceResponse **link = &this->sessions_; *link != nullptr;) {
    auto *ses = *link;
    if (ses->safe_to_delete()) {
      *link = ses->next_;
      delete ses;  // NOLINT(cppcoreguidelines-owning-memory)
    } else {
      ses->loop();
      link = &ses->next_;
    }
  }
  // A session still waiting for its connection keeps the loop alive too
  return this->sessions_ != nullptr || this->pending_sessions_.load(std::memory_order_acquire) != nullptr;
}

void AsyncEventSource::adopt_pending_sessions_main_loop_() {
  AsyncEventSourceResponse *incoming;
  {
    web_server_base::StreamIntakeGuard guard{this->pending_mutex_};
    // Plain load and store under the guard: RP2040 has no atomic read-modify-write
    incoming = this->pending_sessions_.load(std::memory_order_acquire);
    this->pending_sessions_.store(nullptr, std::memory_order_relaxed);
  }
  while (incoming != nullptr) {
    auto *rsp = incoming;
    incoming = rsp->next_;
    // Already disconnected? Drop it; skip session start on a dead session.
    if (rsp->safe_to_delete()) {
      delete rsp;  // NOLINT(cppcoreguidelines-owning-memory)
      continue;
    }
    // httpd commits a stream after the creating handler returns (or destroys it), so it stays pending until then;
    // on ESPAsyncWebServer a session is published already attached
    if (!rsp->is_attached()) {
      web_server_base::StreamIntakeGuard guard{this->pending_mutex_};
      rsp->next_ = this->pending_sessions_.load(std::memory_order_relaxed);
      this->pending_sessions_.store(rsp, std::memory_order_release);
      continue;
    }
    rsp->next_ = this->sessions_;
    this->sessions_ = rsp;
    rsp->start_session_main_loop_();
  }
}
// NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)

void AsyncEventSource::try_send_nodefer(const char *message, size_t message_len, const char *event, uint32_t id,
                                        uint32_t reconnect) {
  for (auto *ses = this->sessions_; ses != nullptr; ses = ses->next_) {
    if (ses->is_connected()) {
      ses->try_send_nodefer(message, message_len, event, id, reconnect);
    }
  }
}

void AsyncEventSource::deferrable_send_state(void *source, message_generator_t *message_generator) {
  for (auto *ses = this->sessions_; ses != nullptr; ses = ses->next_) {
    if (ses->is_connected()) {
      ses->deferrable_send_state(source, false, message_generator);
    }
  }
}

// Network context only. start_session_main_loop_() sends the greeting and starts the iterator.
AsyncEventSourceResponse::AsyncEventSourceResponse(WebServer *ws) : web_server_(ws), entities_iterator_(ws, this) {}
static_assert(web_server_base::stream_head_iov_count(SSE_HEADERS_COUNT + WEB_SERVER_DEFAULT_HEADERS_COUNT) <=
                  web_server_base::STREAM_HEAD_MAX_IOV,
              "the event stream head must go out in one write");

// NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks) false positive with ArduinoJson
void AsyncEventSourceResponse::start_session_main_loop_() {
  auto *ws = this->web_server_;

  // The tcp send buffer is empty on connect. A refusal is a closing session or a failed tail
  // allocation; nothing retries the greeting, so close and let the client reconnect.
  auto message = ws->get_config_json();
  if (!this->try_send_nodefer(message.c_str(), message.size(), "ping", millis(), 30000)) {
    // A stream already closing (its head did not go out whole) is not a failed greeting
    if (!this->is_closing()) {
      ESP_LOGW(TAG, "Config not sent to stream %d", this->log_id());
    }
    this->close();
    return;
  }

#ifdef USE_WEBSERVER_SORTING
  for (auto &group : ws->sorting_groups_) {
    json::JsonBuilder builder;
    JsonObject root = builder.root();
    root["name"] = json::linked(group.second.name);
    root["sorting_weight"] = group.second.weight;
    message = builder.serialize();

    // Groups the socket does not take yet queue in the tail behind the ping. A refusal means the
    // tail is full or the stream is closing; the remaining groups are not sent. The session stays
    // up on purpose: partial grouping beats a reconnect loop on a slow link.
    if (!this->try_send_nodefer(message.c_str(), message.size(), "sorting_group")) {
      // A stream closing mid-greeting is not a failed send
      if (!this->is_closing()) {
        ESP_LOGW(TAG, "Sorting groups not sent to stream %d", this->log_id());
      }
      break;
    }
  }
#endif

  this->entities_iterator_.begin(ws->include_internal_);
}
// NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)

// helper for allowing only unique entries in the queue
void AsyncEventSourceResponse::deq_push_back_with_dedup_(void *source, message_generator_t *message_generator) {
  DeferredEvent item(source, message_generator);

  // Use range-based for loop instead of std::find_if to reduce template instantiation overhead and binary size
  for (auto &event : this->deferred_queue_) {
    if (event == item) {
      return;  // Already in queue, no need to update since items are equal
    }
  }
  this->deferred_queue_.push_back(item);
}

void AsyncEventSourceResponse::process_deferred_queue_() {
  if (this->is_closing()) {
    return;
  }
  while (!deferred_queue_.empty()) {
    DeferredEvent &de = deferred_queue_.front();
    if (this->send_json_(de.source_, de.message_generator_)) {
      if (this->is_closing() || deferred_queue_.empty()) {
        return;
      }
      // O(n) but memory efficiency is more important than speed here which is why std::vector was chosen
      deferred_queue_.erase(deferred_queue_.begin());
    } else {
      break;
    }
  }
}

void AsyncEventSourceResponse::release_buffers_() {
  std::vector<DeferredEvent>().swap(this->deferred_queue_);
  this->tail_.reset();
  this->tail_cap_ = 0;
  this->tail_len_ = 0;
  this->tail_sent_ = 0;
}

void AsyncEventSourceResponse::drain_tail_() {
  if (this->sending_ || this->is_closing() || this->tail_len_ == 0) {
    return;
  }
  SendGuard guard{*this};

  const size_t remaining = this->tail_len_ - this->tail_sent_;
  const ssize_t sent = this->write(this->tail_.get() + this->tail_sent_, remaining);
  if (sent <= 0) {
    return;  // write() owns the stall clock and the close
  }
  this->no_memory_logged_ = false;
  this->tail_sent_ += sent;
  if (this->tail_sent_ < this->tail_len_) {
    ESP_LOGV(TAG, "Partial send: %zd/%zu bytes (total: %u/%u)", sent, remaining, this->tail_sent_, this->tail_len_);
    return;
  }
  // Fully sent; the storage stays for the next stall
  this->tail_len_ = 0;
}

bool AsyncEventSourceResponse::reserve_tail_(size_t len) {
  if (this->tail_cap_ >= len) {
    return true;
  }
  if (len > TAIL_MAX_SIZE) {
    return false;
  }
  // Nothing is pending while the tail grows, so free the old block first. PREFER_INTERNAL keeps
  // the tail where plain new put it.
  this->tail_.reset();
  this->tail_ = RAMAllocator<uint8_t>(RAMAllocator<uint8_t>::PREFER_INTERNAL).make_unique_array_for_overwrite(len);
  this->tail_cap_ = this->tail_ ? len : 0;
  return this->tail_cap_ != 0;
}

// Copy a chunk's prefix and its framed message pieces to dst
static void copy_chunk(uint8_t *dst, const char *prefix, size_t prefix_len, const char *message, size_t message_len) {
  std::memcpy(dst, prefix, prefix_len);
  dst += prefix_len;
  for_each_chunk_piece(
      message, message_len,
      [](void *ctx, const char *piece, size_t len) {
        auto &out = *static_cast<uint8_t **>(ctx);
        progmem_memcpy(out, piece, len);
        out += len;
      },
      &dst);
}

bool AsyncEventSourceResponse::stash_chunk_(const char *prefix, size_t prefix_len, const char *message,
                                            size_t message_len, size_t total, size_t sent) {
  // A log event of nothing but line breaks is the largest chunk; reserve_tail_() must never
  // refuse it for size, only for memory
  static_assert(SSE_SEP_LEN * LOG_EVENT_MAX_LEN + PREFIX_BUF_SIZE + SSE_SUFFIX_LEN <= TAIL_MAX_SIZE,
                "the log cut in web_server.h must keep a worst case log event inside the tail ceiling");
  if (!this->reserve_tail_(total)) {
    if (sent != 0) {
      // Part of the chunk is on the wire, so the stream is broken and the client has to go
      ESP_LOGW(TAG, "Cannot buffer a %zu byte chunk, closing", total);
      this->close();
    } else {
      this->tail_alloc_failed_(total);  // nothing on the wire, the caller retries on the stall clock
    }
    return false;
  }
  copy_chunk(this->tail_.get(), prefix, prefix_len, message, message_len);
  this->tail_len_ = total;
  this->tail_sent_ = sent;
  return true;
}

bool AsyncEventSourceResponse::append_chunk_(const char *prefix, size_t prefix_len, const char *message,
                                             size_t message_len, size_t total) {
  const size_t pending = this->tail_len_ - this->tail_sent_;
  const size_t need = pending + total;
  if (need > TAIL_QUEUE_SIZE) {
    return false;
  }
  if (need > this->tail_cap_) {
    // Half the queue first, then all of it; kept, so a burst does not allocate per event
    const size_t cap = need <= TAIL_QUEUE_SIZE / 2 ? TAIL_QUEUE_SIZE / 2 : TAIL_QUEUE_SIZE;
    auto grown = RAMAllocator<uint8_t>(RAMAllocator<uint8_t>::PREFER_INTERNAL).make_unique_array_for_overwrite(cap);
    if (!grown) {
      return false;
    }
    std::memcpy(grown.get(), this->tail_.get() + this->tail_sent_, pending);
    this->tail_ = std::move(grown);
    this->tail_cap_ = cap;
  } else if (this->tail_sent_ != 0) {
    std::memmove(this->tail_.get(), this->tail_.get() + this->tail_sent_, pending);
  }
  copy_chunk(this->tail_.get() + pending, prefix, prefix_len, message, message_len);
  this->tail_len_ = need;
  this->tail_sent_ = 0;
  return true;
}

void AsyncEventSourceResponse::loop() {
  AsyncStreamResponse::loop();
  if (this->is_closing()) {
    if (this->tail_cap_ != 0 || this->deferred_queue_.capacity() != 0) {
      this->release_buffers_();
    }
    return;
  }
  drain_tail_();
  process_deferred_queue_();
  if (this->is_closing())
    return;
  // One step per loop; refusals retry next pass
  this->entities_iterator_.try_advance(1);
}

#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 14 && defined(__OPTIMIZE_SIZE__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstack-usage="  // the one frame that holds the JSON arena
#endif
AsyncEventSourceResponse::JsonPrepareResult AsyncEventSourceResponse::prepare_json_(void *source,
                                                                                    message_generator_t *generator,
                                                                                    char *buffer, size_t *size_out) {
  // Log lines emitted while the generator runs are dropped for this session: on the large path they would fill
  // tail_ before it is overwritten, and on the small path they would go out ahead of the event being built.
  SendGuard guard{*this};
#ifdef USE_JSON_ARENA
  json::JsonArena<json::JSON_ARENA_SIZE> arena;
  json::JsonBuilder builder(&arena);
#else
  json::JsonBuilder builder;
#endif
  generator(this->web_server_, source, builder);
  *size_out = builder.serialize_to(buffer, JSON_BUF_SIZE);
  if (*size_out < JSON_BUF_SIZE) {
    return JsonPrepareResult::JSON_PREPARE_RESULT_BUFFER;
  }

  char prefix[PREFIX_BUF_SIZE];
  const size_t prefix_len = build_chunk_prefix(prefix, sizeof(prefix), "state", 0, 0, true);

  // Nothing reaches the wire until the complete document and its framing are owned by the caller.
  size_t json_len = 0;
  size_t cap = std::max<size_t>(JSON_BUF_SIZE * 2, this->tail_cap_);
  for (;;) {
    if (!this->reserve_tail_(cap)) {
      *size_out = cap;
      return JsonPrepareResult::JSON_PREPARE_RESULT_NO_MEMORY;
    }
    const size_t room = cap - prefix_len - SSE_SUFFIX_LEN;
    json_len = builder.serialize_to(reinterpret_cast<char *>(this->tail_.get()) + prefix_len, room);
    if (json_len < room) {
      break;
    }
    if (cap >= TAIL_MAX_SIZE) {
      *size_out = room;
      return JsonPrepareResult::JSON_PREPARE_RESULT_TOO_LARGE;
    }
    cap = std::min<size_t>(cap * 2, TAIL_MAX_SIZE);
  }

  const size_t total = prefix_len + json_len + SSE_SUFFIX_LEN;
  write_chunk_header(prefix, total - CHUNK_HDR_LEN - CHUNK_END_LEN);
  uint8_t *dst = this->tail_.get();
  std::memcpy(dst, prefix, prefix_len);
  progmem_memcpy(dst + prefix_len + json_len, SSE_SUFFIX, SSE_SUFFIX_LEN);
  this->tail_len_ = total;
  this->tail_sent_ = 0;
  return JsonPrepareResult::JSON_PREPARE_RESULT_TAIL;
}
#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 14 && defined(__OPTIMIZE_SIZE__)
#pragma GCC diagnostic pop
#endif

bool AsyncEventSourceResponse::send_json_(void *source, message_generator_t *generator) {
  if (!this->drain_if_idle_() || this->tail_len_ != 0) {
    return false;
  }
  char buffer[JSON_BUF_SIZE];
  size_t size;
  const auto result = this->prepare_json_(source, generator, buffer, &size);
  switch (result) {
    case JsonPrepareResult::JSON_PREPARE_RESULT_BUFFER:
      return this->try_send_nodefer(buffer, size, "state");
    case JsonPrepareResult::JSON_PREPARE_RESULT_TAIL:
      this->drain_tail_();
      return true;
    case JsonPrepareResult::JSON_PREPARE_RESULT_NO_MEMORY: {
      SendGuard guard{*this};
      this->tail_alloc_failed_(size);
      return false;
    }
    case JsonPrepareResult::JSON_PREPARE_RESULT_TOO_LARGE: {
      SendGuard guard{*this};
      ESP_LOGW(TAG, "State event does not fit %zu bytes, dropped", size);
      this->tail_.reset();
      this->tail_cap_ = 0;
      this->stall_.clear();
      this->no_memory_logged_ = false;
      return true;
    }
  }
  return false;
}

void AsyncEventSourceResponse::tail_alloc_failed_(size_t cap) {
  // Same stall clock as a socket that stops draining, so a session cannot retry forever. A refused write may
  // already have started it, so the warning keeps its own flag.
  if (!this->no_memory_logged_) {
    this->no_memory_logged_ = true;
    ESP_LOGW(TAG, "No memory for a %zu byte chunk", cap);
  }
  this->stall_or_close_(LOG_STR("without memory"));
}

bool AsyncEventSourceResponse::drain_if_idle_() {
  if (this->sending_ || !this->is_connected() || this->is_closing()) {
    return false;
  }
  drain_tail_();
  return !this->is_closing();
}

bool AsyncEventSourceResponse::try_send_nodefer(const char *message, size_t message_len, const char *event, uint32_t id,
                                                uint32_t reconnect) {
  if (!this->drain_if_idle_()) {
    return false;
  }
  SendGuard guard{*this};

  // Everything after the prefix goes out straight from the caller's buffer
  char prefix[PREFIX_BUF_SIZE];
  const size_t prefix_len = build_chunk_prefix(prefix, sizeof(prefix), event, id, reconnect, message != nullptr);
  if (message == nullptr && prefix_len == CHUNK_HDR_LEN) {
    return true;  // Match ESPAsyncWebServer: nothing to send
  }
  if (prefix_len >= PREFIX_BUF_SIZE - 1) {
    // The appenders truncate silently, which would put a malformed event on the wire
    ESP_LOGW(TAG, "Event name too long, dropped");
    return true;
  }

  // Gather list: the prefix, then the data lines and their separators from the message
  struct Gather {
    web_server_base::StreamIovec iov[MAX_SEND_IOV];  // left uninitialized on purpose
    int iovcnt{1};
    size_t total{0};
    bool fits{true};
  } g;
  g.total = prefix_len;
  for_each_chunk_piece(
      message, message_len,
      [](void *ctx, const char *piece, size_t len) {
        auto &g = *static_cast<Gather *>(ctx);
        g.total += len;
        if (len == 0) {
          return;
        }
        if (g.iovcnt == MAX_SEND_IOV) {
          g.fits = false;
          return;
        }
        g.iov[g.iovcnt++] = {const_cast<char *>(piece), len};
      },
      &g);
  // The header and the terminator are not part of the chunk length
  write_chunk_header(prefix, g.total - CHUNK_HDR_LEN - CHUNK_END_LEN);
  g.iov[0] = {prefix, prefix_len};

  if (this->tail_len_ != 0) {
    // Behind an unsent chunk (on ESP8266, one waiting for its ACK): queue it rather than drop it
    return this->append_chunk_(prefix, prefix_len, message, message_len, g.total);
  }
  // A message with more lines than the list holds skips straight to the tail
  const ssize_t sent = g.fits ? this->write(g.iov, g.iovcnt) : 0;
  if (sent < 0) {
    return false;
  }
  if (sent > 0) {
    this->no_memory_logged_ = false;
  }
  if (static_cast<size_t>(sent) == g.total) {
    return true;
  }
  // The caller's buffers do not outlive this call, so keep the chunk and continue from loop()
  return this->stash_chunk_(prefix, prefix_len, message, message_len, g.total, sent);
}

void AsyncEventSourceResponse::deferrable_send_state(void *source, bool detail_all,
                                                     message_generator_t *message_generator) {
  // Let the detail_all listing finish before bare state events, so the web UI never shows unnamed entries and
  // initial connect sends less
  if (!detail_all && !this->entities_iterator_.completed())
    return;

  drain_tail_();
  process_deferred_queue_();

  if (this->is_closing()) {
    return;
  }

  if (this->tail_len_ != 0 || !deferred_queue_.empty()) {
    // outgoing event buffer or deferred queue still not empty which means downstream tcp send buffer full, no point
    // trying to send first
    deq_push_back_with_dedup_(source, message_generator);
  } else {
    // A send error closes the session and clears the queue; nothing is queued after that
    if (!this->send_json_(source, message_generator) && !this->is_closing()) {
      deq_push_back_with_dedup_(source, message_generator);
    }
  }
}

}  // namespace esphome::web_server

#endif  // USE_WEBSERVER && (USE_ESP32 || USE_ARDUINO)
