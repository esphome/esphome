#include "ota_component.h"
#include "ota_backend.h"
#include "ota_backend_arduino_esp8266.h"
#include "ota_backend_arduino_rp2040.h"
#include "ota_backend_arduino_libretiny.h"
#include "ota_backend_esp_idf.h"

#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/util.h"
#include "esphome/components/md5/md5.h"
#include "esphome/components/network/util.h"

#include <cerrno>
#include <cstdio>

namespace esphome {
namespace ota {

static const char *const TAG = "ota";

static const uint8_t OTA_VERSION_1_0 = 1;

OTAComponent *global_ota_component = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

std::unique_ptr<OTABackend> make_ota_backend() {
#ifdef USE_ARDUINO
#ifdef USE_ESP8266
  return make_unique<ArduinoESP8266OTABackend>();
#endif  // USE_ESP8266
#endif  // USE_ARDUINO
#ifdef USE_ESP32
  return make_unique<IDFOTABackend>();
#endif  // USE_ESP_IDF
#ifdef USE_RP2040
  return make_unique<ArduinoRP2040OTABackend>();
#endif  // USE_RP2040
#ifdef USE_LIBRETINY
  return make_unique<ArduinoLibreTinyOTABackend>();
#endif
}

OTAComponent::OTAComponent() { global_ota_component = this; }

void OTAComponent::setup() {
  server_ = socket::socket_ip(SOCK_STREAM, 0);
  if (server_ == nullptr) {
    ESP_LOGW(TAG, "Could not create socket.");
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

  this->dump_config();
}

void OTAComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Over-The-Air Updates:");
  ESP_LOGCONFIG(TAG, "  Address: %s:%u", network::get_use_address().c_str(), this->port_);
#ifdef USE_OTA_PASSWORD
  if (!this->password_.empty()) {
    ESP_LOGCONFIG(TAG, "  Using Password.");
  }
#endif
  if (this->has_safe_mode_ && this->safe_mode_rtc_value_ > 1 &&
      this->safe_mode_rtc_value_ != esphome::ota::OTAComponent::ENTER_SAFE_MODE_MAGIC) {
    ESP_LOGW(TAG, "Last Boot was an unhandled reset, will proceed to safe mode in %" PRIu32 " restarts",
             this->safe_mode_num_attempts_ - this->safe_mode_rtc_value_);
  }
}

void OTAComponent::loop() {
  this->handle_();

  if (this->has_safe_mode_ && (millis() - this->safe_mode_start_time_) > this->safe_mode_enable_time_) {
    this->has_safe_mode_ = false;
    // successful boot, reset counter
    ESP_LOGI(TAG, "Boot seems successful, resetting boot loop counter.");
    this->clean_rtc();
  }
}

static const uint8_t FEATURE_SUPPORTS_COMPRESSION = 0x01;
static const uint8_t FEATURE_SUPPORTS_EXTENDED = 0x02;

void OTAComponent::handle_() {
  OTAResponseTypes error_code = OTA_RESPONSE_ERROR_UNKNOWN;
  uint8_t buf[1024];
  char *sbuf = reinterpret_cast<char *>(buf);
  size_t ota_size;
  uint8_t ota_features;
  uint8_t command;
  std::unique_ptr<OTABackend> backend;
  (void) ota_features;
  size_t features_reply_length;
  OTAPartitionType bin_type;

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
    ESP_LOGW(TAG, "Socket could not enable tcp nodelay, errno: %d", errno);
    return;
  }

  ESP_LOGD(TAG, "Starting OTA Update from %s...", this->client_->getpeername().c_str());
  this->status_set_warning();
#ifdef USE_OTA_STATE_CALLBACK
  this->state_callback_.call(OTA_STARTED, 0.0f, 0);
#endif

