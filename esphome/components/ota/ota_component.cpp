#include "ota_component.h"

#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/util.h"
#include "esphome/components/network/util.h"

#include <cerrno>
#include <cstdio>

#ifdef USE_ARDUINO
#ifdef USE_OTA_PASSWORD
#include <MD5Builder.h>
#endif  // USE_OTA_PASSWORD

#ifdef USE_ESP32
#include <Update.h>
#endif  // USE_ESP32
#endif  // USE_ARDUINO

#ifdef USE_ESP8266
#include <Updater.h>
#include "esphome/components/esp8266/preferences.h"
#endif  // USE_ESP8266

#ifdef USE_ESP_IDF
#include <esp_ota_ops.h>
#endif

namespace esphome {
namespace ota {

static const char *const TAG = "ota";

static const uint8_t OTA_VERSION_1_0 = 1;

class OTABackend {
 public:
  virtual ~OTABackend() = default;
  virtual OTAResponseTypes begin(size_t image_size) = 0;
  virtual void set_update_md5(const char *md5) = 0;
  virtual OTAResponseTypes write(uint8_t *data, size_t len) = 0;
  virtual OTAResponseTypes end() = 0;
  virtual void abort() = 0;
};

#ifdef USE_ARDUINO
class ArduinoOTABackend : public OTABackend {
 public:
  OTAResponseTypes begin(size_t image_size) override {
    bool ret = Update.begin(image_size, U_FLASH);
    if (ret) {
#ifdef USE_ESP8266
      esp8266::preferences_prevent_write(true);
#endif
      return OTA_RESPONSE_OK;
    }

    uint8_t error = Update.getError();
#ifdef USE_ESP8266
    if (error == UPDATE_ERROR_BOOTSTRAP)
      return OTA_RESPONSE_ERROR_INVALID_BOOTSTRAPPING;
    if (error == UPDATE_ERROR_NEW_FLASH_CONFIG)
      return OTA_RESPONSE_ERROR_WRONG_NEW_FLASH_CONFIG;
    if (error == UPDATE_ERROR_FLASH_CONFIG)
      return OTA_RESPONSE_ERROR_WRONG_CURRENT_FLASH_CONFIG;
    if (error == UPDATE_ERROR_SPACE)
      return OTA_RESPONSE_ERROR_ESP8266_NOT_ENOUGH_SPACE;
#endif
#ifdef USE_ESP32
    if (error == UPDATE_ERROR_SIZE)
      return OTA_RESPONSE_ERROR_ESP32_NOT_ENOUGH_SPACE;
#endif
    return OTA_RESPONSE_ERROR_UNKNOWN;
  }
  void set_update_md5(const char *md5) override { Update.setMD5(md5); }
  OTAResponseTypes write(uint8_t *data, size_t len) override {
    size_t written = Update.write(data, len);
    if (written != len) {
      return OTA_RESPONSE_ERROR_WRITING_FLASH;
    }
    return OTA_RESPONSE_OK;
  }
  OTAResponseTypes end() override {
    if (!Update.end())
      return OTA_RESPONSE_ERROR_UPDATE_END;
    return OTA_RESPONSE_OK;
  }
  void abort() override {
#ifdef USE_ESP32
    Update.abort();
#endif

#ifdef USE_ESP8266
    Update.end();
    esp8266::preferences_prevent_write(false);
#endif
  }
};
std::unique_ptr<OTABackend> make_ota_backend() { return make_unique<ArduinoOTABackend>(); }
#endif  // USE_ARDUINO

#ifdef USE_ESP_IDF
class IDFOTABackend : public OTABackend {
 public:
  esp_ota_handle_t update_handle = 0;

  OTAResponseTypes begin(size_t image_size) override {
    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(nullptr);
    if (update_partition == nullptr) {
      return OTA_RESPONSE_ERROR_NO_UPDATE_PARTITION;
    }
    esp_err_t err = esp_ota_begin(update_partition, image_size, &update_handle);
    if (err != ESP_OK) {
      esp_ota_abort(update_handle);
      update_handle = 0;
      if (err == ESP_ERR_INVALID_SIZE) {
        return OTA_RESPONSE_ERROR_ESP32_NOT_ENOUGH_SPACE;
      } else if (err == ESP_ERR_FLASH_OP_TIMEOUT || err == ESP_ERR_FLASH_OP_FAIL) {
        return OTA_RESPONSE_ERROR_WRITING_FLASH;
      }
      return OTA_RESPONSE_ERROR_UNKNOWN;
    }
    return OTA_RESPONSE_OK;
  }
  void set_update_md5(const char *md5) override {
    // pass
  }
  OTAResponseTypes write(uint8_t *data, size_t len) override {
    esp_err_t err = esp_ota_write(update_handle, data, len);
    if (err != ESP_OK) {
      if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
        return OTA_RESPONSE_ERROR_MAGIC;
      } else if (err == ESP_ERR_FLASH_OP_TIMEOUT || err == ESP_ERR_FLASH_OP_FAIL) {
        return OTA_RESPONSE_ERROR_WRITING_FLASH;
      }
      return OTA_RESPONSE_ERROR_UNKNOWN;
    }
    return OTA_RESPONSE_OK;
  }
  OTAResponseTypes end() override {
    esp_err_t err = esp_ota_end(update_handle);
    update_handle = 0;
    if (err != ESP_OK) {
      if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
        return OTA_RESPONSE_ERROR_UPDATE_END;
      }
      return OTA_RESPONSE_ERROR_UNKNOWN;
    }
    return OTA_RESPONSE_OK;
  }
  void abort() override { esp_ota_abort(update_handle); }
};
std::unique_ptr<OTABackend> make_ota_backend() { return make_unique<IDFOTABackend>(); }
#endif  // USE_ESP_IDF

