#pragma once

#include "esphome/core/defines.h"
#ifdef USE_OTA
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"
#include "esphome/components/ota/ota_backend.h"
#include "esphome/components/socket/socket.h"

namespace esphome {

/// ESPHomeOTAComponent provides a simple way to integrate Over-the-Air updates into your app using ArduinoOTA.
class ESPHomeOTAComponent : public ota::OTAComponent {
 public:
#ifdef USE_OTA_PASSWORD
  void set_auth_password(const std::string &password) { password_ = password; }
#endif  // USE_OTA_PASSWORD

  /// Manually set the port OTA should listen on
  void set_port(uint16_t port);

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void loop() override;

  uint16_t get_port() const;

 protected:
  void handle_handshake_();
  void handle_data_();
  bool readall_(uint8_t *buf, size_t len);
  bool writeall_(const uint8_t *buf, size_t len);
  void log_socket_error_(const char *msg);
  void log_read_error_(const char *what);
  void log_start_(const char *phase);
  void cleanup_connection_();
  void yield_and_feed_watchdog_();

#ifdef USE_OTA_PASSWORD
  std::string password_;
#endif  // USE_OTA_PASSWORD

  std::unique_ptr<socket::Socket> server_;
  std::unique_ptr<socket::Socket> client_;

  uint32_t client_connect_time_{0};
  uint16_t port_;
  uint8_t magic_buf_[5];
  uint8_t magic_buf_pos_{0};
};

}  // namespace esphome
#endif
