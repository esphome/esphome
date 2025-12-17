#include "api_server.h"
#ifdef USE_API
#include <cerrno>
#include "api_connection.h"
#include "esphome/components/network/util.h"
#include "esphome/core/application.h"
#include "esphome/core/controller_registry.h"
#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/core/util.h"
#include "esphome/core/version.h"
#ifdef USE_API_HOMEASSISTANT_SERVICES
#include "homeassistant_service.h"
#endif

#ifdef USE_LOGGER
#include "esphome/components/logger/logger.h"
#endif

#include <algorithm>
#include <utility>

namespace esphome::api {

static const char *const TAG = "api";

// APIServer
APIServer *global_api_server = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

APIServer::APIServer() {
  global_api_server = this;
  // Pre-allocate shared write buffer
  shared_write_buffer_.reserve(64);
}

void APIServer::setup() {
  ControllerRegistry::register_controller(this);

#ifdef USE_API_NOISE
  uint32_t hash = 88491486UL;

  this->noise_pref_ = global_preferences->make_preference<SavedNoisePsk>(hash, true);

#ifndef USE_API_NOISE_PSK_FROM_YAML
  // Only load saved PSK if not set from YAML
  SavedNoisePsk noise_pref_saved{};
  if (this->noise_pref_.load(&noise_pref_saved)) {
    ESP_LOGD(TAG, "Loaded saved Noise PSK");
    this->set_noise_psk(noise_pref_saved.psk);
  }
#endif
#endif

  this->socket_ = socket::socket_ip_loop_monitored(SOCK_STREAM, 0);  // monitored for incoming connections
  if (this->socket_ == nullptr) {
    ESP_LOGW(TAG, "Could not create socket");
    this->mark_failed();
    return;
  }
  int enable = 1;
  int err = this->socket_->setsockopt(SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
  if (err != 0) {
    ESP_LOGW(TAG, "Socket unable to set reuseaddr: errno %d", err);
    // we can still continue
  }
  err = this->socket_->setblocking(false);
  if (err != 0) {
    ESP_LOGW(TAG, "Socket unable to set nonblocking mode: errno %d", err);
    this->mark_failed();
    return;
  }

  struct sockaddr_storage server;

  socklen_t sl = socket::set_sockaddr_any((struct sockaddr *) &server, sizeof(server), this->port_);
  if (sl == 0) {
    ESP_LOGW(TAG, "Socket unable to set sockaddr: errno %d", errno);
    this->mark_failed();
    return;
  }

  err = this->socket_->bind((struct sockaddr *) &server, sl);
  if (err != 0) {
    ESP_LOGW(TAG, "Socket unable to bind: errno %d", errno);
    this->mark_failed();
    return;
  }

  err = this->socket_->listen(this->listen_backlog_);
  if (err != 0) {
    ESP_LOGW(TAG, "Socket unable to listen: errno %d", errno);
    this->mark_failed();
    return;
  }

#ifdef USE_LOGGER
  if (logger::global_logger != nullptr) {
    logger::global_logger->add_log_listener(this);
  }
#endif

#ifdef USE_CAMERA
  if (camera::Camera::instance() != nullptr && !camera::Camera::instance()->is_internal()) {
    camera::Camera::instance()->add_listener(this);
  }
#endif

  // Initialize last_connected_ for reboot timeout tracking
  this->last_connected_ = App.get_loop_component_start_time();
  // Set warning status if reboot timeout is enabled
  if (this->reboot_timeout_ != 0) {
    this->status_set_warning();
  }
}

void APIServer::loop() {
  // Accept new clients only if the socket exists and has incoming connections
  if (this->socket_ && this->socket_->ready()) {
    while (true) {
      struct sockaddr_storage source_addr;
      socklen_t addr_len = sizeof(source_addr);

      auto sock = this->socket_->accept_loop_monitored((struct sockaddr *) &source_addr, &addr_len);
      if (!sock)
        break;

      // Check if we're at the connection limit
      if (this->clients_.size() >= this->max_connections_) {
        ESP_LOGW(TAG, "Max connections (%d), rejecting %s", this->max_connections_, sock->getpeername().c_str());
        // Immediately close - socket destructor will handle cleanup
        sock.reset();
        continue;
      }

      ESP_LOGD(TAG, "Accept %s", sock->getpeername().c_str());

      auto *conn = new APIConnection(std::move(sock), this);
      this->clients_.emplace_back(conn);
      conn->start();

      // First client connected - clear warning and update timestamp
      if (this->clients_.size() == 1 && this->reboot_timeout_ != 0) {
        this->status_clear_warning();
        this->last_connected_ = App.get_loop_component_start_time();
      }
    }
  }

  if (this->clients_.empty()) {
    // Check reboot timeout - done in loop to avoid scheduler heap churn
    // (cancelled scheduler items sit in heap memory until their scheduled time)
    if (this->reboot_timeout_ != 0) {
      const uint32_t now = App.get_loop_component_start_time();
      if (now - this->last_connected_ > this->reboot_timeout_) {
        ESP_LOGE(TAG, "No clients; rebooting");
        App.reboot();
      }
    }
    return;
  }

  // Process clients and remove disconnected ones in a single pass
  // Check network connectivity once for all clients
  if (!network::is_connected()) {
    // Network is down - disconnect all clients
    for (auto &client : this->clients_) {
      client->on_fatal_error();
      ESP_LOGW(TAG, "%s (%s): Network down; disconnect", client->client_info_.name.c_str(),
               client->client_info_.peername.c_str());
    }
    // Continue to process and clean up the clients below
  }

  size_t client_index = 0;
  while (client_index < this->clients_.size()) {
    auto &client = this->clients_[client_index];

    if (!client->flags_.remove) {
      // Common case: process active client
      client->loop();
      client_index++;
      continue;
    }

    // Rare case: handle disconnection
#ifdef USE_API_CLIENT_DISCONNECTED_TRIGGER
    this->client_disconnected_trigger_->trigger(client->client_info_.name, client->client_info_.peername);
#endif
#ifdef USE_API_USER_DEFINED_ACTION_RESPONSES
    this->unregister_active_action_calls_for_connection(client.get());
#endif
    ESP_LOGV(TAG, "Remove connection %s", client->client_info_.name.c_str());

    // Swap with the last element and pop (avoids expensive vector shifts)
    if (client_index < this->clients_.size() - 1) {
      std::swap(this->clients_[client_index], this->clients_.back());
    }
    this->clients_.pop_back();

    // Last client disconnected - set warning and start tracking for reboot timeout
    if (this->clients_.empty() && this->reboot_timeout_ != 0) {
      this->status_set_warning();
      this->last_connected_ = App.get_loop_component_start_time();
    }
    // Don't increment client_index since we need to process the swapped element
  }
}

void APIServer::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Server:\n"
                "  Address: %s:%u\n"
                "  Listen backlog: %u\n"
                "  Max connections: %u",
                network::get_use_address(), this->port_, this->listen_backlog_, this->max_connections_);
#ifdef USE_API_NOISE
  ESP_LOGCONFIG(TAG, "  Noise encryption: %s", YESNO(this->noise_ctx_.has_psk()));
  if (!this->noise_ctx_.has_psk()) {
    ESP_LOGCONFIG(TAG, "  Supports encryption: YES");
  }
#else
  ESP_LOGCONFIG(TAG, "  Noise encryption: NO");
#endif
}

