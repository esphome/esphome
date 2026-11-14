#pragma once

#include "mqtt_backend.h"
#ifdef USE_MQTT
#ifdef USE_ESP32

#include <atomic>
#include <string>
#include <cstring>
#include <mqtt_client.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "esphome/components/network/ip_address.h"
#include "esphome/core/helpers.h"
#include "esphome/core/lock_free_queue.h"
#include "esphome/core/event_pool.h"
#include "mqtt_subscription_fingerprint.h"

namespace esphome::mqtt {

struct Event {
  esp_mqtt_event_id_t event_id{};
  std::vector<char> data;
  int total_data_len{0};
  int current_data_offset{0};
  std::string topic;
  int msg_id{0};
  bool retain{false};
  int qos{0};
  bool dup{false};
  bool session_present{false};
  esp_mqtt_error_codes_t error_handle{};

  // Populate from esp_mqtt_event_t
  // Copies pointer-based data to owned storage for safe cross-thread transfer
  void populate(const esp_mqtt_event_t &event) {
    this->event_id = event.event_id;
    this->data.assign(event.data, event.data + event.data_len);
    this->total_data_len = event.total_data_len;
    this->current_data_offset = event.current_data_offset;
    this->topic.assign(event.topic, event.topic_len);
    this->msg_id = event.msg_id;
    this->retain = event.retain;
    this->qos = event.qos;
    this->dup = event.dup;
    this->session_present = event.session_present;
    this->error_handle = *event.error_handle;
  }

  // Release owned resources for pool reuse (keeps allocated capacity for efficiency)
  void release() {
    this->data.clear();
    this->topic.clear();
  }
};

enum MqttQueueTypeT : uint8_t {
  MQTT_QUEUE_TYPE_NONE = 0,
  MQTT_QUEUE_TYPE_SUBSCRIBE,
  MQTT_QUEUE_TYPE_UNSUBSCRIBE,
  MQTT_QUEUE_TYPE_PUBLISH,
};

struct QueueElement {
  char *topic;
  char *payload;
  uint16_t payload_len;  // MQTT max payload is 64KiB
  uint8_t type : 2;
  uint8_t qos : 2;  // QoS only needs values 0-2
  uint8_t retain : 1;
  uint8_t reserved : 3;  // Reserved for future use

  QueueElement() : topic(nullptr), payload(nullptr), payload_len(0), qos(0), retain(0), reserved(0) {}

  // Helper to set topic/payload (uses RAMAllocator)
  bool set_data(const char *topic_str, const char *payload_data, size_t len) {
    // Check payload size limit (MQTT max is 64KiB)
    if (len > std::numeric_limits<uint16_t>::max()) {
      return false;
    }

    // Use RAMAllocator with default flags (tries external RAM first, falls back to internal)
    RAMAllocator<char> allocator;

    // Allocate and copy topic
    size_t topic_len = strlen(topic_str) + 1;
    topic = allocator.allocate(topic_len);
    if (!topic)
      return false;
    memcpy(topic, topic_str, topic_len);

    if (payload_data && len) {
      payload = allocator.allocate(len);
      if (!payload) {
        allocator.deallocate(topic, topic_len);
        topic = nullptr;
        return false;
      }
      memcpy(payload, payload_data, len);
      payload_len = static_cast<uint16_t>(len);
    } else {
      payload = nullptr;
      payload_len = 0;
    }
    return true;
  }

  // Helper to release (uses RAMAllocator)
  void release() {
    RAMAllocator<char> allocator;
    if (topic) {
      allocator.deallocate(topic, strlen(topic) + 1);
      topic = nullptr;
    }
    if (payload) {
      allocator.deallocate(payload, payload_len);
      payload = nullptr;
    }
    payload_len = 0;
  }
};

class MQTTBackendESP32 final : public MQTTBackend {
 public:
  static constexpr size_t MQTT_BUFFER_SIZE = 4096;
  static constexpr size_t TASK_STACK_SIZE = 3072;
  static constexpr size_t TASK_STACK_SIZE_TLS = 4096;  // Larger stack for TLS operations
  static constexpr ssize_t TASK_PRIORITY = 5;
  static constexpr uint8_t MQTT_QUEUE_LENGTH = 30;        // 30*12 bytes = 360
  static constexpr uint8_t MQTT_EVENT_QUEUE_LENGTH = 32;  // Inbound events from broker
  static constexpr uint32_t EVENT_DRAIN_BUDGET_MS = 10;
  static constexpr uint8_t MQTT_EVENT_POOL_SIZE = MQTT_EVENT_QUEUE_LENGTH - 1;