  if (!this->readall_(buf, 5)) {
    ESP_LOGW(TAG, "Reading magic bytes failed!");
    goto error;  // NOLINT(cppcoreguidelines-avoid-goto)
  }
  // 0x6C, 0x26, 0xF7, 0x5C, 0x45
  if (buf[0] != 0x6C || buf[1] != 0x26 || buf[2] != 0xF7 || buf[3] != 0x5C || buf[4] != 0x45) {
    ESP_LOGW(TAG, "Magic bytes do not match! 0x%02X-0x%02X-0x%02X-0x%02X-0x%02X", buf[0], buf[1], buf[2], buf[3],
             buf[4]);
    error_code = OTA_RESPONSE_ERROR_MAGIC;
    goto error;  // NOLINT(cppcoreguidelines-avoid-goto)
  }

  // Send OK and version - 2 bytes
  buf[0] = OTA_RESPONSE_OK;
  buf[1] = OTA_VERSION_1_0;
  this->writeall_(buf, 2);

  backend = make_ota_backend();

  // Read features - 1 byte
  if (!this->readall_(buf, 1)) {
    ESP_LOGW(TAG, "Reading features failed!");
    goto error;  // NOLINT(cppcoreguidelines-avoid-goto)
  }
  ota_features = buf[0];  // NOLINT
  ESP_LOGV(TAG, "OTA features is 0x%02X", ota_features);

  // Acknowledge header - variable, depending on requested features
  features_reply_length = 1;
  buf[0] = OTA_RESPONSE_HEADER_OK;
  if ((ota_features & FEATURE_SUPPORTS_COMPRESSION) != 0 && backend->supports_compression()) {
    buf[0] = OTA_RESPONSE_SUPPORTS_COMPRESSION;
  }
  if ((ota_features & FEATURE_SUPPORTS_EXTENDED) != 0) {
    buf[0] = OTA_RESPONSE_SUPPORTS_EXTENDED;
    uint8_t enabled_features = 0;
    if (backend->supports_compression()) {
      buf[2 + (enabled_features++)] = OTA_RESPONSE_SUPPORTS_EXTENDED;
    }
    if (backend->supports_writing_bootloader()) {
      buf[2 + (enabled_features++)] = OTA_FEATURE_WRITING_BOOTLOADER;
    }
    if (backend->supports_writing_partition_table()) {
      buf[2 + (enabled_features++)] = OTA_FEATURE_WRITING_PARTITION_TABLE;
    }
    if (backend->supports_writing_partitions()) {
      buf[2 + (enabled_features++)] = OTA_FEATURE_WRITING_PARTITIONS;
    }
    if (backend->supports_reading()) {
      buf[2 + (enabled_features++)] = OTA_FEATURE_READING;
    }
    // Each enabled feature consumes a byte plus one byte for the length.
    // So the max number of features that can be supported is 255
    buf[1] = enabled_features;
    features_reply_length += 1 + enabled_features;
  }

  this->writeall_(buf, features_reply_length);

#ifdef USE_OTA_PASSWORD
  if (!this->password_.empty()) {
    buf[0] = OTA_RESPONSE_REQUEST_AUTH;
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
      ESP_LOGW(TAG, "Auth: Writing nonce failed!");
      goto error;  // NOLINT(cppcoreguidelines-avoid-goto)
    }

    // prepare challenge
    md5.init();
    md5.add(this->password_.c_str(), this->password_.length());
    // add nonce
    md5.add(sbuf, 32);

    // Receive cnonce, 32 bytes hex MD5
    if (!this->readall_(buf, 32)) {
      ESP_LOGW(TAG, "Auth: Reading cnonce failed!");
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
      ESP_LOGW(TAG, "Auth: Reading response failed!");
      goto error;  // NOLINT(cppcoreguidelines-avoid-goto)
    }
    sbuf[64 + 32] = '\0';
    ESP_LOGV(TAG, "Auth: Response is %s", sbuf + 64);

    bool matches = true;
    for (uint8_t i = 0; i < 32; i++)
      matches = matches && buf[i] == buf[64 + i];

    if (!matches) {
      ESP_LOGW(TAG, "Auth failed! Passwords do not match!");
      error_code = OTA_RESPONSE_ERROR_AUTH_INVALID;
      goto error;  // NOLINT(cppcoreguidelines-avoid-goto)
    }
  }
#endif  // USE_OTA_PASSWORD

