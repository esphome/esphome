#include "api_connection.h"
#ifdef USE_API
#ifdef USE_API_NOISE
#include "api_frame_helper_noise.h"
#endif
#ifdef USE_API_PLAINTEXT
#include "api_frame_helper_plaintext.h"
#endif
#ifdef USE_API_USER_DEFINED_ACTIONS
#include "user_services.h"
#endif
#include <cerrno>
#include <cinttypes>
#include <functional>
#include <limits>
#include <new>
#include <utility>
#ifdef USE_ESP8266
#include <pgmspace.h>
#endif
#include "esphome/components/network/util.h"
#include "esphome/core/application.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/core/version.h"

#ifdef USE_DEEP_SLEEP
#include "esphome/components/deep_sleep/deep_sleep_component.h"
#endif
#ifdef USE_HOMEASSISTANT_TIME
#include "esphome/components/homeassistant/time/homeassistant_time.h"
#endif
#ifdef USE_BLUETOOTH_PROXY
#include "esphome/components/bluetooth_proxy/bluetooth_proxy.h"
#endif
#ifdef USE_CLIMATE
#include "esphome/components/climate/climate_mode.h"
#endif
#ifdef USE_VOICE_ASSISTANT
#include "esphome/components/voice_assistant/voice_assistant.h"
#endif
#ifdef USE_ZWAVE_PROXY
#include "esphome/components/zwave_proxy/zwave_proxy.h"
#endif
#ifdef USE_WATER_HEATER
#include "esphome/components/water_heater/water_heater.h"
#endif
#ifdef USE_INFRARED
#include "esphome/components/infrared/infrared.h"
#endif
#ifdef USE_RADIO_FREQUENCY
#include "esphome/components/radio_frequency/radio_frequency.h"
#endif

namespace esphome::api {

// Maximum messages to read per loop iteration to prevent starving other components.
// This is a balance between API responsiveness and allowing other components to run.
// Since each message could contain multiple protobuf messages when using packet batching,
// this limits the number of messages processed, not the number of TCP packets.
static constexpr uint8_t MAX_MESSAGES_PER_LOOP = 10;
static constexpr uint8_t MAX_PING_RETRIES = 60;
static constexpr uint16_t PING_RETRY_INTERVAL = 1000;
static constexpr uint32_t KEEPALIVE_DISCONNECT_TIMEOUT = (KEEPALIVE_TIMEOUT_MS * 5) / 2;
// Timeout for completing the handshake (Noise transport + HelloRequest).
// A stalled handshake from a buggy client or network glitch holds a connection
// slot, which can prevent legitimate clients from reconnecting. Also hardens
// against the less likely case of intentional connection slot exhaustion.
//
// 60s is intentionally high: on ESP8266 with power_save_mode: LIGHT and weak
// WiFi (-70 dBm+), TCP retransmissions push real-world handshake times to
// 28-30s. See https://github.com/esphome/esphome/issues/14999
static constexpr uint32_t HANDSHAKE_TIMEOUT_MS = 60000;

static constexpr auto ESPHOME_VERSION_REF = StringRef::from_lit(ESPHOME_VERSION);

// Cross-validate C++ constants against proto max_data_length annotations in api.proto
static_assert(MAC_ADDRESS_PRETTY_BUFFER_SIZE - 1 == 17,
              "Update max_data_length for mac_address/bluetooth_mac_address in api.proto");
static_assert(Application::BUILD_TIME_STR_SIZE - 1 == 25, "Update max_data_length for compilation_time in api.proto");
static_assert(sizeof(ESPHOME_VERSION) - 1 <= 32, "Update max_data_length for esphome_version in api.proto");
static_assert(ESPHOME_DEVICE_NAME_MAX_LEN <= 31, "Update max_data_length for name in api.proto");
static_assert(ESPHOME_FRIENDLY_NAME_MAX_LEN <= 120, "Update max_data_length for friendly_name in api.proto");

static const char *const TAG = "api.connection";
#ifdef USE_CAMERA
static const int CAMERA_STOP_STREAM = 5000;
#endif

#ifdef USE_DEVICES
// Helper macro for entity command handlers - gets entity by key and device_id, returns if not found, and creates call
// object
#define ENTITY_COMMAND_MAKE_CALL(entity_type, entity_var, getter_name) \
  entity_type *entity_var = App.get_##getter_name##_by_key(msg.key, msg.device_id); \
  if ((entity_var) == nullptr) \
    return; \
  auto call = (entity_var)->make_call();

// Helper macro for entity command handlers that don't use make_call() - gets entity by key and device_id and returns if
// not found
#define ENTITY_COMMAND_GET(entity_type, entity_var, getter_name) \
  entity_type *entity_var = App.get_##getter_name##_by_key(msg.key, msg.device_id); \
  if ((entity_var) == nullptr) \
    return;

// Helper macro for multi-entity dispatch: looks up an entity by key and device_id without early return or make_call().
// Use when multiple entity types must be checked in sequence (at most one will match).
#define ENTITY_COMMAND_LOOKUP(entity_type, entity_var, getter_name) \
  entity_type *entity_var = App.get_##getter_name##_by_key(msg.key, msg.device_id)

#else  // No device support, use simpler macros
// Helper macro for entity command handlers - gets entity by key, returns if not found, and creates call
// object
#define ENTITY_COMMAND_MAKE_CALL(entity_type, entity_var, getter_name) \
  entity_type *entity_var = App.get_##getter_name##_by_key(msg.key); \
  if ((entity_var) == nullptr) \
    return; \
  auto call = (entity_var)->make_call();

// Helper macro for entity command handlers that don't use make_call() - gets entity by key and returns if
// not found
#define ENTITY_COMMAND_GET(entity_type, entity_var, getter_name) \
  entity_type *entity_var = App.get_##getter_name##_by_key(msg.key); \
  if ((entity_var) == nullptr) \
    return;

// Helper macro for multi-entity dispatch: looks up an entity by key without early return or make_call().
// Use when multiple entity types must be checked in sequence (at most one will match).
#define ENTITY_COMMAND_LOOKUP(entity_type, entity_var, getter_name) \
  entity_type *entity_var = App.get_##getter_name##_by_key(msg.key)

#endif  // USE_DEVICES

APIConnection::APIConnection(std::unique_ptr<socket::Socket> sock, APIServer *parent) : parent_(parent) {
#if defined(USE_API_PLAINTEXT) && defined(USE_API_NOISE)
  auto &noise_ctx = parent->get_noise_ctx();
  if (noise_ctx.has_psk()) {
    this->helper_ = std::unique_ptr<APIFrameHelper>{new APINoiseFrameHelper(std::move(sock), noise_ctx)};
  } else {
    this->helper_ = std::unique_ptr<APIFrameHelper>{new APIPlaintextFrameHelper(std::move(sock))};
  }
#elif defined(USE_API_PLAINTEXT)
  this->helper_ = std::unique_ptr<APIPlaintextFrameHelper>{new APIPlaintextFrameHelper(std::move(sock))};
#elif defined(USE_API_NOISE)
  this->helper_ =
      std::unique_ptr<APINoiseFrameHelper>{new APINoiseFrameHelper(std::move(sock), parent->get_noise_ctx())};
#else
#error "No frame helper defined"
#endif
#ifdef USE_CAMERA
  if (camera::Camera::instance() != nullptr) {
    this->image_reader_ = std::unique_ptr<camera::CameraImageReader>{camera::Camera::instance()->create_image_reader()};
  }
#endif
}

void APIConnection::start() {
  this->last_traffic_ = App.get_loop_component_start_time();

  APIError err = this->helper_->init();
  if (err != APIError::OK) {
    this->fatal_error_with_log_(LOG_STR("Helper init failed"), err);
    return;
  }
  // Initialize client name with peername (IP address) until Hello message provides actual name
  char peername[socket::SOCKADDR_STR_LEN];
  this->helper_->set_client_name(this->helper_->get_peername_to(peername), strlen(peername));
}

APIConnection::~APIConnection() {
  this->destroy_active_iterator_();
#ifdef USE_BLUETOOTH_PROXY
  if (bluetooth_proxy::global_bluetooth_proxy->get_api_connection() == this) {
    bluetooth_proxy::global_bluetooth_proxy->unsubscribe_api_connection(this);
  }
#endif
#ifdef USE_VOICE_ASSISTANT
  if (voice_assistant::global_voice_assistant->get_api_connection() == this) {
    voice_assistant::global_voice_assistant->client_subscription(this, false);
  }
#endif
#ifdef USE_ZWAVE_PROXY
  if (zwave_proxy::global_zwave_proxy != nullptr && zwave_proxy::global_zwave_proxy->get_api_connection() == this) {
    zwave_proxy::global_zwave_proxy->zwave_proxy_request(this, enums::ZWAVE_PROXY_REQUEST_TYPE_UNSUBSCRIBE);
  }
#endif
#ifdef USE_SERIAL_PROXY
  for (auto *proxy : App.get_serial_proxies()) {
    if (proxy->get_api_connection() == this) {
      proxy->serial_proxy_request(this, enums::SERIAL_PROXY_REQUEST_TYPE_UNSUBSCRIBE);
    }
  }
#endif
}

void APIConnection::destroy_active_iterator_() {
  switch (this->active_iterator_) {
    case ActiveIterator::LIST_ENTITIES:
      this->iterator_storage_.list_entities.~ListEntitiesIterator();
      break;
    case ActiveIterator::INITIAL_STATE:
      this->iterator_storage_.initial_state.~InitialStateIterator();
      break;
    case ActiveIterator::NONE:
      break;
  }
  this->active_iterator_ = ActiveIterator::NONE;
}

void APIConnection::begin_iterator_(ActiveIterator type) {
  this->destroy_active_iterator_();
  this->active_iterator_ = type;
  if (type == ActiveIterator::LIST_ENTITIES) {
    new (&this->iterator_storage_.list_entities) ListEntitiesIterator(this);
    this->iterator_storage_.list_entities.begin();
  } else {
    new (&this->iterator_storage_.initial_state) InitialStateIterator(this);
    this->iterator_storage_.initial_state.begin();
  }
}

void APIConnection::loop() {
  if (this->flags_.next_close) {
    // requested a disconnect - don't close socket here, let APIServer::loop() do it
    // so getpeername() still works for the disconnect trigger
    this->flags_.remove = true;
    return;
  }

  APIError err = this->helper_->loop();
  if (err != APIError::OK) {
    this->fatal_error_with_log_(LOG_STR("Socket operation failed"), err);
    return;
  }

  const uint32_t now = App.get_loop_component_start_time();
  // Check if socket has data ready before attempting to read.
  // Also try reading if we hit the message limit last time — LWIP's rcvevent
  // (used by is_socket_ready) tracks pbuf dequeues, not bytes. When multiple
  // messages share a TCP segment, the last message's data stays in LWIP's
  // lastdata cache after rcvevent hits 0, making is_socket_ready() return false
  // even though data remains.
  if (this->helper_->is_socket_ready() || this->flags_.may_have_remaining_data) {
    this->flags_.may_have_remaining_data = false;
    // Read up to MAX_MESSAGES_PER_LOOP messages per loop to improve throughput
    uint8_t message_count = 0;
    for (; message_count < MAX_MESSAGES_PER_LOOP; message_count++) {
      ReadPacketBuffer buffer;
      err = this->helper_->read_packet(&buffer);
      if (err == APIError::WOULD_BLOCK) {
        // No more data available
        break;
      } else if (err != APIError::OK) {
        this->fatal_error_with_log_(LOG_STR("Reading failed"), err);
        return;
      } else {
        // Only update last_traffic_ after authentication to ensure the
        // handshake timeout is an absolute deadline from connection start.
        // Pre-auth messages (e.g. PingRequest) must not reset the timer.
        if (this->is_authenticated()) {
          this->last_traffic_ = now;
        }
        // read a packet
        this->read_message_(buffer.data_len, buffer.type, buffer.data);
        if (this->flags_.remove)
          return;
      }
    }
    // If we hit the limit, there may be more data remaining in LWIP's
    // lastdata cache that rcvevent doesn't account for.
    if (message_count == MAX_MESSAGES_PER_LOOP) {
      this->flags_.may_have_remaining_data = true;
    }
  }

  // Process deferred batch if scheduled and timer has expired
  if (this->flags_.batch_scheduled && now - this->deferred_batch_.batch_start_time >= this->get_batch_delay_ms_()) {
    this->process_batch_();
  }

  if (this->active_iterator_ != ActiveIterator::NONE) {
    this->process_active_iterator_();
  }

  // Disconnect clients that haven't completed the handshake in time.
  // Stale half-open connections from buggy clients or network issues can
  // accumulate and block legitimate clients from reconnecting.
  if (!this->is_authenticated() && now - this->last_traffic_ > HANDSHAKE_TIMEOUT_MS) {
    this->on_fatal_error();
    this->log_client_(ESPHOME_LOG_LEVEL_WARN, LOG_STR("handshake timeout; disconnecting"));
    return;
  }

  // Keepalive: only call into the cold path when enough time has elapsed.
  // When sent_ping is true, last_traffic_ hasn't been updated so this
  // condition is already satisfied — covers both send-ping and disconnect cases.
  if (now - this->last_traffic_ > KEEPALIVE_TIMEOUT_MS) {
    this->check_keepalive_(now);
  }

#ifdef USE_API_HOMEASSISTANT_STATES
  if (state_subs_at_ >= 0) {
    this->process_state_subscriptions_();
  }
#endif

#ifdef USE_CAMERA
  // Process camera last - state updates are higher priority
  // (missing a frame is fine, missing a state update is not)
  this->try_send_camera_image_();
#endif
}

void APIConnection::check_keepalive_(uint32_t now) {
  // Caller guarantees: now - last_traffic_ > KEEPALIVE_TIMEOUT_MS
  if (this->flags_.sent_ping) {
    // Disconnect if not responded within 2.5*keepalive
    if (now - this->last_traffic_ > KEEPALIVE_DISCONNECT_TIMEOUT) {
      on_fatal_error();
      this->log_client_(ESPHOME_LOG_LEVEL_WARN, LOG_STR("is unresponsive; disconnecting"));
    }
  } else if (!this->flags_.remove) {
    // Only send ping if we're not disconnecting
    ESP_LOGVV(TAG, "Sending keepalive PING");
    PingRequest req;
    this->flags_.sent_ping = this->send_message(req);
    if (!this->flags_.sent_ping) {
      // If we can't send the ping request directly (tx_buffer full),
      // schedule it at the front of the batch so it will be sent with priority
      ESP_LOGW(TAG, "Buffer full, ping queued");
      this->schedule_message_front_(nullptr, PingRequest::MESSAGE_TYPE, PingRequest::ESTIMATED_SIZE);
      this->flags_.sent_ping = true;  // Mark as sent to avoid scheduling multiple pings
    }
  }
}

void APIConnection::process_active_iterator_() {
  // Caller ensures active_iterator_ != NONE
  if (this->active_iterator_ == ActiveIterator::LIST_ENTITIES) {
    if (this->iterator_storage_.list_entities.completed()) {
      this->destroy_active_iterator_();
      if (this->flags_.state_subscription) {
        this->begin_iterator_(ActiveIterator::INITIAL_STATE);
      } else {
        this->finalize_iterator_sync_();
      }
    } else {
      this->process_iterator_batch_(this->iterator_storage_.list_entities);
    }
  } else {  // INITIAL_STATE
    if (this->iterator_storage_.initial_state.completed()) {
      this->destroy_active_iterator_();
      this->finalize_iterator_sync_();
    } else {
      this->process_iterator_batch_(this->iterator_storage_.initial_state);
    }
  }
}

void APIConnection::finalize_iterator_sync_() {
  // Flush any remaining batched messages immediately so clients
  // receive completion responses (e.g. ListEntitiesDoneResponse)
  // without waiting for the batch timer.
  if (!this->deferred_batch_.empty()) {
    this->process_batch_();
  }
  // Enable immediate sending for future state changes
  this->flags_.should_try_send_immediately = true;
  // Release excess memory from buffers that grew during initial sync
  this->deferred_batch_.release_buffer();
  this->helper_->release_buffers();
}

void APIConnection::process_iterator_batch_(ComponentIterator &iterator) {
  size_t initial_size = this->deferred_batch_.size();
  size_t max_batch = this->get_max_batch_size_();
  while (!iterator.completed() && (this->deferred_batch_.size() - initial_size) < max_batch) {
    iterator.advance();
  }

  // If the batch is full, process it immediately
  // Note: iterator.advance() already calls schedule_batch_() via schedule_message_()
  if (this->deferred_batch_.size() >= max_batch) {
    this->process_batch_();
  }
}

bool APIConnection::send_disconnect_response_() {
  // remote initiated disconnect_client
  // don't close yet, we still need to send the disconnect response
  // close will happen on next loop
  this->log_client_(ESPHOME_LOG_LEVEL_DEBUG, LOG_STR("disconnected"));
  this->flags_.next_close = true;
  DisconnectResponse resp;
  return this->send_message(resp);
}
void APIConnection::on_disconnect_response() {
  // Don't close socket here, let APIServer::loop() do it
  // so getpeername() still works for the disconnect trigger
  this->flags_.remove = true;
}

uint16_t APIConnection::fill_and_encode_entity_state(EntityBase *entity, StateResponseProtoMessage &msg,
                                                     CalculateSizeFn size_fn, MessageEncodeFn encode_fn,
                                                     APIConnection *conn, uint32_t remaining_size) {
  msg.key = entity->get_object_id_hash();
#ifdef USE_DEVICES
  msg.device_id = entity->get_device_id();
#endif
  return encode_to_buffer(size_fn(&msg), encode_fn, &msg, conn, remaining_size);
}

uint16_t APIConnection::fill_and_encode_entity_info(EntityBase *entity, InfoResponseProtoMessage &msg,
                                                    CalculateSizeFn size_fn, MessageEncodeFn encode_fn,
                                                    APIConnection *conn, uint32_t remaining_size) {
  // Set common fields that are shared by all entity types
  msg.key = entity->get_object_id_hash();

  // API 1.14+ clients compute object_id client-side from the entity name
  // For older clients, we must send object_id for backward compatibility
  // See: https://github.com/esphome/backlog/issues/76
  // TODO: Remove this backward compat code before 2026.7.0 - all clients should support API 1.14 by then
  // Buffer must remain in scope until encode_to_buffer is called
  char object_id_buf[OBJECT_ID_MAX_LEN];
  if (!conn->client_supports_api_version(1, 14)) {
    msg.object_id = entity->get_object_id_to(object_id_buf);
  }

  if (entity->has_own_name()) {
    msg.name = entity->get_name();
  }

  // Set common EntityBase properties
#ifdef USE_ENTITY_ICON
  char icon_buf[MAX_ICON_LENGTH];
  msg.icon = StringRef(entity->get_icon_to(icon_buf));
#endif
  msg.disabled_by_default = entity->is_disabled_by_default();
  msg.entity_category = static_cast<enums::EntityCategory>(entity->get_entity_category());
#ifdef USE_DEVICES
  msg.device_id = entity->get_device_id();
#endif
  return encode_to_buffer_slow(size_fn(&msg), encode_fn, &msg, conn, remaining_size);
}

uint16_t APIConnection::fill_and_encode_entity_info_with_device_class(EntityBase *entity, InfoResponseProtoMessage &msg,
                                                                      StringRef &device_class_field,
                                                                      CalculateSizeFn size_fn,
                                                                      MessageEncodeFn encode_fn, APIConnection *conn,
                                                                      uint32_t remaining_size) {
  char dc_buf[MAX_DEVICE_CLASS_LENGTH];
  device_class_field = StringRef(entity->get_device_class_to(dc_buf));
  return fill_and_encode_entity_info(entity, msg, size_fn, encode_fn, conn, remaining_size);
}

#ifdef USE_BINARY_SENSOR
bool APIConnection::send_binary_sensor_state(binary_sensor::BinarySensor *binary_sensor) {
  return this->send_message_smart_(binary_sensor, BinarySensorStateResponse::MESSAGE_TYPE,
                                   BinarySensorStateResponse::ESTIMATED_SIZE);
}

uint16_t APIConnection::try_send_binary_sensor_state(EntityBase *entity, APIConnection *conn, uint32_t remaining_size) {
  auto *binary_sensor = static_cast<binary_sensor::BinarySensor *>(entity);
  BinarySensorStateResponse resp;
  resp.state = binary_sensor->state;
  resp.missing_state = !binary_sensor->has_state();
  return fill_and_encode_entity_state(binary_sensor, resp, conn, remaining_size);
}

uint16_t APIConnection::try_send_binary_sensor_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size) {
  auto *binary_sensor = static_cast<binary_sensor::BinarySensor *>(entity);
  ListEntitiesBinarySensorResponse msg;
  msg.is_status_binary_sensor = binary_sensor->is_status_binary_sensor();
  return fill_and_encode_entity_info_with_device_class(binary_sensor, msg, msg.device_class, conn, remaining_size);
}
#endif

