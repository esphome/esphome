#include "ota_esphome.h"
#ifdef USE_OTA
#ifdef USE_OTA_MD5
#include "esphome/components/md5/md5.h"
#endif
#ifdef USE_OTA_SHA256
#include "esphome/components/sha256/sha256.h"
#endif
#include "esphome/components/network/util.h"
#include "esphome/components/ota/ota_backend.h"
#include "esphome/components/ota/ota_backend_arduino_esp32.h"
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
static constexpr uint32_t OTA_SOCKET_TIMEOUT_HANDSHAKE = 10000;  // milliseconds for initial handshake
static constexpr uint32_t OTA_SOCKET_TIMEOUT_DATA = 90000;       // milliseconds for data transfer

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

  err = this->server_->listen(4);
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
                network::get_use_address().c_str(), this->port_, USE_OTA_VERSION);
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
  /// Handle the initial OTA handshake.
  ///
  /// This method is non-blocking and will return immediately if no data is available.
  /// It reads all 5 magic bytes (0x6C, 0x26, 0xF7, 0x5C, 0x45) non-blocking
  /// before proceeding to handle_data_(). A 10-second timeout is enforced from initial connection.

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
    this->magic_buf_pos_ = 0;  // Reset magic buffer position
  }

  // Check for handshake timeout
  uint32_t now = App.get_loop_component_start_time();
  if (now - this->client_connect_time_ > OTA_SOCKET_TIMEOUT_HANDSHAKE) {
    ESP_LOGW(TAG, "Handshake timeout");
    this->cleanup_connection_();
    return;
  }

  // Try to read remaining magic bytes
  if (this->magic_buf_pos_ < 5) {
    // Read as many bytes as available
    uint8_t bytes_to_read = 5 - this->magic_buf_pos_;
    ssize_t read = this->client_->read(this->magic_buf_ + this->magic_buf_pos_, bytes_to_read);

    if (read == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      return;  // No data yet, try again next loop
    }

    if (read <= 0) {
      // Error or connection closed
      if (read == -1) {
        this->log_socket_error_(LOG_STR("reading magic bytes"));
      } else {
        ESP_LOGW(TAG, "Remote closed during handshake");
      }
      this->cleanup_connection_();
      return;
    }

    this->magic_buf_pos_ += read;
  }

  // Check if we have all 5 magic bytes
  if (this->magic_buf_pos_ == 5) {
    // Validate magic bytes
    static const uint8_t MAGIC_BYTES[5] = {0x6C, 0x26, 0xF7, 0x5C, 0x45};
    if (memcmp(this->magic_buf_, MAGIC_BYTES, 5) != 0) {
      ESP_LOGW(TAG, "Magic bytes mismatch! 0x%02X-0x%02X-0x%02X-0x%02X-0x%02X", this->magic_buf_[0],
               this->magic_buf_[1], this->magic_buf_[2], this->magic_buf_[3], this->magic_buf_[4]);
      // Send error response (non-blocking, best effort)
      uint8_t error = static_cast<uint8_t>(ota::OTA_RESPONSE_ERROR_MAGIC);
      this->client_->write(&error, 1);
      this->cleanup_connection_();
      return;
    }

    // All 5 magic bytes are valid, continue with data handling
    this->handle_data_();
  }
}