  // Acknowledge auth OK - 1 byte
  buf[0] = OTA_RESPONSE_AUTH_OK;
  this->writeall_(buf, 1);

  bin_type.type = OTA_BIN_APP;
  if ((ota_features & FEATURE_SUPPORTS_EXTENDED) != 0) {
    while (true) {
      // Read command
      if (!this->readall_(buf, 1)) {
        error_code = OTA_RESPONSE_ERROR_SOCKET_READ;
        ESP_LOGE(TAG, "Reading command failed!");
        goto error;  // NOLINT(cppcoreguidelines-avoid-goto)
      }
      command = buf[0];
      switch (command) {
        case OTA_COMMAND_WRITE:
          error_code = this->get_partition_info_(buf, bin_type, ota_size);
          if (error_code)
            goto error;  // NOLINT(cppcoreguidelines-avoid-goto)
          error_code = this->write_flash_(buf, backend, bin_type, ota_size);
          if (error_code)
            goto error;  // NOLINT(cppcoreguidelines-avoid-goto)
          break;
        case OTA_COMMAND_REBOOT:
          buf[0] = OTA_RESPONSE_OK;
          this->writeall_(buf, 1);
          this->client_->close();
          this->client_ = nullptr;
          delay(100);  // NOLINT
          App.safe_reboot();
          return;  // Will never be reached
        case OTA_COMMAND_END:
          ESP_LOGI(TAG, "OTA session finished!");
          // close connection
          this->client_->close();
          this->client_ = nullptr;
          return;
        case OTA_COMMAND_READ:
          error_code = this->get_partition_info_(buf, bin_type, ota_size);
          if (error_code)
            goto error;  // NOLINT(cppcoreguidelines-avoid-goto)
          error_code = this->read_flash_(buf, backend, bin_type);
          if (error_code)
            goto error;  // NOLINT(cppcoreguidelines-avoid-goto)
          break;
        default:
          ESP_LOGE(TAG, "Reading command failed!");
          error_code = OTA_RESPONSE_ERROR_UNKNOWN_COMMAND;
          goto error;  // NOLINT(cppcoreguidelines-avoid-goto)
      }
    }
  } else {
    // Compatibility with firmware prior to FEATURE_SUPPORTS_EXTENDED
    // Read size, 4 bytes MSB first
    if (!this->readall_(buf, 4)) {
      ESP_LOGE(TAG, "Reading size failed!");
      error_code = OTA_RESPONSE_ERROR_SOCKET_READ;
      goto error;  // NOLINT(cppcoreguidelines-avoid-goto)
    }
    ota_size = 0;
    for (uint8_t i = 0; i < 4; i++) {
      ota_size <<= 8;
      ota_size |= buf[i];
    }
    error_code = this->write_flash_(buf, backend, bin_type, ota_size);
    if (error_code)
      goto error;  // NOLINT(cppcoreguidelines-avoid-goto)

    // Read ACK
    this->readall_(buf, 1);

    // close connection and reboot
    this->client_->close();
    this->client_ = nullptr;
    delay(100);  // NOLINT
    App.safe_reboot();
    return;  // Will never be reached
  }

error:
  buf[0] = static_cast<uint8_t>(error_code);
  this->writeall_(buf, 1);
  this->client_->close();
  this->client_ = nullptr;

  this->status_momentary_error("onerror", 5000);
#ifdef USE_OTA_STATE_CALLBACK
  this->state_callback_.call(OTA_ERROR, 0.0f, static_cast<uint8_t>(error_code));
#endif
}

