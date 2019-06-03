#include <utility>

#include "api_server.h"
#include "basic_messages.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/util.h"
#include "esphome/core/defines.h"
#include "esphome/core/version.h"

#ifdef USE_DEEP_SLEEP
#include "esphome/components/deep_sleep/deep_sleep_component.h"
#endif
#ifdef USE_HOMEASSISTANT_TIME
#include "esphome/components/homeassistant/time/homeassistant_time.h"
#endif
#ifdef USE_LOGGER
#include "esphome/components/logger/logger.h"
#endif

#include <algorithm>

namespace esphome {
namespace api {

static const char *TAG = "api";

// APIServer
void APIServer::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Home Assistant API server...");
  this->setup_controller();
  this->server_ = AsyncServer(this->port_);
  this->server_.setNoDelay(false);
  this->server_.begin();
  this->server_.onClient(
      [](void *s, AsyncClient *client) {
        if (client == nullptr)
          return;

        // can't print here because in lwIP thread
        // ESP_LOGD(TAG, "New client connected from %s", client->remoteIP().toString().c_str());
        auto *a_this = (APIServer *) s;
        a_this->clients_.push_back(new APIConnection(client, a_this));
      },
      this);
#ifdef USE_LOGGER
  if (logger::global_logger != nullptr) {
    logger::global_logger->add_on_log_callback([this](int level, const char *tag, const char *message) {
      for (auto *c : this->clients_) {
        if (!c->remove_)
          c->send_log_message(level, tag, message);
      }
    });
  }
#endif

