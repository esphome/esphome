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
MIIDazCCAlOgAwIBAgIUXTkbRuSG7SRBseVHgJonqXAltOQwDQYJKoZIhvcNAQEL
BQAwRTELMAkGA1UEBhMCQVUxEzARBgNVBAgMClNvbWUtU3RhdGUxITAfBgNVBAoM
GEludGVybmV0IFdpZGdpdHMgUHR5IEx0ZDAeFw0yMTA4MTAxNzMyMDVaFw0yNDA1
MDYxNzMyMDVaMEUxCzAJBgNVBAYTAkFVMRMwEQYDVQQIDApTb21lLVN0YXRlMSEw
HwYDVQQKDBhJbnRlcm5ldCBXaWRnaXRzIFB0eSBMdGQwggEiMA0GCSqGSIb3DQEB
AQUAA4IBDwAwggEKAoIBAQDLfANRbcMi1wx/frK1KYk1SsymtXPzKSlqI9NSX3Eg
zqk3I9lpbiT2bQB11WAt6a5GDa0Jzh0z+BOEfPQXIMhIu6AriLN96Vpp3f7XMuG3
APF/ckwC9dMsd1/6O5HGoO73bCK9e4CxRSK7mm8iFdaF3xW/1dpsNEYAYA0bGVyf
BPFHouKGj8txG5uscORq7PGgBu6WqkIHWvnrZ+FpSJVqGU9ehPJhgP7PpIkoWDH7
NR1LIAT3r8HzrWSTkxoo/EUV8NvHu4Ejf2GDFKZVCzdi8t7A0Yed2OCkiEsJkuoO
S6OLharfd2iRTVUuXPxHfwtIB244vKldlVp10iQqGwgJAgMBAAGjUzBRMB0GA1Ud
DgQWBBQ+8OgEExObKlldOApexMeoh2OwIjAfBgNVHSMEGDAWgBQ+8OgEExObKlld
OApexMeoh2OwIjAPBgNVHRMBAf8EBTADAQH/MA0GCSqGSIb3DQEBCwUAA4IBAQCh
pxd9qROcW0SdI7erOdxgh4e1cDSCVA7VOpdWwZb/pyTRbPJEBTxJk6+8V18KFsCj
7jkpxCmYWkUO7b6uLm8DW+dJREmC+H2pyZ70Y+81lhb4C1ijMrsowSsxookSXzBO
QaU3LaQ3/56PA0FKm+NbpCxgb2pIwYmAgTTEM4urOXUwew3D+3griLU6EQfhixkj
dt5+wKtC7/ciUjKQMGUD12z2sWaTxiKLVQK8659NW4Fib0TEPDjJCOH21eK/QW7f
RH8lylnANDNg+LegACgrFc/0LsRJ8qeulLTgo4j3nr7mkUhaFrdPZ2tf1lIQ4pS7
zhM/i3UJKb2+pp3VceAF
-----END CERTIFICATE-----
)");
  ssl_->set_private_key(R"(-----BEGIN RSA PRIVATE KEY-----
MIIEpAIBAAKCAQEAy3wDUW3DItcMf36ytSmJNUrMprVz8ykpaiPTUl9xIM6pNyPZ
aW4k9m0AddVgLemuRg2tCc4dM/gThHz0FyDISLugK4izfelaad3+1zLhtwDxf3JM
AvXTLHdf+juRxqDu92wivXuAsUUiu5pvIhXWhd8Vv9XabDRGAGANGxlcnwTxR6Li
ho/LcRubrHDkauzxoAbulqpCB1r562fhaUiVahlPXoTyYYD+z6SJKFgx+zUdSyAE
96/B861kk5MaKPxFFfDbx7uBI39hgxSmVQs3YvLewNGHndjgpIhLCZLqDkuji4Wq
33dokU1VLlz8R38LSAduOLypXZVaddIkKhsICQIDAQABAoIBAQCH9zqEW5JrIkTQ
oEkrMjDjcjKjJlXu4c8xg/S9cta1tEFvfb3Va+q42obXNZeJtj5jwtmf1qC+zAHG
onO7TIVnzC8vR62f8aAfY13YT8BwtumnXikaRUYwzSdNniY/EeP7Yb8o23BXgzm9
H2ko3my0kScIQw4rBRbNQ2EefWjdO46drPPMb1neVQOFixgDh6lMfIdda0DSsbSz
f9QdhqZsgmemVtM3hvyN0Ayix/2GmlatrwE0mHEoq9+2rwpgr0fDQo/yDak/LqAn
xnZkhJ/+5ioIsDmbP08SIf/f5M5xVzuX125NLYda+EY6ZxRg3hjZl7gTGmeZ17ok
4pdZSEg9AoGBAPzLAnYmFgfZ010NWfELPexqZUKoQamvaqRQsYpUTKAYhiF0hQn6
mGINhIDItN71z9rbI8lgIP0oWy8hx17mn9+Yf1fh6HTWhBlGTf5mUsphDlEpFaOW
fpfMJsY7XijzWWhnlxnfx/i9aJjLlKSGFDxuV07fecTR6IMx4dmNzIZjAoGBAM4Q
3WxN7/Apje3fuYd1sJIOG6pvXdrLc6HSmLEXCZpfTV23x7gZxqcZ2I64aTI7x/14
/ukvGrRlSqRQx4KmDc0GRBdLP4MV4l1hPsoOMGY0mv/vPA+8cvJejohUyxw1Ks17
Say4+UMgbMyZPF5fmxjioQM7Du4JuT7+Ckf7Ct2jAoGBAOCvZ+6vh2FPAIhyd4DB
ZzackogemM5DNdv6RPMYXkrlwUI3GJB4Bb0RGraErg6GGUsC/9na8TdSCUnpEhWN
kofgOT9ZcY13pU35qRT2yZtLjLgidjNCbh5pvATxe42WTd94Q1siBdFWOIOmXuWk
rpPP1xYpl0eS8sC5M250lFbVAoGAZxgg71J+vFTN4TiRBahn0mqkhLx65D/tdgR6
x/6Sm10aw8xyazTaIjfYFG9Gkg8+mx2xys/6OE6E7XhMmJyjQvPjlRPcjqSgP7+r
PkOjCz8+XNU/40+Q4plUDfj+NPqUk4Ee9v8EFgVe6W2wujZyzhx0Gb2uVoEe4NkQ
I7sBUKkCgYA1IOiBDrGomjVoVcTp4riNWS++3o0rGlnLmRdN6n9UBrg21f7JQ6jS
ljgFkDBI+9sf9sRgHgGsJl/mQzWSxaQj9A3YToFd3fFOWbSVc9cfOhUK1JaVjgC7
y4KXzepo3XxjoC8Vm71SDDlJ3Kzz45F2lR0sjM4TigzgfhDmGkGx4g==
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