bool OTAComponent::readall_(uint8_t *buf, size_t len) {
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
      ESP_LOGW(TAG, "Failed to read %d bytes of data, errno: %d", len, errno);
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
bool OTAComponent::writeall_(const uint8_t *buf, size_t len) {
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
      ESP_LOGW(TAG, "Failed to write %d bytes of data, errno: %d", len, errno);
      return false;
    } else {
      at += written;
    }
    App.feed_wdt();
    delay(1);
  }
  return true;
}
OTAResponseTypes OTAComponent::get_partition_info_(uint8_t *buf, OTAPartitionType &bin_type, size_t &ota_size) {
  // Read partition info
  // - [ 0   ] version: 0x1
  // - [ 1   ] bin type (if version >= 1)
  // - [ 2- 5] bin length (if version >= 1)
  // - [ 6   ] partition type - when bin type = partition (if version >= 1)
  // - [ 7   ] partition subtype - when bin type = partition (if version >= 1)
  // - [ 8   ] partition index - when bin type = partition (if version >= 1)
  // - [16-31] partition label - when bin type = partition (if version >= 1)
  if (!this->readall_(buf, 1)) {
    ESP_LOGE(TAG, "Reading partition info failed!");
    return OTA_RESPONSE_ERROR_UNKNOWN;
  }
  if (buf[0] == 0) {
    ESP_LOGE(TAG, "Unknown partition info (0) version!");
    return OTA_RESPONSE_ERROR_UNKNOWN_PARTITION_INFO_VERSION;
  }
  if (!this->readall_(&buf[1], 31)) {
    ESP_LOGE(TAG, "Reading partition info (1-31) failed!");
    return OTA_RESPONSE_ERROR_UNKNOWN;
  }
  bin_type.type = (OTABinType) buf[1];
  ota_size = 0;
  for (uint8_t i = 2; i < 6; i++) {
    ota_size <<= 8;
    ota_size |= buf[i];
  }
  bin_type.part_type = buf[6];
  bin_type.part_subtype = buf[7];
  bin_type.part_index = buf[8];
  strncpy(bin_type.part_label, (char *) &buf[16], sizeof(bin_type.part_label));
  bin_type.part_label[sizeof(bin_type.part_label) - 1] = '\0';

  return OTA_RESPONSE_OK;
}

OTAResponseTypes OTAComponent::write_flash_(uint8_t *buf, std::unique_ptr<OTABackend> &backend,
                                            const OTAPartitionType &bin_type, size_t ota_size) {
  bool update_started = false;
  size_t total = 0;
  uint32_t last_progress = 0;
  char *sbuf = reinterpret_cast<char *>(buf);

  ESP_LOGI(TAG, "OTA type is %u and size is %u bytes", bin_type.type, ota_size);

  OTAResponseTypes error_code = backend->begin(bin_type, ota_size);
  if (error_code != OTA_RESPONSE_OK)
    goto error;  // NOLINT(cppcoreguidelines-avoid-goto)
  update_started = true;

  // Acknowledge prepare OK - 1 byte
  buf[0] = OTA_RESPONSE_UPDATE_PREPARE_OK;
  this->writeall_(buf, 1);

  // Read binary MD5, 32 bytes
  if (!this->readall_(buf, 32)) {
    ESP_LOGW(TAG, "Reading binary MD5 checksum failed!");
    goto error;  // NOLINT(cppcoreguidelines-avoid-goto)
  }
  sbuf[32] = '\0';
  ESP_LOGV(TAG, "Update: Binary MD5 is %s", sbuf);
  backend->set_update_md5(sbuf);

  // Acknowledge MD5 OK - 1 byte
  buf[0] = OTA_RESPONSE_BIN_MD5_OK;
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
      ESP_LOGW(TAG, "Error receiving data for update, errno: %d", errno);
      goto error;  // NOLINT(cppcoreguidelines-avoid-goto)
    } else if (read == 0) {
      // $ man recv
      // "When  a  stream socket peer has performed an orderly shutdown, the return value will
      // be 0 (the traditional "end-of-file" return)."
      ESP_LOGW(TAG, "Remote end closed connection");
      goto error;  // NOLINT(cppcoreguidelines-avoid-goto)
    }

    error_code = backend->write(buf, read);
    if (error_code != OTA_RESPONSE_OK) {
      ESP_LOGW(TAG, "Error writing binary data to flash!, error_code: %d", error_code);
      goto error;  // NOLINT(cppcoreguidelines-avoid-goto)
    }
    total += read;

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
  buf[0] = OTA_RESPONSE_RECEIVE_OK;
  this->writeall_(buf, 1);

  error_code = backend->end();
  if (error_code != OTA_RESPONSE_OK) {
    ESP_LOGW(TAG, "Error ending OTA!, error_code: %d", error_code);
    goto error;  // NOLINT(cppcoreguidelines-avoid-goto)
  }

  // Acknowledge Update end OK - 1 byte
  buf[0] = OTA_RESPONSE_UPDATE_END_OK;
  this->writeall_(buf, 1);

  ESP_LOGI(TAG, "OTA update finished!");
  this->status_clear_warning();

