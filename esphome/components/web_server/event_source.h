#pragma once

#include "esphome/core/defines.h"
#if defined(USE_ESP32) && defined(USE_WEBSERVER)

#include "esphome/components/json/json_util.h"
#include "esphome/components/web_server_base/stream_response.h"
#include "esphome/core/helpers.h"
#include "esphome/core/string_ref.h"
#include "list_entities.h"
#include "sse_chunk.h"

#include <atomic>
#include <cstdint>
#include <vector>

namespace esphome::web_server {

class WebServer;
class AsyncEventSource;
class AsyncEventSourceResponse;

using message_generator_t = void(WebServer *, void *, json::JsonBuilder &);

/*
  This class holds a pointer to the source component that wants to publish a state event, and a pointer to a function
  that will lazily generate that event.  The two pointers allow dedup in the deferred queue if multiple publishes for
  the same component are backed up, and take up only two pointers of memory.  The entry in the deferred queue (a
  std::vector) is the DeferredEvent instance itself (not a pointer to one elsewhere in heap) so still only two pointers
  per entry (and no heap fragmentation).  Even 100 backed up events (you'd have to have at least 100 sensors publishing
  because of dedup) would take up only 0.8 kB.
*/
struct DeferredEvent {
  friend class AsyncEventSourceResponse;

 protected:
  void *source_;
  message_generator_t *message_generator_;

 public:
  DeferredEvent(void *source, message_generator_t *message_generator)
      : source_(source), message_generator_(message_generator) {}
  bool operator==(const DeferredEvent &test) const {
    return (source_ == test.source_ && message_generator_ == test.message_generator_);
  }
};
static_assert(sizeof(DeferredEvent) == sizeof(void *) + sizeof(message_generator_t *),
              "DeferredEvent should have no padding");

class AsyncEventSourceResponse : public web_server_base::AsyncStreamResponse {
  friend class AsyncEventSource;

 public:
  bool try_send_nodefer(const char *message, size_t message_len, const char *event = nullptr, uint32_t id = 0,
                        uint32_t reconnect = 0);
  // detail_all events go out while entities are still being listed; state events wait until listing is done
  void deferrable_send_state(void *source, bool detail_all, message_generator_t *message_generator);
  // The old event name argument would otherwise convert to true
  void deferrable_send_state(void *source, const char *event_type, message_generator_t *message_generator) = delete;
  void loop();

 protected:
  AsyncEventSourceResponse(AsyncWebServerRequest *request, AsyncEventSource *server, WebServer *ws);

  // Main-loop only: sends initial ping/config/sorting_groups, starts entity iterator.
  void start_session_main_loop_();

  void deq_push_back_with_dedup_(void *source, message_generator_t *message_generator);
  void process_deferred_queue_();
  // A new chunk may go out: not re-entered from a log line, session alive, tail empty
  bool ready_to_send_();
  // Push what is left of the chunk in tail_ to the socket
  void drain_tail_();
  // Grow tail_ to hold len bytes, kept at its high-water mark. False on OOM.
  bool reserve_tail_(size_t len);
  // Keep the whole chunk in tail_ and continue from sent; false when the tail cannot be allocated.
  bool stash_chunk_(const char *prefix, size_t prefix_len, const char *message, size_t message_len, size_t total,
                    size_t sent);
  // Send a state event; JSON too large for the stack buffer is serialized into tail_ instead
  bool send_json_(void *source, message_generator_t *generator);
  enum class JsonPrepareResult : uint8_t {
    JSON_PREPARE_RESULT_BUFFER,
    JSON_PREPARE_RESULT_TAIL,
    JSON_PREPARE_RESULT_NO_MEMORY,
    JSON_PREPARE_RESULT_TOO_LARGE,
  };
  // Keep the arena out of the caller's frame so it is gone before the state send.
  // size_out: BUFFER = JSON length, NO_MEMORY = requested tail capacity, TOO_LARGE = available JSON room;
  // unused for TAIL.
  JsonPrepareResult __attribute__((noinline)) prepare_json_(void *source, message_generator_t *generator, char *buffer,
                                                            size_t *size_out);
  // Warn once per stall, and close the session once the stall timeout passes with no memory for the tail
  void tail_alloc_failed_(size_t cap);

