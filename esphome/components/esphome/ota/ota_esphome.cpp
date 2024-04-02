#include "ota_esphome.h"

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
namespace ota_esphome {

static const char *const TAG = "esphome.ota";
static constexpr u_int16_t OTA_BLOCK_SIZE = 8192;

OTAESPHomeComponent *global_ota_component = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

std::unique_ptr<ota::OTABackend> make_ota_backend() {
#ifdef USE_ARDUINO
#ifdef USE_ESP8266
  return make_unique<ota::ArduinoESP8266OTABackend>();
#endif  // USE_ESP8266
#ifdef USE_ESP32
  return make_unique<ota::ArduinoESP32OTABackend>();
#endif  // USE_ESP32
#endif  // USE_ARDUINO
#ifdef USE_ESP_IDF
  return make_unique<ota::IDFOTABackend>();
#endif  // USE_ESP_IDF
#ifdef USE_RP2040
  return make_unique<ota::ArduinoRP2040OTABackend>();
#endif  // USE_RP2040
#ifdef USE_LIBRETINY
  return make_unique<ota::ArduinoLibreTinyOTABackend>();
#endif
}

OTAESPHomeComponent::OTAESPHomeComponent() { global_ota_component = this; }

void OTAESPHomeComponent::setup() {
  server_ = socket::socket_ip(SOCK_STREAM, 0);
  if (server_ == nullptr) {
    ESP_LOGW(TAG, "Could not create socket");
    this->mark_failed();
    return;
  }
  int enable = 1;
  int err = server_->setsockopt(SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
  if (err != 0) {
    ESP_LOGW(TAG, "Socket unable to set reuseaddr: errno %d", err);
    // we can still continue
  }
  err = server_->setblocking(false);
  if (err != 0) {
    ESP_LOGW(TAG, "Socket unable to set nonblocking mode: errno %d", err);
    this->mark_failed();
    return;
  }

  struct sockaddr_storage server;

  socklen_t sl = socket::set_sockaddr_any((struct sockaddr *) &server, sizeof(server), this->port_);
  if (sl == 0) {
    ESP_LOGW(TAG, "Socket unable to set sockaddr: errno %d", errno);
    this->mark_failed();
    return;
  }

  err = server_->bind((struct sockaddr *) &server, sizeof(server));
  if (err != 0) {
    ESP_LOGW(TAG, "Socket unable to bind: errno %d", errno);
    this->mark_failed();
    return;
  }

  err = server_->listen(4);
  if (err != 0) {
    ESP_LOGW(TAG, "Socket unable to listen: errno %d", errno);
    this->mark_failed();
    return;
  }
}

void OTAESPHomeComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Over-The-Air updates:");
  ESP_LOGCONFIG(TAG, "  Address: %s:%u", network::get_use_address().c_str(), this->port_);
  ESP_LOGCONFIG(TAG, "  OTA version: %d", USE_OTA_VERSION);
#ifdef USE_OTA_PASSWORD
  if (!this->password_.empty()) {
    ESP_LOGCONFIG(TAG, "  Password configured");
  }
#endif
  if (this->has_safe_mode_ && this->safe_mode_rtc_value_ > 1 &&
      this->safe_mode_rtc_value_ != OTAESPHomeComponent::ENTER_SAFE_MODE_MAGIC) {
    ESP_LOGW(TAG, "Last reset occurred too quickly; safe mode will be invoked in %" PRIu32 " restarts",
             this->safe_mode_num_attempts_ - this->safe_mode_rtc_value_);
  }
}

void OTAESPHomeComponent::loop() {
  this->handle_();

  if (this->has_safe_mode_ && (millis() - this->safe_mode_start_time_) > this->safe_mode_enable_time_) {
    this->has_safe_mode_ = false;
    // successful boot, reset counter
    ESP_LOGI(TAG, "Boot seems successful; resetting boot loop counter");
    this->clean_rtc();
  }
}

static const uint8_t FEATURE_SUPPORTS_COMPRESSION = 0x01;

void OTAESPHomeComponent::handle_() {
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

  if (client_ == nullptr) {
    struct sockaddr_storage source_addr;
    socklen_t addr_len = sizeof(source_addr);
    client_ = server_->accept((struct sockaddr *) &source_addr, &addr_len);
  }
  if (client_ == nullptr)
    return;

  int enable = 1;
  int err = client_->setsockopt(IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int));
  if (err != 0) {
    ESP_LOGW(TAG, "Socket could not enable TCP nodelay, errno %d", errno);
    return;
  }

  ESP_LOGD(TAG, "Starting OTA update from %s...", this->client_->getpeername().c_str());
  this->status_set_warning();
#ifdef USE_OTA_STATE_CALLBACK
  this->state_callback_.call(OTA_STARTED, 0.0f, 0);
#endif