#ifdef USE_API_PASSWORD
bool APIServer::check_password(const uint8_t *password_data, size_t password_len) const {
  // depend only on input password length
  const char *a = this->password_.c_str();
  uint32_t len_a = this->password_.length();
  const char *b = reinterpret_cast<const char *>(password_data);
  uint32_t len_b = password_len;

  // disable optimization with volatile
  volatile uint32_t length = len_b;
  volatile const char *left = nullptr;
  volatile const char *right = b;
  uint8_t result = 0;

  if (len_a == length) {
    left = *((volatile const char **) &a);
    result = 0;
  }
  if (len_a != length) {
    left = b;
    result = 1;
  }

  for (size_t i = 0; i < length; i++) {
    result |= *left++ ^ *right++;  // NOLINT
  }

  return result == 0;
}

#endif

void APIServer::handle_disconnect(APIConnection *conn) {}

// Macro for controller update dispatch
#define API_DISPATCH_UPDATE(entity_type, entity_name) \
  void APIServer::on_##entity_name##_update(entity_type *obj) { /* NOLINT(bugprone-macro-parentheses) */ \
    if (obj->is_internal()) \
      return; \
    for (auto &c : this->clients_) \
      c->send_##entity_name##_state(obj); \
  }

#ifdef USE_BINARY_SENSOR
API_DISPATCH_UPDATE(binary_sensor::BinarySensor, binary_sensor)
#endif