#ifdef USE_COVER
bool APIConnection::send_cover_state(cover::Cover *cover) {
  return this->send_message_smart_(cover, CoverStateResponse::MESSAGE_TYPE, CoverStateResponse::ESTIMATED_SIZE);
}
uint16_t APIConnection::try_send_cover_state(EntityBase *entity, APIConnection *conn, uint32_t remaining_size) {
  auto *cover = static_cast<cover::Cover *>(entity);
  CoverStateResponse msg;
  auto traits = cover->get_traits();
  msg.position = cover->position;
  if (traits.get_supports_tilt())
    msg.tilt = cover->tilt;
  msg.current_operation = static_cast<enums::CoverOperation>(cover->current_operation);
  return fill_and_encode_entity_state(cover, msg, conn, remaining_size);
}
uint16_t APIConnection::try_send_cover_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size) {
  auto *cover = static_cast<cover::Cover *>(entity);
  ListEntitiesCoverResponse msg;
  auto traits = cover->get_traits();
  msg.assumed_state = traits.get_is_assumed_state();
  msg.supports_position = traits.get_supports_position();
  msg.supports_tilt = traits.get_supports_tilt();
  msg.supports_stop = traits.get_supports_stop();
  return fill_and_encode_entity_info_with_device_class(cover, msg, msg.device_class, conn, remaining_size);
}
void APIConnection::on_cover_command_request(const CoverCommandRequest &msg) {
  ENTITY_COMMAND_MAKE_CALL(cover::Cover, cover, cover)
  if (msg.has_position)
    call.set_position(msg.position);
  if (msg.has_tilt)
    call.set_tilt(msg.tilt);
  if (msg.stop)
    call.set_command_stop();
  call.perform();
}
#endif

#ifdef USE_FAN
bool APIConnection::send_fan_state(fan::Fan *fan) {
  return this->send_message_smart_(fan, FanStateResponse::MESSAGE_TYPE, FanStateResponse::ESTIMATED_SIZE);
}
uint16_t APIConnection::try_send_fan_state(EntityBase *entity, APIConnection *conn, uint32_t remaining_size) {
  auto *fan = static_cast<fan::Fan *>(entity);
  FanStateResponse msg;
  auto traits = fan->get_traits();
  msg.state = fan->state;
  if (traits.supports_oscillation())
    msg.oscillating = fan->oscillating;
  if (traits.supports_speed()) {
    msg.speed_level = fan->speed;
  }
  if (traits.supports_direction())
    msg.direction = static_cast<enums::FanDirection>(fan->direction);
  if (traits.supports_preset_modes() && fan->has_preset_mode())
    msg.preset_mode = fan->get_preset_mode();
  return fill_and_encode_entity_state(fan, msg, conn, remaining_size);
}
uint16_t APIConnection::try_send_fan_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size) {
  auto *fan = static_cast<fan::Fan *>(entity);
  ListEntitiesFanResponse msg;
  auto traits = fan->get_traits();
  msg.supports_oscillation = traits.supports_oscillation();
  msg.supports_speed = traits.supports_speed();
  msg.supports_direction = traits.supports_direction();
  msg.supported_speed_count = traits.supported_speed_count();
  msg.supported_preset_modes = &traits.supported_preset_modes();
  return fill_and_encode_entity_info(fan, msg, conn, remaining_size);
}
void APIConnection::on_fan_command_request(const FanCommandRequest &msg) {
  ENTITY_COMMAND_MAKE_CALL(fan::Fan, fan, fan)
  if (msg.has_state)
    call.set_state(msg.state);
  if (msg.has_oscillating)
    call.set_oscillating(msg.oscillating);
  if (msg.has_speed_level) {
    // Prefer level
    call.set_speed(msg.speed_level);
  }
  if (msg.has_direction)
    call.set_direction(static_cast<fan::FanDirection>(msg.direction));
  if (msg.has_preset_mode)
    call.set_preset_mode(msg.preset_mode.c_str(), msg.preset_mode.size());
  call.perform();
}
#endif

#ifdef USE_LIGHT
bool APIConnection::send_light_state(light::LightState *light) {
  return this->send_message_smart_(light, LightStateResponse::MESSAGE_TYPE, LightStateResponse::ESTIMATED_SIZE);
}
uint16_t APIConnection::try_send_light_state(EntityBase *entity, APIConnection *conn, uint32_t remaining_size) {
  auto *light = static_cast<light::LightState *>(entity);
  LightStateResponse resp;
  auto values = light->remote_values;
  auto color_mode = values.get_color_mode();
  resp.state = values.is_on();
  resp.color_mode = static_cast<enums::ColorMode>(color_mode);
  resp.brightness = values.get_brightness();
  resp.color_brightness = values.get_color_brightness();
  resp.red = values.get_red();
  resp.green = values.get_green();
  resp.blue = values.get_blue();
  resp.white = values.get_white();
  resp.color_temperature = values.get_color_temperature();
  resp.cold_white = values.get_cold_white();
  resp.warm_white = values.get_warm_white();
  if (light->supports_effects()) {
    resp.effect = light->get_effect_name();
  }
  return fill_and_encode_entity_state(light, resp, conn, remaining_size);
}
uint16_t APIConnection::try_send_light_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size) {
  auto *light = static_cast<light::LightState *>(entity);
  ListEntitiesLightResponse msg;
  auto traits = light->get_traits();
  auto supported_modes = traits.get_supported_color_modes();
  // Pass pointer to ColorModeMask so the iterator can encode actual ColorMode enum values
  msg.supported_color_modes = &supported_modes;
  if (traits.supports_color_capability(light::ColorCapability::COLOR_TEMPERATURE) ||
      traits.supports_color_capability(light::ColorCapability::COLD_WARM_WHITE)) {
    msg.min_mireds = traits.get_min_mireds();
    msg.max_mireds = traits.get_max_mireds();
  }
  FixedVector<const char *> effects_list;
  if (light->supports_effects()) {
    auto &light_effects = light->get_effects();
    effects_list.init(light_effects.size() + 1);
    effects_list.push_back("None");
    for (auto *effect : light_effects) {
      // c_str() is safe as effect names are null-terminated strings from codegen
      effects_list.push_back(effect->get_name().c_str());
    }
  }
  msg.effects = &effects_list;
  return fill_and_encode_entity_info(light, msg, conn, remaining_size);
}
void APIConnection::on_light_command_request(const LightCommandRequest &msg) {
  ENTITY_COMMAND_MAKE_CALL(light::LightState, light, light)
  if (msg.has_state)
    call.set_state(msg.state);
  if (msg.has_brightness)
    call.set_brightness(msg.brightness);
  if (msg.has_color_mode)
    call.set_color_mode(static_cast<light::ColorMode>(msg.color_mode));
  if (msg.has_color_brightness)
    call.set_color_brightness(msg.color_brightness);
  if (msg.has_rgb) {
    call.set_red(msg.red);
    call.set_green(msg.green);
    call.set_blue(msg.blue);
  }
  if (msg.has_white)
    call.set_white(msg.white);
  if (msg.has_color_temperature)
    call.set_color_temperature(msg.color_temperature);
  if (msg.has_cold_white)
    call.set_cold_white(msg.cold_white);
  if (msg.has_warm_white)
    call.set_warm_white(msg.warm_white);
  if (msg.has_transition_length)
    call.set_transition_length(msg.transition_length);
  if (msg.has_flash_length)
    call.set_flash_length(msg.flash_length);
  if (msg.has_effect)
    call.set_effect(msg.effect.c_str(), msg.effect.size());
  call.perform();
}
#endif

