#include "ota_esphome.h"
#ifdef USE_OTA
#ifdef USE_OTA_PASSWORD
#ifdef USE_OTA_MD5
#include "esphome/components/md5/md5.h"
#endif
#ifdef USE_OTA_SHA256
#include "esphome/components/sha256/sha256.h"
#endif
#endif
#include "esphome/components/network/util.h"
#include "esphome/components/ota/ota_backend.h"
#include "esphome/components/ota/ota_backend_arduino_esp8266.h"
#include "esphome/components/ota/ota_backend_arduino_libretiny.h"
#include "esphome/components/ota/ota_backend_arduino_rp2040.h"
#include "esphome/components/ota/ota_backend_esp_idf.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/util.h"

#include <cerrno>
#include <cstdio>

namespace esphome {

static const char *const TAG = "esphome.ota";
static constexpr uint16_t OTA_BLOCK_SIZE = 8192;
static constexpr size_t OTA_BUFFER_SIZE = 1024;                  // buffer size for OTA data transfer
static constexpr uint32_t OTA_SOCKET_TIMEOUT_HANDSHAKE = 20000;  // milliseconds for initial handshake
static constexpr uint32_t OTA_SOCKET_TIMEOUT_DATA = 90000;       // milliseconds for data transfer

#ifdef USE_OTA_PASSWORD
#ifdef USE_OTA_MD5
static constexpr size_t MD5_HEX_SIZE = 32;  // MD5 hash as hex string (16 bytes * 2)
#endif
#ifdef USE_OTA_SHA256
static constexpr size_t SHA256_HEX_SIZE = 64;  // SHA256 hash as hex string (32 bytes * 2)
#endif
#endif  // USE_OTA_PASSWORD

void ESPHomeOTAComponent::setup() {
#ifdef USE_OTA_STATE_CALLBACK
  ota::register_ota_platform(this);
#endif

  this->server_ = socket::socket_ip_loop_monitored(SOCK_STREAM, 0);  // monitored for incoming connections
  if (this->server_ == nullptr) {
    this->log_socket_error_(LOG_STR("creation"));
    this->mark_failed();
    return;
  }
  int enable = 1;
  int err = this->server_->setsockopt(SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
  if (err != 0) {
    this->log_socket_error_(LOG_STR("reuseaddr"));
    // we can still continue
  }
  err = this->server_->setblocking(false);
  if (err != 0) {
    this->log_socket_error_(LOG_STR("non-blocking"));
    this->mark_failed();
    return;
  }

  struct sockaddr_storage server;

  socklen_t sl = socket::set_sockaddr_any((struct sockaddr *) &server, sizeof(server), this->port_);
  if (sl == 0) {
    this->log_socket_error_(LOG_STR("set sockaddr"));
    this->mark_failed();
    return;
  }

  err = this->server_->bind((struct sockaddr *) &server, sizeof(server));
  if (err != 0) {
    this->log_socket_error_(LOG_STR("bind"));
    this->mark_failed();
    return;
  }

  err = this->server_->listen(1);  // Only one client at a time
  if (err != 0) {
    this->log_socket_error_(LOG_STR("listen"));
    this->mark_failed();
    return;
  }
}

void ESPHomeOTAComponent::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Over-The-Air updates:\n"
                "  Address: %s:%u\n"
                "  Version: %d",
                network::get_use_address(), this->port_, USE_OTA_VERSION);
#ifdef USE_OTA_PASSWORD
  if (!this->password_.empty()) {
    ESP_LOGCONFIG(TAG, "  Password configured");
  }
#endif
}

void ESPHomeOTAComponent::loop() {
  // Skip handle_handshake_() call if no client connected and no incoming connections
  // This optimization reduces idle loop overhead when OTA is not active
  // Note: No need to check server_ for null as the component is marked failed in setup()
  // if server_ creation fails
  if (this->client_ != nullptr || this->server_->ready()) {
    this->handle_handshake_();
  }
}