#ifdef USE_COVER
API_DISPATCH_UPDATE(cover::Cover, cover)
#endif

#ifdef USE_FAN
API_DISPATCH_UPDATE(fan::Fan, fan)
#endif

#ifdef USE_LIGHT
API_DISPATCH_UPDATE(light::LightState, light)
#endif

#ifdef USE_SENSOR
API_DISPATCH_UPDATE(sensor::Sensor, sensor)
#endif

#ifdef USE_SWITCH
API_DISPATCH_UPDATE(switch_::Switch, switch)
#endif

#ifdef USE_TEXT_SENSOR
API_DISPATCH_UPDATE(text_sensor::TextSensor, text_sensor)
#endif

#ifdef USE_CLIMATE
API_DISPATCH_UPDATE(climate::Climate, climate)
#endif

#ifdef USE_NUMBER
API_DISPATCH_UPDATE(number::Number, number)
#endif

#ifdef USE_DATETIME_DATE
API_DISPATCH_UPDATE(datetime::DateEntity, date)
#endif

#ifdef USE_DATETIME_TIME
API_DISPATCH_UPDATE(datetime::TimeEntity, time)
#endif

#ifdef USE_DATETIME_DATETIME
API_DISPATCH_UPDATE(datetime::DateTimeEntity, datetime)
#endif

#ifdef USE_TEXT
API_DISPATCH_UPDATE(text::Text, text)
#endif

#ifdef USE_SELECT
API_DISPATCH_UPDATE(select::Select, select)
#endif

#ifdef USE_LOCK
API_DISPATCH_UPDATE(lock::Lock, lock)
#endif

#ifdef USE_VALVE
API_DISPATCH_UPDATE(valve::Valve, valve)
#endif

#ifdef USE_MEDIA_PLAYER
API_DISPATCH_UPDATE(media_player::MediaPlayer, media_player)
#endif

#ifdef USE_EVENT
// Event is a special case - unlike other entities with simple state fields,
// events store their state in a member accessed via obj->get_last_event_type()
void APIServer::on_event(event::Event *obj) {
  if (obj->is_internal())
    return;
  for (auto &c : this->clients_)
    c->send_event(obj, obj->get_last_event_type());
}
#endif

#ifdef USE_UPDATE
// Update is a special case - the method is called on_update, not on_update_update
void APIServer::on_update(update::UpdateEntity *obj) {
  if (obj->is_internal())
    return;
  for (auto &c : this->clients_)
    c->send_update_state(obj);
}
#endif

#ifdef USE_ZWAVE_PROXY
void APIServer::on_zwave_proxy_request(const esphome::api::ProtoMessage &msg) {
  // We could add code to manage a second subscription type, but, since this message type is
  //  very infrequent and small, we simply send it to all clients
  for (auto &c : this->clients_)
    c->send_message(msg, api::ZWaveProxyRequest::MESSAGE_TYPE);
}
#endif

#ifdef USE_ALARM_CONTROL_PANEL
API_DISPATCH_UPDATE(alarm_control_panel::AlarmControlPanel, alarm_control_panel)
#endif

float APIServer::get_setup_priority() const { return setup_priority::AFTER_WIFI; }

void APIServer::set_port(uint16_t port) { this->port_ = port; }

#ifdef USE_API_PASSWORD
void APIServer::set_password(const std::string &password) { this->password_ = password; }
#endif

void APIServer::set_batch_delay(uint16_t batch_delay) { this->batch_delay_ = batch_delay; }