#ifdef USE_SENSOR
bool APIConnection::send_sensor_state(sensor::Sensor *sensor) {
  return this->send_message_smart_(sensor, SensorStateResponse::MESSAGE_TYPE, SensorStateResponse::ESTIMATED_SIZE);
}

uint16_t APIConnection::try_send_sensor_state(EntityBase *entity, APIConnection *conn, uint32_t remaining_size) {
  auto *sensor = static_cast<sensor::Sensor *>(entity);
  SensorStateResponse resp;
  resp.state = sensor->state;
  resp.missing_state = !sensor->has_state();
  return fill_and_encode_entity_state(sensor, resp, conn, remaining_size);
}

uint16_t APIConnection::try_send_sensor_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size) {
  auto *sensor = static_cast<sensor::Sensor *>(entity);
  ListEntitiesSensorResponse msg;
  msg.unit_of_measurement = sensor->get_unit_of_measurement_ref();
  msg.accuracy_decimals = sensor->get_accuracy_decimals();
  msg.force_update = sensor->get_force_update();
  msg.state_class = static_cast<enums::SensorStateClass>(sensor->get_state_class());
  return fill_and_encode_entity_info_with_device_class(sensor, msg, msg.device_class, conn, remaining_size);
}
#endif

#ifdef USE_SWITCH
bool APIConnection::send_switch_state(switch_::Switch *a_switch) {
  return this->send_message_smart_(a_switch, SwitchStateResponse::MESSAGE_TYPE, SwitchStateResponse::ESTIMATED_SIZE);
}

uint16_t APIConnection::try_send_switch_state(EntityBase *entity, APIConnection *conn, uint32_t remaining_size) {
  auto *a_switch = static_cast<switch_::Switch *>(entity);
  SwitchStateResponse resp;
  resp.state = a_switch->state;
  return fill_and_encode_entity_state(a_switch, resp, conn, remaining_size);
}

uint16_t APIConnection::try_send_switch_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size) {
  auto *a_switch = static_cast<switch_::Switch *>(entity);
  ListEntitiesSwitchResponse msg;
  msg.assumed_state = a_switch->assumed_state();
  return fill_and_encode_entity_info_with_device_class(a_switch, msg, msg.device_class, conn, remaining_size);
}
void APIConnection::on_switch_command_request(const SwitchCommandRequest &msg) {
  ENTITY_COMMAND_GET(switch_::Switch, a_switch, switch)

  if (msg.state) {
    a_switch->turn_on();
  } else {
    a_switch->turn_off();
  }
}
#endif

#ifdef USE_TEXT_SENSOR
bool APIConnection::send_text_sensor_state(text_sensor::TextSensor *text_sensor) {
  return this->send_message_smart_(text_sensor, TextSensorStateResponse::MESSAGE_TYPE,
                                   TextSensorStateResponse::ESTIMATED_SIZE);
}

uint16_t APIConnection::try_send_text_sensor_state(EntityBase *entity, APIConnection *conn, uint32_t remaining_size) {
  auto *text_sensor = static_cast<text_sensor::TextSensor *>(entity);
  TextSensorStateResponse resp;
  resp.state = StringRef(text_sensor->state);
  resp.missing_state = !text_sensor->has_state();
  return fill_and_encode_entity_state(text_sensor, resp, conn, remaining_size);
}
uint16_t APIConnection::try_send_text_sensor_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size) {
  auto *text_sensor = static_cast<text_sensor::TextSensor *>(entity);
  ListEntitiesTextSensorResponse msg;
  return fill_and_encode_entity_info_with_device_class(text_sensor, msg, msg.device_class, conn, remaining_size);
}
#endif

#ifdef USE_CLIMATE
bool APIConnection::send_climate_state(climate::Climate *climate) {
  return this->send_message_smart_(climate, ClimateStateResponse::MESSAGE_TYPE, ClimateStateResponse::ESTIMATED_SIZE);
}
uint16_t APIConnection::try_send_climate_state(EntityBase *entity, APIConnection *conn, uint32_t remaining_size) {
  auto *climate = static_cast<climate::Climate *>(entity);
  ClimateStateResponse resp;
  auto traits = climate->get_traits();
  resp.mode = static_cast<enums::ClimateMode>(climate->mode);
  resp.action = static_cast<enums::ClimateAction>(climate->action);
  if (traits.has_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE))
    resp.current_temperature = climate->current_temperature;
  if (traits.has_feature_flags(climate::CLIMATE_SUPPORTS_TWO_POINT_TARGET_TEMPERATURE |
                               climate::CLIMATE_REQUIRES_TWO_POINT_TARGET_TEMPERATURE)) {
    resp.target_temperature_low = climate->target_temperature_low;
    resp.target_temperature_high = climate->target_temperature_high;
  } else {
    resp.target_temperature = climate->target_temperature;
  }
  if (traits.get_supports_fan_modes() && climate->fan_mode.has_value())
    resp.fan_mode = static_cast<enums::ClimateFanMode>(climate->fan_mode.value());
  if (!traits.get_supported_custom_fan_modes().empty() && climate->has_custom_fan_mode()) {
    resp.custom_fan_mode = climate->get_custom_fan_mode();
  }
  if (traits.get_supports_presets() && climate->preset.has_value()) {
    resp.preset = static_cast<enums::ClimatePreset>(climate->preset.value());
  }
  if (!traits.get_supported_custom_presets().empty() && climate->has_custom_preset()) {
    resp.custom_preset = climate->get_custom_preset();
  }
  if (traits.get_supports_swing_modes())
    resp.swing_mode = static_cast<enums::ClimateSwingMode>(climate->swing_mode);
  if (traits.has_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_HUMIDITY))
    resp.current_humidity = climate->current_humidity;
  if (traits.has_feature_flags(climate::CLIMATE_SUPPORTS_TARGET_HUMIDITY))
    resp.target_humidity = climate->target_humidity;
  return fill_and_encode_entity_state(climate, resp, conn, remaining_size);
}
uint16_t APIConnection::try_send_climate_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size) {
  auto *climate = static_cast<climate::Climate *>(entity);
  ListEntitiesClimateResponse msg;
  auto traits = climate->get_traits();
  // Flags set for backward compatibility, deprecated in 2025.11.0
  msg.supports_current_temperature = traits.has_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE);
  msg.supports_current_humidity = traits.has_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_HUMIDITY);
  msg.supports_two_point_target_temperature = traits.has_feature_flags(
      climate::CLIMATE_SUPPORTS_TWO_POINT_TARGET_TEMPERATURE | climate::CLIMATE_REQUIRES_TWO_POINT_TARGET_TEMPERATURE);
  msg.supports_target_humidity = traits.has_feature_flags(climate::CLIMATE_SUPPORTS_TARGET_HUMIDITY);
  msg.supports_action = traits.has_feature_flags(climate::CLIMATE_SUPPORTS_ACTION);
  // Current feature flags and other supported parameters
  msg.feature_flags = traits.get_feature_flags();
  msg.supported_modes = &traits.get_supported_modes();
  msg.visual_min_temperature = traits.get_visual_min_temperature();
  msg.visual_max_temperature = traits.get_visual_max_temperature();
  msg.visual_target_temperature_step = traits.get_visual_target_temperature_step();
  msg.visual_current_temperature_step = traits.get_visual_current_temperature_step();
  msg.visual_min_humidity = traits.get_visual_min_humidity();
  msg.visual_max_humidity = traits.get_visual_max_humidity();
  msg.supported_fan_modes = &traits.get_supported_fan_modes();
  msg.supported_custom_fan_modes = &traits.get_supported_custom_fan_modes();
  msg.supported_presets = &traits.get_supported_presets();
  msg.supported_custom_presets = &traits.get_supported_custom_presets();
  msg.supported_swing_modes = &traits.get_supported_swing_modes();
  return fill_and_encode_entity_info(climate, msg, conn, remaining_size);
}
void APIConnection::on_climate_command_request(const ClimateCommandRequest &msg) {
  ENTITY_COMMAND_MAKE_CALL(climate::Climate, climate, climate)
  if (msg.has_mode)
    call.set_mode(static_cast<climate::ClimateMode>(msg.mode));
  if (msg.has_target_temperature)
    call.set_target_temperature(msg.target_temperature);
  if (msg.has_target_temperature_low)
    call.set_target_temperature_low(msg.target_temperature_low);
  if (msg.has_target_temperature_high)
    call.set_target_temperature_high(msg.target_temperature_high);
  if (msg.has_target_humidity)
    call.set_target_humidity(msg.target_humidity);
  if (msg.has_fan_mode)
    call.set_fan_mode(static_cast<climate::ClimateFanMode>(msg.fan_mode));
  if (msg.has_custom_fan_mode)
    call.set_fan_mode(msg.custom_fan_mode.c_str(), msg.custom_fan_mode.size());
  if (msg.has_preset)
    call.set_preset(static_cast<climate::ClimatePreset>(msg.preset));
  if (msg.has_custom_preset)
    call.set_preset(msg.custom_preset.c_str(), msg.custom_preset.size());
  if (msg.has_swing_mode)
    call.set_swing_mode(static_cast<climate::ClimateSwingMode>(msg.swing_mode));
  call.perform();
}
#endif

#ifdef USE_NUMBER
bool APIConnection::send_number_state(number::Number *number) {
  return this->send_message_smart_(number, NumberStateResponse::MESSAGE_TYPE, NumberStateResponse::ESTIMATED_SIZE);
}

uint16_t APIConnection::try_send_number_state(EntityBase *entity, APIConnection *conn, uint32_t remaining_size) {
  auto *number = static_cast<number::Number *>(entity);
  NumberStateResponse resp;
  resp.state = number->state;
  resp.missing_state = !number->has_state();
  return fill_and_encode_entity_state(number, resp, conn, remaining_size);
}

uint16_t APIConnection::try_send_number_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size) {
  auto *number = static_cast<number::Number *>(entity);
  ListEntitiesNumberResponse msg;
  msg.unit_of_measurement = number->get_unit_of_measurement_ref();
  msg.mode = static_cast<enums::NumberMode>(number->traits.get_mode());
  msg.min_value = number->traits.get_min_value();
  msg.max_value = number->traits.get_max_value();
  msg.step = number->traits.get_step();
  return fill_and_encode_entity_info_with_device_class(number, msg, msg.device_class, conn, remaining_size);
}
void APIConnection::on_number_command_request(const NumberCommandRequest &msg) {
  ENTITY_COMMAND_MAKE_CALL(number::Number, number, number)
  call.set_value(msg.state);
  call.perform();
}
#endif

#ifdef USE_DATETIME_DATE
bool APIConnection::send_date_state(datetime::DateEntity *date) {
  return this->send_message_smart_(date, DateStateResponse::MESSAGE_TYPE, DateStateResponse::ESTIMATED_SIZE);
}
uint16_t APIConnection::try_send_date_state(EntityBase *entity, APIConnection *conn, uint32_t remaining_size) {
  auto *date = static_cast<datetime::DateEntity *>(entity);
  DateStateResponse resp;
  resp.missing_state = !date->has_state();
  resp.year = date->year;
  resp.month = date->month;
  resp.day = date->day;
  return fill_and_encode_entity_state(date, resp, conn, remaining_size);
}
uint16_t APIConnection::try_send_date_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size) {
  auto *date = static_cast<datetime::DateEntity *>(entity);
  ListEntitiesDateResponse msg;
  return fill_and_encode_entity_info(date, msg, conn, remaining_size);
}
void APIConnection::on_date_command_request(const DateCommandRequest &msg) {
  ENTITY_COMMAND_MAKE_CALL(datetime::DateEntity, date, date)
  call.set_date(msg.year, msg.month, msg.day);
  call.perform();
}
#endif

#ifdef USE_DATETIME_TIME
bool APIConnection::send_time_state(datetime::TimeEntity *time) {
  return this->send_message_smart_(time, TimeStateResponse::MESSAGE_TYPE, TimeStateResponse::ESTIMATED_SIZE);
}
uint16_t APIConnection::try_send_time_state(EntityBase *entity, APIConnection *conn, uint32_t remaining_size) {
  auto *time = static_cast<datetime::TimeEntity *>(entity);
  TimeStateResponse resp;
  resp.missing_state = !time->has_state();
  resp.hour = time->hour;
  resp.minute = time->minute;
  resp.second = time->second;
  return fill_and_encode_entity_state(time, resp, conn, remaining_size);
}
uint16_t APIConnection::try_send_time_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size) {
  auto *time = static_cast<datetime::TimeEntity *>(entity);
  ListEntitiesTimeResponse msg;
  return fill_and_encode_entity_info(time, msg, conn, remaining_size);
}
void APIConnection::on_time_command_request(const TimeCommandRequest &msg) {
  ENTITY_COMMAND_MAKE_CALL(datetime::TimeEntity, time, time)
  call.set_time(msg.hour, msg.minute, msg.second);
  call.perform();
}
#endif

#ifdef USE_DATETIME_DATETIME
bool APIConnection::send_datetime_state(datetime::DateTimeEntity *datetime) {
  return this->send_message_smart_(datetime, DateTimeStateResponse::MESSAGE_TYPE,
                                   DateTimeStateResponse::ESTIMATED_SIZE);
}
uint16_t APIConnection::try_send_datetime_state(EntityBase *entity, APIConnection *conn, uint32_t remaining_size) {
  auto *datetime = static_cast<datetime::DateTimeEntity *>(entity);
  DateTimeStateResponse resp;
  resp.missing_state = !datetime->has_state();
  if (datetime->has_state()) {
    ESPTime state = datetime->state_as_esptime();
    resp.epoch_seconds = state.timestamp;
  }
  return fill_and_encode_entity_state(datetime, resp, conn, remaining_size);
}
uint16_t APIConnection::try_send_datetime_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size) {
  auto *datetime = static_cast<datetime::DateTimeEntity *>(entity);
  ListEntitiesDateTimeResponse msg;
  return fill_and_encode_entity_info(datetime, msg, conn, remaining_size);
}
void APIConnection::on_date_time_command_request(const DateTimeCommandRequest &msg) {
  ENTITY_COMMAND_MAKE_CALL(datetime::DateTimeEntity, datetime, datetime)
  call.set_datetime(msg.epoch_seconds);
  call.perform();
}
#endif