void ESPHomeOTAComponent::handle_data_() {
  /// Handle the OTA data transfer and update process.
  ///
  /// This method is blocking and will not return until the OTA update completes,
  /// fails, or times out. It handles authentication, receives the firmware data,
  /// writes it to flash, and reboots on success.
  ota::OTAResponseTypes error_code = ota::OTA_RESPONSE_ERROR_UNKNOWN;
  bool update_started = false;
  size_t total = 0;
  uint32_t last_progress = 0;
  uint8_t buf[1024];
  char *sbuf = reinterpret_cast<char *>(buf);
  size_t ota_size;
  uint8_t ota_features;
  std::unique_ptr<ota::OTABackend> backend;
  (void) ota_features;
#if USE_OTA_VERSION == 2
  size_t size_acknowledged = 0;
#endif

  // Send OK and version - 2 bytes
  buf[0] = ota::OTA_RESPONSE_OK;
  buf[1] = USE_OTA_VERSION;
  this->writeall_(buf, 2);

  backend = ota::make_ota_backend();

  // Read features - 1 byte
  if (!this->readall_(buf, 1)) {
    this->log_read_error_(LOG_STR("features"));
    goto error;  // NOLINT(cppcoreguidelines-avoid-goto)
  }
  ota_features = buf[0];  // NOLINT
  ESP_LOGV(TAG, "Features: 0x%02X", ota_features);

  // Acknowledge header - 1 byte
  buf[0] = ota::OTA_RESPONSE_HEADER_OK;
  if ((ota_features & FEATURE_SUPPORTS_COMPRESSION) != 0 && backend->supports_compression()) {
    buf[0] = ota::OTA_RESPONSE_SUPPORTS_COMPRESSION;
  }

  this->writeall_(buf, 1);

#ifdef USE_OTA_PASSWORD
  if (!this->password_.empty()) {
    bool auth_success = false;

#ifdef USE_OTA_SHA256
    // SECURITY HARDENING: Prefer SHA256 authentication on platforms that support it.
    //
    // This is a hardening measure to prevent future downgrade attacks where an attacker
    // could force the use of MD5 authentication by manipulating the feature flags.
    //
    // While MD5 is currently still acceptable for our OTA authentication use case
    // (where the password is a shared secret and we're only authenticating, not
    // encrypting), at some point in the future MD5 will likely become so weak that
    // it could be practically attacked.
    //
    // We enforce SHA256 now on capable platforms because:
    // 1. We can't retroactively update device firmware in the field
    // 2. Clients (like esphome CLI) can always be updated to support SHA256
    // 3. This prevents any possibility of downgrade attacks in the future
    //
    // Devices that don't support SHA256 (due to platform limitations) will
    // continue to use MD5 as their only option (see #else branch below).

    bool client_supports_sha256 = (ota_features & FEATURE_SUPPORTS_SHA256_AUTH) != 0;

#ifdef ALLOW_OTA_DOWNGRADE_MD5
    // Temporary compatibility mode: Allow MD5 for ~3 versions to enable OTA downgrades
    // This prevents users from being locked out if they need to downgrade after updating
    // TODO: Remove this entire ifdef block in 2026.1.0
    if (client_supports_sha256) {
      sha256::SHA256 sha_hasher;
      auth_success = this->perform_hash_auth_(&sha_hasher, this->password_, ota::OTA_RESPONSE_REQUEST_SHA256_AUTH,
                                              LOG_STR("SHA256"), sbuf);
    } else {
#ifdef USE_OTA_MD5
      ESP_LOGW(TAG, "Using MD5 auth for compatibility (deprecated)");
      md5::MD5Digest md5_hasher;
      auth_success =
          this->perform_hash_auth_(&md5_hasher, this->password_, ota::OTA_RESPONSE_REQUEST_AUTH, LOG_STR("MD5"), sbuf);
#endif  // USE_OTA_MD5
    }
#else
    // Strict mode: SHA256 required on capable platforms (future default)
    if (!client_supports_sha256) {
      ESP_LOGW(TAG, "Client requires SHA256");
      error_code = ota::OTA_RESPONSE_ERROR_AUTH_INVALID;
      goto error;  // NOLINT(cppcoreguidelines-avoid-goto)
    }
    sha256::SHA256 sha_hasher;
    auth_success = this->perform_hash_auth_(&sha_hasher, this->password_, ota::OTA_RESPONSE_REQUEST_SHA256_AUTH,
                                            LOG_STR("SHA256"), sbuf);
#endif  // ALLOW_OTA_DOWNGRADE_MD5
#else
    // Platform only supports MD5 - use it as the only available option
    // This is not a security downgrade as the platform cannot support SHA256
#ifdef USE_OTA_MD5
    md5::MD5Digest md5_hasher;
    auth_success =
        this->perform_hash_auth_(&md5_hasher, this->password_, ota::OTA_RESPONSE_REQUEST_AUTH, LOG_STR("MD5"), sbuf);
#endif  // USE_OTA_MD5
#endif  // USE_OTA_SHA256

    if (!auth_success) {
      error_code = ota::OTA_RESPONSE_ERROR_AUTH_INVALID;
      goto error;  // NOLINT(cppcoreguidelines-avoid-goto)
    }
  }
#endif  // USE_OTA_PASSWORD

  // Acknowledge auth OK - 1 byte
  buf[0] = ota::OTA_RESPONSE_AUTH_OK;
  this->writeall_(buf, 1);

  // Read size, 4 bytes MSB first
  if (!this->readall_(buf, 4)) {
    this->log_read_error_(LOG_STR("size"));
    goto error;  // NOLINT(cppcoreguidelines-avoid-goto)
  }
  ota_size = 0;
  for (uint8_t i = 0; i < 4; i++) {
    ota_size <<= 8;
    ota_size |= buf[i];
  }
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
  error_code = backend->begin(ota_size);
  if (error_code != ota::OTA_RESPONSE_OK)
    goto error;  // NOLINT(cppcoreguidelines-avoid-goto)
  update_started = true;

  // Acknowledge prepare OK - 1 byte
  buf[0] = ota::OTA_RESPONSE_UPDATE_PREPARE_OK;
  this->writeall_(buf, 1);

  // Read binary MD5, 32 bytes
  if (!this->readall_(buf, 32)) {
    this->log_read_error_(LOG_STR("MD5 checksum"));
    goto error;  // NOLINT(cppcoreguidelines-avoid-goto)
  }
  sbuf[32] = '\0';
  ESP_LOGV(TAG, "Update: Binary MD5 is %s", sbuf);
  backend->set_update_md5(sbuf);

  // Acknowledge MD5 OK - 1 byte
  buf[0] = ota::OTA_RESPONSE_BIN_MD5_OK;
  this->writeall_(buf, 1);

  while (total < ota_size) {
    // TODO: timeout check
    size_t requested = std::min(sizeof(buf), ota_size - total);
    ssize_t read = this->client_->read(buf, requested);
    if (read == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        this->yield_and_feed_watchdog_();
        continue;
      }
      ESP_LOGW(TAG, "Read error, errno %d", errno);
      goto error;  // NOLINT(cppcoreguidelines-avoid-goto)
    } else if (read == 0) {
      // $ man recv
      // "When  a  stream socket peer has performed an orderly shutdown, the return value will
      // be 0 (the traditional "end-of-file" return)."
      ESP_LOGW(TAG, "Remote closed connection");
      goto error;  // NOLINT(cppcoreguidelines-avoid-goto)
    }

    error_code = backend->write(buf, read);
    if (error_code != ota::OTA_RESPONSE_OK) {
      ESP_LOGW(TAG, "Flash write error, code: %d", error_code);
      goto error;  // NOLINT(cppcoreguidelines-avoid-goto)
    }
    total += read;
#if USE_OTA_VERSION == 2
    while (size_acknowledged + OTA_BLOCK_SIZE <= total || (total == ota_size && size_acknowledged < ota_size)) {
      buf[0] = ota::OTA_RESPONSE_CHUNK_OK;
      this->writeall_(buf, 1);
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
  buf[0] = ota::OTA_RESPONSE_RECEIVE_OK;
  this->writeall_(buf, 1);

  error_code = backend->end();
  if (error_code != ota::OTA_RESPONSE_OK) {
    ESP_LOGW(TAG, "Error ending update! code: %d", error_code);
    goto error;  // NOLINT(cppcoreguidelines-avoid-goto)
  }

  // Acknowledge Update end OK - 1 byte
  buf[0] = ota::OTA_RESPONSE_UPDATE_END_OK;
  this->writeall_(buf, 1);

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
  buf[0] = static_cast<uint8_t>(error_code);
  this->writeall_(buf, 1);
  this->cleanup_connection_();

  if (backend != nullptr && update_started) {
    backend->abort();
  }

  this->status_momentary_error("onerror", 5000);
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
      if (errno != EAGAIN && errno != EWOULDBLOCK) {
        ESP_LOGW(TAG, "Error reading %d bytes, errno %d", len, errno);
        return false;
      }
    } else if (read == 0) {
      ESP_LOGW(TAG, "Remote closed connection");
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
      if (errno != EAGAIN && errno != EWOULDBLOCK) {
        ESP_LOGW(TAG, "Error writing %d bytes, errno %d", len, errno);
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

void ESPHomeOTAComponent::cleanup_connection_() {
  this->client_->close();
  this->client_ = nullptr;
  this->client_connect_time_ = 0;
  this->magic_buf_pos_ = 0;
}

void ESPHomeOTAComponent::yield_and_feed_watchdog_() {
  App.feed_wdt();
  delay(1);
}

#ifdef USE_OTA_PASSWORD
void ESPHomeOTAComponent::log_auth_warning_(const LogString *action, const LogString *hash_name) {
  ESP_LOGW(TAG, "Auth: %s %s failed", LOG_STR_ARG(action), LOG_STR_ARG(hash_name));
}

// Non-template function definition to reduce binary size
bool ESPHomeOTAComponent::perform_hash_auth_(HashBase *hasher, const std::string &password, uint8_t auth_request,
                                             const LogString *name, char *buf) {
  // Get sizes from the hasher
  const size_t hex_size = hasher->get_size() * 2;   // Hex is twice the byte size
  const size_t nonce_len = hasher->get_size() / 4;  // Nonce is 1/4 of hash size in bytes

  // Use the provided buffer for all hex operations

  // Small stack buffer for nonce seed bytes
  uint8_t nonce_bytes[8];  // Max 8 bytes (2 x uint32_t for SHA256)

  // Send auth request type
  this->writeall_(&auth_request, 1);

  hasher->init();

  // Generate nonce seed bytes using random_bytes
  if (!random_bytes(nonce_bytes, nonce_len)) {
    this->log_auth_warning_(LOG_STR("Random bytes generation failed"), name);
    return false;
  }
  hasher->add(nonce_bytes, nonce_len);
  hasher->calculate();

  // Generate and send nonce
  hasher->get_hex(buf);
  buf[hex_size] = '\0';
  ESP_LOGV(TAG, "Auth: %s Nonce is %s", LOG_STR_ARG(name), buf);

  if (!this->writeall_(reinterpret_cast<uint8_t *>(buf), hex_size)) {
    this->log_auth_warning_(LOG_STR("Writing nonce"), name);
    return false;
  }

  // Start challenge: password + nonce
  hasher->init();
  hasher->add(password.c_str(), password.length());
  hasher->add(buf, hex_size);

  // Read cnonce and add to hash
  if (!this->readall_(reinterpret_cast<uint8_t *>(buf), hex_size)) {
    this->log_auth_warning_(LOG_STR("Reading cnonce"), name);
    return false;
  }
  buf[hex_size] = '\0';
  ESP_LOGV(TAG, "Auth: %s CNonce is %s", LOG_STR_ARG(name), buf);

  hasher->add(buf, hex_size);
  hasher->calculate();

  // Log expected result (digest is already in hasher)
  hasher->get_hex(buf);
  buf[hex_size] = '\0';
  ESP_LOGV(TAG, "Auth: %s Result is %s", LOG_STR_ARG(name), buf);

  // Read response into the buffer
  if (!this->readall_(reinterpret_cast<uint8_t *>(buf), hex_size)) {
    this->log_auth_warning_(LOG_STR("Reading response"), name);
    return false;
  }
  buf[hex_size] = '\0';
  ESP_LOGV(TAG, "Auth: %s Response is %s", LOG_STR_ARG(name), buf);

  // Compare response directly with digest in hasher
  bool matches = hasher->equals_hex(buf);

  if (!matches) {
    this->log_auth_warning_(LOG_STR("Password mismatch"), name);
  }

  return matches;
}
#endif  // USE_OTA_PASSWORD

}  // namespace esphome
#endif