#ifdef USE_API_HOMEASSISTANT_SERVICES
void APIServer::send_homeassistant_action(const HomeassistantActionRequest &call) {
  for (auto &client : this->clients_) {
    client->send_homeassistant_action(call);
  }
}
#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES
void APIServer::register_action_response_callback(uint32_t call_id, ActionResponseCallback callback) {
  this->action_response_callbacks_.push_back({call_id, std::move(callback)});
}

void APIServer::handle_action_response(uint32_t call_id, bool success, const std::string &error_message) {
  for (auto it = this->action_response_callbacks_.begin(); it != this->action_response_callbacks_.end(); ++it) {
    if (it->call_id == call_id) {
      auto callback = std::move(it->callback);
      this->action_response_callbacks_.erase(it);
      ActionResponse response(success, error_message);
      callback(response);
      return;
    }
  }
}
#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES_JSON
void APIServer::handle_action_response(uint32_t call_id, bool success, const std::string &error_message,
                                       const uint8_t *response_data, size_t response_data_len) {
  for (auto it = this->action_response_callbacks_.begin(); it != this->action_response_callbacks_.end(); ++it) {
    if (it->call_id == call_id) {
      auto callback = std::move(it->callback);
      this->action_response_callbacks_.erase(it);
      ActionResponse response(success, error_message, response_data, response_data_len);
      callback(response);
      return;
    }
  }
}
#endif  // USE_API_HOMEASSISTANT_ACTION_RESPONSES_JSON
#endif  // USE_API_HOMEASSISTANT_ACTION_RESPONSES
#endif  // USE_API_HOMEASSISTANT_SERVICES

#ifdef USE_API_HOMEASSISTANT_STATES
// Helper to add subscription (reduces duplication)
void APIServer::add_state_subscription_(const char *entity_id, const char *attribute,
                                        std::function<void(std::string)> f, bool once) {
  this->state_subs_.push_back(HomeAssistantStateSubscription{
      .entity_id = entity_id, .attribute = attribute, .callback = std::move(f), .once = once,
      // entity_id_dynamic_storage and attribute_dynamic_storage remain nullptr (no heap allocation)
  });
}

// Helper to add subscription with heap-allocated strings (reduces duplication)
void APIServer::add_state_subscription_(std::string entity_id, optional<std::string> attribute,
                                        std::function<void(std::string)> f, bool once) {
  HomeAssistantStateSubscription sub;
  // Allocate heap storage for the strings
  sub.entity_id_dynamic_storage = std::make_unique<std::string>(std::move(entity_id));
  sub.entity_id = sub.entity_id_dynamic_storage->c_str();

  if (attribute.has_value()) {
    sub.attribute_dynamic_storage = std::make_unique<std::string>(std::move(attribute.value()));
    sub.attribute = sub.attribute_dynamic_storage->c_str();
  } else {
    sub.attribute = nullptr;
  }

  sub.callback = std::move(f);
  sub.once = once;
  this->state_subs_.push_back(std::move(sub));
}

// New const char* overload (for internal components - zero allocation)
void APIServer::subscribe_home_assistant_state(const char *entity_id, const char *attribute,
                                               std::function<void(std::string)> f) {
  this->add_state_subscription_(entity_id, attribute, std::move(f), false);
}

void APIServer::get_home_assistant_state(const char *entity_id, const char *attribute,
                                         std::function<void(std::string)> f) {
  this->add_state_subscription_(entity_id, attribute, std::move(f), true);
}

// Existing std::string overload (for custom_api_device.h - heap allocation)
void APIServer::subscribe_home_assistant_state(std::string entity_id, optional<std::string> attribute,
                                               std::function<void(std::string)> f) {
  this->add_state_subscription_(std::move(entity_id), std::move(attribute), std::move(f), false);
}

void APIServer::get_home_assistant_state(std::string entity_id, optional<std::string> attribute,
                                         std::function<void(std::string)> f) {
  this->add_state_subscription_(std::move(entity_id), std::move(attribute), std::move(f), true);
}

const std::vector<APIServer::HomeAssistantStateSubscription> &APIServer::get_state_subs() const {
  return this->state_subs_;
}
#endif

uint16_t APIServer::get_port() const { return this->port_; }

void APIServer::set_reboot_timeout(uint32_t reboot_timeout) { this->reboot_timeout_ = reboot_timeout; }