#ifdef USE_TEXT
bool APIConnection::send_text_state(text::Text *text) {
  return this->send_message_smart_(text, TextStateResponse::MESSAGE_TYPE, TextStateResponse::ESTIMATED_SIZE);
}

uint16_t APIConnection::try_send_text_state(EntityBase *entity, APIConnection *conn, uint32_t remaining_size) {
  auto *text = static_cast<text::Text *>(entity);
  TextStateResponse resp;
  resp.state = StringRef(text->state);
  resp.missing_state = !text->has_state();
  return fill_and_encode_entity_state(text, resp, conn, remaining_size);
}

uint16_t APIConnection::try_send_text_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size) {
  auto *text = static_cast<text::Text *>(entity);
  ListEntitiesTextResponse msg;
  msg.mode = static_cast<enums::TextMode>(text->traits.get_mode());
  msg.min_length = text->traits.get_min_length();
  msg.max_length = text->traits.get_max_length();
  msg.pattern = text->traits.get_pattern_ref();
  return fill_and_encode_entity_info(text, msg, conn, remaining_size);
}
void APIConnection::on_text_command_request(const TextCommandRequest &msg) {
  ENTITY_COMMAND_MAKE_CALL(text::Text, text, text)
  call.set_value(msg.state.c_str(), msg.state.size());
  call.perform();
}
#endif

#ifdef USE_SELECT
bool APIConnection::send_select_state(select::Select *select) {
  return this->send_message_smart_(select, SelectStateResponse::MESSAGE_TYPE, SelectStateResponse::ESTIMATED_SIZE);
}

uint16_t APIConnection::try_send_select_state(EntityBase *entity, APIConnection *conn, uint32_t remaining_size) {
  auto *select = static_cast<select::Select *>(entity);
  SelectStateResponse resp;
  resp.state = select->current_option();
  resp.missing_state = !select->has_state();
  return fill_and_encode_entity_state(select, resp, conn, remaining_size);
}

uint16_t APIConnection::try_send_select_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size) {
  auto *select = static_cast<select::Select *>(entity);
  ListEntitiesSelectResponse msg;
  msg.options = &select->traits.get_options();
  return fill_and_encode_entity_info(select, msg, conn, remaining_size);
}
void APIConnection::on_select_command_request(const SelectCommandRequest &msg) {
  ENTITY_COMMAND_MAKE_CALL(select::Select, select, select)
  call.set_option(msg.state.c_str(), msg.state.size());
  call.perform();
}
#endif

#ifdef USE_BUTTON
uint16_t APIConnection::try_send_button_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size) {
  auto *button = static_cast<button::Button *>(entity);
  ListEntitiesButtonResponse msg;
  return fill_and_encode_entity_info_with_device_class(button, msg, msg.device_class, conn, remaining_size);
}
void esphome::api::APIConnection::on_button_command_request(const ButtonCommandRequest &msg) {
  ENTITY_COMMAND_GET(button::Button, button, button)
  button->press();
}
#endif

#ifdef USE_LOCK
bool APIConnection::send_lock_state(lock::Lock *a_lock) {
  return this->send_message_smart_(a_lock, LockStateResponse::MESSAGE_TYPE, LockStateResponse::ESTIMATED_SIZE);
}

uint16_t APIConnection::try_send_lock_state(EntityBase *entity, APIConnection *conn, uint32_t remaining_size) {
  auto *a_lock = static_cast<lock::Lock *>(entity);
  LockStateResponse resp;
  resp.state = static_cast<enums::LockState>(a_lock->state);
  return fill_and_encode_entity_state(a_lock, resp, conn, remaining_size);
}

uint16_t APIConnection::try_send_lock_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size) {
  auto *a_lock = static_cast<lock::Lock *>(entity);
  ListEntitiesLockResponse msg;
  msg.assumed_state = a_lock->traits.get_assumed_state();
  msg.supports_open = a_lock->traits.get_supports_open();
  msg.requires_code = a_lock->traits.get_requires_code();
  return fill_and_encode_entity_info(a_lock, msg, conn, remaining_size);
}
void APIConnection::on_lock_command_request(const LockCommandRequest &msg) {
  ENTITY_COMMAND_GET(lock::Lock, a_lock, lock)

  switch (msg.command) {
    case enums::LOCK_UNLOCK:
      a_lock->unlock();
      break;
    case enums::LOCK_LOCK:
      a_lock->lock();
      break;
    case enums::LOCK_OPEN:
      a_lock->open();
      break;
  }
}
#endif

#ifdef USE_VALVE
bool APIConnection::send_valve_state(valve::Valve *valve) {
  return this->send_message_smart_(valve, ValveStateResponse::MESSAGE_TYPE, ValveStateResponse::ESTIMATED_SIZE);
}
uint16_t APIConnection::try_send_valve_state(EntityBase *entity, APIConnection *conn, uint32_t remaining_size) {
  auto *valve = static_cast<valve::Valve *>(entity);
  ValveStateResponse resp;
  resp.position = valve->position;
  resp.current_operation = static_cast<enums::ValveOperation>(valve->current_operation);
  return fill_and_encode_entity_state(valve, resp, conn, remaining_size);
}
uint16_t APIConnection::try_send_valve_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size) {
  auto *valve = static_cast<valve::Valve *>(entity);
  ListEntitiesValveResponse msg;
  auto traits = valve->get_traits();
  msg.assumed_state = traits.get_is_assumed_state();
  msg.supports_position = traits.get_supports_position();
  msg.supports_stop = traits.get_supports_stop();
  return fill_and_encode_entity_info_with_device_class(valve, msg, msg.device_class, conn, remaining_size);
}
void APIConnection::on_valve_command_request(const ValveCommandRequest &msg) {
  ENTITY_COMMAND_MAKE_CALL(valve::Valve, valve, valve)
  if (msg.has_position)
    call.set_position(msg.position);
  if (msg.stop)
    call.set_command_stop();
  call.perform();
}
#endif

#ifdef USE_MEDIA_PLAYER
bool APIConnection::send_media_player_state(media_player::MediaPlayer *media_player) {
  return this->send_message_smart_(media_player, MediaPlayerStateResponse::MESSAGE_TYPE,
                                   MediaPlayerStateResponse::ESTIMATED_SIZE);
}
uint16_t APIConnection::try_send_media_player_state(EntityBase *entity, APIConnection *conn, uint32_t remaining_size) {
  auto *media_player = static_cast<media_player::MediaPlayer *>(entity);
  MediaPlayerStateResponse resp;
  media_player::MediaPlayerState report_state = media_player->state == media_player::MEDIA_PLAYER_STATE_ANNOUNCING
                                                    ? media_player::MEDIA_PLAYER_STATE_PLAYING
                                                    : media_player->state;
  resp.state = static_cast<enums::MediaPlayerState>(report_state);
  resp.volume = media_player->volume;
  resp.muted = media_player->is_muted();
  return fill_and_encode_entity_state(media_player, resp, conn, remaining_size);
}
uint16_t APIConnection::try_send_media_player_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size) {
  auto *media_player = static_cast<media_player::MediaPlayer *>(entity);
  ListEntitiesMediaPlayerResponse msg;
  auto traits = media_player->get_traits();
  msg.supports_pause = traits.get_supports_pause();
  msg.feature_flags = traits.get_feature_flags();
  for (auto &supported_format : traits.get_supported_formats()) {
    msg.supported_formats.emplace_back();
    auto &media_format = msg.supported_formats.back();
    media_format.format = StringRef(supported_format.format);
    media_format.sample_rate = supported_format.sample_rate;
    media_format.num_channels = supported_format.num_channels;
    media_format.purpose = static_cast<enums::MediaPlayerFormatPurpose>(supported_format.purpose);
    media_format.sample_bytes = supported_format.sample_bytes;
  }
  return fill_and_encode_entity_info(media_player, msg, conn, remaining_size);
}
void APIConnection::on_media_player_command_request(const MediaPlayerCommandRequest &msg) {
  ENTITY_COMMAND_MAKE_CALL(media_player::MediaPlayer, media_player, media_player)
  if (msg.has_command) {
    call.set_command(static_cast<media_player::MediaPlayerCommand>(msg.command));
  }
  if (msg.has_volume) {
    call.set_volume(msg.volume);
  }
  if (msg.has_media_url) {
    call.set_media_url(msg.media_url);
  }
  if (msg.has_announcement) {
    call.set_announcement(msg.announcement);
  }
  call.perform();
}
#endif

#ifdef USE_CAMERA
void APIConnection::try_send_camera_image_() {
  if (!this->image_reader_)
    return;

  // Send as many chunks as possible without blocking
  while (this->image_reader_->available()) {
    if (!this->helper_->can_write_without_blocking())
      return;

    uint32_t to_send = std::min((size_t) MAX_BATCH_PACKET_SIZE, this->image_reader_->available());
    bool done = this->image_reader_->available() == to_send;

    CameraImageResponse msg;
    msg.key = camera::Camera::instance()->get_object_id_hash();
    msg.set_data(this->image_reader_->peek_data_buffer(), to_send);
    msg.done = done;
#ifdef USE_DEVICES
    msg.device_id = camera::Camera::instance()->get_device_id();
#endif

    if (!this->send_message(msg)) {
      return;  // Send failed, try again later
    }
    this->image_reader_->consume_data(to_send);
    if (done) {
      this->image_reader_->return_image();
      return;
    }
  }
}
void APIConnection::set_camera_state(std::shared_ptr<camera::CameraImage> image) {
  if (!this->flags_.state_subscription)
    return;
  if (!this->image_reader_)
    return;
  if (this->image_reader_->available())
    return;
  if (image->was_requested_by(esphome::camera::API_REQUESTER) || image->was_requested_by(esphome::camera::IDLE)) {
    this->image_reader_->set_image(std::move(image));
    // Try to send immediately to reduce latency
    this->try_send_camera_image_();
  }
}
uint16_t APIConnection::try_send_camera_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size) {
  auto *camera = static_cast<camera::Camera *>(entity);
  ListEntitiesCameraResponse msg;
  return fill_and_encode_entity_info(camera, msg, conn, remaining_size);
}
void APIConnection::on_camera_image_request(const CameraImageRequest &msg) {
  if (camera::Camera::instance() == nullptr)
    return;

  if (msg.single)
    camera::Camera::instance()->request_image(esphome::camera::API_REQUESTER);
  if (msg.stream) {
    camera::Camera::instance()->start_stream(esphome::camera::API_REQUESTER);

    App.scheduler.set_timeout(this->parent_, "api_camera_stop_stream", CAMERA_STOP_STREAM,
                              []() { camera::Camera::instance()->stop_stream(esphome::camera::API_REQUESTER); });
  }
}
#endif

#ifdef USE_HOMEASSISTANT_TIME
void APIConnection::on_get_time_response(const GetTimeResponse &value) {
  if (homeassistant::global_homeassistant_time != nullptr) {
    homeassistant::global_homeassistant_time->set_epoch_time(value.epoch_seconds);
#ifdef USE_TIME_TIMEZONE
    if (!value.timezone.empty()) {
      // Check if the sender provided pre-parsed timezone data.
      // If std_offset is non-zero or DST rules are present, the parsed data was populated.
      // For UTC (all zeros), string parsing produces the same result, so the fallback is equivalent.
      const auto &pt = value.parsed_timezone;
      if (pt.std_offset_seconds != 0 || pt.dst_start.type != enums::DST_RULE_TYPE_NONE) {
        time::ParsedTimezone tz{};
        tz.std_offset_seconds = pt.std_offset_seconds;
        tz.dst_offset_seconds = pt.dst_offset_seconds;
        tz.dst_start.time_seconds = pt.dst_start.time_seconds;
        tz.dst_start.day = static_cast<uint16_t>(pt.dst_start.day);
        tz.dst_start.type = static_cast<time::DSTRuleType>(pt.dst_start.type);
        tz.dst_start.month = static_cast<uint8_t>(pt.dst_start.month);
        tz.dst_start.week = static_cast<uint8_t>(pt.dst_start.week);
        tz.dst_start.day_of_week = static_cast<uint8_t>(pt.dst_start.day_of_week);
        tz.dst_end.time_seconds = pt.dst_end.time_seconds;
        tz.dst_end.day = static_cast<uint16_t>(pt.dst_end.day);
        tz.dst_end.type = static_cast<time::DSTRuleType>(pt.dst_end.type);
        tz.dst_end.month = static_cast<uint8_t>(pt.dst_end.month);
        tz.dst_end.week = static_cast<uint8_t>(pt.dst_end.week);
        tz.dst_end.day_of_week = static_cast<uint8_t>(pt.dst_end.day_of_week);
        time::set_global_tz(tz);
      } else {
        homeassistant::global_homeassistant_time->set_timezone(value.timezone.c_str(), value.timezone.size());
      }
    }
#endif
  }
}
#endif

#ifdef USE_BLUETOOTH_PROXY
void APIConnection::on_subscribe_bluetooth_le_advertisements_request(
    const SubscribeBluetoothLEAdvertisementsRequest &msg) {
  bluetooth_proxy::global_bluetooth_proxy->subscribe_api_connection(this, msg.flags);
}
void APIConnection::on_unsubscribe_bluetooth_le_advertisements_request() {
  bluetooth_proxy::global_bluetooth_proxy->unsubscribe_api_connection(this);
}
void APIConnection::on_bluetooth_device_request(const BluetoothDeviceRequest &msg) {
  bluetooth_proxy::global_bluetooth_proxy->bluetooth_device_request(msg);
}
void APIConnection::on_bluetooth_gatt_read_request(const BluetoothGATTReadRequest &msg) {
  bluetooth_proxy::global_bluetooth_proxy->bluetooth_gatt_read(msg);
}
void APIConnection::on_bluetooth_gatt_write_request(const BluetoothGATTWriteRequest &msg) {
  bluetooth_proxy::global_bluetooth_proxy->bluetooth_gatt_write(msg);
}
void APIConnection::on_bluetooth_gatt_read_descriptor_request(const BluetoothGATTReadDescriptorRequest &msg) {
  bluetooth_proxy::global_bluetooth_proxy->bluetooth_gatt_read_descriptor(msg);
}
void APIConnection::on_bluetooth_gatt_write_descriptor_request(const BluetoothGATTWriteDescriptorRequest &msg) {
  bluetooth_proxy::global_bluetooth_proxy->bluetooth_gatt_write_descriptor(msg);
}
void APIConnection::on_bluetooth_gatt_get_services_request(const BluetoothGATTGetServicesRequest &msg) {
  bluetooth_proxy::global_bluetooth_proxy->bluetooth_gatt_send_services(msg);
}