#ifdef USE_OTA_STATE_CALLBACK
  this->state_callback_.call(OTA_COMPLETED, 100.0f, 0);
#endif

  return OTA_RESPONSE_OK;

error:
  if (backend != nullptr && update_started) {
    backend->abort();
  }
  return error_code;
}

OTAResponseTypes OTAComponent::read_flash_(uint8_t *buf, std::unique_ptr<OTABackend> &backend,
                                           const OTAPartitionType &bin_type) {
  bool read_started = false;
  size_t total = 0, ota_size = 0;
  uint32_t last_progress = 0;
  char *sbuf = reinterpret_cast<char *>(buf);

  OTAResponseTypes error_code = backend->begin(bin_type, ota_size);
  if (error_code != OTA_RESPONSE_OK)
    goto error;  // NOLINT(cppcoreguidelines-avoid-goto)
  read_started = true;

  ESP_LOGI(TAG, "OTA READ type is %u and size is %u bytes", bin_type.type, ota_size);

  // Acknowledge read cmd - 5 bytes
  buf[0] = OTA_RESPONSE_READ_PREPARE_OK;
  buf[1] = (ota_size >> 24) & 0xFF;
  buf[2] = (ota_size >> 16) & 0xFF;
  buf[3] = (ota_size >> 8) & 0xFF;
  buf[4] = ota_size & 0xFF;
  this->writeall_(buf, 5);

  while (total < ota_size) {
    size_t chunk_size = std::min(sizeof(buf), ota_size - total);

    error_code = backend->read(buf, chunk_size);
    if (error_code != OTA_RESPONSE_OK) {
      ESP_LOGE(TAG, "Error reading from flash!, error_code: %d", error_code);
      goto error;  // NOLINT(cppcoreguidelines-avoid-goto)
    }
    this->writeall_(buf, chunk_size);
    if (error_code != OTA_RESPONSE_OK) {
      ESP_LOGE(TAG, "Error sending read data!, error_code: %d", error_code);
      goto error;  // NOLINT(cppcoreguidelines-avoid-goto)
    }

    total += chunk_size;

    uint32_t now = millis();
    if (now - last_progress > 1000) {
      last_progress = now;
      float percentage = (total * 100.0f) / ota_size;
      ESP_LOGD(TAG, "OTA read in progress: %0.1f%%", percentage);
      // feed watchdog and give other tasks a chance to run
      App.feed_wdt();
      yield();
    }
  }

  // Read binary MD5, 32 bytes
  if (!this->readall_(buf, 32)) {
    ESP_LOGW(TAG, "Reading binary MD5 checksum failed!");
    goto error;  // NOLINT(cppcoreguidelines-avoid-goto)
  }
  sbuf[32] = '\0';
  ESP_LOGV(TAG, "Received: Binary MD5 is %s", sbuf);
  backend->set_update_md5(sbuf);

  error_code = backend->end();
  if (error_code != OTA_RESPONSE_OK) {
    ESP_LOGW(TAG, "Error ending OTA read!, error_code: %d", error_code);
    goto error;  // NOLINT(cppcoreguidelines-avoid-goto)
  }

  // Acknowledge MD5 OK - 1 byte
  buf[0] = OTA_RESPONSE_BIN_MD5_OK;
  this->writeall_(buf, 1);

  return OTA_RESPONSE_OK;

error:
  if (backend != nullptr && read_started) {
    backend->abort();
  }
  return error_code;
}