#ifdef USE_API_NOISE
bool APIServer::update_noise_psk_(const SavedNoisePsk &new_psk, const LogString *save_log_msg,
                                  const LogString *fail_log_msg, const psk_t &active_psk, bool make_active) {
  if (!this->noise_pref_.save(&new_psk)) {
    ESP_LOGW(TAG, "%s", LOG_STR_ARG(fail_log_msg));
    return false;
  }
  // ensure it's written immediately
  if (!global_preferences->sync()) {
    ESP_LOGW(TAG, "Failed to sync preferences");
    return false;
  }
  ESP_LOGD(TAG, "%s", LOG_STR_ARG(save_log_msg));
  if (make_active) {
    this->set_timeout(100, [this, active_psk]() {
      ESP_LOGW(TAG, "Disconnecting all clients to reset PSK");
      this->set_noise_psk(active_psk);
      for (auto &c : this->clients_) {
        DisconnectRequest req;
        c->send_message(req, DisconnectRequest::MESSAGE_TYPE);
      }
    });
  }
  return true;
}

bool APIServer::save_noise_psk(psk_t psk, bool make_active) {
#ifdef USE_API_NOISE_PSK_FROM_YAML
  // When PSK is set from YAML, this function should never be called
  // but if it is, reject the change
  ESP_LOGW(TAG, "Key set in YAML");
  return false;
#else
  auto &old_psk = this->noise_ctx_.get_psk();
  if (std::equal(old_psk.begin(), old_psk.end(), psk.begin())) {
    ESP_LOGW(TAG, "New PSK matches old");
    return true;
  }

  SavedNoisePsk new_saved_psk{psk};
  return this->update_noise_psk_(new_saved_psk, LOG_STR("Noise PSK saved"), LOG_STR("Failed to save Noise PSK"), psk,
                                 make_active);
#endif
}
bool APIServer::clear_noise_psk(bool make_active) {
#ifdef USE_API_NOISE_PSK_FROM_YAML
  // When PSK is set from YAML, this function should never be called
  // but if it is, reject the change
  ESP_LOGW(TAG, "Key set in YAML");
  return false;
#else
  SavedNoisePsk empty_psk{};
  psk_t empty{};
  return this->update_noise_psk_(empty_psk, LOG_STR("Noise PSK cleared"), LOG_STR("Failed to clear Noise PSK"), empty,
                                 make_active);
#endif
}
#endif

#ifdef USE_HOMEASSISTANT_TIME
void APIServer::request_time() {
  for (auto &client : this->clients_) {
    if (!client->flags_.remove && client->is_authenticated())
      client->send_time_request();
  }
}
#endif

bool APIServer::is_connected(bool state_subscription_only) const {
  if (!state_subscription_only) {
    return !this->clients_.empty();
  }

  for (const auto &client : this->clients_) {
    if (client->flags_.state_subscription) {
      return true;
    }
  }
  return false;
}

#ifdef USE_LOGGER
void APIServer::on_log(uint8_t level, const char *tag, const char *message, size_t message_len) {
  if (this->shutting_down_) {
    // Don't try to send logs during shutdown
    // as it could result in a recursion and
    // we would be filling a buffer we are trying to clear
    return;
  }
  for (auto &c : this->clients_) {
    if (!c->flags_.remove && c->get_log_subscription_level() >= level)
      c->try_send_log_message(level, tag, message, message_len);
  }
}
#endif

#ifdef USE_CAMERA
void APIServer::on_camera_image(const std::shared_ptr<camera::CameraImage> &image) {
  for (auto &c : this->clients_) {
    if (!c->flags_.remove)
      c->set_camera_state(image);
  }
}
#endif

void APIServer::on_shutdown() {
  this->shutting_down_ = true;

  // Close the listening socket to prevent new connections
  if (this->socket_) {
    this->socket_->close();
    this->socket_ = nullptr;
  }

  // Change batch delay to 5ms for quick flushing during shutdown
  this->batch_delay_ = 5;

  // Send disconnect requests to all connected clients
  for (auto &c : this->clients_) {
    DisconnectRequest req;
    if (!c->send_message(req, DisconnectRequest::MESSAGE_TYPE)) {
      // If we can't send the disconnect request directly (tx_buffer full),
      // schedule it at the front of the batch so it will be sent with priority
      c->schedule_message_front_(nullptr, &APIConnection::try_send_disconnect_request, DisconnectRequest::MESSAGE_TYPE,
                                 DisconnectRequest::ESTIMATED_SIZE);
    }
  }
}