void APIConnection::on_bluetooth_gatt_notify_request(const BluetoothGATTNotifyRequest &msg) {
  bluetooth_proxy::global_bluetooth_proxy->bluetooth_gatt_notify(msg);
}

bool APIConnection::send_subscribe_bluetooth_connections_free_response_() {
  bluetooth_proxy::global_bluetooth_proxy->send_connections_free(this);
  return true;
}
void APIConnection::on_subscribe_bluetooth_connections_free_request() {
  if (!this->send_subscribe_bluetooth_connections_free_response_()) {
    this->on_fatal_error();
  }
}

void APIConnection::on_bluetooth_scanner_set_mode_request(const BluetoothScannerSetModeRequest &msg) {
  bluetooth_proxy::global_bluetooth_proxy->bluetooth_scanner_set_mode(
      msg.mode == enums::BluetoothScannerMode::BLUETOOTH_SCANNER_MODE_ACTIVE);
}
void APIConnection::on_bluetooth_set_connection_params_request(const BluetoothSetConnectionParamsRequest &msg) {
  bluetooth_proxy::global_bluetooth_proxy->bluetooth_set_connection_params(msg);
}
#endif

#ifdef USE_VOICE_ASSISTANT
bool APIConnection::check_voice_assistant_api_connection_() const {
  return voice_assistant::global_voice_assistant != nullptr &&
         voice_assistant::global_voice_assistant->get_api_connection() == this;
}

void APIConnection::on_subscribe_voice_assistant_request(const SubscribeVoiceAssistantRequest &msg) {
  if (voice_assistant::global_voice_assistant != nullptr) {
    voice_assistant::global_voice_assistant->client_subscription(this, msg.subscribe);
  }
}
void APIConnection::on_voice_assistant_response(const VoiceAssistantResponse &msg) {
  if (!this->check_voice_assistant_api_connection_()) {
    return;
  }

  if (msg.error) {
    voice_assistant::global_voice_assistant->failed_to_start();
    return;
  }
  if (msg.port == 0) {
    // Use API Audio
    voice_assistant::global_voice_assistant->start_streaming();
  } else {
    struct sockaddr_storage storage;
    socklen_t len = sizeof(storage);
    this->helper_->getpeername((struct sockaddr *) &storage, &len);
    voice_assistant::global_voice_assistant->start_streaming(&storage, msg.port);
  }
};
void APIConnection::on_voice_assistant_event_response(const VoiceAssistantEventResponse &msg) {
  if (this->check_voice_assistant_api_connection_()) {
    voice_assistant::global_voice_assistant->on_event(msg);
  }
}
void APIConnection::on_voice_assistant_audio(const VoiceAssistantAudio &msg) {
  if (this->check_voice_assistant_api_connection_()) {
    voice_assistant::global_voice_assistant->on_audio(msg);
  }
};
void APIConnection::on_voice_assistant_timer_event_response(const VoiceAssistantTimerEventResponse &msg) {
  if (this->check_voice_assistant_api_connection_()) {
    voice_assistant::global_voice_assistant->on_timer_event(msg);
  }
};

void APIConnection::on_voice_assistant_announce_request(const VoiceAssistantAnnounceRequest &msg) {
  if (this->check_voice_assistant_api_connection_()) {
    voice_assistant::global_voice_assistant->on_announce(msg);
  }
}

bool APIConnection::send_voice_assistant_get_configuration_response_(const VoiceAssistantConfigurationRequest &msg) {
  VoiceAssistantConfigurationResponse resp;
  if (!this->check_voice_assistant_api_connection_()) {
    return this->send_message(resp);
  }

  auto &config = voice_assistant::global_voice_assistant->get_configuration();
  for (auto &wake_word : config.available_wake_words) {
    resp.available_wake_words.emplace_back();
    auto &resp_wake_word = resp.available_wake_words.back();
    resp_wake_word.id = StringRef(wake_word.id);
    resp_wake_word.wake_word = StringRef(wake_word.wake_word);
    for (const auto &lang : wake_word.trained_languages) {
      resp_wake_word.trained_languages.push_back(lang);
    }
  }

  // Filter external wake words
  for (auto &wake_word : msg.external_wake_words) {
    if (wake_word.model_type != "micro") {
      // microWakeWord only
      continue;
    }

    resp.available_wake_words.emplace_back();
    auto &resp_wake_word = resp.available_wake_words.back();
    resp_wake_word.id = StringRef(wake_word.id);
    resp_wake_word.wake_word = StringRef(wake_word.wake_word);
    for (const auto &lang : wake_word.trained_languages) {
      resp_wake_word.trained_languages.push_back(lang);
    }
  }

  resp.active_wake_words = &config.active_wake_words;
  resp.max_active_wake_words = config.max_active_wake_words;
  return this->send_message(resp);
}
void APIConnection::on_voice_assistant_configuration_request(const VoiceAssistantConfigurationRequest &msg) {
  if (!this->send_voice_assistant_get_configuration_response_(msg)) {
    this->on_fatal_error();
  }
}

void APIConnection::on_voice_assistant_set_configuration(const VoiceAssistantSetConfiguration &msg) {
  if (this->check_voice_assistant_api_connection_()) {
    voice_assistant::global_voice_assistant->on_set_configuration(msg.active_wake_words);
  }
}
#endif

#ifdef USE_ZWAVE_PROXY
void APIConnection::on_z_wave_proxy_frame(const ZWaveProxyFrame &msg) {
  zwave_proxy::global_zwave_proxy->send_frame(msg.data, msg.data_len);
}

void APIConnection::on_z_wave_proxy_request(const ZWaveProxyRequest &msg) {
  zwave_proxy::global_zwave_proxy->zwave_proxy_request(this, msg.type);
}
#endif

#ifdef USE_ALARM_CONTROL_PANEL
bool APIConnection::send_alarm_control_panel_state(alarm_control_panel::AlarmControlPanel *a_alarm_control_panel) {
  return this->send_message_smart_(a_alarm_control_panel, AlarmControlPanelStateResponse::MESSAGE_TYPE,
                                   AlarmControlPanelStateResponse::ESTIMATED_SIZE);
}
uint16_t APIConnection::try_send_alarm_control_panel_state(EntityBase *entity, APIConnection *conn,
                                                           uint32_t remaining_size) {
  auto *a_alarm_control_panel = static_cast<alarm_control_panel::AlarmControlPanel *>(entity);
  AlarmControlPanelStateResponse resp;
  resp.state = static_cast<enums::AlarmControlPanelState>(a_alarm_control_panel->get_state());
  return fill_and_encode_entity_state(a_alarm_control_panel, resp, conn, remaining_size);
}
uint16_t APIConnection::try_send_alarm_control_panel_info(EntityBase *entity, APIConnection *conn,
                                                          uint32_t remaining_size) {
  auto *a_alarm_control_panel = static_cast<alarm_control_panel::AlarmControlPanel *>(entity);
  ListEntitiesAlarmControlPanelResponse msg;
  msg.supported_features = a_alarm_control_panel->get_supported_features();
  msg.requires_code = a_alarm_control_panel->get_requires_code();
  msg.requires_code_to_arm = a_alarm_control_panel->get_requires_code_to_arm();
  return fill_and_encode_entity_info(a_alarm_control_panel, msg, conn, remaining_size);
}
void APIConnection::on_alarm_control_panel_command_request(const AlarmControlPanelCommandRequest &msg) {
  ENTITY_COMMAND_MAKE_CALL(alarm_control_panel::AlarmControlPanel, a_alarm_control_panel, alarm_control_panel)
  switch (msg.command) {
    case enums::ALARM_CONTROL_PANEL_DISARM:
      call.disarm();
      break;
    case enums::ALARM_CONTROL_PANEL_ARM_AWAY:
      call.arm_away();
      break;
    case enums::ALARM_CONTROL_PANEL_ARM_HOME:
      call.arm_home();
      break;
    case enums::ALARM_CONTROL_PANEL_ARM_NIGHT:
      call.arm_night();
      break;
    case enums::ALARM_CONTROL_PANEL_ARM_VACATION:
      call.arm_vacation();
      break;
    case enums::ALARM_CONTROL_PANEL_ARM_CUSTOM_BYPASS:
      call.arm_custom_bypass();
      break;
    case enums::ALARM_CONTROL_PANEL_TRIGGER:
      call.pending();
      break;
  }
  call.set_code(msg.code.c_str(), msg.code.size());
  call.perform();
}
#endif

#ifdef USE_WATER_HEATER
bool APIConnection::send_water_heater_state(water_heater::WaterHeater *water_heater) {
  return this->send_message_smart_(water_heater, WaterHeaterStateResponse::MESSAGE_TYPE,
                                   WaterHeaterStateResponse::ESTIMATED_SIZE);
}
uint16_t APIConnection::try_send_water_heater_state(EntityBase *entity, APIConnection *conn, uint32_t remaining_size) {
  auto *wh = static_cast<water_heater::WaterHeater *>(entity);
  WaterHeaterStateResponse resp;
  resp.mode = static_cast<enums::WaterHeaterMode>(wh->get_mode());
  resp.current_temperature = wh->get_current_temperature();
  resp.target_temperature = wh->get_target_temperature();
  resp.target_temperature_low = wh->get_target_temperature_low();
  resp.target_temperature_high = wh->get_target_temperature_high();
  resp.state = wh->get_state();

  return fill_and_encode_entity_state(wh, resp, conn, remaining_size);
}
uint16_t APIConnection::try_send_water_heater_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size) {
  auto *wh = static_cast<water_heater::WaterHeater *>(entity);
  ListEntitiesWaterHeaterResponse msg;
  auto traits = wh->get_traits();
  msg.min_temperature = traits.get_min_temperature();
  msg.max_temperature = traits.get_max_temperature();
  msg.target_temperature_step = traits.get_target_temperature_step();
  msg.supported_modes = &traits.get_supported_modes();
  msg.supported_features = traits.get_feature_flags();
  return fill_and_encode_entity_info(wh, msg, conn, remaining_size);
}

void APIConnection::on_water_heater_command_request(const WaterHeaterCommandRequest &msg) {
  ENTITY_COMMAND_MAKE_CALL(water_heater::WaterHeater, water_heater, water_heater)
  if (msg.has_fields & enums::WATER_HEATER_COMMAND_HAS_MODE)
    call.set_mode(static_cast<water_heater::WaterHeaterMode>(msg.mode));
  if (msg.has_fields & enums::WATER_HEATER_COMMAND_HAS_TARGET_TEMPERATURE)
    call.set_target_temperature(msg.target_temperature);
  if (msg.has_fields & enums::WATER_HEATER_COMMAND_HAS_TARGET_TEMPERATURE_LOW)
    call.set_target_temperature_low(msg.target_temperature_low);
  if (msg.has_fields & enums::WATER_HEATER_COMMAND_HAS_TARGET_TEMPERATURE_HIGH)
    call.set_target_temperature_high(msg.target_temperature_high);
  if ((msg.has_fields & enums::WATER_HEATER_COMMAND_HAS_AWAY_STATE) ||
      (msg.has_fields & enums::WATER_HEATER_COMMAND_HAS_STATE)) {
    call.set_away((msg.state & water_heater::WATER_HEATER_STATE_AWAY) != 0);
  }
  if ((msg.has_fields & enums::WATER_HEATER_COMMAND_HAS_ON_STATE) ||
      (msg.has_fields & enums::WATER_HEATER_COMMAND_HAS_STATE)) {
    call.set_on((msg.state & water_heater::WATER_HEATER_STATE_ON) != 0);
  }
  call.perform();
}
#endif

#ifdef USE_EVENT
// Event is a special case - unlike other entities with simple state fields,
// events store their state in a member accessed via obj->get_last_event_type()
void APIConnection::send_event(event::Event *event) {
  this->send_message_smart_(event, EventResponse::MESSAGE_TYPE, EventResponse::ESTIMATED_SIZE,
                            event->get_last_event_type_index());
}
uint16_t APIConnection::try_send_event_response(event::Event *event, StringRef event_type, APIConnection *conn,
                                                uint32_t remaining_size) {
  EventResponse resp;
  resp.event_type = event_type;
  return fill_and_encode_entity_state(event, resp, conn, remaining_size);
}

uint16_t APIConnection::try_send_event_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size) {
  auto *event = static_cast<event::Event *>(entity);
  ListEntitiesEventResponse msg;
  msg.event_types = &event->get_event_types();
  return fill_and_encode_entity_info_with_device_class(event, msg, msg.device_class, conn, remaining_size);
}
#endif

#if defined(USE_IR_RF) || defined(USE_RADIO_FREQUENCY)
void APIConnection::on_infrared_rf_transmit_raw_timings_request(const InfraredRFTransmitRawTimingsRequest &msg) {
  // Dispatch by key: infrared entities are checked first, then radio frequency entities.
  // The key is unique across all entity instances on a device, so at most one lookup will succeed.
#ifdef USE_INFRARED
  ENTITY_COMMAND_LOOKUP(infrared::Infrared, infrared, infrared);
  if (infrared != nullptr) {
    auto call = infrared->make_call();
    call.set_carrier_frequency(msg.carrier_frequency);
    call.set_raw_timings_packed(msg.timings_data_, msg.timings_length_, msg.timings_count_);
    call.set_repeat_count(msg.repeat_count);
    call.perform();
    return;
  }
#endif
#ifdef USE_RADIO_FREQUENCY
  ENTITY_COMMAND_LOOKUP(radio_frequency::RadioFrequency, radio_frequency, radio_frequency);
  if (radio_frequency != nullptr) {
    auto call = radio_frequency->make_call();
    call.set_frequency(msg.carrier_frequency);
    call.set_modulation(static_cast<radio_frequency::RadioFrequencyModulation>(msg.modulation));
    call.set_repeat_count(msg.repeat_count);
    call.set_raw_timings_packed(msg.timings_data_, msg.timings_length_, msg.timings_count_);
    call.perform();
  }
#endif
}
#endif

