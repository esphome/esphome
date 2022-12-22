#include "api_server.h"
#include "api_connection.h"
#include "esphome/core/application.h"
#include "esphome/core/defines.h"
#include "esphome/core/log.h"
#include "esphome/core/util.h"
#include "esphome/core/version.h"
#include "esphome/core/hal.h"
#include "esphome/components/network/util.h"
#include <cerrno>

#ifdef USE_LOGGER
#include "esphome/components/logger/logger.h"
#endif

#include <algorithm>

namespace esphome {
namespace api {

static const char *const TAG = "api";

// APIServer
void APIServer::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Home Assistant API server...");
  this->setup_controller();
  socket_ = socket::socket_ip(SOCK_STREAM, 0);
  if (socket_ == nullptr) {
    ESP_LOGW(TAG, "Could not create socket.");
    this->mark_failed();
    return;
  }
  int enable = 1;
  int err = socket_->setsockopt(SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
  if (err != 0) {
    ESP_LOGW(TAG, "Socket unable to set reuseaddr: errno %d", err);
    // we can still continue
  }
  err = socket_->setblocking(false);
  if (err != 0) {
    ESP_LOGW(TAG, "Socket unable to set nonblocking mode: errno %d", err);
    this->mark_failed();
    return;
  }

  struct sockaddr_storage server;

  socklen_t sl = socket::set_sockaddr_any((struct sockaddr *) &server, sizeof(server), htons(this->port_));
  if (sl == 0) {
    ESP_LOGW(TAG, "Socket unable to set sockaddr: errno %d", errno);
    this->mark_failed();
    return;
  }

  err = socket_->bind((struct sockaddr *) &server, sl);
  if (err != 0) {
    ESP_LOGW(TAG, "Socket unable to bind: errno %d", errno);
    this->mark_failed();
    return;
  }

  err = socket_->listen(4);
  if (err != 0) {
    ESP_LOGW(TAG, "Socket unable to listen: errno %d", errno);
    this->mark_failed();
    return;
  }

#ifdef USE_LOGGER
  if (logger::global_logger != nullptr) {
    logger::global_logger->add_on_log_callback([this](int level, const char *tag, const char *message) {
      for (auto &c : this->clients_) {
        if (!c->remove_)
          c->send_log_message(level, tag, message);
      }
    });
  }
#endif

  this->last_connected_ = millis();

#ifdef USE_ESP32_CAMERA
  if (esp32_camera::global_esp32_camera != nullptr && !esp32_camera::global_esp32_camera->is_internal()) {
    esp32_camera::global_esp32_camera->add_image_callback(
        [this](const std::shared_ptr<esp32_camera::CameraImage> &image) {
          for (auto &c : this->clients_) {
            if (!c->remove_)
              c->send_camera_state(image);
          }
        });
  }
#endif
}
void APIServer::loop() {
  // Accept new clients
  while (true) {
    struct sockaddr_storage source_addr;
    socklen_t addr_len = sizeof(source_addr);
    auto sock = socket_->accept((struct sockaddr *) &source_addr, &addr_len);
    if (!sock)
      break;
    ESP_LOGD(TAG, "Accepted %s", sock->getpeername().c_str());

    auto *conn = new APIConnection(std::move(sock), this);
    clients_.emplace_back(conn);
    conn->start();
  }

  // Partition clients into remove and active
  auto new_end = std::partition(this->clients_.begin(), this->clients_.end(),
                                [](const std::unique_ptr<APIConnection> &conn) { return !conn->remove_; });
  // print disconnection messages
  for (auto it = new_end; it != this->clients_.end(); ++it) {
    ESP_LOGV(TAG, "Removing connection to %s", (*it)->client_info_.c_str());
  }
  // resize vector
  this->clients_.erase(new_end, this->clients_.end());

  for (auto &client : this->clients_) {
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
  ESP_LOGCONFIG(TAG, "  Address: %s:%u", network::get_use_address().c_str(), this->port_);
#ifdef USE_API_NOISE
  ESP_LOGCONFIG(TAG, "  Using noise encryption: YES");
#else
  ESP_LOGCONFIG(TAG, "  Using noise encryption: NO");
#endif
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
  for (auto &c : this->clients_)
    c->send_binary_sensor_state(obj, state);
}
#endif

#ifdef USE_COVER
void APIServer::on_cover_update(cover::Cover *obj) {
  if (obj->is_internal())
    return;
  for (auto &c : this->clients_)
    c->send_cover_state(obj);
}
#endif

#ifdef USE_FAN
void APIServer::on_fan_update(fan::Fan *obj) {
  if (obj->is_internal())
    return;
  for (auto &c : this->clients_)
    c->send_fan_state(obj);
}
#endif

#ifdef USE_LIGHT
void APIServer::on_light_update(light::LightState *obj) {
  if (obj->is_internal())
    return;
  for (auto &c : this->clients_)
    c->send_light_state(obj);
}
#endif

#ifdef USE_SENSOR
void APIServer::on_sensor_update(sensor::Sensor *obj, float state) {
  if (obj->is_internal())
    return;
  for (auto &c : this->clients_)
    c->send_sensor_state(obj, state);
}
#endif

#ifdef USE_SWITCH
void APIServer::on_switch_update(switch_::Switch *obj, bool state) {
  if (obj->is_internal())
    return;
  for (auto &c : this->clients_)
    c->send_switch_state(obj, state);
}
#endif

#ifdef USE_TEXT_SENSOR
void APIServer::on_text_sensor_update(text_sensor::TextSensor *obj, const std::string &state) {
  if (obj->is_internal())
    return;
  for (auto &c : this->clients_)
    c->send_text_sensor_state(obj, state);
}
#endif

#ifdef USE_CLIMATE
void APIServer::on_climate_update(climate::Climate *obj) {
  if (obj->is_internal())
    return;
  for (auto &c : this->clients_)
    c->send_climate_state(obj);
}
#endif

#ifdef USE_NUMBER
void APIServer::on_number_update(number::Number *obj, float state) {
  if (obj->is_internal())
    return;
  for (auto &c : this->clients_)
    c->send_number_state(obj, state);
}
#endif

#ifdef USE_SELECT
void APIServer::on_select_update(select::Select *obj, const std::string &state, size_t index) {
  if (obj->is_internal())
    return;
  for (auto &c : this->clients_)
    c->send_select_state(obj, state);
}
#endif

#ifdef USE_LOCK
void APIServer::on_lock_update(lock::Lock *obj) {
  if (obj->is_internal())
    return;
  for (auto &c : this->clients_)
    c->send_lock_state(obj, obj->state);
}
#endif

#ifdef USE_MEDIA_PLAYER
void APIServer::on_media_player_update(media_player::MediaPlayer *obj) {
  if (obj->is_internal())
    return;
  for (auto &c : this->clients_)
    c->send_media_player_state(obj);
}
#endif

float APIServer::get_setup_priority() const { return setup_priority::AFTER_WIFI; }
void APIServer::set_port(uint16_t port) { this->port_ = port; }
APIServer *global_api_server = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

void APIServer::set_password(const std::string &password) { this->password_ = password; }
void APIServer::send_homeassistant_service_call(const HomeassistantServiceResponse &call) {
  for (auto &client : this->clients_) {
    client->send_homeassistant_service_call(call);
  }
}
#ifdef USE_BLUETOOTH_PROXY
void APIServer::send_bluetooth_le_advertisement(const BluetoothLEAdvertisementResponse &call) {
  for (auto &client : this->clients_) {
    client->send_bluetooth_le_advertisement(call);
  }
}
void APIServer::send_bluetooth_device_connection(uint64_t address, bool connected, uint16_t mtu, esp_err_t error) {
  BluetoothDeviceConnectionResponse call;
  call.address = address;
  call.connected = connected;
  call.mtu = mtu;
  call.error = error;

  for (auto &client : this->clients_) {
    client->send_bluetooth_device_connection_response(call);
  }
}

void APIServer::send_bluetooth_connections_free(uint8_t free, uint8_t limit) {
  BluetoothConnectionsFreeResponse call;
  call.free = free;
  call.limit = limit;

  for (auto &client : this->clients_) {
    client->send_bluetooth_connections_free_response(call);
  }
}

void APIServer::send_bluetooth_gatt_read_response(const BluetoothGATTReadResponse &call) {
  for (auto &client : this->clients_) {
    client->send_bluetooth_gatt_read_response(call);
  }
}
void APIServer::send_bluetooth_gatt_write_response(const BluetoothGATTWriteResponse &call) {
  for (auto &client : this->clients_) {
    client->send_bluetooth_gatt_write_response(call);
  }
}
void APIServer::send_bluetooth_gatt_notify_data_response(const BluetoothGATTNotifyDataResponse &call) {
  for (auto &client : this->clients_) {
    client->send_bluetooth_gatt_notify_data_response(call);
  }
}
void APIServer::send_bluetooth_gatt_notify_response(const BluetoothGATTNotifyResponse &call) {
  for (auto &client : this->clients_) {
    client->send_bluetooth_gatt_notify_response(call);
  }
}
void APIServer::send_bluetooth_gatt_services(const BluetoothGATTGetServicesResponse &call) {
  for (auto &client : this->clients_) {
    client->send_bluetooth_gatt_get_services_response(call);
  }
}
void APIServer::send_bluetooth_gatt_services_done(uint64_t address) {
  BluetoothGATTGetServicesDoneResponse call;
  call.address = address;

  for (auto &client : this->clients_) {
    client->send_bluetooth_gatt_get_services_done_response(call);
  }
}
void APIServer::send_bluetooth_gatt_error(uint64_t address, uint16_t handle, esp_err_t error) {
  BluetoothGATTErrorResponse call;
  call.address = address;
  call.handle = handle;
  call.error = error;

  for (auto &client : this->clients_) {
    client->send_bluetooth_gatt_error_response(call);
  }
}

#endif
APIServer::APIServer() { global_api_server = this; }
void APIServer::subscribe_home_assistant_state(std::string entity_id, optional<std::string> attribute,
                                               std::function<void(std::string)> f) {
  this->state_subs_.push_back(HomeAssistantStateSubscription{
      .entity_id = std::move(entity_id),
      .attribute = std::move(attribute),
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
  for (auto &client : this->clients_) {
    if (!client->remove_ && client->connection_state_ == APIConnection::ConnectionState::CONNECTED)
      client->send_time_request();
  }
}
#endif
bool APIServer::is_connected() const { return !this->clients_.empty(); }
void APIServer::on_shutdown() {
  for (auto &c : this->clients_) {
    c->send_disconnect_request(DisconnectRequest());
  }
  delay(10);
}

}  // namespace api
}  // namespace esphome