  void set_keep_alive(uint16_t keep_alive) final { this->keep_alive_ = keep_alive; }
  void set_client_id(const char *client_id) final { this->client_id_ = client_id; }
  void set_clean_session(bool clean_session) final { this->clean_session_ = clean_session; }

  void set_credentials(const char *username, const char *password) final {
    if (username)
      this->username_ = username;
    if (password)
      this->password_ = password;
  }
  void set_will(const char *topic, uint8_t qos, bool retain, const char *payload) final {
    if (topic)
      this->lwt_topic_ = topic;
    this->lwt_qos_ = qos;
    if (payload)
      this->lwt_message_ = payload;
    this->lwt_retain_ = retain;
  }
  void set_server(network::IPAddress ip, uint16_t port) final {
    char ip_buf[network::IP_ADDRESS_BUFFER_SIZE];
    this->host_ = ip.str_to(ip_buf);
    this->port_ = port;
  }
  void set_server(const char *host, uint16_t port) final {
    this->host_ = host;
    this->port_ = port;
  }
  void set_on_connect(std::function<on_connect_callback_t> &&callback) final {
    this->on_connect_.add(std::move(callback));
  }
  void set_on_disconnect(std::function<on_disconnect_callback_t> &&callback) final {
    this->on_disconnect_.add(std::move(callback));
  }
  void set_on_subscribe(std::function<on_subscribe_callback_t> &&callback) final {
    this->on_subscribe_.add(std::move(callback));
  }
  void set_on_unsubscribe(std::function<on_unsubscribe_callback_t> &&callback) final {
    this->on_unsubscribe_.add(std::move(callback));
  }
  void set_on_message(std::function<on_message_callback_t> &&callback) final {
    this->on_message_.add(std::move(callback));
  }
  void set_on_publish(std::function<on_publish_user_callback_t> &&callback) final {
    this->on_publish_.add(std::move(callback));
  }
  bool connected() const final { return this->is_connected_; }

  void connect() final {
    this->requested_link_state_.store(
        LinkState::LINK_STATE_CONNECTED);  // before start/reconnect, so the MQTT task sees it
    if (!is_initialized_) {
      if (initialize_()) {
        esp_mqtt_client_start(handler_.get());
      }
    } else {
      // Only acts in WAIT_RECONNECT; otherwise the state machine retries after DISCONNECTED
      esp_mqtt_client_reconnect(this->handler_.get());
    }
  }
  void disconnect() final {
    this->requested_link_state_.store(LinkState::LINK_STATE_DISCONNECTED);

    // The MQTT task closes the socket 1 to 20 s later; report it down now so it is never reused
    this->is_connected_ = false;

    if (is_initialized_)
      esp_mqtt_client_disconnect(handler_.get());
  }

  bool subscribe(const char *topic, uint8_t qos) final {
    // Stray SUBACKs for resent SUBSCRIBEs of an earlier connection give no credit once they outnumber
    // ours; one counted mid-burst still frees a slot until the burst ends
    const uint16_t counted = this->sent_on_connection_();
    this->subscribes_sent_ += static_cast<uint16_t>(rebase_sent(counted, this->subacks_.received()) - counted);
#if defined(USE_MQTT_IDF_ENQUEUE)
    const bool sent = this->enqueue_(MQTT_QUEUE_TYPE_SUBSCRIBE, topic, qos);
#else
    const bool sent = esp_mqtt_client_subscribe(this->handler_.get(), topic, qos) != -1;
#endif
    if (sent)
      this->subscribes_sent_++;
    return sent;
  }
  bool can_subscribe() const final {
    const uint16_t sent = this->sent_on_connection_();
    // Loaded before the queue size, so retained data queued after a counted SUBACK is seen
    const uint16_t answered = this->subacks_.received();
    // The event being dispatched still holds its pool slot
    const size_t free_slots = MQTT_EVENT_POOL_SIZE - this->mqtt_event_queue_.size() - this->dispatching_;
    return subscribe_fits(
        sent, answered, free_slots,
        this->on_subscribe_.size() != 0 ? INBOUND_SLOTS_PER_SUBSCRIBE_WITH_ACK : INBOUND_SLOTS_PER_SUBSCRIBE);
  }
  bool unsubscribe(const char *topic) final {
#if defined(USE_MQTT_IDF_ENQUEUE)
    return enqueue_(MQTT_QUEUE_TYPE_UNSUBSCRIBE, topic);
#else
    return esp_mqtt_client_unsubscribe(handler_.get(), topic) != -1;
#endif
  }