static const uint8_t FEATURE_SUPPORTS_COMPRESSION = 0x01;
#ifdef USE_OTA_SHA256
static const uint8_t FEATURE_SUPPORTS_SHA256_AUTH = 0x02;
#endif

// Temporary flag to allow MD5 downgrade for ~3 versions (until 2026.1.0)
// This allows users to downgrade via OTA if they encounter issues after updating.
// Without this, users would need to do a serial flash to downgrade.
// TODO: Remove this flag and all associated code in 2026.1.0
#define ALLOW_OTA_DOWNGRADE_MD5

void ESPHomeOTAComponent::handle_handshake_() {
  /// Handle the OTA handshake and authentication.
  ///
  /// This method is non-blocking and will return immediately if no data is available.
  /// It manages the state machine through connection, magic bytes validation, feature
  /// negotiation, and authentication before entering the blocking data transfer phase.

  if (this->client_ == nullptr) {
    // We already checked server_->ready() in loop(), so we can accept directly
    struct sockaddr_storage source_addr;
    socklen_t addr_len = sizeof(source_addr);
    int enable = 1;

    this->client_ = this->server_->accept_loop_monitored((struct sockaddr *) &source_addr, &addr_len);
    if (this->client_ == nullptr)
      return;
    int err = this->client_->setsockopt(IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int));
    if (err != 0) {
      this->log_socket_error_(LOG_STR("nodelay"));
      this->cleanup_connection_();
      return;
    }
    err = this->client_->setblocking(false);
    if (err != 0) {
      this->log_socket_error_(LOG_STR("non-blocking"));
      this->cleanup_connection_();
      return;
    }
    this->log_start_(LOG_STR("handshake"));
    this->client_connect_time_ = App.get_loop_component_start_time();
    this->handshake_buf_pos_ = 0;  // Reset handshake buffer position
    this->ota_state_ = OTAState::MAGIC_READ;
  }

  // Check for handshake timeout
  uint32_t now = App.get_loop_component_start_time();
  if (now - this->client_connect_time_ > OTA_SOCKET_TIMEOUT_HANDSHAKE) {
    ESP_LOGW(TAG, "Handshake timeout");
    this->cleanup_connection_();
    return;
  }

  switch (this->ota_state_) {
    case OTAState::MAGIC_READ: {
      // Try to read remaining magic bytes (5 total)
      if (!this->try_read_(5, LOG_STR("read magic"))) {
        return;
      }

      // Validate magic bytes
      static const uint8_t MAGIC_BYTES[5] = {0x6C, 0x26, 0xF7, 0x5C, 0x45};
      if (memcmp(this->handshake_buf_, MAGIC_BYTES, 5) != 0) {
        ESP_LOGW(TAG, "Magic bytes mismatch! 0x%02X-0x%02X-0x%02X-0x%02X-0x%02X", this->handshake_buf_[0],
                 this->handshake_buf_[1], this->handshake_buf_[2], this->handshake_buf_[3], this->handshake_buf_[4]);
        this->send_error_and_cleanup_(ota::OTA_RESPONSE_ERROR_MAGIC);
        return;
      }

      // Magic bytes valid, move to next state
      this->transition_ota_state_(OTAState::MAGIC_ACK);
      this->handshake_buf_[0] = ota::OTA_RESPONSE_OK;
      this->handshake_buf_[1] = USE_OTA_VERSION;
      [[fallthrough]];
    }

    case OTAState::MAGIC_ACK: {
      // Send OK and version - 2 bytes
      if (!this->try_write_(2, LOG_STR("ack magic"))) {
        return;
      }
      // All bytes sent, create backend and move to next state
      this->backend_ = ota::make_ota_backend();
      this->transition_ota_state_(OTAState::FEATURE_READ);
      [[fallthrough]];
    }

    case OTAState::FEATURE_READ: {
      // Read features - 1 byte
      if (!this->try_read_(1, LOG_STR("read feature"))) {
        return;
      }
      this->ota_features_ = this->handshake_buf_[0];
      ESP_LOGV(TAG, "Features: 0x%02X", this->ota_features_);
      this->transition_ota_state_(OTAState::FEATURE_ACK);
      this->handshake_buf_[0] =
          ((this->ota_features_ & FEATURE_SUPPORTS_COMPRESSION) != 0 && this->backend_->supports_compression())
              ? ota::OTA_RESPONSE_SUPPORTS_COMPRESSION
              : ota::OTA_RESPONSE_HEADER_OK;
      [[fallthrough]];
    }

    case OTAState::FEATURE_ACK: {
      // Acknowledge header - 1 byte
      if (!this->try_write_(1, LOG_STR("ack feature"))) {
        return;
      }
#ifdef USE_OTA_PASSWORD
      // If password is set, move to auth phase
      if (!this->password_.empty()) {
        this->transition_ota_state_(OTAState::AUTH_SEND);
      } else
#endif
      {
        // No password, move directly to data phase
        this->transition_ota_state_(OTAState::DATA);
      }
      [[fallthrough]];
    }

#ifdef USE_OTA_PASSWORD
    case OTAState::AUTH_SEND: {
      // Non-blocking authentication send
      if (!this->handle_auth_send_()) {
        return;
      }
      this->transition_ota_state_(OTAState::AUTH_READ);
      [[fallthrough]];
    }

    case OTAState::AUTH_READ: {
      // Non-blocking authentication read & verify
      if (!this->handle_auth_read_()) {
        return;
      }
      this->transition_ota_state_(OTAState::DATA);
      [[fallthrough]];
    }
#endif

    case OTAState::DATA:
      this->handle_data_();
      return;

    default:
      break;
  }
}