#if defined(USE_IR_RF) || defined(USE_RADIO_FREQUENCY)
void APIConnection::send_infrared_rf_receive_event(const InfraredRFReceiveEvent &msg) { this->send_message(msg); }
#endif

#ifdef USE_SERIAL_PROXY
void APIConnection::on_serial_proxy_configure_request(const SerialProxyConfigureRequest &msg) {
  auto &proxies = App.get_serial_proxies();
  if (msg.instance >= proxies.size()) {
    ESP_LOGW(TAG, "Serial proxy instance %" PRIu32 " out of range (max %" PRIu32 ")", msg.instance,
             static_cast<uint32_t>(proxies.size()));
    return;
  }
  proxies[msg.instance]->configure(msg.baudrate, msg.flow_control, static_cast<uint8_t>(msg.parity), msg.stop_bits,
                                   msg.data_size);
}

void APIConnection::on_serial_proxy_write_request(const SerialProxyWriteRequest &msg) {
  auto &proxies = App.get_serial_proxies();
  if (msg.instance >= proxies.size()) {
    ESP_LOGW(TAG, "Serial proxy instance %" PRIu32 " out of range", msg.instance);
    return;
  }
  proxies[msg.instance]->write_from_client(msg.data, msg.data_len);
}

void APIConnection::on_serial_proxy_set_modem_pins_request(const SerialProxySetModemPinsRequest &msg) {
  auto &proxies = App.get_serial_proxies();
  if (msg.instance >= proxies.size()) {
    ESP_LOGW(TAG, "Serial proxy instance %" PRIu32 " out of range", msg.instance);
    return;
  }
  proxies[msg.instance]->set_modem_pins(msg.line_states);
}

void APIConnection::on_serial_proxy_get_modem_pins_request(const SerialProxyGetModemPinsRequest &msg) {
  auto &proxies = App.get_serial_proxies();
  if (msg.instance >= proxies.size()) {
    ESP_LOGW(TAG, "Serial proxy instance %" PRIu32 " out of range", msg.instance);
    return;
  }
  SerialProxyGetModemPinsResponse resp{};
  resp.instance = msg.instance;
  resp.line_states = proxies[msg.instance]->get_modem_pins();
  this->send_message(resp);
}

void APIConnection::on_serial_proxy_request(const SerialProxyRequest &msg) {
  auto &proxies = App.get_serial_proxies();
  if (msg.instance >= proxies.size()) {
    ESP_LOGW(TAG, "Serial proxy instance %" PRIu32 " out of range", msg.instance);
    return;
  }
  switch (msg.type) {
    case enums::SERIAL_PROXY_REQUEST_TYPE_SUBSCRIBE:
    case enums::SERIAL_PROXY_REQUEST_TYPE_UNSUBSCRIBE:
      proxies[msg.instance]->serial_proxy_request(this, msg.type);
      break;
    case enums::SERIAL_PROXY_REQUEST_TYPE_FLUSH: {
      SerialProxyRequestResponse resp{};
      resp.instance = msg.instance;
      resp.type = enums::SERIAL_PROXY_REQUEST_TYPE_FLUSH;
      switch (proxies[msg.instance]->flush_port()) {
        case uart::UARTFlushResult::UART_FLUSH_RESULT_SUCCESS:
          resp.status = enums::SERIAL_PROXY_STATUS_OK;
          break;
        case uart::UARTFlushResult::UART_FLUSH_RESULT_ASSUMED_SUCCESS:
          resp.status = enums::SERIAL_PROXY_STATUS_ASSUMED_SUCCESS;
          break;
        case uart::UARTFlushResult::UART_FLUSH_RESULT_TIMEOUT:
          resp.status = enums::SERIAL_PROXY_STATUS_TIMEOUT;
          break;
        case uart::UARTFlushResult::UART_FLUSH_RESULT_FAILED:
          resp.status = enums::SERIAL_PROXY_STATUS_ERROR;
          break;
      }
      this->send_message(resp);
      break;
    }
    default:
      ESP_LOGW(TAG, "Unknown serial proxy request type: %" PRIu32, static_cast<uint32_t>(msg.type));
      break;
  }
}

void APIConnection::send_serial_proxy_data(const SerialProxyDataReceived &msg) { this->send_message(msg); }
#endif

#ifdef USE_INFRARED
uint16_t APIConnection::try_send_infrared_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size) {
  auto *infrared = static_cast<infrared::Infrared *>(entity);
  ListEntitiesInfraredResponse msg;
  msg.capabilities = infrared->get_capability_flags();
  msg.receiver_frequency = infrared->get_traits().get_receiver_frequency_hz();
  return fill_and_encode_entity_info(infrared, msg, conn, remaining_size);
}
#endif

#ifdef USE_RADIO_FREQUENCY
uint16_t APIConnection::try_send_radio_frequency_info(EntityBase *entity, APIConnection *conn,
                                                      uint32_t remaining_size) {
  auto *rf = static_cast<radio_frequency::RadioFrequency *>(entity);
  ListEntitiesRadioFrequencyResponse msg;
  msg.capabilities = rf->get_capability_flags();
  msg.frequency_min = rf->get_traits().get_frequency_min_hz();
  msg.frequency_max = rf->get_traits().get_frequency_max_hz();
  msg.supported_modulations = rf->get_traits().get_supported_modulations();
  return fill_and_encode_entity_info(rf, msg, conn, remaining_size);
}
#endif

#ifdef USE_UPDATE
bool APIConnection::send_update_state(update::UpdateEntity *update) {
  return this->send_message_smart_(update, UpdateStateResponse::MESSAGE_TYPE, UpdateStateResponse::ESTIMATED_SIZE);
}
uint16_t APIConnection::try_send_update_state(EntityBase *entity, APIConnection *conn, uint32_t remaining_size) {
  auto *update = static_cast<update::UpdateEntity *>(entity);
  UpdateStateResponse resp;
  resp.missing_state = !update->has_state();
  if (update->has_state()) {
    resp.in_progress = update->state == update::UpdateState::UPDATE_STATE_INSTALLING;
    if (update->update_info.has_progress) {
      resp.has_progress = true;
      resp.progress = update->update_info.progress;
    }
    resp.current_version = StringRef(update->update_info.current_version);
    resp.latest_version = StringRef(update->update_info.latest_version);
    resp.title = StringRef(update->update_info.title);
    resp.release_summary = StringRef(update->update_info.summary);
    resp.release_url = StringRef(update->update_info.release_url);
  }
  return fill_and_encode_entity_state(update, resp, conn, remaining_size);
}
uint16_t APIConnection::try_send_update_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size) {
  auto *update = static_cast<update::UpdateEntity *>(entity);
  ListEntitiesUpdateResponse msg;
  return fill_and_encode_entity_info_with_device_class(update, msg, msg.device_class, conn, remaining_size);
}
void APIConnection::on_update_command_request(const UpdateCommandRequest &msg) {
  ENTITY_COMMAND_GET(update::UpdateEntity, update, update)

  switch (msg.command) {
    case enums::UPDATE_COMMAND_UPDATE:
      update->perform();
      break;
    case enums::UPDATE_COMMAND_CHECK:
      update->check();
      break;
    case enums::UPDATE_COMMAND_NONE:
      ESP_LOGE(TAG, "UPDATE_COMMAND_NONE not handled; confirm command is correct");
      break;
    default:
      ESP_LOGW(TAG, "Unknown update command: %" PRIu32, msg.command);
      break;
  }
}
#endif

bool APIConnection::try_send_log_message(int level, const char *tag, const char *line, size_t message_len) {
  SubscribeLogsResponse msg;
  msg.level = static_cast<enums::LogLevel>(level);
  msg.set_message(reinterpret_cast<const uint8_t *>(line), message_len);
  return this->send_message(msg);
}

void APIConnection::complete_authentication_() {
  // Early return if already authenticated
  if (this->flags_.connection_state == static_cast<uint8_t>(ConnectionState::AUTHENTICATED)) {
    return;
  }

  this->flags_.connection_state = static_cast<uint8_t>(ConnectionState::AUTHENTICATED);
  // Reset traffic timer so keepalive starts from authentication, not connection start
  this->last_traffic_ = App.get_loop_component_start_time();
  this->log_client_(ESPHOME_LOG_LEVEL_DEBUG, LOG_STR("connected"));
#ifdef USE_API_CLIENT_CONNECTED_TRIGGER
  {
    char peername[socket::SOCKADDR_STR_LEN];
    this->parent_->get_client_connected_trigger()->trigger(std::string(this->helper_->get_client_name()),
                                                           std::string(this->helper_->get_peername_to(peername)));
  }
#endif
#ifdef USE_HOMEASSISTANT_TIME
  if (homeassistant::global_homeassistant_time != nullptr) {
    this->send_time_request();
  }
#endif
#ifdef USE_ZWAVE_PROXY
  if (zwave_proxy::global_zwave_proxy != nullptr) {
    zwave_proxy::global_zwave_proxy->api_connection_authenticated(this);
  }
#endif
}

bool APIConnection::send_hello_response_(const HelloRequest &msg) {
  // Copy client name with truncation if needed (set_client_name handles truncation)
  this->helper_->set_client_name(msg.client_info.c_str(), msg.client_info.size());
  this->client_api_version_major_ = msg.api_version_major;
  this->client_api_version_minor_ = msg.api_version_minor;
  char peername[socket::SOCKADDR_STR_LEN];
  ESP_LOGV(TAG, "Hello from client: '%s' | %s | API Version %" PRIu16 ".%" PRIu16, this->helper_->get_client_name(),
           this->helper_->get_peername_to(peername), this->client_api_version_major_, this->client_api_version_minor_);

  // TODO: Remove before 2026.8.0 (one version after get_object_id backward compat removal)
  if (!this->client_supports_api_version(1, 14)) {
    ESP_LOGW(TAG, "'%s' using outdated API %" PRIu16 ".%" PRIu16 ", update to 1.14+", this->helper_->get_client_name(),
             this->client_api_version_major_, this->client_api_version_minor_);
  }

  HelloResponse resp;
  resp.api_version_major = 1;
  resp.api_version_minor = 14;
  // Send only the version string - the client only logs this for debugging and doesn't use it otherwise
  resp.server_info = ESPHOME_VERSION_REF;
  resp.name = StringRef(App.get_name());

  // Auto-authenticate - password auth was removed in ESPHome 2026.1.0
  this->complete_authentication_();

  return this->send_message(resp);
}

bool APIConnection::send_ping_response_() {
  PingResponse resp;
  return this->send_message(resp);
}