  bool publish(const char *topic, const char *payload, size_t length, uint8_t qos, bool retain) final {
#if defined(USE_MQTT_IDF_ENQUEUE)
    return enqueue_(MQTT_QUEUE_TYPE_PUBLISH, topic, qos, retain, payload, length);
#else
    // might block for several seconds, either due to network timeout (10s)
    // or if publishing payloads longer than internal buffer (due to message fragmentation)
    return esp_mqtt_client_publish(handler_.get(), topic, payload, length, qos, retain) != -1;
#endif
  }
  using MQTTBackend::publish;
  bool publish_tracked(const MQTTMessage &message) final;
  bool publish_acknowledged() const final { return this->tracked_acked_.load(std::memory_order_acquire) != 0; }
  const SubackCounter &get_subacks() const { return this->subacks_; }

  void loop() final;

#ifdef USE_MQTT_TLS
  // The certificates are string literals from code generation, so only the pointers are kept
  void set_ca_certificate(const char *cert) { this->ca_certificate_ = cert; }
  void set_cl_certificate(const char *cert) { this->cl_certificate_ = cert; }
  void set_cl_key(const char *key) { this->cl_key_ = key; }
  void set_skip_cert_cn_check(bool skip_check) { this->skip_cert_cn_check_ = skip_check; }
#endif
  void set_transport(MQTTTransport transport) { this->transport_ = transport; }
  void set_ws_path(const char *path) { this->ws_path_ = path; }
  MQTTTransport get_transport() const { return this->transport_; }
  const char *get_ws_path() const { return this->ws_path_; }

  // No destructor needed: ESPHome components live for the entire device runtime.
  // The MQTT task and queue will run until the device reboots or loses power,
  // at which point the entire process terminates and FreeRTOS cleans up all tasks.
  // Implementing a destructor would add complexity and potential race conditions
  // for a scenario that never occurs in practice.

 protected:
  bool initialize_();
  void mqtt_event_handler_(const Event &event);
  void handle_disconnected_();
  static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);

  struct MqttClientDeleter {
    void operator()(esp_mqtt_client *client_handler) { esp_mqtt_client_destroy(client_handler); }
  };
  /// SUBSCRIBEs sent on this connection, less those the task could not send
  uint16_t sent_on_connection_() const {
#if defined(USE_MQTT_IDF_ENQUEUE)
    return static_cast<uint16_t>(this->subscribes_sent_ - (this->subscribes_lost_.load(std::memory_order_relaxed) -
                                                           this->subscribes_lost_at_connect_));
#else
    return this->subscribes_sent_;
#endif
  }
  using ClientHandler_ = std::unique_ptr<esp_mqtt_client, MqttClientDeleter>;
  ClientHandler_ handler_;

  bool is_connected_{false};
  bool is_initialized_{false};
#if defined(USE_MQTT_IDF_ENQUEUE)
  // Queued SUBSCRIBEs the task could not send
  std::atomic<uint16_t> subscribes_lost_{0};
#endif

  // uint8_t-sized, not bool: GCC on Xtensa emits an out-of-line call for atomic<bool>
  enum class LinkState : uint8_t { LINK_STATE_DISCONNECTED = 0, LINK_STATE_CONNECTED };

  // Last connect()/disconnect() request; read by the MQTT task and the main loop
  std::atomic<LinkState> requested_link_state_{LinkState::LINK_STATE_DISCONNECTED};