  // A log line emitted inside a send re-enters try_send_nodefer on this session; refuse it
  struct SendGuard {
    AsyncEventSourceResponse &owner;
    explicit SendGuard(AsyncEventSourceResponse &owner) : owner(owner) { owner.sending_ = true; }
    ~SendGuard() { this->owner.sending_ = false; }
  };
  // Frees the tail and the queue's storage once the session is closing
  void release_buffers_();

  AsyncEventSource *server_;
  std::vector<DeferredEvent> deferred_queue_;
  WebServer *web_server_;
  ListEntitiesIterator entities_iterator_;
  // One chunk the socket did not take whole, allocated on the first stall; the only heap use
  // on the send path
  RAMUniquePtr<uint8_t[]> tail_;
  uint16_t tail_cap_{0};
  uint16_t tail_len_{0};  // Zero means nothing pending
  uint16_t tail_sent_{0};
  bool sending_{false};
  // The no-memory warning went out during the current stall; cleared on send progress
  bool no_memory_logged_{false};
  // The longest multi line log message in the tree (a climate dump_config) has 22 lines; a
  // longer one goes through the tail
  static constexpr size_t MAX_SEND_LINES = 22;
  static constexpr size_t MAX_SEND_IOV = 1 + 2 * MAX_SEND_LINES;
  // Chunk header, retry/id/event lines and the first "data: "
  static constexpr size_t PREFIX_BUF_SIZE = 128;

  // Stack buffer for a state event's JSON; a larger document is serialized into the tail
  static constexpr size_t JSON_BUF_SIZE = 1024;
  // Same ceiling JsonBuilder::serialize() applies (max_heap_size in json_util.cpp); a larger
  // document is dropped before anything is on the wire
  static constexpr size_t JSON_MAX_SIZE = 5120;
  // Most RAM a stalled session keeps: the largest state document plus any accepted framing
  static constexpr size_t TAIL_MAX_SIZE = JSON_MAX_SIZE + PREFIX_BUF_SIZE + SSE_SUFFIX_LEN;
};

class AsyncEventSource : public AsyncWebHandler {
  friend class AsyncEventSourceResponse;

 public:
  AsyncEventSource(StringRef url, WebServer *ws) : url_(url), web_server_(ws) {}
  ~AsyncEventSource() override;

  // NOLINTNEXTLINE(readability-identifier-naming)
  bool canHandle(AsyncWebServerRequest *request) const override {
    if (request->method() != HTTP_GET)
      return false;
    char url_buf[AsyncWebServerRequest::URL_BUF_SIZE];
    return request->url_to(url_buf) == this->url_;
  }
  // NOLINTNEXTLINE(readability-identifier-naming)
  void handleRequest(AsyncWebServerRequest *request) override;
  void try_send_nodefer(const char *message, size_t message_len, const char *event = nullptr, uint32_t id = 0,
                        uint32_t reconnect = 0);
  void deferrable_send_state(void *source, bool detail_all, message_generator_t *message_generator);
  // The old event name argument would otherwise convert to true
  void deferrable_send_state(void *source, const char *event_type, message_generator_t *message_generator) = delete;
  /// Returns true if there are sessions remaining (including pending cleanup).
  bool loop();
  bool empty() { return this->count() == 0; }

  size_t count() const { return this->sessions_.size(); }

 protected:
  // Cold path: move sessions from pending_sessions_ into sessions_ and greet each one.
  void __attribute__((noinline, cold)) adopt_pending_sessions_main_loop_();

  StringRef url_;  // Must outlive this object (string literal)
  // Main-loop only. Vector: SSE sessions are 1-5 connections, linear search beats set.
  std::vector<AsyncEventSourceResponse *> sessions_;
  // Httpd-task intake; guarded by pending_mutex_, gated by has_pending_sessions_.
  std::vector<AsyncEventSourceResponse *> pending_sessions_;
  Mutex pending_mutex_;
  WebServer *web_server_;
  std::atomic<bool> has_pending_sessions_{false};
};

}  // namespace esphome::web_server

#endif  // USE_ESP32 && USE_WEBSERVER