void OTAComponent::setup() {
  server_ = socket::socket(AF_INET, SOCK_STREAM, 0);
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

  struct sockaddr_in server;
  memset(&server, 0, sizeof(server));
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = ESPHOME_INADDR_ANY;
  server.sin_port = htons(this->port_);

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
  if (this->has_safe_mode_ && this->safe_mode_rtc_value_ > 1) {
    ESP_LOGW(TAG, "Last Boot was an unhandled reset, will proceed to safe mode in %d restarts",
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

void OTAComponent::handle_() {
  OTAResponseTypes error_code = OTA_RESPONSE_ERROR_UNKNOWN;
  bool update_started = false;
  uint32_t total = 0;
  uint32_t last_progress = 0;
  uint8_t buf[1024];
  char *sbuf = reinterpret_cast<char *>(buf);
  uint32_t ota_size;
  uint8_t ota_features;
  std::unique_ptr<OTABackend> backend;
  (void) ota_features;

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
    goto error;
  }
  // 0x6C, 0x26, 0xF7, 0x5C, 0x45
  if (buf[0] != 0x6C || buf[1] != 0x26 || buf[2] != 0xF7 || buf[3] != 0x5C || buf[4] != 0x45) {
    ESP_LOGW(TAG, "Magic bytes do not match! 0x%02X-0x%02X-0x%02X-0x%02X-0x%02X", buf[0], buf[1], buf[2], buf[3],
             buf[4]);
    error_code = OTA_RESPONSE_ERROR_MAGIC;
    goto error;
  }

  // Send OK and version - 2 bytes
  buf[0] = OTA_RESPONSE_OK;
  buf[1] = OTA_VERSION_1_0;
  this->writeall_(buf, 2);

  // Read features - 1 byte
  if (!this->readall_(buf, 1)) {
    ESP_LOGW(TAG, "Reading features failed!");
    goto error;
  }
  ota_features = buf[0];  // NOLINT
  ESP_LOGV(TAG, "OTA features is 0x%02X", ota_features);

  // Acknowledge header - 1 byte
  buf[0] = OTA_RESPONSE_HEADER_OK;
  this->writeall_(buf, 1);

#ifdef USE_OTA_PASSWORD
  if (!this->password_.empty()) {
    buf[0] = OTA_RESPONSE_REQUEST_AUTH;
    this->writeall_(buf, 1);
    MD5Builder md5_builder{};
    md5_builder.begin();
    sprintf(sbuf, "%08X", random_uint32());
    md5_builder.add(sbuf);
    md5_builder.calculate();
    md5_builder.getChars(sbuf);
    ESP_LOGV(TAG, "Auth: Nonce is %s", sbuf);

    // Send nonce, 32 bytes hex MD5
    if (!this->writeall_(reinterpret_cast<uint8_t *>(sbuf), 32)) {
      ESP_LOGW(TAG, "Auth: Writing nonce failed!");
      goto error;
    }

    // prepare challenge
    md5_builder.begin();
    md5_builder.add(this->password_.c_str());
    // add nonce
    md5_builder.add(sbuf);

    // Receive cnonce, 32 bytes hex MD5
    if (!this->readall_(buf, 32)) {
      ESP_LOGW(TAG, "Auth: Reading cnonce failed!");
      goto error;
    }
    sbuf[32] = '\0';
    ESP_LOGV(TAG, "Auth: CNonce is %s", sbuf);
    // add cnonce
    md5_builder.add(sbuf);

    // calculate result
    md5_builder.calculate();
    md5_builder.getChars(sbuf);
    ESP_LOGV(TAG, "Auth: Result is %s", sbuf);

    // Receive result, 32 bytes hex MD5
    if (!this->readall_(buf + 64, 32)) {
      ESP_LOGW(TAG, "Auth: Reading response failed!");
      goto error;
    }
    sbuf[64 + 32] = '\0';
    ESP_LOGV(TAG, "Auth: Response is %s", sbuf + 64);

    bool matches = true;
    for (uint8_t i = 0; i < 32; i++)
      matches = matches && buf[i] == buf[64 + i];

    if (!matches) {
      ESP_LOGW(TAG, "Auth failed! Passwords do not match!");
      error_code = OTA_RESPONSE_ERROR_AUTH_INVALID;
      goto error;
    }
  }
#endif  // USE_OTA_PASSWORD

  // Acknowledge auth OK - 1 byte
  buf[0] = OTA_RESPONSE_AUTH_OK;
  this->writeall_(buf, 1);

  // Read size, 4 bytes MSB first
  if (!this->readall_(buf, 4)) {
    ESP_LOGW(TAG, "Reading size failed!");
    goto error;
  }
  ota_size = 0;
  for (uint8_t i = 0; i < 4; i++) {
    ota_size <<= 8;
    ota_size |= buf[i];
  }
  ESP_LOGV(TAG, "OTA size is %u bytes", ota_size);

  backend = make_ota_backend();
  error_code = backend->begin(ota_size);
  if (error_code != OTA_RESPONSE_OK)
    goto error;
  update_started = true;

  // Acknowledge prepare OK - 1 byte
  buf[0] = OTA_RESPONSE_UPDATE_PREPARE_OK;
  this->writeall_(buf, 1);

  // Read binary MD5, 32 bytes
  if (!this->readall_(buf, 32)) {
    ESP_LOGW(TAG, "Reading binary MD5 checksum failed!");
    goto error;
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
        delay(1);
        continue;
      }
      ESP_LOGW(TAG, "Error receiving data for update, errno: %d", errno);
      goto error;
    }

    error_code = backend->write(buf, read);
    if (error_code != OTA_RESPONSE_OK) {
      ESP_LOGW(TAG, "Error writing binary data to flash!");
      goto error;
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
      // slow down OTA update to avoid getting killed by task watchdog (task_wdt)
      delay(10);
    }
  }

  // Acknowledge receive OK - 1 byte
  buf[0] = OTA_RESPONSE_RECEIVE_OK;
  this->writeall_(buf, 1);

  error_code = backend->end();
  if (error_code != OTA_RESPONSE_OK) {
    ESP_LOGW(TAG, "Error ending OTA!");
    goto error;
  }

  // Acknowledge Update end OK - 1 byte
  buf[0] = OTA_RESPONSE_UPDATE_END_OK;
  this->writeall_(buf, 1);

  // Read ACK
  if (!this->readall_(buf, 1) || buf[0] != OTA_RESPONSE_OK) {
    ESP_LOGW(TAG, "Reading back acknowledgement failed!");
    // do not go to error, this is not fatal
  }

  this->client_->close();
  this->client_ = nullptr;
  delay(10);
  ESP_LOGI(TAG, "OTA update finished!");
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
        delay(1);
        continue;
      }
      ESP_LOGW(TAG, "Failed to read %d bytes of data, errno: %d", len, errno);
      return false;
    } else {
      at += read;
    }
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
        delay(1);
        continue;
      }
      ESP_LOGW(TAG, "Failed to write %d bytes of data, errno: %d", len, errno);
      return false;
    } else {
      at += written;
    }
    delay(1);
  }
  return true;
}