bool APIConnection::send_device_info_response_() {
  DeviceInfoResponse resp;
  resp.name = StringRef(App.get_name());
  resp.friendly_name = StringRef(App.get_friendly_name());
#ifdef USE_AREAS
  resp.suggested_area = StringRef(App.get_area());
#endif
  // Stack buffer for MAC address (XX:XX:XX:XX:XX:XX\0 = 18 bytes)
  char mac_address[18];
  uint8_t mac[6];
  get_mac_address_raw(mac);
  format_mac_addr_upper(mac, mac_address);
  resp.mac_address = StringRef(mac_address);

  resp.esphome_version = ESPHOME_VERSION_REF;

  // Stack buffer for build time string
  char build_time_str[Application::BUILD_TIME_STR_SIZE];
  App.get_build_time_string(build_time_str);
  resp.compilation_time = StringRef(build_time_str);

  // Manufacturer string - define once, handle ESP8266 PROGMEM separately
#if defined(USE_ESP8266) || defined(USE_ESP32)
#define ESPHOME_MANUFACTURER "Espressif"
#elif defined(USE_RP2040)
#define ESPHOME_MANUFACTURER "Raspberry Pi"
#elif defined(USE_BK72XX)
#define ESPHOME_MANUFACTURER "Beken"
#elif defined(USE_LN882X)
#define ESPHOME_MANUFACTURER "Lightning"
#elif defined(USE_NRF52)
#define ESPHOME_MANUFACTURER "Nordic Semiconductor"
#elif defined(USE_RTL87XX)
#define ESPHOME_MANUFACTURER "Realtek"
#elif defined(USE_HOST)
#define ESPHOME_MANUFACTURER "Host"
#endif

#ifdef USE_ESP8266
  // ESP8266 requires PROGMEM for flash storage, copy to stack for memcpy compatibility
  static const char MANUFACTURER_PROGMEM[] PROGMEM = ESPHOME_MANUFACTURER;
  char manufacturer_buf[sizeof(MANUFACTURER_PROGMEM)];
  memcpy_P(manufacturer_buf, MANUFACTURER_PROGMEM, sizeof(MANUFACTURER_PROGMEM));
  resp.manufacturer = StringRef(manufacturer_buf, sizeof(MANUFACTURER_PROGMEM) - 1);
#else
  static constexpr auto MANUFACTURER = StringRef::from_lit(ESPHOME_MANUFACTURER);
  resp.manufacturer = MANUFACTURER;
#endif
  static_assert(sizeof(ESPHOME_MANUFACTURER) - 1 <= 20, "Update max_data_length for manufacturer in api.proto");
#undef ESPHOME_MANUFACTURER

#ifdef USE_ESP8266
  static const char MODEL_PROGMEM[] PROGMEM = ESPHOME_BOARD;
  char model_buf[sizeof(MODEL_PROGMEM)];
  memcpy_P(model_buf, MODEL_PROGMEM, sizeof(MODEL_PROGMEM));
  resp.model = StringRef(model_buf, sizeof(MODEL_PROGMEM) - 1);
#else
  static constexpr auto MODEL = StringRef::from_lit(ESPHOME_BOARD);
  resp.model = MODEL;
#endif
#ifdef USE_DEEP_SLEEP
  resp.has_deep_sleep = deep_sleep::global_has_deep_sleep;
#endif
#ifdef ESPHOME_PROJECT_NAME
#ifdef USE_ESP8266
  static const char PROJECT_NAME_PROGMEM[] PROGMEM = ESPHOME_PROJECT_NAME;
  static const char PROJECT_VERSION_PROGMEM[] PROGMEM = ESPHOME_PROJECT_VERSION;
  char project_name_buf[sizeof(PROJECT_NAME_PROGMEM)];
  char project_version_buf[sizeof(PROJECT_VERSION_PROGMEM)];
  memcpy_P(project_name_buf, PROJECT_NAME_PROGMEM, sizeof(PROJECT_NAME_PROGMEM));
  memcpy_P(project_version_buf, PROJECT_VERSION_PROGMEM, sizeof(PROJECT_VERSION_PROGMEM));
  resp.project_name = StringRef(project_name_buf, sizeof(PROJECT_NAME_PROGMEM) - 1);
  resp.project_version = StringRef(project_version_buf, sizeof(PROJECT_VERSION_PROGMEM) - 1);
#else
  static constexpr auto PROJECT_NAME = StringRef::from_lit(ESPHOME_PROJECT_NAME);
  static constexpr auto PROJECT_VERSION = StringRef::from_lit(ESPHOME_PROJECT_VERSION);
  resp.project_name = PROJECT_NAME;
  resp.project_version = PROJECT_VERSION;
#endif
#endif
#ifdef USE_WEBSERVER
  resp.webserver_port = USE_WEBSERVER_PORT;
#endif
#ifdef USE_BLUETOOTH_PROXY
  resp.bluetooth_proxy_feature_flags = bluetooth_proxy::global_bluetooth_proxy->get_feature_flags();
  // Stack buffer for Bluetooth MAC address (XX:XX:XX:XX:XX:XX\0 = 18 bytes)
  char bluetooth_mac[18];
  bluetooth_proxy::global_bluetooth_proxy->get_bluetooth_mac_address_pretty(bluetooth_mac);
  resp.bluetooth_mac_address = StringRef(bluetooth_mac);
#endif
#ifdef USE_VOICE_ASSISTANT
  resp.voice_assistant_feature_flags = voice_assistant::global_voice_assistant->get_feature_flags();
#endif
#ifdef USE_ZWAVE_PROXY
  resp.zwave_proxy_feature_flags = zwave_proxy::global_zwave_proxy->get_feature_flags();
  resp.zwave_home_id = zwave_proxy::global_zwave_proxy->get_home_id();
#endif
#ifdef USE_SERIAL_PROXY
  size_t serial_proxy_index = 0;
  for (auto const &proxy : App.get_serial_proxies()) {
    if (serial_proxy_index >= SERIAL_PROXY_COUNT)
      break;
    auto &info = resp.serial_proxies[serial_proxy_index++];
    info.name = StringRef(proxy->get_name());
    info.port_type = proxy->get_port_type();
  }
#endif
#ifdef USE_API_NOISE
  resp.api_encryption_supported = true;
#endif
#ifdef USE_DEVICES
  size_t device_index = 0;
  for (auto const &device : App.get_devices()) {
    if (device_index >= ESPHOME_DEVICE_COUNT)
      break;
    auto &device_info = resp.devices[device_index++];
    device_info.device_id = device->get_device_id();
    device_info.name = StringRef(device->get_name());
    device_info.area_id = device->get_area_id();
  }
#endif
#ifdef USE_AREAS
  size_t area_index = 0;
  for (auto const &area : App.get_areas()) {
    if (area_index >= ESPHOME_AREA_COUNT)
      break;
    auto &area_info = resp.areas[area_index++];
    area_info.area_id = area->get_area_id();
    area_info.name = StringRef(area->get_name());
  }
#endif

  return this->send_message(resp);
}
void APIConnection::on_hello_request(const HelloRequest &msg) {
  if (!this->send_hello_response_(msg)) {
    this->on_fatal_error();
  }
}
void APIConnection::on_disconnect_request() {
  if (!this->send_disconnect_response_()) {
    this->on_fatal_error();
  }
}
void APIConnection::on_ping_request() {
  if (!this->send_ping_response_()) {
    this->on_fatal_error();
  }
}
void APIConnection::on_device_info_request() {
  if (!this->send_device_info_response_()) {
    this->on_fatal_error();
  }
}

#ifdef USE_API_HOMEASSISTANT_STATES
void APIConnection::on_home_assistant_state_response(const HomeAssistantStateResponse &msg) {
  // Skip if entity_id is empty (invalid message)
  if (msg.entity_id.empty()) {
    return;
  }

  // Null-terminate state in-place for safe c_str() usage (e.g., parse_number in callbacks).
  // Safe: decode is complete, byte after string data was already consumed during parse,
  // and frame helpers reserve RX_BUF_NULL_TERMINATOR extra byte in rx_buf_.
  // const_cast is safe: msg references mutable rx_buf_ data; the const& handler
  // signature is a generated protobuf pattern, not a true immutability contract.
  if (!msg.state.empty()) {
    const_cast<char *>(msg.state.c_str())[msg.state.size()] = '\0';
  }

  for (auto &it : this->parent_->get_state_subs()) {
    if (msg.entity_id != it.entity_id) {
      continue;
    }

    // If subscriber has attribute filter (non-null), message attribute must match it;
    // if subscriber has no filter (nullptr), message must have no attribute.
    if (it.attribute != nullptr ? msg.attribute != it.attribute : !msg.attribute.empty()) {
      continue;
    }

    it.callback(msg.state);
  }
}
#endif
#ifdef USE_API_USER_DEFINED_ACTIONS
void APIConnection::on_execute_service_request(const ExecuteServiceRequest &msg) {
  // Null-terminate string args in-place for safe c_str() usage in YAML service triggers.
  // Safe: full ExecuteServiceRequest decode is complete, all bytes in rx_buf_ consumed,
  // and frame helpers reserve RX_BUF_NULL_TERMINATOR extra byte for the last field.
  // const_cast is safe: msg references mutable rx_buf_ data; the const& handler
  // signature is a generated protobuf pattern, not a true immutability contract.
  for (auto &arg : const_cast<ExecuteServiceRequest &>(msg).args) {
    if (!arg.string_.empty()) {
      const_cast<char *>(arg.string_.c_str())[arg.string_.size()] = '\0';
    }
  }
  bool found = false;
#ifdef USE_API_USER_DEFINED_ACTION_RESPONSES
  // Register the call and get a unique server-generated action_call_id
  // This avoids collisions when multiple clients use the same call_id
  uint32_t action_call_id = 0;
  if (msg.call_id != 0) {
    action_call_id = this->parent_->register_active_action_call(msg.call_id, this);
  }
  // Use the overload that passes action_call_id separately (avoids copying msg)
  for (auto *service : this->parent_->get_user_services()) {
    if (service->execute_service(msg, action_call_id)) {
      found = true;
    }
  }
#else
  for (auto *service : this->parent_->get_user_services()) {
    if (service->execute_service(msg)) {
      found = true;
    }
  }
#endif
  if (!found) {
    ESP_LOGV(TAG, "Could not find service");
  }
  // Note: For services with supports_response != none, the call is unregistered
  // by an automatically appended APIUnregisterServiceCallAction at the end of
  // the action list. This ensures async actions (delays, waits) complete first.
}
#ifdef USE_API_USER_DEFINED_ACTION_RESPONSES
void APIConnection::send_execute_service_response(uint32_t call_id, bool success, StringRef error_message) {
  ExecuteServiceResponse resp;
  resp.call_id = call_id;
  resp.success = success;
  resp.error_message = error_message;
  this->send_message(resp);
}
#ifdef USE_API_USER_DEFINED_ACTION_RESPONSES_JSON
void APIConnection::send_execute_service_response(uint32_t call_id, bool success, StringRef error_message,
                                                  const uint8_t *response_data, size_t response_data_len) {
  ExecuteServiceResponse resp;
  resp.call_id = call_id;
  resp.success = success;
  resp.error_message = error_message;
  resp.response_data = response_data;
  resp.response_data_len = response_data_len;
  this->send_message(resp);
}
#endif  // USE_API_USER_DEFINED_ACTION_RESPONSES_JSON
#endif  // USE_API_USER_DEFINED_ACTION_RESPONSES
#endif

#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES
void APIConnection::on_homeassistant_action_response(const HomeassistantActionResponse &msg) {
#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES_JSON
  if (msg.response_data_len > 0) {
    this->parent_->handle_action_response(msg.call_id, msg.success, msg.error_message, msg.response_data,
                                          msg.response_data_len);
  } else
#endif
  {
    this->parent_->handle_action_response(msg.call_id, msg.success, msg.error_message);
  }
};
#endif
#ifdef USE_API_NOISE
bool APIConnection::send_noise_encryption_set_key_response_(const NoiseEncryptionSetKeyRequest &msg) {
  NoiseEncryptionSetKeyResponse resp;
  resp.success = false;

  psk_t psk{};
  if (msg.key_len == 0) {
    if (this->parent_->clear_noise_psk(true)) {
      resp.success = true;
    } else {
      ESP_LOGW(TAG, "Failed to clear encryption key");
    }
  } else if (base64_decode(msg.key, msg.key_len, psk.data(), psk.size()) != psk.size()) {
    ESP_LOGW(TAG, "Invalid encryption key length");
  } else if (!this->parent_->save_noise_psk(psk, true)) {
    ESP_LOGW(TAG, "Failed to save encryption key");
  } else {
    resp.success = true;
  }

  return this->send_message(resp);
}
void APIConnection::on_noise_encryption_set_key_request(const NoiseEncryptionSetKeyRequest &msg) {
  if (!this->send_noise_encryption_set_key_response_(msg)) {
    this->on_fatal_error();
  }
}
#endif
#ifdef USE_API_HOMEASSISTANT_STATES
void APIConnection::on_subscribe_home_assistant_states_request() { state_subs_at_ = 0; }
#endif
bool APIConnection::try_to_clear_buffer_slow_(bool log_out_of_space) {
  delay(0);
  APIError err = this->helper_->loop();
  if (err != APIError::OK) {
    this->fatal_error_with_log_(LOG_STR("Socket operation failed"), err);
    return false;
  }
  if (this->helper_->can_write_without_blocking())
    return true;
  if (log_out_of_space) {
    ESP_LOGV(TAG, "Cannot send message because of TCP buffer space");
  }
  return false;
}
bool APIConnection::send_message_(uint32_t payload_size, uint8_t message_type, MessageEncodeFn encode_fn,
                                  const void *msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  // Skip dump for log messages (recursive logging risk) and camera frames (high-frequency noise)
  if (message_type != SubscribeLogsResponse::MESSAGE_TYPE
#ifdef USE_CAMERA
      && message_type != CameraImageResponse::MESSAGE_TYPE
#endif
  ) {
    auto *proto_msg = static_cast<const ProtoMessage *>(msg);
    DumpBuffer dump_buf;
    this->log_send_message_(proto_msg->message_name(), proto_msg->dump_to(dump_buf));
  }
#endif
  auto &shared_buf = this->parent_->get_shared_buffer_ref();
  this->prepare_first_message_buffer(shared_buf, payload_size);
  size_t write_start = shared_buf.size();
  shared_buf.resize(write_start + payload_size);
  ProtoWriteBuffer buffer{&shared_buf, write_start};
  encode_fn(msg, buffer PROTO_ENCODE_DEBUG_INIT(&shared_buf));
  return this->send_buffer(ProtoWriteBuffer{&shared_buf}, message_type);
}
// encode_to_buffer is defined inline in api_connection.h (ESPHOME_ALWAYS_INLINE)

// Noinline version for cold paths — single shared copy
uint16_t APIConnection::encode_to_buffer_slow(uint32_t calculated_size, MessageEncodeFn encode_fn, const void *msg,
                                              APIConnection *conn, uint32_t remaining_size) {
  return encode_to_buffer(calculated_size, encode_fn, msg, conn, remaining_size);
}
bool APIConnection::send_buffer(ProtoWriteBuffer buffer, uint8_t message_type) {
  const bool is_log_message = (message_type == SubscribeLogsResponse::MESSAGE_TYPE);

  if (!this->try_to_clear_buffer(!is_log_message)) {
    return false;
  }

  // Set TCP_NODELAY based on message type - see set_nodelay_for_message() for details
  this->helper_->set_nodelay_for_message(is_log_message);

  APIError err = this->helper_->write_protobuf_packet(message_type, buffer);
  if (err == APIError::WOULD_BLOCK)
    return false;
  if (err != APIError::OK) {
    this->fatal_error_with_log_(LOG_STR("Packet write failed"), err);
    return false;
  }
  // Do not set last_traffic_ on send
  return true;
}
void APIConnection::on_no_setup_connection() {
  this->on_fatal_error();
  this->log_client_(ESPHOME_LOG_LEVEL_DEBUG, LOG_STR("no connection setup"));
}
void APIConnection::on_fatal_error() {
  // Don't close socket here - keep it open so getpeername() works for logging
  // Socket will be closed when client is removed from the list in APIServer::loop()
  this->flags_.remove = true;
}

bool APIConnection::schedule_message_front_(EntityBase *entity, uint8_t message_type, uint8_t estimated_size) {
  this->deferred_batch_.add_item_front(entity, message_type, estimated_size);
  return this->schedule_batch_();
}

bool APIConnection::send_message_smart_(EntityBase *entity, uint8_t message_type, uint8_t estimated_size,
                                        uint8_t aux_data_index) {
  if (this->should_send_immediately_(message_type) && this->helper_->can_write_without_blocking()) {
    auto &shared_buf = this->parent_->get_shared_buffer_ref();
    this->prepare_first_message_buffer(shared_buf, estimated_size);
    DeferredBatch::BatchItem item{entity, message_type, estimated_size, aux_data_index};
    if (this->dispatch_message_(item, MAX_BATCH_PACKET_SIZE, true) &&
        this->send_buffer(ProtoWriteBuffer{&shared_buf}, message_type)) {
#ifdef HAS_PROTO_MESSAGE_DUMP
      this->log_batch_item_(item);
#endif
      return true;
    }
  }
  return this->schedule_message_(entity, message_type, estimated_size, aux_data_index);
}

bool APIConnection::schedule_batch_() {
  if (!this->flags_.batch_scheduled) {
    this->flags_.batch_scheduled = true;
    this->deferred_batch_.batch_start_time = App.get_loop_component_start_time();
  }
  return true;
}