float OTAComponent::get_setup_priority() const { return setup_priority::AFTER_WIFI; }
uint16_t OTAComponent::get_port() const { return this->port_; }
void OTAComponent::set_port(uint16_t port) { this->port_ = port; }

void OTAComponent::set_safe_mode_pending(const bool &pending) {
  if (!this->has_safe_mode_)
    return;

  uint32_t current_rtc = this->read_rtc_();

  if (pending && current_rtc != esphome::ota::OTAComponent::ENTER_SAFE_MODE_MAGIC) {
    ESP_LOGI(TAG, "Device will enter safe mode on next boot.");
    this->write_rtc_(esphome::ota::OTAComponent::ENTER_SAFE_MODE_MAGIC);
  }

  if (!pending && current_rtc == esphome::ota::OTAComponent::ENTER_SAFE_MODE_MAGIC) {
    ESP_LOGI(TAG, "Safe mode pending has been cleared");
    this->clean_rtc();
  }
}
bool OTAComponent::get_safe_mode_pending() {
  return this->has_safe_mode_ && this->read_rtc_() == esphome::ota::OTAComponent::ENTER_SAFE_MODE_MAGIC;
}

bool OTAComponent::should_enter_safe_mode(uint8_t num_attempts, uint32_t enable_time) {
  this->has_safe_mode_ = true;
  this->safe_mode_start_time_ = millis();
  this->safe_mode_enable_time_ = enable_time;
  this->safe_mode_num_attempts_ = num_attempts;
  this->rtc_ = global_preferences->make_preference<uint32_t>(233825507UL, false);
  this->safe_mode_rtc_value_ = this->read_rtc_();

  bool is_manual_safe_mode = this->safe_mode_rtc_value_ == esphome::ota::OTAComponent::ENTER_SAFE_MODE_MAGIC;

  if (is_manual_safe_mode) {
    ESP_LOGI(TAG, "Safe mode has been entered manually");
  } else {
    ESP_LOGCONFIG(TAG, "There have been %" PRIu32 " suspected unsuccessful boot attempts.", this->safe_mode_rtc_value_);
  }

  if (this->safe_mode_rtc_value_ >= num_attempts || is_manual_safe_mode) {
    this->clean_rtc();

    if (!is_manual_safe_mode) {
      ESP_LOGE(TAG, "Boot loop detected. Proceeding to safe mode.");
    }

    this->status_set_error();
    this->set_timeout(enable_time, []() {
      ESP_LOGE(TAG, "No OTA attempt made, restarting.");
      App.reboot();
    });

    // Delay here to allow power to stabilise before Wi-Fi/Ethernet is initialised.
    delay(300);  // NOLINT
    App.setup();

    ESP_LOGI(TAG, "Waiting for OTA attempt.");

    return true;
  } else {
    // increment counter
    this->write_rtc_(this->safe_mode_rtc_value_ + 1);
    return false;
  }
}
void OTAComponent::write_rtc_(uint32_t val) {
  this->rtc_.save(&val);
  global_preferences->sync();
}
uint32_t OTAComponent::read_rtc_() {
  uint32_t val;
  if (!this->rtc_.load(&val))
    return 0;
  return val;
}
void OTAComponent::clean_rtc() { this->write_rtc_(0); }
void OTAComponent::on_safe_shutdown() {
  if (this->has_safe_mode_ && this->read_rtc_() != esphome::ota::OTAComponent::ENTER_SAFE_MODE_MAGIC)
    this->clean_rtc();
}

#ifdef USE_OTA_STATE_CALLBACK
void OTAComponent::add_on_state_callback(std::function<void(OTAState, float, uint8_t)> &&callback) {
  this->state_callback_.add(std::move(callback));
}
#endif

}  // namespace ota
}  // namespace esphome