bool APIServer::teardown() {
  // If network is disconnected, no point trying to flush buffers
  if (!network::is_connected()) {
    return true;
  }
  this->loop();

  // Return true only when all clients have been torn down
  return this->clients_.empty();
}

#ifdef USE_API_USER_DEFINED_ACTION_RESPONSES
// Timeout for action calls - matches aioesphomeapi client timeout (default 30s)
// Can be overridden via USE_API_ACTION_CALL_TIMEOUT_MS define for testing
#ifndef USE_API_ACTION_CALL_TIMEOUT_MS
#define USE_API_ACTION_CALL_TIMEOUT_MS 30000  // NOLINT
#endif

uint32_t APIServer::register_active_action_call(uint32_t client_call_id, APIConnection *conn) {
  uint32_t action_call_id = this->next_action_call_id_++;
  // Handle wraparound (skip 0 as it means "no call")
  if (this->next_action_call_id_ == 0) {
    this->next_action_call_id_ = 1;
  }
  this->active_action_calls_.push_back({action_call_id, client_call_id, conn});

  // Schedule automatic cleanup after timeout (client will have given up by then)
  this->set_timeout(str_sprintf("action_call_%u", action_call_id), USE_API_ACTION_CALL_TIMEOUT_MS,
                    [this, action_call_id]() {
                      ESP_LOGD(TAG, "Action call %u timed out", action_call_id);
                      this->unregister_active_action_call(action_call_id);
                    });

  return action_call_id;
}

void APIServer::unregister_active_action_call(uint32_t action_call_id) {
  // Cancel the timeout for this action call
  this->cancel_timeout(str_sprintf("action_call_%u", action_call_id));

  // Swap-and-pop is more efficient than remove_if for unordered vectors
  for (size_t i = 0; i < this->active_action_calls_.size(); i++) {
    if (this->active_action_calls_[i].action_call_id == action_call_id) {
      std::swap(this->active_action_calls_[i], this->active_action_calls_.back());
      this->active_action_calls_.pop_back();
      return;
    }
  }
}

void APIServer::unregister_active_action_calls_for_connection(APIConnection *conn) {
  // Remove all active action calls for disconnected connection using swap-and-pop
  for (size_t i = 0; i < this->active_action_calls_.size();) {
    if (this->active_action_calls_[i].connection == conn) {
      // Cancel the timeout for this action call
      this->cancel_timeout(str_sprintf("action_call_%u", this->active_action_calls_[i].action_call_id));

      std::swap(this->active_action_calls_[i], this->active_action_calls_.back());
      this->active_action_calls_.pop_back();
      // Don't increment i - need to check the swapped element
    } else {
      i++;
    }
  }
}

void APIServer::send_action_response(uint32_t action_call_id, bool success, const std::string &error_message) {
  for (auto &call : this->active_action_calls_) {
    if (call.action_call_id == action_call_id) {
      call.connection->send_execute_service_response(call.client_call_id, success, error_message);
      return;
    }
  }
  ESP_LOGW(TAG, "Cannot send response: no active call found for action_call_id %u", action_call_id);
}
#ifdef USE_API_USER_DEFINED_ACTION_RESPONSES_JSON
void APIServer::send_action_response(uint32_t action_call_id, bool success, const std::string &error_message,
                                     const uint8_t *response_data, size_t response_data_len) {
  for (auto &call : this->active_action_calls_) {
    if (call.action_call_id == action_call_id) {
      call.connection->send_execute_service_response(call.client_call_id, success, error_message, response_data,
                                                     response_data_len);
      return;
    }
  }
  ESP_LOGW(TAG, "Cannot send response: no active call found for action_call_id %u", action_call_id);
}
#endif  // USE_API_USER_DEFINED_ACTION_RESPONSES_JSON
#endif  // USE_API_USER_DEFINED_ACTION_RESPONSES

}  // namespace esphome::api
#endif