void APIConnection::process_batch_() {
  if (this->deferred_batch_.empty()) {
    this->flags_.batch_scheduled = false;
    return;
  }

  // Ensure TCP_NODELAY is on before draining overflow and writing batch data.
  // Log messages enable Nagle (NODELAY off) to coalesce small packets.
  // If Nagle is still on when we try to drain, LWIP holds data in the
  // Nagle buffer, the TCP send buffer stays full, and the overflow
  // buffer can never drain — blocking the batch write indefinitely.
  this->helper_->set_nodelay_for_message(false);

  // Try to clear buffer first
  if (!this->try_to_clear_buffer(true)) {
    // Can't write now, we'll try again later
    return;
  }

  // Get shared buffer reference once to avoid multiple calls
  auto &shared_buf = this->parent_->get_shared_buffer_ref();
  size_t num_items = this->deferred_batch_.size();

  // Cache these values to avoid repeated virtual calls
  const uint8_t header_padding = this->helper_->frame_header_padding();
  const uint8_t footer_size = this->helper_->frame_footer_size();

  // Pre-calculate exact buffer size needed based on message types
  uint32_t total_estimated_size = num_items * (header_padding + footer_size);
  for (size_t i = 0; i < num_items; i++) {
    total_estimated_size += this->deferred_batch_[i].estimated_size;
  }
  // Clamp to MAX_BATCH_PACKET_SIZE — we won't send more than that per batch
  if (total_estimated_size > MAX_BATCH_PACKET_SIZE) {
    total_estimated_size = MAX_BATCH_PACKET_SIZE;
  }

  this->prepare_first_message_buffer(shared_buf, header_padding, total_estimated_size);

  // Fast path for single message - buffer already allocated above
  if (num_items == 1) {
    const auto &item = this->deferred_batch_[0];
    // Let dispatch_message_ calculate size and encode if it fits
    uint16_t payload_size = this->dispatch_message_(item, std::numeric_limits<uint16_t>::max(), true);

    if (payload_size > 0 && this->send_buffer(ProtoWriteBuffer{&shared_buf}, item.message_type)) {
#ifdef HAS_PROTO_MESSAGE_DUMP
      // Log message after send attempt for VV debugging
      this->log_batch_item_(item);
#endif
      this->clear_batch_();
    } else if (payload_size == 0) {
      // Message too large to fit in available space
      ESP_LOGW(TAG, "Message too large to send: type=%u", item.message_type);
      this->clear_batch_();
    }
    return;
  }

  // Multi-message path — heavy stack frame isolated in separate noinline function
  this->process_batch_multi_(shared_buf, num_items, header_padding, footer_size);
}

// Separated from process_batch_() so the single-message fast path gets a minimal
// stack frame without the MAX_MESSAGES_PER_BATCH * sizeof(MessageInfo) array.
void APIConnection::process_batch_multi_(APIBuffer &shared_buf, size_t num_items, uint8_t header_padding,
                                         uint8_t footer_size) {
  // Ensure MessageInfo remains trivially destructible for our placement new approach
  static_assert(std::is_trivially_destructible<MessageInfo>::value,
                "MessageInfo must remain trivially destructible with this placement-new approach");

  const size_t messages_to_process = std::min(num_items, MAX_MESSAGES_PER_BATCH);

  // Stack-allocated array for message info
  alignas(MessageInfo) char message_info_storage[MAX_MESSAGES_PER_BATCH * sizeof(MessageInfo)];
  MessageInfo *message_info = reinterpret_cast<MessageInfo *>(message_info_storage);
  size_t items_processed = 0;
  uint16_t remaining_size = std::numeric_limits<uint16_t>::max();
  // Track where each message's header begins in the buffer
  // First message: offset 0 (max padding, may have unused leading bytes)
  // Subsequent messages: offset points to exact header start (no gaps)
  uint32_t current_offset = 0;

  // Process items and encode directly to buffer (up to our limit)
  for (size_t i = 0; i < messages_to_process; i++) {
    const auto &item = this->deferred_batch_[i];
    // Try to encode message via dispatch
    // The dispatch function calculates overhead to determine if the message fits
    uint16_t payload_size = this->dispatch_message_(item, remaining_size, i == 0);

    if (payload_size == 0) {
      // Message won't fit, stop processing
      break;
    }

    // Message was encoded successfully
    // payload_size = header_size + proto_payload_size + footer_size
    uint16_t proto_payload_size = payload_size - this->batch_header_size_ - footer_size;
    // Use placement new to construct MessageInfo in pre-allocated stack array
    // This avoids default-constructing all MAX_MESSAGES_PER_BATCH elements
    // Explicit destruction is not needed because MessageInfo is trivially destructible,
    // as ensured by the static_assert in its definition.
    new (&message_info[items_processed++])
        MessageInfo(item.message_type, current_offset, proto_payload_size, this->batch_header_size_);
    // After first message, set remaining size to MAX_BATCH_PACKET_SIZE to avoid fragmentation
    if (items_processed == 1) {
      remaining_size = MAX_BATCH_PACKET_SIZE;
    }
    remaining_size -= payload_size;
    // Calculate where the next message's header padding will start
    // Current buffer size + footer space for this message
    current_offset = shared_buf.size() + footer_size;
  }

  if (items_processed > 0) {
    // Add footer space for the last message (for Noise protocol MAC)
    if (footer_size > 0) {
      shared_buf.resize(shared_buf.size() + footer_size);
    }

    // Send all collected messages
    APIError err = this->helper_->write_protobuf_messages(ProtoWriteBuffer{&shared_buf},
                                                          std::span<const MessageInfo>(message_info, items_processed));
    if (err != APIError::OK && err != APIError::WOULD_BLOCK) {
      this->fatal_error_with_log_(LOG_STR("Batch write failed"), err);
    }

#ifdef HAS_PROTO_MESSAGE_DUMP
    // Log messages after send attempt for VV debugging
    // It's safe to use the buffer for logging at this point regardless of send result
    for (size_t i = 0; i < items_processed; i++) {
      const auto &item = this->deferred_batch_[i];
      this->log_batch_item_(item);
    }
#endif

    // Partial batch — remove processed items and reschedule
    if (items_processed < this->deferred_batch_.size()) {
      this->deferred_batch_.remove_front(items_processed);
      this->schedule_batch_();
      return;
    }
  }

  // All items processed (or none could be processed)
  this->clear_batch_();
}

// Dispatch message encoding based on message_type
// Switch assigns function pointer, single call site for smaller code size
uint16_t APIConnection::dispatch_message_(const DeferredBatch::BatchItem &item, uint32_t remaining_size,
                                          bool batch_first) {
  this->flags_.batch_first_message = batch_first;
  this->batch_message_type_ = item.message_type;
#ifdef USE_EVENT
  // Events need aux_data_index to look up event type from entity
  if (item.message_type == EventResponse::MESSAGE_TYPE) {
    // Skip if aux_data_index is invalid (should never happen in normal operation)
    if (item.aux_data_index == DeferredBatch::AUX_DATA_UNUSED)
      return 0;
    auto *event = static_cast<event::Event *>(item.entity);
    return try_send_event_response(event, StringRef::from_maybe_nullptr(event->get_event_type(item.aux_data_index)),
                                   this, remaining_size);
  }
#endif

  // All other message types use function pointer lookup via switch
  MessageCreatorPtr func = nullptr;

// Macros to reduce repetitive switch cases
#define CASE_STATE_INFO(entity_name, StateResp, InfoResp) \
  case StateResp::MESSAGE_TYPE: \
    func = &try_send_##entity_name##_state; \
    break; \
  case InfoResp::MESSAGE_TYPE: \
    func = &try_send_##entity_name##_info; \
    break;
#define CASE_INFO_ONLY(entity_name, InfoResp) \
  case InfoResp::MESSAGE_TYPE: \
    func = &try_send_##entity_name##_info; \
    break;

  switch (item.message_type) {
#ifdef USE_BINARY_SENSOR
    CASE_STATE_INFO(binary_sensor, BinarySensorStateResponse, ListEntitiesBinarySensorResponse)
#endif
#ifdef USE_COVER
    CASE_STATE_INFO(cover, CoverStateResponse, ListEntitiesCoverResponse)
#endif
#ifdef USE_FAN
    CASE_STATE_INFO(fan, FanStateResponse, ListEntitiesFanResponse)
#endif
#ifdef USE_LIGHT
    CASE_STATE_INFO(light, LightStateResponse, ListEntitiesLightResponse)
#endif
#ifdef USE_SENSOR
    CASE_STATE_INFO(sensor, SensorStateResponse, ListEntitiesSensorResponse)
#endif
#ifdef USE_SWITCH
    CASE_STATE_INFO(switch, SwitchStateResponse, ListEntitiesSwitchResponse)
#endif
#ifdef USE_BUTTON
    CASE_INFO_ONLY(button, ListEntitiesButtonResponse)
#endif
#ifdef USE_TEXT_SENSOR
    CASE_STATE_INFO(text_sensor, TextSensorStateResponse, ListEntitiesTextSensorResponse)
#endif
#ifdef USE_CLIMATE
    CASE_STATE_INFO(climate, ClimateStateResponse, ListEntitiesClimateResponse)
#endif
#ifdef USE_NUMBER
    CASE_STATE_INFO(number, NumberStateResponse, ListEntitiesNumberResponse)
#endif
#ifdef USE_DATETIME_DATE
    CASE_STATE_INFO(date, DateStateResponse, ListEntitiesDateResponse)
#endif
#ifdef USE_DATETIME_TIME
    CASE_STATE_INFO(time, TimeStateResponse, ListEntitiesTimeResponse)
#endif
#ifdef USE_DATETIME_DATETIME
    CASE_STATE_INFO(datetime, DateTimeStateResponse, ListEntitiesDateTimeResponse)
#endif
#ifdef USE_TEXT
    CASE_STATE_INFO(text, TextStateResponse, ListEntitiesTextResponse)
#endif
#ifdef USE_SELECT
    CASE_STATE_INFO(select, SelectStateResponse, ListEntitiesSelectResponse)
#endif
#ifdef USE_LOCK
    CASE_STATE_INFO(lock, LockStateResponse, ListEntitiesLockResponse)
#endif
#ifdef USE_VALVE
    CASE_STATE_INFO(valve, ValveStateResponse, ListEntitiesValveResponse)
#endif
#ifdef USE_MEDIA_PLAYER
    CASE_STATE_INFO(media_player, MediaPlayerStateResponse, ListEntitiesMediaPlayerResponse)
#endif
#ifdef USE_ALARM_CONTROL_PANEL
    CASE_STATE_INFO(alarm_control_panel, AlarmControlPanelStateResponse, ListEntitiesAlarmControlPanelResponse)
#endif
#ifdef USE_WATER_HEATER
    CASE_STATE_INFO(water_heater, WaterHeaterStateResponse, ListEntitiesWaterHeaterResponse)
#endif
#ifdef USE_CAMERA
    CASE_INFO_ONLY(camera, ListEntitiesCameraResponse)
#endif
#ifdef USE_INFRARED
    CASE_INFO_ONLY(infrared, ListEntitiesInfraredResponse)
#endif
#ifdef USE_RADIO_FREQUENCY
    CASE_INFO_ONLY(radio_frequency, ListEntitiesRadioFrequencyResponse)
#endif
#ifdef USE_EVENT
    CASE_INFO_ONLY(event, ListEntitiesEventResponse)
#endif
#ifdef USE_UPDATE
    CASE_STATE_INFO(update, UpdateStateResponse, ListEntitiesUpdateResponse)
#endif
    // Special messages (not entity state/info)
    case ListEntitiesDoneResponse::MESSAGE_TYPE:
      func = &try_send_list_info_done;
      break;
    case DisconnectRequest::MESSAGE_TYPE:
      func = &try_send_disconnect_request;
      break;
    case PingRequest::MESSAGE_TYPE:
      func = &try_send_ping_request;
      break;
    default:
      return 0;
  }

#undef CASE_STATE_INFO
#undef CASE_INFO_ONLY

  return func(item.entity, this, remaining_size);
}

uint16_t APIConnection::try_send_list_info_done(EntityBase *entity, APIConnection *conn, uint32_t remaining_size) {
  ListEntitiesDoneResponse resp;
  return encode_message_to_buffer(resp, conn, remaining_size);
}

uint16_t APIConnection::try_send_disconnect_request(EntityBase *entity, APIConnection *conn, uint32_t remaining_size) {
  DisconnectRequest req;
  return encode_message_to_buffer(req, conn, remaining_size);
}

uint16_t APIConnection::try_send_ping_request(EntityBase *entity, APIConnection *conn, uint32_t remaining_size) {
  PingRequest req;
  return encode_message_to_buffer(req, conn, remaining_size);
}

#ifdef USE_API_HOMEASSISTANT_STATES
void APIConnection::process_state_subscriptions_() {
  const auto &subs = this->parent_->get_state_subs();
  if (this->state_subs_at_ >= static_cast<int>(subs.size())) {
    this->state_subs_at_ = -1;
    return;
  }

  const auto &it = subs[this->state_subs_at_];
  SubscribeHomeAssistantStateResponse resp;
  resp.entity_id = StringRef(it.entity_id);

  // Avoid string copy by using the const char* pointer if it exists
  resp.attribute = it.attribute != nullptr ? StringRef(it.attribute) : StringRef("");

  resp.once = it.once;
  if (this->send_message(resp)) {
    this->state_subs_at_++;
  }
}
#endif  // USE_API_HOMEASSISTANT_STATES

void APIConnection::log_client_(int level, const LogString *message) {
  char peername[socket::SOCKADDR_STR_LEN];
  esp_log_printf_(level, TAG, __LINE__, ESPHOME_LOG_FORMAT("%s (%s): %s"), this->helper_->get_client_name(),
                  this->helper_->get_peername_to(peername), LOG_STR_ARG(message));
}

void APIConnection::log_warning_(const LogString *message, APIError err) {
  char peername[socket::SOCKADDR_STR_LEN];
  ESP_LOGW(TAG, "%s (%s): %s %s errno=%d", this->helper_->get_client_name(), this->helper_->get_peername_to(peername),
           LOG_STR_ARG(message), LOG_STR_ARG(api_error_to_logstr(err)), errno);
}

}  // namespace esphome::api
#endif