void ESPHomeOTAComponent::handle_data_() {
  /// Handle the OTA data transfer and update process.
  ///
  /// This method is blocking and will not return until the OTA update completes,
  /// fails, or times out. It receives the firmware data, writes it to flash,
  /// and reboots on success.
  ///
  /// Authentication has already been handled in the non-blocking states AUTH_SEND/AUTH_READ.
  ota::OTAResponseTypes error_code = ota::OTA_RESPONSE_ERROR_UNKNOWN;
  bool update_started = false;
  size_t total = 0;
  uint32_t last_progress = 0;
  uint8_t buf[OTA_BUFFER_SIZE];
  char *sbuf = reinterpret_cast<char *>(buf);
  size_t ota_size;
#if USE_OTA_VERSION == 2
  size_t size_acknowledged = 0;
#endif

  // Acknowledge auth OK - 1 byte
  this->write_byte_(ota::OTA_RESPONSE_AUTH_OK);

  // Read size, 4 bytes MSB first
  if (!this->readall_(buf, 4)) {
    this->log_read_error_(LOG_STR("size"));
    goto error;  // NOLINT(cppcoreguidelines-avoid-goto)
  }
  ota_size = (static_cast<size_t>(buf[0]) << 24) | (static_cast<size_t>(buf[1]) << 16) |
             (static_cast<size_t>(buf[2]) << 8) | buf[3];
  ESP_LOGV(TAG, "Size is %u bytes", ota_size);

  // Now that we've passed authentication and are actually
  // starting the update, set the warning status and notify
  // listeners. This ensures that port scanners do not
  // accidentally trigger the update process.
  this->log_start_(LOG_STR("update"));
  this->status_set_warning();
#ifdef USE_OTA_STATE_CALLBACK
  this->state_callback_.call(ota::OTA_STARTED, 0.0f, 0);
#endif

  // This will block for a few seconds as it locks flash
  error_code = this->backend_->begin(ota_size);
  if (error_code != ota::OTA_RESPONSE_OK)
    goto error;  // NOLINT(cppcoreguidelines-avoid-goto)
  update_started = true;

  // Acknowledge prepare OK - 1 byte
  this->write_byte_(ota::OTA_RESPONSE_UPDATE_PREPARE_OK);

  // Read binary MD5, 32 bytes
  if (!this->readall_(buf, 32)) {
    this->log_read_error_(LOG_STR("MD5 checksum"));
    goto error;  // NOLINT(cppcoreguidelines-avoid-goto)
  }
  sbuf[32] = '\0';
  ESP_LOGV(TAG, "Update: Binary MD5 is %s", sbuf);
  this->backend_->set_update_md5(sbuf);

  // Acknowledge MD5 OK - 1 byte
  this->write_byte_(ota::OTA_RESPONSE_BIN_MD5_OK);

  while (total < ota_size) {
    // TODO: timeout check
    size_t remaining = ota_size - total;
    size_t requested = remaining < OTA_BUFFER_SIZE ? remaining : OTA_BUFFER_SIZE;
    ssize_t read = this->client_->read(buf, requested);
    if (read == -1) {
      if (this->would_block_(errno)) {
        this->yield_and_feed_watchdog_();
        continue;
      }
      ESP_LOGW(TAG, "Read err %d", errno);
      goto error;  // NOLINT(cppcoreguidelines-avoid-goto)
    } else if (read == 0) {
      ESP_LOGW(TAG, "Remote closed");
      goto error;  // NOLINT(cppcoreguidelines-avoid-goto)
    }

    error_code = this->backend_->write(buf, read);
    if (error_code != ota::OTA_RESPONSE_OK) {
      ESP_LOGW(TAG, "Flash write err %d", error_code);
      goto error;  // NOLINT(cppcoreguidelines-avoid-goto)
    }
    total += read;
#if USE_OTA_VERSION == 2
    while (size_acknowledged + OTA_BLOCK_SIZE <= total || (total == ota_size && size_acknowledged < ota_size)) {
      this->write_byte_(ota::OTA_RESPONSE_CHUNK_OK);
      size_acknowledged += OTA_BLOCK_SIZE;
    }
#endif

    uint32_t now = millis();
    if (now - last_progress > 1000) {
      last_progress = now;
      float percentage = (total * 100.0f) / ota_size;
      ESP_LOGD(TAG, "Progress: %0.1f%%", percentage);
#ifdef USE_OTA_STATE_CALLBACK
      this->state_callback_.call(ota::OTA_IN_PROGRESS, percentage, 0);
#endif
      // feed watchdog and give other tasks a chance to run
      this->yield_and_feed_watchdog_();
    }
  }

  // Acknowledge receive OK - 1 byte
  this->write_byte_(ota::OTA_RESPONSE_RECEIVE_OK);

  error_code = this->backend_->end();
  if (error_code != ota::OTA_RESPONSE_OK) {
    ESP_LOGW(TAG, "End update err %d", error_code);
    goto error;  // NOLINT(cppcoreguidelines-avoid-goto)
  }

  // Acknowledge Update end OK - 1 byte
  this->write_byte_(ota::OTA_RESPONSE_UPDATE_END_OK);

  // Read ACK
  if (!this->readall_(buf, 1) || buf[0] != ota::OTA_RESPONSE_OK) {
    this->log_read_error_(LOG_STR("ack"));
    // do not go to error, this is not fatal
  }

  this->cleanup_connection_();
  delay(10);
  ESP_LOGI(TAG, "Update complete");
  this->status_clear_warning();
#ifdef USE_OTA_STATE_CALLBACK
  this->state_callback_.call(ota::OTA_COMPLETED, 100.0f, 0);
#endif
  delay(100);  // NOLINT
  App.safe_reboot();

error:
  this->write_byte_(static_cast<uint8_t>(error_code));
  this->cleanup_connection_();

  if (this->backend_ != nullptr && update_started) {
    this->backend_->abort();
  }

  this->status_momentary_error("err", 5000);
#ifdef USE_OTA_STATE_CALLBACK
  this->state_callback_.call(ota::OTA_ERROR, 0.0f, static_cast<uint8_t>(error_code));
#endif
}

