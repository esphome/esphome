#include "ota_esphome.h"
#ifdef USE_OTA
#include "esphome/components/md5/md5.h"
#include "esphome/components/network/util.h"
#include "esphome/components/ota/ota_backend.h"
#include "esphome/components/ota/ota_backend_arduino_esp32.h"
#include "esphome/components/ota/ota_backend_arduino_esp8266.h"
#include "esphome/components/ota/ota_backend_arduino_libretiny.h"
#include "esphome/components/ota/ota_backend_arduino_rp2040.h"
#include "esphome/components/ota/ota_backend_esp_idf.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
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
    this->log_socket_error_("creation");
    this->mark_failed();
    return;
  }
  int enable = 1;
  int err = this->server_->setsockopt(SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
  if (err != 0) {
    this->log_socket_error_("reuseaddr");
    // we can still continue
  }
  err = this->server_->setblocking(false);
  if (err != 0) {
    this->log_socket_error_("non-blocking");
    this->mark_failed();
    return;
  }

  struct sockaddr_storage server;

  socklen_t sl = socket::set_sockaddr_any((struct sockaddr *) &server, sizeof(server), this->port_);
  if (sl == 0) {
    this->log_socket_error_("set sockaddr");
    this->mark_failed();
    return;
  }

  err = this->server_->bind((struct sockaddr *) &server, sizeof(server));
  if (err != 0) {
    this->log_socket_error_("bind");
    this->mark_failed();
    return;
  }

  err = this->server_->listen(4);
  if (err != 0) {
    this->log_socket_error_("listen");
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
      this->log_socket_error_("nodelay");
      this->cleanup_connection_();
      return;
    }
    err = this->client_->setblocking(false);
    if (err != 0) {
      this->log_socket_error_("non-blocking");
      this->cleanup_connection_();
      return;
    }
    this->log_start_("handshake");
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
        this->log_socket_error_("reading magic bytes");
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
    this->log_read_error_("features");
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
    buf[0] = ota::OTA_RESPONSE_REQUEST_AUTH;
    this->writeall_(buf, 1);
    md5::MD5Digest md5{};
    md5.init();
    sprintf(sbuf, "%08" PRIx32, random_uint32());
    md5.add(sbuf, 8);
    md5.calculate();
    md5.get_hex(sbuf);
    ESP_LOGV(TAG, "Auth: Nonce is %s", sbuf);

    // Send nonce, 32 bytes hex MD5
    if (!this->writeall_(reinterpret_cast<uint8_t *>(sbuf), 32)) {
      ESP_LOGW(TAG, "Auth: Writing nonce failed");
      goto error;  // NOLINT(cppcoreguidelines-avoid-goto)
    }

    // prepare challenge
    md5.init();
    md5.add(this->password_.c_str(), this->password_.length());
    // add nonce
    md5.add(sbuf, 32);

    // Receive cnonce, 32 bytes hex MD5
    if (!this->readall_(buf, 32)) {
      ESP_LOGW(TAG, "Auth: Reading cnonce failed");
      goto error;  // NOLINT(cppcoreguidelines-avoid-goto)
    }
    sbuf[32] = '\0';
    ESP_LOGV(TAG, "Auth: CNonce is %s", sbuf);
    // add cnonce
    md5.add(sbuf, 32);

    // calculate result
    md5.calculate();
    md5.get_hex(sbuf);
    ESP_LOGV(TAG, "Auth: Result is %s", sbuf);

    // Receive result, 32 bytes hex MD5
    if (!this->readall_(buf + 64, 32)) {
      ESP_LOGW(TAG, "Auth: Reading response failed");
      goto error;  // NOLINT(cppcoreguidelines-avoid-goto)
    }
    sbuf[64 + 32] = '\0';
    ESP_LOGV(TAG, "Auth: Response is %s", sbuf + 64);

    bool matches = true;
    for (uint8_t i = 0; i < 32; i++)
      matches = matches && buf[i] == buf[64 + i];

    if (!matches) {
      ESP_LOGW(TAG, "Auth failed! Passwords do not match");
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
    this->log_read_error_("size");
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
  this->log_start_("update");
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
    this->log_read_error_("MD5 checksum");
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
    this->log_read_error_("ack");
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

void ESPHomeOTAComponent::log_socket_error_(const char *msg) { ESP_LOGW(TAG, "Socket %s: errno %d", msg, errno); }

void ESPHomeOTAComponent::log_read_error_(const char *what) { ESP_LOGW(TAG, "Read %s failed", what); }

void ESPHomeOTAComponent::log_start_(const char *phase) {
  ESP_LOGD(TAG, "Starting %s from %s", phase, this->client_->getpeername().c_str());
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

}  // namespace esphome
#endif
