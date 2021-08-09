#include "api_server.h"
#include "api_connection.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/util.h"
#include "esphome/core/defines.h"
#include "esphome/core/version.h"
#include <errno.h>
//#include <arpa/inet.h>

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
  socket_ = socket::socket(AF_INET, SOCK_STREAM, 0);
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

  /*struct sockaddr_storage dest_addr;
  memset(&dest_addr, 0, sizeof(dest_addr));
  struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *) &dest_addr;
  dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
  dest_addr_ip4->sin_family = AF_INET;
  dest_addr_ip4->sin_port = htons(this->port_);

  err = socket_->bind((struct sockaddr *) &dest_addr, sizeof(dest_addr));*/

  struct sockaddr_in server;
  memset(&server, 0, sizeof(server));
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = INADDR_ANY;
  server.sin_port = htons(this->port_);

  err = socket_->bind((struct sockaddr *) &server, sizeof(server));
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

  ssl_ = ssl::create_context();
  if (!ssl_) {
    ESP_LOGW(TAG, "Failed to create SSL context: errno %d", errno);
    this->mark_failed();
    return;
  }
  ssl_->set_server_certificate(R"(-----BEGIN CERTIFICATE-----
MIIDETCCAfkCFGNtbm6nA3CZM7no7HqdWikhUMSkMA0GCSqGSIb3DQEBCwUAMEUx
CzAJBgNVBAYTAkFVMRMwEQYDVQQIDApTb21lLVN0YXRlMSEwHwYDVQQKDBhJbnRl
cm5ldCBXaWRnaXRzIFB0eSBMdGQwHhcNMjEwODA5MTgwOTMyWhcNMjEwOTA4MTgw
OTMyWjBFMQswCQYDVQQGEwJBVTETMBEGA1UECAwKU29tZS1TdGF0ZTEhMB8GA1UE
CgwYSW50ZXJuZXQgV2lkZ2l0cyBQdHkgTHRkMIIBIjANBgkqhkiG9w0BAQEFAAOC
AQ8AMIIBCgKCAQEAwbt/qjWftqZtdRaJ5QjRf/8Sh6JT8KN4Bu9cGbHJIKAQLhy6
8/qdB24Ar8SyuKEaV8HRcCguTQ58jdK5rbaQu/Zpppgy9lF3AHH1MhVHavGNca3A
ejFtJr4DuTLkv/HjpgcAHjhZk+mFeNXrHeFrPIzF3imSyV1xyqoBxpa1cCFH/D3J
o2S6PMdAEcHSoaP5TEuM9e2j9Sc97LughMaFkR1R4cz2kEyMZIOASHkFCJMV6pjg
PVOqxu11oFYJn9/zh1Ea6PChYq+bGBmj60vwh+tpA6E8T0PzkxUuVklAD5pBfXoD
y8xW8ulc0CPSGSaxbn2vudUBJyZFvQBTQhFVcwIDAQABMA0GCSqGSIb3DQEBCwUA
A4IBAQBQM80osk+ryQ+CqBhyOLQBOeQkmCVNzMUjVBG7tP4vkuJtfAdyUuKBWGtr
X2VrkL0yueeDt9rdib5QbXWih4sT7KdQlnSBmnrac0MM7wCh+lhCnJhWWCUBHP9s
8rkL2XOrISbVi80wqpJn0y4FMaoK6KnxyelallHuNZ+3EZAuZrhGAkV68Z83CIFO
5emvAIGq73U/lddLDV6sz7zWeDdnyfTpkLzml8wJLO9Ob7o6aw7WJK/edjYdc2XW
pIMatEESaN9MlWI5SXQS4AcMnqdUqab5587cHDrgVjBd8RmvdyT9j2v7nS2JyEK0
DkASogRqBmCLR1/0UW+dFARCZI9k
-----END CERTIFICATE-----
)");
  ssl_->set_private_key(R"(-----BEGIN RSA PRIVATE KEY-----
MIIEpAIBAAKCAQEAwbt/qjWftqZtdRaJ5QjRf/8Sh6JT8KN4Bu9cGbHJIKAQLhy6
8/qdB24Ar8SyuKEaV8HRcCguTQ58jdK5rbaQu/Zpppgy9lF3AHH1MhVHavGNca3A
ejFtJr4DuTLkv/HjpgcAHjhZk+mFeNXrHeFrPIzF3imSyV1xyqoBxpa1cCFH/D3J
o2S6PMdAEcHSoaP5TEuM9e2j9Sc97LughMaFkR1R4cz2kEyMZIOASHkFCJMV6pjg
PVOqxu11oFYJn9/zh1Ea6PChYq+bGBmj60vwh+tpA6E8T0PzkxUuVklAD5pBfXoD
y8xW8ulc0CPSGSaxbn2vudUBJyZFvQBTQhFVcwIDAQABAoIBAGVpQuDUhTBVWkLK
c5CC1zfLS+XYIVx8FZ57uZhxqjj70LxyqaKBc6Wp/Y4ExxFCs8lwWbP+NI59oNGU
l0HJqWXbDV75mOO7rTF8db+rx+DBZSs2quTL7rkzCjvt2jRn6KTGUVeAY9O7j/S6
9gKEN2BQyFsNJBtoYOKXr6pGxd9Vg3K0j7DJXf5uK7lWIrxtU9k7QgMJFdnhbzEu
0TnhFEVMDdBIm+yrTdL8lmIdT8DOUIJgyTJ1iICFndksPQSgBAWQaGKaaxZbn0c3
Oy778VFqT2HywHVbJQL4XBe/yYUhjbpF1Hv9EEbK3Rm04xsCDbZru6/AK88gHBk4
b7uUSwECgYEA4BmML1isP9h8zqAvCEFjFmAWpoLBBZ+5I9Go1PIWhlCYY+G7AUXw
zxb0J6d9UGsYTkJXlgE77+HBzqlgyhCkngNuAAPm37ebdwuy5iBr32c9RLahR5W5
Nh+J3le9JTXe9B9uwfggD06dBFmhgG0PQdyBr4Daa3a8VRJAD1MGYMECgYEA3U9U
QwxQOYBkdJTbIQnTP7vnFuhWn9V5BMn5PczJSwGJEgaHgIL5Bm5NHa/ON3UX6QIi
uk73fGfohN8Ii1MjVKNFKM/LZ30XSufVHrm7yH6xRR4qbZUk4KhKxV/uOVluv38P
bis9B9cye3ETnjDhkWK4/TJeTHHlTAKMQuOQzzMCgYEAmtlsYYbvNwq7aveKqDSu
aFarMBGnmOA+SP7ln4dMgzELq/DdjEqs1BwzR3dXgwsNd34mEVP2+5HOnqOxas7H
QRxzlPUdQjcX6NGfo56Bi5RF5MYheVp+6WQvmwCbhSvNTHivyr5OQOV8X/YjP5+c
bFEXF5N82cbo6gu7Uht3i8ECgYAh511JSEGiDYFWOte3IAI06VxlrgJXSiTYDvkX
9p9/1iRhlo57qZTs30kBG0XESTP4hlM7p41SibidYm20qm/nL3wQ3ISUvh0rZIjJ
xDp4ZLBTnmNxlj+oCyApTKD6ODE3NQfwIL+gy973+kK/IU3tL+qXH3hCzdAK7Pj/
5kzw8QKBgQDUYQGH1JT93Yn9uIyfX1v6HcB1azDbF16JEOFZoGlS1gxFCobIb7jA
2/Y0HfFUUfDGexjQNReFi0IXjgBvYmJX7rF9tGsTdXh35Lu2cTd0DcykGPVcFyJW
PSf0vGzbAqpdriYQStaed+HgTdW6kHsOBNeJbbJkjsQpoaoWX3tEDw==
-----END RSA PRIVATE KEY-----
)");
  err = ssl_->init();
  if (err != 0) {
    ESP_LOGW(TAG, "Failed to initialize SSL context: errno %d", errno);
    this->mark_failed();
    return;
  }

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
  // Accept new clients
  while (true) {
    struct sockaddr_storage source_addr;
    socklen_t addr_len = sizeof(source_addr);
    auto sock = socket_->accept((struct sockaddr *) &source_addr, &addr_len);
    if (!sock)
      break;
    ESP_LOGD(TAG, "Accepted %s", sock->getpeername().c_str());

    // wrap socket
    auto sock2 = ssl_->wrap_socket(std::move(sock));
    if (!sock2) {
      ESP_LOGW(TAG, "Failed to wrap socket with SSL: errno %d", errno);
      continue;
    }

    auto *conn = new APIConnection(std::move(sock2), this);
    clients_.push_back(conn);
    conn->start();
  }

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
void APIServer::on_text_sensor_update(text_sensor::TextSensor *obj, const std::string &state) {
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

#ifdef USE_NUMBER
void APIServer::on_number_update(number::Number *obj, float state) {
  if (obj->is_internal())
    return;
  for (auto *c : this->clients_)
    c->send_number_state(obj, state);
}
#endif

#ifdef USE_SELECT
void APIServer::on_select_update(select::Select *obj, const std::string &state) {
  if (obj->is_internal())
    return;
  for (auto *c : this->clients_)
    c->send_select_state(obj, state);
}
#endif

float APIServer::get_setup_priority() const { return setup_priority::AFTER_WIFI; }
void APIServer::set_port(uint16_t port) { this->port_ = port; }
APIServer *global_api_server = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

void APIServer::set_password(const std::string &password) { this->password_ = password; }
void APIServer::send_homeassistant_service_call(const HomeassistantServiceResponse &call) {
  for (auto *client : this->clients_) {
    client->send_homeassistant_service_call(call);
  }
}
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
  for (auto *client : this->clients_) {
    if (!client->remove_ && client->connection_state_ == APIConnection::ConnectionState::CONNECTED)
      client->send_time_request();
  }
}
#endif
bool APIServer::is_connected() const { return !this->clients_.empty(); }
void APIServer::on_shutdown() {
  for (auto *c : this->clients_) {
    c->send_disconnect_request(DisconnectRequest());
  }
  delay(10);
}

}  // namespace api
}  // namespace esphome