bool ESPHomeOTAComponent::readall_(uint8_t *buf, size_t len) {
  uint32_t start = millis();
  uint32_t at = 0;
  while (len - at > 0) {
    uint32_t now = millis();
    if (now - start > OTA_SOCKET_TIMEOUT_DATA) {
      ESP_LOGW(TAG, "Timeout reading %d bytes", len);
      return false;
    }

    ssize_t read = this->client_->read(buf + at, len - at);
    if (read == -1) {
      if (!this->would_block_(errno)) {
        ESP_LOGW(TAG, "Read err %d bytes, errno %d", len, errno);
        return false;
      }
    } else if (read == 0) {
      ESP_LOGW(TAG, "Remote closed");
      return false;
    } else {
      at += read;
    }
    this->yield_and_feed_watchdog_();
  }

  return true;
}
bool ESPHomeOTAComponent::writeall_(const uint8_t *buf, size_t len) {
  uint32_t start = millis();
  uint32_t at = 0;
  while (len - at > 0) {
    uint32_t now = millis();
    if (now - start > OTA_SOCKET_TIMEOUT_DATA) {
      ESP_LOGW(TAG, "Timeout writing %d bytes", len);
      return false;
    }

    ssize_t written = this->client_->write(buf + at, len - at);
    if (written == -1) {
      if (!this->would_block_(errno)) {
        ESP_LOGW(TAG, "Write err %d bytes, errno %d", len, errno);
        return false;
      }
    } else {
      at += written;
    }
    this->yield_and_feed_watchdog_();
  }
  return true;
}

