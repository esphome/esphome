#pragma once

#include "esphome/core/defines.h"
#ifdef USE_OTA
#include "esphome/components/ota/ota_backend_factory.h"
#include "esphome/components/socket/socket.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/preferences.h"
#include "esphome/core/hash_base.h"

namespace esphome {

/// ESPHomeOTAComponent provides a simple way to integrate Over-the-Air updates into your app using ArduinoOTA.
class ESPHomeOTAComponent final : public ota::OTAComponent {
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
#else
  // Stub so lambdas referencing set_auth_password() produce a clear error instead of
  // a cryptic "no member" diagnostic. Only fires if the stub is actually instantiated.
  template<bool B = false> void set_auth_password(const std::string &) {
    static_assert(B, "set_auth_password() requires the OTA auth path to be compiled. "
                     "Add 'password: \"\"' (empty string) to your 'ota: - platform: esphome' "
                     "config to enable runtime password rotation.");
  }
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
  static constexpr size_t SHA256_HEX_SIZE = 64;  // SHA256 hash as hex string (32 bytes * 2)
  bool handle_auth_send_();
  bool handle_auth_read_();
  bool select_auth_type_();
  void cleanup_auth_();
  void log_auth_warning_(const LogString *msg);
#endif  // USE_OTA_PASSWORD
  bool readall_(uint8_t *buf, size_t len);
  bool writeall_(const uint8_t *buf, size_t len);
  inline bool write_byte_(uint8_t byte) { return this->writeall_(&byte, 1); }

  bool try_read_(size_t to_read, const LogString *desc);
  bool try_write_(size_t to_write, const LogString *desc);

  inline bool would_block_(int error_code) const { return error_code == EAGAIN || error_code == EWOULDBLOCK; }
  bool handle_read_error_(ssize_t read, const LogString *desc);
  bool handle_write_error_(ssize_t written, const LogString *desc);
  inline void transition_ota_state_(OTAState next_state) {
    this->ota_state_ = next_state;
    this->handshake_buf_pos_ = 0;  // Reset buffer position for next state
  }

  void server_failed_(const LogString *msg);
  void log_socket_error_(const LogString *msg);
  void log_read_error_(const LogString *what);
  void log_start_(const LogString *phase);
  void log_remote_closed_(const LogString *during);
  void cleanup_connection_();
  inline void send_error_and_cleanup_(ota::OTAResponseTypes error) {
    uint8_t error_byte = static_cast<uint8_t>(error);
    this->client_->write(&error_byte, 1);  // Best effort, non-blocking
    this->cleanup_connection_();
  }
  void yield_and_feed_watchdog_();

#ifdef USE_OTA_PASSWORD
  std::string password_;
  std::unique_ptr<uint8_t[]> auth_buf_;
#endif  // USE_OTA_PASSWORD

  socket::ListenSocket *server_{nullptr};
  std::unique_ptr<socket::Socket> client_;
  ota::OTABackendPtr backend_;

  uint32_t client_connect_time_{0};
  static constexpr size_t HANDSHAKE_BUF_SIZE = 5;
#ifdef USE_OTA_PARTITIONS
  uint32_t running_app_offset_{0};
  size_t running_app_size_{0};
#endif
  uint16_t port_;
  uint8_t handshake_buf_[HANDSHAKE_BUF_SIZE];
  OTAState ota_state_{OTAState::IDLE};
  uint8_t handshake_buf_pos_{0};
  uint8_t ota_features_{0};
#ifdef USE_OTA_PASSWORD
  uint8_t auth_buf_pos_{0};
  uint8_t auth_type_{0};  // Store auth type to know which hasher to use
#endif                    // USE_OTA_PASSWORD
  bool extended_proto_{false};
};

}  // namespace esphome
#endif