  if (!this->readall_(buf, 5)) {
    ESP_LOGW(TAG, "Reading magic bytes failed");
    goto error;  // NOLINT(cppcoreguidelines-avoid-goto)
  }
  // 0x6C, 0x26, 0xF7, 0x5C, 0x45
  if (buf[0] != 0x6C || buf[1] != 0x26 || buf[2] != 0xF7 || buf[3] != 0x5C || buf[4] != 0x45) {
    ESP_LOGW(TAG, "Magic bytes do not match! 0x%02X-0x%02X-0x%02X-0x%02X-0x%02X", buf[0], buf[1], buf[2], buf[3],
             buf[4]);
    error_code = ota::OTA_RESPONSE_ERROR_MAGIC;
    goto error;  // NOLINT(cppcoreguidelines-avoid-goto)
  }

  // Send OK and version - 2 bytes
  buf[0] = ota::OTA_RESPONSE_OK;
  buf[1] = USE_OTA_VERSION;
  this->writeall_(buf, 2);

  backend = make_ota_backend();

  // Read features - 1 byte
  if (!this->readall_(buf, 1)) {
    ESP_LOGW(TAG, "Reading features failed");
    goto error;  // NOLINT(cppcoreguidelines-avoid-goto)
  }
  ota_features = buf[0];  // NOLINT
  ESP_LOGV(TAG, "OTA features is 0x%02X", ota_features);

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
    ESP_LOGW(TAG, "Reading size failed");
    goto error;  // NOLINT(cppcoreguidelines-avoid-goto)
  }
  ota_size = 0;
  for (uint8_t i = 0; i < 4; i++) {
    ota_size <<= 8;
    ota_size |= buf[i];
  }
  ESP_LOGV(TAG, "OTA size is %u bytes", ota_size);

  error_code = backend->begin(ota_size);
  if (error_code != ota::OTA_RESPONSE_OK)
    goto error;  // NOLINT(cppcoreguidelines-avoid-goto)
  update_started = true;

  // Acknowledge prepare OK - 1 byte
  buf[0] = ota::OTA_RESPONSE_UPDATE_PREPARE_OK;
  this->writeall_(buf, 1);

  // Read binary MD5, 32 bytes
  if (!this->readall_(buf, 32)) {
    ESP_LOGW(TAG, "Reading binary MD5 checksum failed");
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
        App.feed_wdt();
        delay(1);
        continue;
      }
      ESP_LOGW(TAG, "Error receiving data for update, errno %d", errno);
      goto error;  // NOLINT(cppcoreguidelines-avoid-goto)
    } else if (read == 0) {
      // $ man recv
      // "When  a  stream socket peer has performed an orderly shutdown, the return value will
      // be 0 (the traditional "end-of-file" return)."
      ESP_LOGW(TAG, "Remote end closed connection");
      goto error;  // NOLINT(cppcoreguidelines-avoid-goto)
    }

    error_code = backend->write(buf, read);
    if (error_code != ota::OTA_RESPONSE_OK) {
      ESP_LOGW(TAG, "Error writing binary data to flash!, error_code: %d", error_code);
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
      ESP_LOGD(TAG, "OTA in progress: %0.1f%%", percentage);
#ifdef USE_OTA_STATE_CALLBACK
      this->state_callback_.call(OTA_IN_PROGRESS, percentage, 0);
#endif
      // feed watchdog and give other tasks a chance to run
      App.feed_wdt();
      yield();
    }
  }

  // Acknowledge receive OK - 1 byte
  buf[0] = ota::OTA_RESPONSE_RECEIVE_OK;
  this->writeall_(buf, 1);

  error_code = backend->end();
  if (error_code != ota::OTA_RESPONSE_OK) {
    ESP_LOGW(TAG, "Error ending OTA!, error_code: %d", error_code);
    goto error;  // NOLINT(cppcoreguidelines-avoid-goto)
  }

  // Acknowledge Update end OK - 1 byte
  buf[0] = ota::OTA_RESPONSE_UPDATE_END_OK;
  this->writeall_(buf, 1);

  // Read ACK
  if (!this->readall_(buf, 1) || buf[0] != ota::OTA_RESPONSE_OK) {
    ESP_LOGW(TAG, "Reading back acknowledgement failed");
    // do not go to error, this is not fatal
  }

  this->client_->close();
  this->client_ = nullptr;
  delay(10);
  ESP_LOGI(TAG, "OTA update finished");
  this->status_clear_warning();
#ifdef USE_OTA_STATE_CALLBACK
  this->state_callback_.call(OTA_COMPLETED, 100.0f, 0);
#endif
  delay(100);  // NOLINT
  App.safe_reboot();

error:
  buf[0] = static_cast<uint8_t>(error_code);
  this->writeall_(buf, 1);
  this->client_->close();
  this->client_ = nullptr;

  if (backend != nullptr && update_started) {
    backend->abort();
  }

  this->status_momentary_error("onerror", 5000);
#ifdef USE_OTA_STATE_CALLBACK
  this->state_callback_.call(OTA_ERROR, 0.0f, static_cast<uint8_t>(error_code));
#endif
}