float ESPHomeOTAComponent::get_setup_priority() const { return setup_priority::AFTER_WIFI; }
uint16_t ESPHomeOTAComponent::get_port() const { return this->port_; }
void ESPHomeOTAComponent::set_port(uint16_t port) { this->port_ = port; }

void ESPHomeOTAComponent::log_socket_error_(const LogString *msg) {
  ESP_LOGW(TAG, "Socket %s: errno %d", LOG_STR_ARG(msg), errno);
}

void ESPHomeOTAComponent::log_read_error_(const LogString *what) { ESP_LOGW(TAG, "Read %s failed", LOG_STR_ARG(what)); }

void ESPHomeOTAComponent::log_start_(const LogString *phase) {
  ESP_LOGD(TAG, "Starting %s from %s", LOG_STR_ARG(phase), this->client_->getpeername().c_str());
}

void ESPHomeOTAComponent::log_remote_closed_(const LogString *during) {
  ESP_LOGW(TAG, "Remote closed at %s", LOG_STR_ARG(during));
}

bool ESPHomeOTAComponent::handle_read_error_(ssize_t read, const LogString *desc) {
  if (read == -1 && this->would_block_(errno)) {
    return false;  // No data yet, try again next loop
  }

  if (read <= 0) {
    read == 0 ? this->log_remote_closed_(desc) : this->log_socket_error_(desc);
    this->cleanup_connection_();
    return false;
  }
  return true;
}

bool ESPHomeOTAComponent::handle_write_error_(ssize_t written, const LogString *desc) {
  if (written == -1) {
    if (this->would_block_(errno)) {
      return false;  // Try again next loop
    }
    this->log_socket_error_(desc);
    this->cleanup_connection_();
    return false;
  }
  return true;
}

bool ESPHomeOTAComponent::try_read_(size_t to_read, const LogString *desc) {
  // Read bytes into handshake buffer, starting at handshake_buf_pos_
  size_t bytes_to_read = to_read - this->handshake_buf_pos_;
  ssize_t read = this->client_->read(this->handshake_buf_ + this->handshake_buf_pos_, bytes_to_read);

  if (!this->handle_read_error_(read, desc)) {
    return false;
  }

  this->handshake_buf_pos_ += read;
  // Return true only if we have all the requested bytes
  return this->handshake_buf_pos_ >= to_read;
}

