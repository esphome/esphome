#include "ota_component.h"

#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/application.h"
#include "esphome/core/util.h"

#include <cstdio>
#include <MD5Builder.h>
#ifdef ARDUINO_ARCH_ESP32
#include <Update.h>
#endif
#include <StreamString.h>

namespace esphome {
namespace ota {

static const char *TAG = "ota";

uint8_t OTA_VERSION_1_0 = 1;

void OTAComponent::setup() {
  this->server_ = new WiFiServer(this->port_);
  this->server_->begin();

  this->dump_config();
}
void OTAComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Over-The-Air Updates:");
  ESP_LOGCONFIG(TAG, "  Address: %s:%u", network_get_address().c_str(), this->port_);
  if (!this->password_.empty()) {
    ESP_LOGCONFIG(TAG, "  Using Password.");
  }
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
  (void) ota_features;

  if (!this->client_.connected()) {
    this->client_ = this->server_->available();

    if (!this->client_.connected())
      return;
  }

  // enable nodelay for outgoing data
  this->client_.setNoDelay(true);

  ESP_LOGD(TAG, "Starting OTA Update from %s...", this->client_.remoteIP().toString().c_str());
  this->status_set_warning();

  if (!this->wait_receive_(buf, 5)) {
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
  this->client_.write(OTA_RESPONSE_OK);
  this->client_.write(OTA_VERSION_1_0);

  // Read features - 1 byte
  if (!this->wait_receive_(buf, 1)) {
    ESP_LOGW(TAG, "Reading features failed!");
    goto error;
  }
  ota_features = buf[0];  // NOLINT
  ESP_LOGV(TAG, "OTA features is 0x%02X", ota_features);

  // Acknowledge header - 1 byte
  this->client_.write(OTA_RESPONSE_HEADER_OK);

  if (!this->password_.empty()) {
    this->client_.write(OTA_RESPONSE_REQUEST_AUTH);
    MD5Builder md5_builder{};
    md5_builder.begin();
    sprintf(sbuf, "%08X", random_uint32());
    md5_builder.add(sbuf);
    md5_builder.calculate();
    md5_builder.getChars(sbuf);
    ESP_LOGV(TAG, "Auth: Nonce is %s", sbuf);

    // Send nonce, 32 bytes hex MD5
    if (this->client_.write(reinterpret_cast<uint8_t *>(sbuf), 32) != 32) {
      ESP_LOGW(TAG, "Auth: Writing nonce failed!");
      goto error;
    }

    // prepare challenge
    md5_builder.begin();
    md5_builder.add(this->password_.c_str());
    // add nonce
    md5_builder.add(sbuf);

    // Receive cnonce, 32 bytes hex MD5
    if (!this->wait_receive_(buf, 32)) {
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
    if (!this->wait_receive_(buf + 64, 32)) {
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

  // Acknowledge auth OK - 1 byte
  this->client_.write(OTA_RESPONSE_AUTH_OK);

  // Read size, 4 bytes MSB first
  if (!this->wait_receive_(buf, 4)) {
    ESP_LOGW(TAG, "Reading size failed!");
    goto error;
  }
  ota_size = 0;
  for (uint8_t i = 0; i < 4; i++) {
    ota_size <<= 8;
    ota_size |= buf[i];
  }
  ESP_LOGV(TAG, "OTA size is %u bytes", ota_size);

#ifdef ARDUINO_ARCH_ESP8266
  global_preferences.prevent_write(true);
#endif

  if (!Update.begin(ota_size, U_FLASH)) {
    StreamString ss;
    Update.printError(ss);
#ifdef ARDUINO_ARCH_ESP8266
    if (ss.indexOf("Invalid bootstrapping") != -1) {
      error_code = OTA_RESPONSE_ERROR_INVALID_BOOTSTRAPPING;
      goto error;
    }
    if (ss.indexOf("new Flash config wrong") != -1 || ss.indexOf("new Flash config wsong") != -1) {
      error_code = OTA_RESPONSE_ERROR_WRONG_NEW_FLASH_CONFIG;
      goto error;
    }
    if (ss.indexOf("Flash config wrong real") != -1 || ss.indexOf("Flash config wsong real") != -1) {
      error_code = OTA_RESPONSE_ERROR_WRONG_CURRENT_FLASH_CONFIG;
      goto error;
    }
    if (ss.indexOf("Not Enough Space") != -1) {
      error_code = OTA_RESPONSE_ERROR_ESP8266_NOT_ENOUGH_SPACE;
      goto error;
    }
#endif
#ifdef ARDUINO_ARCH_ESP32
    if (ss.indexOf("Bad Size Given") != -1) {
      error_code = OTA_RESPONSE_ERROR_ESP32_NOT_ENOUGH_SPACE;
      goto error;
    }
#endif
    ESP_LOGW(TAG, "Preparing OTA partition failed! '%s'", ss.c_str());
    error_code = OTA_RESPONSE_ERROR_UPDATE_PREPARE;
    goto error;
  }
  update_started = true;

  // Acknowledge prepare OK - 1 byte
  this->client_.write(OTA_RESPONSE_UPDATE_PREPARE_OK);

  // Read binary MD5, 32 bytes
  if (!this->wait_receive_(buf, 32)) {
    ESP_LOGW(TAG, "Reading binary MD5 checksum failed!");
    goto error;
  }
  sbuf[32] = '\0';
  ESP_LOGV(TAG, "Update: Binary MD5 is %s", sbuf);
  Update.setMD5(sbuf);

  // Acknowledge MD5 OK - 1 byte
  this->client_.write(OTA_RESPONSE_BIN_MD5_OK);

  while (!Update.isFinished()) {
    size_t available = this->wait_receive_(buf, 0);
    if (!available) {
      goto error;
    }

    uint32_t written = Update.write(buf, available);
    if (written != available) {
      ESP_LOGW(TAG, "Error writing binary data to flash: %u != %u!", written, available);  // NOLINT
      error_code = OTA_RESPONSE_ERROR_WRITING_FLASH;
      goto error;
    }
    total += written;

    uint32_t now = millis();
    if (now - last_progress > 1000) {
      last_progress = now;
      float percentage = (total * 100.0f) / ota_size;
      ESP_LOGD(TAG, "OTA in progress: %0.1f%%", percentage);
    }
  }

  // Acknowledge receive OK - 1 byte
  this->client_.write(OTA_RESPONSE_RECEIVE_OK);

  if (!Update.end()) {
    error_code = OTA_RESPONSE_ERROR_UPDATE_END;
    goto error;
  }

  // Acknowledge Update end OK - 1 byte
  this->client_.write(OTA_RESPONSE_UPDATE_END_OK);

  // Read ACK
  if (!this->wait_receive_(buf, 1, false) || buf[0] != OTA_RESPONSE_OK) {
    ESP_LOGW(TAG, "Reading back acknowledgement failed!");
    // do not go to error, this is not fatal
  }

  this->client_.flush();
  this->client_.stop();
  delay(10);
  ESP_LOGI(TAG, "OTA update finished!");
  this->status_clear_warning();
  delay(100);  // NOLINT
  App.safe_reboot();

error:
  if (update_started) {
    StreamString ss;
    Update.printError(ss);
    ESP_LOGW(TAG, "Update end failed! Error: %s", ss.c_str());
  }
  if (this->client_.connected()) {
    this->client_.write(static_cast<uint8_t>(error_code));
    this->client_.flush();
  }
  this->client_.stop();

#ifdef ARDUINO_ARCH_ESP32
  if (update_started) {
    Update.abort();
  }
#endif

#ifdef ARDUINO_ARCH_ESP8266
  if (update_started) {
    Update.end();
  }
#endif

  this->status_momentary_error("onerror", 5000);

#ifdef ARDUINO_ARCH_ESP8266
  global_preferences.prevent_write(false);
#endif
}

size_t OTAComponent::wait_receive_(uint8_t *buf, size_t bytes, bool check_disconnected) {
  size_t available = 0;
  uint32_t start = millis();
  do {
    App.feed_wdt();
    if (check_disconnected && !this->client_.connected()) {
      ESP_LOGW(TAG, "Error client disconnected while receiving data!");
      return 0;
    }
    int availi = this->client_.available();
    if (availi < 0) {
      ESP_LOGW(TAG, "Error reading data!");
      return 0;
    }
    uint32_t now = millis();
    if (availi == 0 && now - start > 10000) {
      ESP_LOGW(TAG, "Timeout waiting for data!");
      return 0;
    }
    available = size_t(availi);
    yield();
  } while (bytes == 0 ? available == 0 : available < bytes);

  if (bytes == 0)
    bytes = std::min(available, size_t(1024));

  bool success = false;
  for (uint32_t i = 0; !success && i < 100; i++) {
    int res = this->client_.read(buf, bytes);

    if (res != int(bytes)) {
      // ESP32 implementation has an issue where calling read can fail with EAGAIN (race condition)
      // so just re-try it until it works (with generous timeout of 1s)
      // because we check with available() first this should not cause us any trouble in all other cases
      delay(10);
    } else {
      success = true;
    }
  }

  if (!success) {
    ESP_LOGW(TAG, "Reading %u bytes of binary data failed!", bytes);  // NOLINT
    return 0;
  }

  return bytes;
}

void OTAComponent::set_auth_password(const std::string &password) { this->password_ = password; }

float OTAComponent::get_setup_priority() const { return setup_priority::AFTER_WIFI; }
uint16_t OTAComponent::get_port() const { return this->port_; }
void OTAComponent::set_port(uint16_t port) { this->port_ = port; }
void OTAComponent::start_safe_mode(uint8_t num_attempts, uint32_t enable_time) {
  this->has_safe_mode_ = true;
  this->safe_mode_start_time_ = millis();
  this->safe_mode_enable_time_ = enable_time;
  this->safe_mode_num_attempts_ = num_attempts;
  this->rtc_ = global_preferences.make_preference<uint32_t>(233825507UL, false);
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

    while (true) {
      App.loop();
    }
  } else {
    // increment counter
    this->write_rtc_(this->safe_mode_rtc_value_ + 1);
  }
}
void OTAComponent::write_rtc_(uint32_t val) { this->rtc_.save(&val); }
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

}  // namespace ota
}  // namespace esphome
