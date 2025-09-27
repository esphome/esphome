#pragma once

#include "esphome/core/defines.h"
#ifdef USE_OTA
#include "esphome/components/ota/ota_backend.h"
#include "esphome/components/socket/socket.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/preferences.h"
#include "esphome/core/hash_base.h"

namespace esphome {

/// ESPHomeOTAComponent provides a simple way to integrate Over-the-Air updates into your app using ArduinoOTA.
class ESPHomeOTAComponent : public ota::OTAComponent {
 public:
  enum class OTAState : uint8_t {
    IDLE,
    MAGIC_READ,    // Reading magic bytes
    MAGIC_ACK,     // Sending OK and version after magic bytes
    FEATURE_READ,  // Reading feature flags from client
    FEATURE_ACK,   // Sending feature acknowledgment
#ifdef USE_OTA_PASSWORD
    AUTH_SEND,  // Sending authentication request
    AUTH_READ,  // Reading authentication data
#endif          // USE_OTA_PASSWORD
    DATA,       // BLOCKING! Processing OTA data (update, etc.)
  };
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
#ifdef USE_OTA_PASSWORD
  bool handle_auth_send_();
  bool handle_auth_read_();
  bool prepare_auth_nonce_(HashBase *hasher);
  bool verify_hash_auth_(HashBase *hasher, size_t hex_size);
  size_t get_auth_hex_size_() const;
  void cleanup_auth_();
  void log_auth_warning_(const LogString *msg);
#endif  // USE_OTA_PASSWORD
  bool readall_(uint8_t *buf, size_t len);
  bool writeall_(const uint8_t *buf, size_t len);

  bool try_read_(size_t to_read, const LogString *error_desc, const LogString *close_desc);
  bool try_write_(size_t to_write, const LogString *error_desc);
  void transition_ota_state_(OTAState next_state);

  void log_socket_error_(const LogString *msg);
  void log_read_error_(const LogString *what);
  void log_start_(const LogString *phase);
  void log_remote_closed_(const LogString *during);
  void cleanup_connection_();
  void send_error_and_cleanup_(ota::OTAResponseTypes error);
  void yield_and_feed_watchdog_();

#ifdef USE_OTA_PASSWORD
  std::string password_;
#endif  // USE_OTA_PASSWORD

  std::unique_ptr<socket::Socket> server_;
  std::unique_ptr<socket::Socket> client_;
  std::unique_ptr<ota::OTABackend> backend_;

  uint32_t client_connect_time_{0};
  uint16_t port_;
  uint8_t handshake_buf_[5];
  OTAState ota_state_{OTAState::IDLE};
  uint8_t handshake_buf_pos_{0};
  uint8_t ota_features_{0};
#ifdef USE_OTA_PASSWORD
  std::unique_ptr<uint8_t[]> auth_buf_;
  size_t auth_buf_pos_{0};
  uint8_t auth_type_{0};  // Store auth type to know which hasher to use
#endif                    // USE_OTA_PASSWORD
};

}  // namespace esphome
#endif