bool ESPHomeOTAComponent::try_write_(size_t to_write, const LogString *desc) {
  // Write bytes from handshake buffer, starting at handshake_buf_pos_
  size_t bytes_to_write = to_write - this->handshake_buf_pos_;
  ssize_t written = this->client_->write(this->handshake_buf_ + this->handshake_buf_pos_, bytes_to_write);

  if (!this->handle_write_error_(written, desc)) {
    return false;
  }

  this->handshake_buf_pos_ += written;
  // Return true only if we have written all the requested bytes
  return this->handshake_buf_pos_ >= to_write;
}

void ESPHomeOTAComponent::cleanup_connection_() {
  this->client_->close();
  this->client_ = nullptr;
  this->client_connect_time_ = 0;
  this->handshake_buf_pos_ = 0;
  this->ota_state_ = OTAState::IDLE;
  this->ota_features_ = 0;
  this->backend_ = nullptr;
#ifdef USE_OTA_PASSWORD
  this->cleanup_auth_();
#endif
}

void ESPHomeOTAComponent::yield_and_feed_watchdog_() {
  App.feed_wdt();
  delay(1);
}

#ifdef USE_OTA_PASSWORD
void ESPHomeOTAComponent::log_auth_warning_(const LogString *msg) { ESP_LOGW(TAG, "Auth: %s", LOG_STR_ARG(msg)); }

bool ESPHomeOTAComponent::select_auth_type_() {
#ifdef USE_OTA_SHA256
  bool client_supports_sha256 = (this->ota_features_ & FEATURE_SUPPORTS_SHA256_AUTH) != 0;

#ifdef ALLOW_OTA_DOWNGRADE_MD5
  // Allow fallback to MD5 if client doesn't support SHA256
  if (client_supports_sha256) {
    this->auth_type_ = ota::OTA_RESPONSE_REQUEST_SHA256_AUTH;
    return true;
  }
#ifdef USE_OTA_MD5
  this->log_auth_warning_(LOG_STR("Using deprecated MD5"));
  this->auth_type_ = ota::OTA_RESPONSE_REQUEST_AUTH;
  return true;
#else
  this->log_auth_warning_(LOG_STR("SHA256 required"));
  this->send_error_and_cleanup_(ota::OTA_RESPONSE_ERROR_AUTH_INVALID);
  return false;
#endif  // USE_OTA_MD5

#else   // !ALLOW_OTA_DOWNGRADE_MD5
  // Require SHA256
  if (!client_supports_sha256) {
    this->log_auth_warning_(LOG_STR("SHA256 required"));
    this->send_error_and_cleanup_(ota::OTA_RESPONSE_ERROR_AUTH_INVALID);
    return false;
  }
  this->auth_type_ = ota::OTA_RESPONSE_REQUEST_SHA256_AUTH;
  return true;
#endif  // ALLOW_OTA_DOWNGRADE_MD5

#else  // !USE_OTA_SHA256
#ifdef USE_OTA_MD5
  // Only MD5 available
  this->auth_type_ = ota::OTA_RESPONSE_REQUEST_AUTH;
  return true;
#else
  // No auth methods available
  this->log_auth_warning_(LOG_STR("No auth methods available"));
  this->send_error_and_cleanup_(ota::OTA_RESPONSE_ERROR_AUTH_INVALID);
  return false;
#endif  // USE_OTA_MD5
#endif  // USE_OTA_SHA256
}