  // Link state seen by the MQTT task, so loop() can catch a dropped DISCONNECTED
  std::atomic<LinkState> task_link_state_{LinkState::LINK_STATE_DISCONNECTED};

  esp_mqtt_client_config_t mqtt_cfg_{};

  std::string host_;
  uint16_t port_;
  /// SUBSCRIBEs sent on this connection
  uint16_t subscribes_sent_{0};
  std::string username_;
  std::string password_;
  std::string lwt_topic_;
  std::string lwt_message_;
  uint8_t lwt_qos_;
  bool lwt_retain_;
#if defined(USE_MQTT_IDF_ENQUEUE)
  // subscribes_lost_ when this connection started
  uint16_t subscribes_lost_at_connect_{0};
#endif
  std::string client_id_;
  uint16_t keep_alive_;
  bool clean_session_;
  bool dispatching_{false};
  // uint8_t because atomic<bool> is out of line on Xtensa
  std::atomic<uint8_t> tracked_acked_{0};
#ifdef USE_MQTT_TLS
  const char *ca_certificate_{nullptr};
  const char *cl_certificate_{nullptr};
  const char *cl_key_{nullptr};
  bool skip_cert_cn_check_{false};
#endif
  MQTTTransport transport_{MQTTTransport::MQTT_TRANSPORT_TCP};
  const char *ws_path_{nullptr};
#if defined(USE_MQTT_IDF_ENQUEUE)
  static void esphome_mqtt_task(void *params);
  // Pool sized to queue capacity (SIZE-1) — see mqtt_event_pool_ comment.
  EventPool<struct QueueElement, MQTT_QUEUE_LENGTH - 1> mqtt_outbound_pool_;
  NotifyingLockFreeQueue<struct QueueElement, MQTT_QUEUE_LENGTH> mqtt_queue_;
  TaskHandle_t task_handle_{nullptr};
  // Only the task writes, so no atomic read-modify-write is needed
  void count_lost_subscribe_() {
    this->subscribes_lost_.store(this->subscribes_lost_.load(std::memory_order_relaxed) + 1, std::memory_order_relaxed);
  }
  bool enqueue_(MqttQueueTypeT type, const char *topic, int qos = 0, bool retain = false, const char *payload = NULL,
                size_t len = 0);
#endif

  // callbacks
  CallbackManager<on_connect_callback_t> on_connect_;
  CallbackManager<on_disconnect_callback_t> on_disconnect_;
  CallbackManager<on_subscribe_callback_t> on_subscribe_;
  CallbackManager<on_unsubscribe_callback_t> on_unsubscribe_;
  CallbackManager<on_message_callback_t> on_message_;
  CallbackManager<on_publish_user_callback_t> on_publish_;
  std::string cached_topic_;
  // Set by the ESP-IDF MQTT task, which runs at a higher priority; publish_tracked() checks the last
  // confirmed id in case the confirmation beat its store. Packet ids are never 0.
  std::atomic<uint16_t> tracked_msg_id_{0};
  std::atomic<uint16_t> acked_msg_id_{0};
  // Counted in the ESP-IDF MQTT task, see mqtt_event_handler()
  SubackCounter subacks_;
  // Pool sized to queue capacity (SIZE-1) because LockFreeQueue<T,N> is a ring
  // buffer that holds N-1 elements (one slot distinguishes full from empty).
  // This guarantees allocate() returns nullptr before push() can fail, which:
  //  1. Prevents leaking a pool slot (the Nth allocate succeeds but push fails)
  //  2. Avoids needing release() on the producer path after a failed push(),
  //     preserving the SPSC contract on the pool's internal free list
  EventPool<Event, MQTT_EVENT_POOL_SIZE> mqtt_event_pool_;
  LockFreeQueue<Event, MQTT_EVENT_QUEUE_LENGTH> mqtt_event_queue_;

#if defined(USE_MQTT_IDF_ENQUEUE)
  uint32_t last_dropped_log_time_{0};
  static constexpr uint32_t DROP_LOG_INTERVAL_MS = 10000;  // Log every 10 seconds
#endif
};

}  // namespace esphome::mqtt

#endif
#endif