float OTAComponent::get_setup_priority() const { return setup_priority::AFTER_WIFI; }
uint16_t OTAComponent::get_port() const { return this->port_; }
void OTAComponent::set_port(uint16_t port) { this->port_ = port; }
bool OTAComponent::should_enter_safe_mode(uint8_t num_attempts, uint32_t enable_time) {
  this->has_safe_mode_ = true;
  this->safe_mode_start_time_ = millis();
  this->safe_mode_enable_time_ = enable_time;
  this->safe_mode_num_attempts_ = num_attempts;
  this->rtc_ = global_preferences->make_preference<uint32_t>(233825507UL, false);
  this->safe_mode_rtc_value_ = this->read_rtc_();

  ESP_LOGCONFIG(TAG, "There have been %u suspected unsuccessful boot attempts.", this->safe_mode_rtc_value_);

  if (this->safe_mode_rtc_value_ >= num_attempts) {
    this->clean_rtc();

    ESP_LOGE(TAG, "Boot loop detected. Proceeding to safe mode.");

    this->status_set_error();
    this->set_timeout(enable_time, []() {
      ESP_LOGE(TAG, "No OTA attempt made, restarting.");
      App.reboot();
    });

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
  if (this->has_safe_mode_)
    this->clean_rtc();
}

#ifdef USE_OTA_STATE_CALLBACK
void OTAComponent::add_on_state_callback(std::function<void(OTAState, float, uint8_t)> &&callback) {
  this->state_callback_.add(std::move(callback));
}
#endif

}  // namespace ota
}  // namespace esphome