bool ESPHomeOTAComponent::handle_auth_send_() {
  // Initialize auth buffer if not already done
  if (!this->auth_buf_) {
    // Select auth type based on client capabilities and configuration
    if (!this->select_auth_type_()) {
      return false;
    }

    // Generate nonce - hasher must be created and used in same stack frame
    // CRITICAL ESP32-S3 HARDWARE SHA ACCELERATION REQUIREMENTS:
    // 1. Hash objects must NEVER be passed to another function (different stack frame)
    // 2. NO Variable Length Arrays (VLAs) - they corrupt the stack with hardware DMA
    // 3. All hash operations (init/add/calculate) must happen in the SAME function where object is created
    // Violating these causes truncated hash output (20 bytes instead of 32) or memory corruption.
    //
    // Buffer layout after AUTH_READ completes:
    //   [0]: auth_type (1 byte)
    //   [1...hex_size]: nonce (hex_size bytes) - our random nonce sent in AUTH_SEND
    //   [1+hex_size...1+2*hex_size-1]: cnonce (hex_size bytes) - client's nonce
    //   [1+2*hex_size...1+3*hex_size-1]: response (hex_size bytes) - client's hash

    // Declare both hash objects in same stack frame, use pointer to select.
    // NOTE: Both objects are declared here even though only one is used. This is REQUIRED for ESP32-S3
    // hardware SHA acceleration - the object must exist in this stack frame for all operations.
    // Do NOT try to "optimize" by creating the object inside the if block, as it would go out of scope.
#ifdef USE_OTA_SHA256
    sha256::SHA256 sha_hasher;
#endif
#ifdef USE_OTA_MD5
    md5::MD5Digest md5_hasher;
#endif
    HashBase *hasher = nullptr;

#ifdef USE_OTA_SHA256
    if (this->auth_type_ == ota::OTA_RESPONSE_REQUEST_SHA256_AUTH) {
      hasher = &sha_hasher;
    }
#endif
#ifdef USE_OTA_MD5
    if (this->auth_type_ == ota::OTA_RESPONSE_REQUEST_AUTH) {
      hasher = &md5_hasher;
    }
#endif

    const size_t hex_size = hasher->get_size() * 2;
    const size_t nonce_len = hasher->get_size() / 4;
    const size_t auth_buf_size = 1 + 3 * hex_size;
    this->auth_buf_ = std::make_unique<uint8_t[]>(auth_buf_size);
    this->auth_buf_pos_ = 0;

    char *buf = reinterpret_cast<char *>(this->auth_buf_.get() + 1);
    if (!random_bytes(reinterpret_cast<uint8_t *>(buf), nonce_len)) {
      this->log_auth_warning_(LOG_STR("Random failed"));
      this->send_error_and_cleanup_(ota::OTA_RESPONSE_ERROR_UNKNOWN);
      return false;
    }

    hasher->init();
    hasher->add(buf, nonce_len);
    hasher->calculate();
    this->auth_buf_[0] = this->auth_type_;
    hasher->get_hex(buf);

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
    char log_buf[65];  // Fixed size for SHA256 hex (64) + null, works for MD5 (32) too
    memcpy(log_buf, buf, hex_size);
    log_buf[hex_size] = '\0';
    ESP_LOGV(TAG, "Auth: Nonce is %s", log_buf);
#endif
  }

  // Try to write auth_type + nonce
  size_t hex_size = this->get_auth_hex_size_();
  const size_t to_write = 1 + hex_size;
  size_t remaining = to_write - this->auth_buf_pos_;

  ssize_t written = this->client_->write(this->auth_buf_.get() + this->auth_buf_pos_, remaining);
  if (!this->handle_write_error_(written, LOG_STR("ack auth"))) {
    return false;
  }

  this->auth_buf_pos_ += written;

  // Check if we still have more to write
  if (this->auth_buf_pos_ < to_write) {
    return false;  // More to write, try again next loop
  }

  // All written, prepare for reading phase
  this->auth_buf_pos_ = 0;
  return true;
}