bool OTAESPHomeComponent::readall_(uint8_t *buf, size_t len) {
  uint32_t start = millis();
  uint32_t at = 0;
  while (len - at > 0) {
    uint32_t now = millis();
    if (now - start > 1000) {
      ESP_LOGW(TAG, "Timed out reading %d bytes of data", len);
      return false;
    }

    ssize_t read = this->client_->read(buf + at, len - at);
    if (read == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        App.feed_wdt();
        delay(1);
        continue;
      }
      ESP_LOGW(TAG, "Failed to read %d bytes of data, errno %d", len, errno);
      return false;
    } else if (read == 0) {
      ESP_LOGW(TAG, "Remote closed connection");
      return false;
    } else {
      at += read;
    }
    App.feed_wdt();
    delay(1);
  }

  return true;
}
bool OTAESPHomeComponent::writeall_(const uint8_t *buf, size_t len) {
  uint32_t start = millis();
  uint32_t at = 0;
  while (len - at > 0) {
    uint32_t now = millis();
    if (now - start > 1000) {
      ESP_LOGW(TAG, "Timed out writing %d bytes of data", len);
      return false;
    }

    ssize_t written = this->client_->write(buf + at, len - at);
    if (written == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        App.feed_wdt();
        delay(1);
        continue;
      }
      ESP_LOGW(TAG, "Failed to write %d bytes of data, errno %d", len, errno);
      return false;
    } else {
      at += written;
    }
    App.feed_wdt();
    delay(1);
  }
  return true;
}

float OTAESPHomeComponent::get_setup_priority() const { return setup_priority::AFTER_WIFI; }
uint16_t OTAESPHomeComponent::get_port() const { return this->port_; }
void OTAESPHomeComponent::set_port(uint16_t port) { this->port_ = port; }

void OTAESPHomeComponent::set_safe_mode_pending(const bool &pending) {
  if (!this->has_safe_mode_)
    return;

  uint32_t current_rtc = this->read_rtc_();

  if (pending && current_rtc != OTAESPHomeComponent::ENTER_SAFE_MODE_MAGIC) {
    ESP_LOGI(TAG, "Device will enter safe mode on next boot");
    this->write_rtc_(OTAESPHomeComponent::ENTER_SAFE_MODE_MAGIC);
  }

  if (!pending && current_rtc == OTAESPHomeComponent::ENTER_SAFE_MODE_MAGIC) {
    ESP_LOGI(TAG, "Safe mode pending has been cleared");
    this->clean_rtc();
  }
}
bool OTAESPHomeComponent::get_safe_mode_pending() {
  return this->has_safe_mode_ && this->read_rtc_() == OTAESPHomeComponent::ENTER_SAFE_MODE_MAGIC;
}

bool OTAESPHomeComponent::should_enter_safe_mode(uint8_t num_attempts, uint32_t enable_time) {
  this->has_safe_mode_ = true;
  this->safe_mode_start_time_ = millis();
  this->safe_mode_enable_time_ = enable_time;
  this->safe_mode_num_attempts_ = num_attempts;
  this->rtc_ = global_preferences->make_preference<uint32_t>(233825507UL, false);
  this->safe_mode_rtc_value_ = this->read_rtc_();

  bool is_manual_safe_mode = this->safe_mode_rtc_value_ == OTAESPHomeComponent::ENTER_SAFE_MODE_MAGIC;

  if (is_manual_safe_mode) {
    ESP_LOGI(TAG, "Safe mode has been entered manually");
  } else {
    ESP_LOGCONFIG(TAG, "There have been %" PRIu32 " suspected unsuccessful boot attempts", this->safe_mode_rtc_value_);
  }

  if (this->safe_mode_rtc_value_ >= num_attempts || is_manual_safe_mode) {
    this->clean_rtc();

    if (!is_manual_safe_mode) {
      ESP_LOGE(TAG, "Boot loop detected. Proceeding to safe mode");
    }

    this->status_set_error();
    this->set_timeout(enable_time, []() {
      ESP_LOGE(TAG, "No OTA attempt made, restarting");
      App.reboot();
    });

    // Delay here to allow power to stabilise before Wi-Fi/Ethernet is initialised.
    delay(300);  // NOLINT
    App.setup();

    ESP_LOGI(TAG, "Waiting for OTA attempt");

    return true;
  } else {
    // increment counter
    this->write_rtc_(this->safe_mode_rtc_value_ + 1);
    return false;
  }
}
void OTAESPHomeComponent::write_rtc_(uint32_t val) {
  this->rtc_.save(&val);
  global_preferences->sync();
}
uint32_t OTAESPHomeComponent::read_rtc_() {
  uint32_t val;
  if (!this->rtc_.load(&val))
    return 0;
  return val;
}
void OTAESPHomeComponent::clean_rtc() { this->write_rtc_(0); }
void OTAESPHomeComponent::on_safe_shutdown() {
  if (this->has_safe_mode_ && this->read_rtc_() != OTAESPHomeComponent::ENTER_SAFE_MODE_MAGIC)
    this->clean_rtc();
}

#ifdef USE_OTA_STATE_CALLBACK
void OTAESPHomeComponent::add_on_state_callback(std::function<void(OTAESPHomeState, float, uint8_t)> &&callback) {
  this->state_callback_.add(std::move(callback));
}
#endif

}  // namespace ota_esphome
}  // namespace esphome