  this->last_connected_ = millis();

#ifdef USE_ESP32_CAMERA
  if (esp32_camera::global_esp32_camera != nullptr) {
    esp32_camera::global_esp32_camera->add_image_callback([this](std::shared_ptr<esp32_camera::CameraImage> image) {
      for (auto *c : this->clients_)
        if (!c->remove_)
          c->send_camera_state(image);
    });
  }
#endif
}
void APIServer::loop() {
  // Partition clients into remove and active
  auto new_end =
      std::partition(this->clients_.begin(), this->clients_.end(), [](APIConnection *conn) { return !conn->remove_; });
  // print disconnection messages
  for (auto it = new_end; it != this->clients_.end(); ++it) {
    ESP_LOGD(TAG, "Disconnecting %s", (*it)->client_info_.c_str());
  }
  // only then delete the pointers, otherwise log routine
  // would access freed memory
  for (auto it = new_end; it != this->clients_.end(); ++it)
    delete *it;
  // resize vector
  this->clients_.erase(new_end, this->clients_.end());

  for (auto *client : this->clients_) {
    client->loop();
  }

  if (this->reboot_timeout_ != 0) {
    const uint32_t now = millis();
    if (!this->is_connected()) {
      if (now - this->last_connected_ > this->reboot_timeout_) {
        ESP_LOGE(TAG, "No client connected to API. Rebooting...");
        App.reboot();
      }
      this->status_set_warning();
    } else {
      this->last_connected_ = now;
      this->status_clear_warning();
    }
  }
}
void APIServer::dump_config() {
  ESP_LOGCONFIG(TAG, "API Server:");
  ESP_LOGCONFIG(TAG, "  Address: %s:%u", network_get_address().c_str(), this->port_);
}
bool APIServer::uses_password() const { return !this->password_.empty(); }
bool APIServer::check_password(const std::string &password) const {
  // depend only on input password length
  const char *a = this->password_.c_str();
  uint32_t len_a = this->password_.length();
  const char *b = password.c_str();
  uint32_t len_b = password.length();

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
void APIServer::handle_disconnect(APIConnection *conn) {}
#ifdef USE_BINARY_SENSOR
void APIServer::on_binary_sensor_update(binary_sensor::BinarySensor *obj, bool state) {
  if (obj->is_internal())
    return;
  for (auto *c : this->clients_)
    c->send_binary_sensor_state(obj, state);
}
#endif

#ifdef USE_COVER
void APIServer::on_cover_update(cover::Cover *obj) {
  if (obj->is_internal())
    return;
  for (auto *c : this->clients_)
    c->send_cover_state(obj);
}
#endif

#ifdef USE_FAN
void APIServer::on_fan_update(fan::FanState *obj) {
  if (obj->is_internal())
    return;
  for (auto *c : this->clients_)
    c->send_fan_state(obj);
}
#endif

#ifdef USE_LIGHT
void APIServer::on_light_update(light::LightState *obj) {
  if (obj->is_internal())
    return;
  for (auto *c : this->clients_)
    c->send_light_state(obj);
}
#endif

#ifdef USE_SENSOR
void APIServer::on_sensor_update(sensor::Sensor *obj, float state) {
  if (obj->is_internal())
    return;
  for (auto *c : this->clients_)
    c->send_sensor_state(obj, state);
}
#endif

#ifdef USE_SWITCH
void APIServer::on_switch_update(switch_::Switch *obj, bool state) {
  if (obj->is_internal())
    return;
  for (auto *c : this->clients_)
    c->send_switch_state(obj, state);
}
#endif

#ifdef USE_TEXT_SENSOR
void APIServer::on_text_sensor_update(text_sensor::TextSensor *obj, std::string state) {
  if (obj->is_internal())
    return;
  for (auto *c : this->clients_)
    c->send_text_sensor_state(obj, state);
}
#endif

#ifdef USE_CLIMATE
void APIServer::on_climate_update(climate::Climate *obj) {
  if (obj->is_internal())
    return;
  for (auto *c : this->clients_)
    c->send_climate_state(obj);
}
#endif

float APIServer::get_setup_priority() const { return setup_priority::AFTER_WIFI; }
void APIServer::set_port(uint16_t port) { this->port_ = port; }
APIServer *global_api_server = nullptr;

void APIServer::set_password(const std::string &password) { this->password_ = password; }
void APIServer::send_service_call(ServiceCallResponse &call) {
  for (auto *client : this->clients_) {
    client->send_service_call(call);
  }
}
APIServer::APIServer() { global_api_server = this; }
void APIServer::subscribe_home_assistant_state(std::string entity_id, std::function<void(std::string)> f) {
  this->state_subs_.push_back(HomeAssistantStateSubscription{
      .entity_id = std::move(entity_id),
      .callback = std::move(f),
  });
}
const std::vector<APIServer::HomeAssistantStateSubscription> &APIServer::get_state_subs() const {
  return this->state_subs_;
}
uint16_t APIServer::get_port() const { return this->port_; }
void APIServer::set_reboot_timeout(uint32_t reboot_timeout) { this->reboot_timeout_ = reboot_timeout; }
#ifdef USE_HOMEASSISTANT_TIME
void APIServer::request_time() {
  for (auto *client : this->clients_) {
    if (!client->remove_ && client->connection_state_ == APIConnection::ConnectionState::CONNECTED)
      client->send_time_request();
  }
}
#endif
bool APIServer::is_connected() const { return !this->clients_.empty(); }
void APIServer::on_shutdown() {
  for (auto *c : this->clients_) {
    c->send_disconnect_request();
  }
  delay(10);
}

// APIConnection
APIConnection::APIConnection(AsyncClient *client, APIServer *parent)
    : client_(client), parent_(parent), initial_state_iterator_(parent, this), list_entities_iterator_(parent, this) {
  this->client_->onError([](void *s, AsyncClient *c, int8_t error) { ((APIConnection *) s)->on_error_(error); }, this);
  this->client_->onDisconnect([](void *s, AsyncClient *c) { ((APIConnection *) s)->on_disconnect_(); }, this);
  this->client_->onTimeout([](void *s, AsyncClient *c, uint32_t time) { ((APIConnection *) s)->on_timeout_(time); },
                           this);
  this->client_->onData([](void *s, AsyncClient *c, void *buf,
                           size_t len) { ((APIConnection *) s)->on_data_(reinterpret_cast<uint8_t *>(buf), len); },
                        this);

  this->send_buffer_.reserve(64);
  this->recv_buffer_.reserve(32);
  this->client_info_ = this->client_->remoteIP().toString().c_str();
  this->last_traffic_ = millis();
}
APIConnection::~APIConnection() { delete this->client_; }
void APIConnection::on_error_(int8_t error) {
  // disconnect will also be called, nothing to do here
  this->remove_ = true;
}
void APIConnection::on_disconnect_() {
  // delete self, generally unsafe but not in this case.
  this->remove_ = true;
}
void APIConnection::on_timeout_(uint32_t time) { this->disconnect_client(); }
void APIConnection::on_data_(uint8_t *buf, size_t len) {
  if (len == 0 || buf == nullptr)
    return;

  this->recv_buffer_.insert(this->recv_buffer_.end(), buf, buf + len);
  // TODO: On ESP32, use queue to notify main thread of new data
}
void APIConnection::parse_recv_buffer_() {
  if (this->recv_buffer_.empty() || this->remove_)
    return;

  while (!this->recv_buffer_.empty()) {
    if (this->recv_buffer_[0] != 0x00) {
      ESP_LOGW(TAG, "Invalid preamble from %s", this->client_info_.c_str());
      this->fatal_error_();
      return;
    }
    uint32_t i = 1;
    const uint32_t size = this->recv_buffer_.size();
    uint32_t msg_size = 0;
    while (i < size) {
      const uint8_t dat = this->recv_buffer_[i];
      msg_size |= (dat & 0x7F);
      // consume
      i += 1;
      if ((dat & 0x80) == 0x00) {
        break;
      } else {
        msg_size <<= 7;
      }
    }
    if (i == size)
      // not enough data there yet
      return;

    uint32_t msg_type = 0;
    bool msg_type_done = false;
    while (i < size) {
      const uint8_t dat = this->recv_buffer_[i];
      msg_type |= (dat & 0x7F);
      // consume
      i += 1;
      if ((dat & 0x80) == 0x00) {
        msg_type_done = true;
        break;
      } else {
        msg_type <<= 7;
      }
    }
    if (!msg_type_done)
      // not enough data there yet
      return;

    if (size - i < msg_size)
      // message body not fully received
      return;

    // ESP_LOGVV(TAG, "RECV Message: Size=%u Type=%u", msg_size, msg_type);

    if (!this->valid_rx_message_type_(msg_type)) {
      ESP_LOGE(TAG, "Not a valid message type: %u", msg_type);
      this->fatal_error_();
      return;
    }

    uint8_t *msg = &this->recv_buffer_[i];
    this->read_message_(msg_size, msg_type, msg);
    if (this->remove_)
      return;
    // pop front
    uint32_t total = i + msg_size;
    this->recv_buffer_.erase(this->recv_buffer_.begin(), this->recv_buffer_.begin() + total);
  }
}
void APIConnection::read_message_(uint32_t size, uint32_t type, uint8_t *msg) {
  this->last_traffic_ = millis();

  switch (static_cast<APIMessageType>(type)) {
    case APIMessageType::HELLO_REQUEST: {
      HelloRequest req;
      req.decode(msg, size);
      this->on_hello_request_(req);
      break;
    }
    case APIMessageType::HELLO_RESPONSE: {
      // Invalid
      break;
    }
    case APIMessageType::CONNECT_REQUEST: {
      ConnectRequest req;
      req.decode(msg, size);
      this->on_connect_request_(req);
      break;
    }
    case APIMessageType::CONNECT_RESPONSE:
      // Invalid
      break;
    case APIMessageType::DISCONNECT_REQUEST: {
      DisconnectRequest req;
      req.decode(msg, size);
      this->on_disconnect_request_(req);
      break;
    }
    case APIMessageType::DISCONNECT_RESPONSE: {
      DisconnectResponse req;
      req.decode(msg, size);
      this->on_disconnect_response_(req);
      break;
    }
    case APIMessageType::PING_REQUEST: {
      PingRequest req;
      req.decode(msg, size);
      this->on_ping_request_(req);
      break;
    }
    case APIMessageType::PING_RESPONSE: {
      PingResponse req;
      req.decode(msg, size);
      this->on_ping_response_(req);
      break;
    }
    case APIMessageType::DEVICE_INFO_REQUEST: {
      DeviceInfoRequest req;
      req.decode(msg, size);
      this->on_device_info_request_(req);
      break;
    }
    case APIMessageType::DEVICE_INFO_RESPONSE: {
      // Invalid
      break;
    }
    case APIMessageType::LIST_ENTITIES_REQUEST: {
      ListEntitiesRequest req;
      req.decode(msg, size);
      this->on_list_entities_request_(req);
      break;
    }
    case APIMessageType::LIST_ENTITIES_BINARY_SENSOR_RESPONSE:
    case APIMessageType::LIST_ENTITIES_COVER_RESPONSE:
    case APIMessageType::LIST_ENTITIES_FAN_RESPONSE:
    case APIMessageType::LIST_ENTITIES_LIGHT_RESPONSE:
    case APIMessageType::LIST_ENTITIES_SENSOR_RESPONSE:
    case APIMessageType::LIST_ENTITIES_SWITCH_RESPONSE:
    case APIMessageType::LIST_ENTITIES_TEXT_SENSOR_RESPONSE:
    case APIMessageType::LIST_ENTITIES_SERVICE_RESPONSE:
    case APIMessageType::LIST_ENTITIES_CAMERA_RESPONSE:
    case APIMessageType::LIST_ENTITIES_CLIMATE_RESPONSE:
    case APIMessageType::LIST_ENTITIES_DONE_RESPONSE:
      // Invalid
      break;
    case APIMessageType::SUBSCRIBE_STATES_REQUEST: {
      SubscribeStatesRequest req;
      req.decode(msg, size);
      this->on_subscribe_states_request_(req);
      break;
    }
    case APIMessageType::BINARY_SENSOR_STATE_RESPONSE:
    case APIMessageType::COVER_STATE_RESPONSE:
    case APIMessageType::FAN_STATE_RESPONSE:
    case APIMessageType::LIGHT_STATE_RESPONSE:
    case APIMessageType::SENSOR_STATE_RESPONSE:
    case APIMessageType::SWITCH_STATE_RESPONSE:
    case APIMessageType::TEXT_SENSOR_STATE_RESPONSE:
    case APIMessageType::CAMERA_IMAGE_RESPONSE:
    case APIMessageType::CLIMATE_STATE_RESPONSE:
      // Invalid
      break;
    case APIMessageType::SUBSCRIBE_LOGS_REQUEST: {
      SubscribeLogsRequest req;
      req.decode(msg, size);
      this->on_subscribe_logs_request_(req);
      break;
    }
    case APIMessageType ::SUBSCRIBE_LOGS_RESPONSE:
      // Invalid
      break;
    case APIMessageType::COVER_COMMAND_REQUEST: {
#ifdef USE_COVER
      CoverCommandRequest req;
      req.decode(msg, size);
      this->on_cover_command_request_(req);
#endif
      break;
    }
    case APIMessageType::FAN_COMMAND_REQUEST: {
#ifdef USE_FAN
      FanCommandRequest req;
      req.decode(msg, size);
      this->on_fan_command_request_(req);
#endif
      break;
    }
    case APIMessageType::LIGHT_COMMAND_REQUEST: {
#ifdef USE_LIGHT
      LightCommandRequest req;
      req.decode(msg, size);
      this->on_light_command_request_(req);
#endif
      break;
    }
    case APIMessageType::SWITCH_COMMAND_REQUEST: {
#ifdef USE_SWITCH
      SwitchCommandRequest req;
      req.decode(msg, size);
      this->on_switch_command_request_(req);
#endif
      break;
    }
    case APIMessageType::CLIMATE_COMMAND_REQUEST: {
#ifdef USE_CLIMATE
      ClimateCommandRequest req;
      req.decode(msg, size);
      this->on_climate_command_request_(req);
#endif
      break;
    }
    case APIMessageType::SUBSCRIBE_SERVICE_CALLS_REQUEST: {
      SubscribeServiceCallsRequest req;
      req.decode(msg, size);
      this->on_subscribe_service_calls_request_(req);
      break;
    }
    case APIMessageType::SERVICE_CALL_RESPONSE:
      // Invalid
      break;
    case APIMessageType::GET_TIME_REQUEST:
      // Invalid
      break;
    case APIMessageType::GET_TIME_RESPONSE: {
#ifdef USE_HOMEASSISTANT_TIME
      homeassistant::GetTimeResponse req;
      req.decode(msg, size);
#endif
      break;
    }
    case APIMessageType::SUBSCRIBE_HOME_ASSISTANT_STATES_REQUEST: {
      SubscribeHomeAssistantStatesRequest req;
      req.decode(msg, size);
      this->on_subscribe_home_assistant_states_request_(req);
      break;
    }
    case APIMessageType::SUBSCRIBE_HOME_ASSISTANT_STATE_RESPONSE:
      // Invalid
      break;
    case APIMessageType::HOME_ASSISTANT_STATE_RESPONSE: {
      HomeAssistantStateResponse req;
      req.decode(msg, size);
      this->on_home_assistant_state_response_(req);
      break;
    }
    case APIMessageType::EXECUTE_SERVICE_REQUEST: {
      ExecuteServiceRequest req;
      req.decode(msg, size);
      this->on_execute_service_(req);
      break;
    }
    case APIMessageType::CAMERA_IMAGE_REQUEST: {
#ifdef USE_ESP32_CAMERA
      CameraImageRequest req;
      req.decode(msg, size);
      this->on_camera_image_request_(req);
#endif
      break;
    }
  }
}
void APIConnection::on_hello_request_(const HelloRequest &req) {
  ESP_LOGVV(TAG, "on_hello_request_(client_info='%s')", req.get_client_info().c_str());
  this->client_info_ = req.get_client_info() + " (" + this->client_->remoteIP().toString().c_str();
  this->client_info_ += ")";
  ESP_LOGV(TAG, "Hello from client: '%s'", this->client_info_.c_str());

  auto buffer = this->get_buffer();
  // uint32 api_version_major = 1; -> 1
  buffer.encode_uint32(1, 1);
  // uint32 api_version_minor = 2; -> 1
  buffer.encode_uint32(2, 1);

  // string server_info = 3;
  buffer.encode_string(3, App.get_name() + " (esphome v" ESPHOME_VERSION ")");
  bool success = this->send_buffer(APIMessageType::HELLO_RESPONSE);
  if (!success) {
    this->fatal_error_();
    return;
  }

  this->connection_state_ = ConnectionState::WAITING_FOR_CONNECT;
}
void APIConnection::on_connect_request_(const ConnectRequest &req) {
  ESP_LOGVV(TAG, "on_connect_request_(password='%s')", req.get_password().c_str());
  bool correct = this->parent_->check_password(req.get_password());
  auto buffer = this->get_buffer();
  // bool invalid_password = 1;
  buffer.encode_bool(1, !correct);
  bool success = this->send_buffer(APIMessageType::CONNECT_RESPONSE);
  if (!success) {
    this->fatal_error_();
    return;
  }

  if (correct) {
    ESP_LOGD(TAG, "Client '%s' connected successfully!", this->client_info_.c_str());
    this->connection_state_ = ConnectionState::CONNECTED;

#ifdef USE_HOMEASSISTANT_TIME
    if (homeassistant::global_homeassistant_time != nullptr) {
      this->send_time_request();
    }
#endif
  }
}
void APIConnection::on_disconnect_request_(const DisconnectRequest &req) {
  ESP_LOGVV(TAG, "on_disconnect_request_");
  // remote initiated disconnect_client
  if (!this->send_empty_message(APIMessageType::DISCONNECT_RESPONSE)) {
    this->fatal_error_();
    return;
  }
  this->disconnect_client();
}
void APIConnection::on_disconnect_response_(const DisconnectResponse &req) {
  ESP_LOGVV(TAG, "on_disconnect_response_");
  // we initiated disconnect_client
  this->disconnect_client();
}
void APIConnection::on_ping_request_(const PingRequest &req) {
  ESP_LOGVV(TAG, "on_ping_request_");
  PingResponse resp;
  this->send_message(resp);
}
void APIConnection::on_ping_response_(const PingResponse &req) {
  ESP_LOGVV(TAG, "on_ping_response_");
  // we initiated ping
  this->sent_ping_ = false;
}
void APIConnection::on_device_info_request_(const DeviceInfoRequest &req) {
  ESP_LOGVV(TAG, "on_device_info_request_");
  auto buffer = this->get_buffer();
  // bool uses_password = 1;
  buffer.encode_bool(1, this->parent_->uses_password());
  // string name = 2;
  buffer.encode_string(2, App.get_name());
  // string mac_address = 3;
  buffer.encode_string(3, get_mac_address_pretty());
  // string esphome_version = 4;
  buffer.encode_string(4, ESPHOME_VERSION);
  // string compilation_time = 5;
  buffer.encode_string(5, App.get_compilation_time());
#ifdef ARDUINO_BOARD
  // string model = 6;
  buffer.encode_string(6, ARDUINO_BOARD);
#endif
#ifdef USE_DEEP_SLEEP
  // bool has_deep_sleep = 7;
  buffer.encode_bool(7, deep_sleep::global_has_deep_sleep);
#endif
  this->send_buffer(APIMessageType::DEVICE_INFO_RESPONSE);
}
void APIConnection::on_list_entities_request_(const ListEntitiesRequest &req) {
  ESP_LOGVV(TAG, "on_list_entities_request_");
  this->list_entities_iterator_.begin();
}
void APIConnection::on_subscribe_states_request_(const SubscribeStatesRequest &req) {
  ESP_LOGVV(TAG, "on_subscribe_states_request_");
  this->state_subscription_ = true;
  this->initial_state_iterator_.begin();
}
void APIConnection::on_subscribe_logs_request_(const SubscribeLogsRequest &req) {
  ESP_LOGVV(TAG, "on_subscribe_logs_request_");
  this->log_subscription_ = req.get_level();
  if (req.get_dump_config()) {
    App.schedule_dump_config();
  }
}

void APIConnection::fatal_error_() {
  this->client_->close();
  this->remove_ = true;
}
bool APIConnection::valid_rx_message_type_(uint32_t type) {
  switch (static_cast<APIMessageType>(type)) {
    case APIMessageType::HELLO_RESPONSE:
    case APIMessageType::CONNECT_RESPONSE:
      return false;
    case APIMessageType::HELLO_REQUEST:
      return this->connection_state_ == ConnectionState::WAITING_FOR_HELLO;
    case APIMessageType::CONNECT_REQUEST:
      return this->connection_state_ == ConnectionState::WAITING_FOR_CONNECT;
    case APIMessageType::PING_REQUEST:
    case APIMessageType::PING_RESPONSE:
    case APIMessageType::DISCONNECT_REQUEST:
    case APIMessageType::DISCONNECT_RESPONSE:
    case APIMessageType::DEVICE_INFO_REQUEST:
      if (this->connection_state_ == ConnectionState::WAITING_FOR_CONNECT)
        return true;
    default:
      return this->connection_state_ == ConnectionState::CONNECTED;
  }
}
bool APIConnection::send_message(APIMessage &msg) {
  this->send_buffer_.clear();
  APIBuffer buf(&this->send_buffer_);
  msg.encode(buf);
  return this->send_buffer(msg.message_type());
}
bool APIConnection::send_empty_message(APIMessageType type) {
  this->send_buffer_.clear();
  return this->send_buffer(type);
}

void APIConnection::disconnect_client() {
  this->client_->close();
  this->remove_ = true;
}
void encode_varint(uint8_t *dat, uint8_t *len, uint32_t value) {
  if (value <= 0x7F) {
    *dat = value;
    (*len)++;
    return;
  }

  while (value) {
    uint8_t temp = value & 0x7F;
    value >>= 7;
    if (value) {
      *dat = temp | 0x80;
    } else {
      *dat = temp;
    }
    dat++;
    (*len)++;
  }
}

bool APIConnection::send_buffer(APIMessageType type) {
  uint8_t header[20];
  header[0] = 0x00;
  uint8_t header_len = 1;
  encode_varint(header + header_len, &header_len, this->send_buffer_.size());
  encode_varint(header + header_len, &header_len, static_cast<uint32_t>(type));

  size_t needed_space = this->send_buffer_.size() + header_len;

  if (needed_space > this->client_->space()) {
    delay(0);
    if (needed_space > this->client_->space()) {
      if (type != APIMessageType::SUBSCRIBE_LOGS_RESPONSE) {
        ESP_LOGV(TAG, "Cannot send message because of TCP buffer space");
      }
      delay(0);
      return false;
    }
  }

  //  char buffer[512];
  //  uint32_t offset = 0;
  //  for (int j = 0; j < header_len; j++) {
  //    offset += snprintf(buffer + offset, 512 - offset, "0x%02X ", header[j]);
  //  }
  //  offset += snprintf(buffer + offset, 512 - offset, "| ");
  //  for (auto &it : this->send_buffer_) {
  //    int i = snprintf(buffer + offset, 512 - offset, "0x%02X ", it);
  //    if (i <= 0)
  //      break;
  //    offset += i;
  //  }
  //  ESP_LOGVV(TAG, "SEND %s", buffer);

  this->client_->add(reinterpret_cast<char *>(header), header_len);
  this->client_->add(reinterpret_cast<char *>(this->send_buffer_.data()), this->send_buffer_.size());
  return this->client_->send();
}

void APIConnection::loop() {
  if (!network_is_connected()) {
    // when network is disconnected force disconnect immediately
    // don't wait for timeout
    this->fatal_error_();
    return;
  }
  if (this->client_->disconnected()) {
    // failsafe for disconnect logic
    this->on_disconnect_();
    return;
  }
  this->parse_recv_buffer_();

  this->list_entities_iterator_.advance();
  this->initial_state_iterator_.advance();

  const uint32_t keepalive = 60000;
  if (this->sent_ping_) {
    if (millis() - this->last_traffic_ > (keepalive * 3) / 2) {
      ESP_LOGW(TAG, "'%s' didn't respond to ping request in time. Disconnecting...", this->client_info_.c_str());
      this->disconnect_client();
    }
  } else if (millis() - this->last_traffic_ > keepalive) {
    this->sent_ping_ = true;
    this->send_ping_request();
  }

#ifdef USE_ESP32_CAMERA
  if (this->image_reader_.available()) {
    uint32_t space = this->client_->space();
    // reserve 15 bytes for metadata, and at least 64 bytes of data
    if (space >= 15 + 64) {
      uint32_t to_send = std::min(space - 15, this->image_reader_.available());
      auto buffer = this->get_buffer();
      // fixed32 key = 1;
      buffer.encode_fixed32(1, esp32_camera::global_esp32_camera->get_object_id_hash());
      // bytes data = 2;
      buffer.encode_bytes(2, this->image_reader_.peek_data_buffer(), to_send);
      // bool done = 3;
      bool done = this->image_reader_.available() == to_send;
      buffer.encode_bool(3, done);
      bool success = this->send_buffer(APIMessageType::CAMERA_IMAGE_RESPONSE);
      if (success) {
        this->image_reader_.consume_data(to_send);
      }
      if (success && done) {
        this->image_reader_.return_image();
      }
    }
  }
#endif
}

#ifdef USE_BINARY_SENSOR
bool APIConnection::send_binary_sensor_state(binary_sensor::BinarySensor *binary_sensor, bool state) {
  if (!this->state_subscription_)
    return false;

  auto buffer = this->get_buffer();
  // fixed32 key = 1;
  buffer.encode_fixed32(1, binary_sensor->get_object_id_hash());
  // bool state = 2;
  buffer.encode_bool(2, state);
  return this->send_buffer(APIMessageType::BINARY_SENSOR_STATE_RESPONSE);
}
#endif

#ifdef USE_COVER
bool APIConnection::send_cover_state(cover::Cover *cover) {
  if (!this->state_subscription_)
    return false;

  auto buffer = this->get_buffer();
  auto traits = cover->get_traits();
  // fixed32 key = 1;
  buffer.encode_fixed32(1, cover->get_object_id_hash());
  // enum LegacyCoverState {
  //   OPEN = 0;
  //   CLOSED = 1;
  // }
  // LegacyCoverState legacy_state = 2;
  uint32_t state = (cover->position == cover::COVER_OPEN) ? 0 : 1;
  buffer.encode_uint32(2, state);
  // float position = 3;
  buffer.encode_float(3, cover->position);
  if (traits.get_supports_tilt()) {
    // float tilt = 4;
    buffer.encode_float(4, cover->tilt);
  }
  // enum CoverCurrentOperation {
  //   IDLE = 0;
  //   IS_OPENING = 1;
  //   IS_CLOSING = 2;
  // }
  // CoverCurrentOperation current_operation = 5;
  buffer.encode_uint32(5, cover->current_operation);
  return this->send_buffer(APIMessageType::COVER_STATE_RESPONSE);
}
#endif

#ifdef USE_FAN
bool APIConnection::send_fan_state(fan::FanState *fan) {
  if (!this->state_subscription_)
    return false;

  auto buffer = this->get_buffer();
  // fixed32 key = 1;
  buffer.encode_fixed32(1, fan->get_object_id_hash());
  // bool state = 2;
  buffer.encode_bool(2, fan->state);
  // bool oscillating = 3;
  if (fan->get_traits().supports_oscillation()) {
    buffer.encode_bool(3, fan->oscillating);
  }
  // enum FanSpeed {
  //   LOW = 0;
  //   MEDIUM = 1;
  //   HIGH = 2;
  // }
  // FanSpeed speed = 4;
  if (fan->get_traits().supports_speed()) {
    buffer.encode_uint32(4, fan->speed);
  }
  return this->send_buffer(APIMessageType::FAN_STATE_RESPONSE);
}
#endif

#ifdef USE_LIGHT
bool APIConnection::send_light_state(light::LightState *light) {
  if (!this->state_subscription_)
    return false;

  auto buffer = this->get_buffer();
  auto traits = light->get_traits();
  auto values = light->remote_values;

  // fixed32 key = 1;
  buffer.encode_fixed32(1, light->get_object_id_hash());
  // bool state = 2;
  buffer.encode_bool(2, values.get_state() != 0.0f);
  // float brightness = 3;
  if (traits.get_supports_brightness()) {
    buffer.encode_float(3, values.get_brightness());
  }
  if (traits.get_supports_rgb()) {
    // float red = 4;
    buffer.encode_float(4, values.get_red());
    // float green = 5;
    buffer.encode_float(5, values.get_green());
    // float blue = 6;
    buffer.encode_float(6, values.get_blue());
  }
  // float white = 7;
  if (traits.get_supports_rgb_white_value()) {
    buffer.encode_float(7, values.get_white());
  }
  // float color_temperature = 8;
  if (traits.get_supports_color_temperature()) {
    buffer.encode_float(8, values.get_color_temperature());
  }
  // string effect = 9;
  if (light->supports_effects()) {
    buffer.encode_string(9, light->get_effect_name());
  }
  return this->send_buffer(APIMessageType::LIGHT_STATE_RESPONSE);
}
#endif

#ifdef USE_SENSOR
bool APIConnection::send_sensor_state(sensor::Sensor *sensor, float state) {
  if (!this->state_subscription_)
    return false;

  auto buffer = this->get_buffer();
  // fixed32 key = 1;
  buffer.encode_fixed32(1, sensor->get_object_id_hash());
  // float state = 2;
  buffer.encode_float(2, state);
  return this->send_buffer(APIMessageType::SENSOR_STATE_RESPONSE);
}
#endif

#ifdef USE_SWITCH
bool APIConnection::send_switch_state(switch_::Switch *a_switch, bool state) {
  if (!this->state_subscription_)
    return false;

  auto buffer = this->get_buffer();
  // fixed32 key = 1;
  buffer.encode_fixed32(1, a_switch->get_object_id_hash());
  // bool state = 2;
  buffer.encode_bool(2, state);
  return this->send_buffer(APIMessageType::SWITCH_STATE_RESPONSE);
}
#endif

#ifdef USE_TEXT_SENSOR
bool APIConnection::send_text_sensor_state(text_sensor::TextSensor *text_sensor, std::string state) {
  if (!this->state_subscription_)
    return false;

  auto buffer = this->get_buffer();
  // fixed32 key = 1;
  buffer.encode_fixed32(1, text_sensor->get_object_id_hash());
  // string state = 2;
  buffer.encode_string(2, state);
  return this->send_buffer(APIMessageType::TEXT_SENSOR_STATE_RESPONSE);
}
#endif

#ifdef USE_CLIMATE
bool APIConnection::send_climate_state(climate::Climate *climate) {
  if (!this->state_subscription_)
    return false;

  auto buffer = this->get_buffer();
  auto traits = climate->get_traits();
  // fixed32 key = 1;
  buffer.encode_fixed32(1, climate->get_object_id_hash());
  // ClimateMode mode = 2;
  buffer.encode_uint32(2, static_cast<uint32_t>(climate->mode));
  // float current_temperature = 3;
  if (traits.get_supports_current_temperature()) {
    buffer.encode_float(3, climate->current_temperature);
  }
  if (traits.get_supports_two_point_target_temperature()) {
    // float target_temperature_low = 5;
    buffer.encode_float(5, climate->target_temperature_low);
    // float target_temperature_high = 6;
    buffer.encode_float(6, climate->target_temperature_high);
  } else {
    // float target_temperature = 4;
    buffer.encode_float(4, climate->target_temperature);
  }
  // bool away = 7;
  if (traits.get_supports_away()) {
    buffer.encode_bool(7, climate->away);
  }
  return this->send_buffer(APIMessageType::CLIMATE_STATE_RESPONSE);
}
#endif

bool APIConnection::send_log_message(int level, const char *tag, const char *line) {
  if (this->log_subscription_ < level)
    return false;

  auto buffer = this->get_buffer();
  // LogLevel level = 1;
  buffer.encode_uint32(1, static_cast<uint32_t>(level));
  // string tag = 2;
  // buffer.encode_string(2, tag, strlen(tag));
  // string message = 3;
  buffer.encode_string(3, line, strlen(line));
  bool success = this->send_buffer(APIMessageType::SUBSCRIBE_LOGS_RESPONSE);

  if (!success) {
    buffer = this->get_buffer();
    // bool send_failed = 4;
    buffer.encode_bool(4, true);
    return this->send_buffer(APIMessageType::SUBSCRIBE_LOGS_RESPONSE);
  } else {
    return true;
  }
}
bool APIConnection::send_disconnect_request() {
  DisconnectRequest req;
  return this->send_message(req);
}
bool APIConnection::send_ping_request() {
  ESP_LOGVV(TAG, "Sending ping...");
  PingRequest req;
  return this->send_message(req);
}

#ifdef USE_COVER
void APIConnection::on_cover_command_request_(const CoverCommandRequest &req) {
  ESP_LOGVV(TAG, "on_cover_command_request_");
  cover::Cover *cover = App.get_cover_by_key(req.get_key());
  if (cover == nullptr)
    return;

  auto call = cover->make_call();
  if (req.get_legacy_command().has_value()) {
    auto cmd = *req.get_legacy_command();
    switch (cmd) {
      case LEGACY_COVER_COMMAND_OPEN:
        call.set_command_open();
        break;
      case LEGACY_COVER_COMMAND_CLOSE:
        call.set_command_close();
        break;
      case LEGACY_COVER_COMMAND_STOP:
        call.set_command_stop();
        break;
    }
  }
  if (req.get_position().has_value()) {
    auto pos = *req.get_position();
    call.set_position(pos);
  }
  if (req.get_tilt().has_value()) {
    auto tilt = *req.get_tilt();
    call.set_tilt(tilt);
  }
  if (req.get_stop()) {
    call.set_command_stop();
  }
  call.perform();
}
#endif

#ifdef USE_FAN
void APIConnection::on_fan_command_request_(const FanCommandRequest &req) {
  ESP_LOGVV(TAG, "on_fan_command_request_");
  fan::FanState *fan = App.get_fan_by_key(req.get_key());
  if (fan == nullptr)
    return;

  auto call = fan->make_call();
  call.set_state(req.get_state());
  call.set_oscillating(req.get_oscillating());
  call.set_speed(req.get_speed());
  call.perform();
}
#endif

#ifdef USE_LIGHT
void APIConnection::on_light_command_request_(const LightCommandRequest &req) {
  ESP_LOGVV(TAG, "on_light_command_request_");
  light::LightState *light = App.get_light_by_key(req.get_key());
  if (light == nullptr)
    return;

  auto call = light->make_call();
  call.set_state(req.get_state());
  call.set_brightness(req.get_brightness());
  call.set_red(req.get_red());
  call.set_green(req.get_green());
  call.set_blue(req.get_blue());
  call.set_white(req.get_white());
  call.set_color_temperature(req.get_color_temperature());
  call.set_transition_length(req.get_transition_length());
  call.set_flash_length(req.get_flash_length());
  call.set_effect(req.get_effect());
  call.perform();
}
#endif

#ifdef USE_SWITCH
void APIConnection::on_switch_command_request_(const SwitchCommandRequest &req) {
  ESP_LOGVV(TAG, "on_switch_command_request_");
  switch_::Switch *a_switch = App.get_switch_by_key(req.get_key());
  if (a_switch == nullptr || a_switch->is_internal())
    return;

  if (req.get_state()) {
    a_switch->turn_on();
  } else {
    a_switch->turn_off();
  }
}
#endif

#ifdef USE_CLIMATE
void APIConnection::on_climate_command_request_(const ClimateCommandRequest &req) {
  ESP_LOGVV(TAG, "on_climate_command_request_");
  climate::Climate *climate = App.get_climate_by_key(req.get_key());
  if (climate == nullptr)
    return;

  auto call = climate->make_call();
  if (req.get_mode().has_value())
    call.set_mode(*req.get_mode());
  if (req.get_target_temperature().has_value())
    call.set_target_temperature(*req.get_target_temperature());
  if (req.get_target_temperature_low().has_value())
    call.set_target_temperature_low(*req.get_target_temperature_low());
  if (req.get_target_temperature_high().has_value())
    call.set_target_temperature_high(*req.get_target_temperature_high());
  if (req.get_away().has_value())
    call.set_away(*req.get_away());
  call.perform();
}
#endif

void APIConnection::on_subscribe_service_calls_request_(const SubscribeServiceCallsRequest &req) {
  this->service_call_subscription_ = true;
}
void APIConnection::send_service_call(ServiceCallResponse &call) {
  if (!this->service_call_subscription_)
    return;

  this->send_message(call);
}
void APIConnection::on_subscribe_home_assistant_states_request_(const SubscribeHomeAssistantStatesRequest &req) {
  for (auto &it : this->parent_->get_state_subs()) {
    auto buffer = this->get_buffer();
    // string entity_id = 1;
    buffer.encode_string(1, it.entity_id);
    this->send_buffer(APIMessageType::SUBSCRIBE_HOME_ASSISTANT_STATE_RESPONSE);
  }
}
void APIConnection::on_home_assistant_state_response_(const HomeAssistantStateResponse &req) {
  for (auto &it : this->parent_->get_state_subs()) {
    if (it.entity_id == req.get_entity_id()) {
      it.callback(req.get_state());
    }
  }
}
void APIConnection::on_execute_service_(const ExecuteServiceRequest &req) {
  ESP_LOGVV(TAG, "on_execute_service_");
  bool found = false;
  for (auto *service : this->parent_->get_user_services()) {
    if (service->execute_service(req)) {
      found = true;
    }
  }
  if (!found) {
    ESP_LOGV(TAG, "Could not find matching service!");
  }
}

APIBuffer APIConnection::get_buffer() {
  this->send_buffer_.clear();
  return {&this->send_buffer_};
}
#ifdef USE_HOMEASSISTANT_TIME
void APIConnection::send_time_request() { this->send_empty_message(APIMessageType::GET_TIME_REQUEST); }
#endif

#ifdef USE_ESP32_CAMERA
void APIConnection::send_camera_state(std::shared_ptr<esp32_camera::CameraImage> image) {
  if (!this->state_subscription_)
    return;
  if (this->image_reader_.available())
    return;
  this->image_reader_.set_image(image);
}
#endif

#ifdef USE_ESP32_CAMERA
void APIConnection::on_camera_image_request_(const CameraImageRequest &req) {
  if (esp32_camera::global_esp32_camera == nullptr)
    return;

  ESP_LOGV(TAG, "on_camera_image_request_ stream=%s single=%s", YESNO(req.get_stream()), YESNO(req.get_single()));
  if (req.get_single()) {
    esp32_camera::global_esp32_camera->request_image();
  }
  if (req.get_stream()) {
    esp32_camera::global_esp32_camera->request_stream();
  }
}
#endif

}  // namespace api
}  // namespace esphome