bool ESPHomeOTAComponent::handle_auth_read_() {
  size_t hex_size = this->get_auth_hex_size_();
  const size_t to_read = hex_size * 2;  // CNonce + Response

  // Try to read remaining bytes (CNonce + Response)
  // We read cnonce+response starting at offset 1+hex_size (after auth_type and our nonce)
  size_t cnonce_offset = 1 + hex_size;  // Offset where cnonce should be stored in buffer
  size_t remaining = to_read - this->auth_buf_pos_;
  ssize_t read = this->client_->read(this->auth_buf_.get() + cnonce_offset + this->auth_buf_pos_, remaining);

  if (!this->handle_read_error_(read, LOG_STR("read auth"))) {
    return false;
  }

  this->auth_buf_pos_ += read;

  // Check if we still need more data
  if (this->auth_buf_pos_ < to_read) {
    return false;  // More to read, try again next loop
  }

  // We have all the data, verify it
  const char *nonce = reinterpret_cast<char *>(this->auth_buf_.get() + 1);
  const char *cnonce = nonce + hex_size;
  const char *response = cnonce + hex_size;

  // CRITICAL ESP32-S3: Hash objects must stay in same stack frame (no passing to other functions).
  // Declare both hash objects in same stack frame, use pointer to select.
  // NOTE: Both objects are declared here even though only one is used. This is REQUIRED for ESP32-S3
  // hardware SHA acceleration - the object must exist in this stack frame for all operations.
  // Do NOT try to "optimize" by creating the object inside the if block, as it would go out of scope.
#ifdef USE_OTA_SHA256
  sha256::SHA256 sha_hasher;
#endif
#ifdef USE_OTA_MD5
  md5::MD5Digest md5_hasher;
#endif
  HashBase *hasher = nullptr;

#ifdef USE_OTA_SHA256
  if (this->auth_type_ == ota::OTA_RESPONSE_REQUEST_SHA256_AUTH) {
    hasher = &sha_hasher;
  }
#endif
#ifdef USE_OTA_MD5
  if (this->auth_type_ == ota::OTA_RESPONSE_REQUEST_AUTH) {
    hasher = &md5_hasher;
  }
#endif

  hasher->init();
  hasher->add(this->password_.c_str(), this->password_.length());
  hasher->add(nonce, hex_size * 2);  // Add both nonce and cnonce (contiguous in buffer)
  hasher->calculate();

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
  char log_buf[65];  // Fixed size for SHA256 hex (64) + null, works for MD5 (32) too
  // Log CNonce
  memcpy(log_buf, cnonce, hex_size);
  log_buf[hex_size] = '\0';
  ESP_LOGV(TAG, "Auth: CNonce is %s", log_buf);

  // Log computed hash
  hasher->get_hex(log_buf);
  log_buf[hex_size] = '\0';
  ESP_LOGV(TAG, "Auth: Result is %s", log_buf);

  // Log received response
  memcpy(log_buf, response, hex_size);
  log_buf[hex_size] = '\0';
  ESP_LOGV(TAG, "Auth: Response is %s", log_buf);
#endif

  // Compare response
  bool matches = hasher->equals_hex(response);

  if (!matches) {
    this->log_auth_warning_(LOG_STR("Password mismatch"));
    this->send_error_and_cleanup_(ota::OTA_RESPONSE_ERROR_AUTH_INVALID);
    return false;
  }

  // Authentication successful - clean up auth state
  this->cleanup_auth_();

  return true;
}

size_t ESPHomeOTAComponent::get_auth_hex_size_() const {
#ifdef USE_OTA_SHA256
  if (this->auth_type_ == ota::OTA_RESPONSE_REQUEST_SHA256_AUTH) {
    return SHA256_HEX_SIZE;
  }
#endif
#ifdef USE_OTA_MD5
  return MD5_HEX_SIZE;
#else
#ifndef USE_OTA_SHA256
#error "Either USE_OTA_MD5 or USE_OTA_SHA256 must be defined when USE_OTA_PASSWORD is enabled"
#endif
#endif
}

void ESPHomeOTAComponent::cleanup_auth_() {
  this->auth_buf_ = nullptr;
  this->auth_buf_pos_ = 0;
  this->auth_type_ = 0;
}
#endif  // USE_OTA_PASSWORD

}  // namespace esphome
#endif
