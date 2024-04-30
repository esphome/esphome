#include "nextion.h"

#ifdef ARDUINO
#ifdef USE_NEXTION_TFT_UPLOAD

#include "esphome/core/application.h"
#include "esphome/core/defines.h"
#include "esphome/core/util.h"
#include "esphome/core/log.h"
#include "esphome/components/network/util.h"

#ifdef USE_ESP32
#include <esp_heap_caps.h>
#endif

namespace esphome {
namespace nextion {
static const char *const TAG = "nextion.upload.arduino";

// Followed guide
// https://unofficialnextion.com/t/nextion-upload-protocol-v1-2-the-fast-one/1044/2

int Nextion::upload_by_chunks_(HTTPClient *http, int range_start) {
  int range_end = 0;

  if (range_start == 0 && this->transfer_buffer_size_ > 16384) {  // Start small at the first run in case of a big skip
    range_end = 16384 - 1;
  } else {
    range_end = range_start + this->transfer_buffer_size_ - 1;
  }

  if (range_end > this->tft_size_)
    range_end = this->tft_size_;

#ifdef USE_ESP8266
#if USE_ARDUINO_VERSION_CODE >= VERSION_CODE(2, 7, 0)
  http->setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
#elif USE_ARDUINO_VERSION_CODE >= VERSION_CODE(2, 6, 0)
  http->setFollowRedirects(true);
#endif
#if USE_ARDUINO_VERSION_CODE >= VERSION_CODE(2, 6, 0)
  http->setRedirectLimit(3);
#endif
#endif  // USE_ESP8266

  char range_header[64];
  sprintf(range_header, "bytes=%d-%d", range_start, range_end);

  ESP_LOGD(TAG, "Requesting range: %s", range_header);

  int tries = 1;
  int code = 0;
  bool begin_status = false;
  while (tries <= 5) {
#ifdef USE_ESP32
    begin_status = http->begin(this->tft_url_.c_str());
#endif
#ifdef USE_ESP8266
    begin_status = http->begin(*this->get_wifi_client_(), this->tft_url_.c_str());
#endif

    ++tries;
    if (!begin_status) {
      ESP_LOGD(TAG, "upload_by_chunks_: connection failed");
      delay(500);  // NOLINT
      continue;
    }

    http->addHeader("Range", range_header);

    code = http->GET();
    if (code == 200 || code == 206) {
      break;
    }
    ESP_LOGW(TAG, "HTTP Request failed; URL: %s; Error: %s, retries(%d/5)", this->tft_url_.c_str(),
             HTTPClient::errorToString(code).c_str(), tries);
    http->end();
    App.feed_wdt();
    delay(500);  // NOLINT
  }

  if (tries > 5) {
    return -1;
  }

  std::string recv_string;
  size_t size = 0;
  int fetched = 0;
  int range = range_end - range_start;

  while (fetched < range) {
    size = http->getStreamPtr()->available();
    if (!size) {
      App.feed_wdt();
      delay(0);
      continue;
    }
    int c = http->getStreamPtr()->readBytes(
        &this->transfer_buffer_[fetched], ((size > this->transfer_buffer_size_) ? this->transfer_buffer_size_ : size));
    fetched += c;
  }
  http->end();
  ESP_LOGN(TAG, "Fetched %d of %d bytes", fetched, this->content_length_);

  // upload fetched segments to the display in 4KB chunks
  int write_len;
  for (int i = 0; i < range; i += 4096) {
    App.feed_wdt();
    write_len = this->content_length_ < 4096 ? this->content_length_ : 4096;
    this->write_array(&this->transfer_buffer_[i], write_len);
    this->content_length_ -= write_len;
    ESP_LOGD(TAG, "Uploaded %0.2f %%; %d bytes remaining",
             100.0 * (this->tft_size_ - this->content_length_) / this->tft_size_, this->content_length_);

    if (!this->upload_first_chunk_sent_) {
      this->upload_first_chunk_sent_ = true;
      delay(500);  // NOLINT
    }

    this->recv_ret_string_(recv_string, 4096, true);
    if (recv_string[0] != 0x05) {  // 0x05 == "ok"
      ESP_LOGD(TAG, "recv_string [%s]",
               format_hex_pretty(reinterpret_cast<const uint8_t *>(recv_string.data()), recv_string.size()).c_str());
    }

    // handle partial upload request
    if (recv_string[0] == 0x08 && recv_string.size() == 5) {
      uint32_t result = 0;
      for (int j = 0; j < 4; ++j) {
        result += static_cast<uint8_t>(recv_string[j + 1]) << (8 * j);
      }
      if (result > 0) {
        ESP_LOGD(TAG, "Nextion reported new range %d", result);
        this->content_length_ = this->tft_size_ - result;
        return result;
      }
    }
    recv_string.clear();
  }

  return range_end + 1;
}

bool Nextion::upload_tft(uint32_t baud_rate, bool exit_reparse) {
  ESP_LOGD(TAG, "Nextion TFT upload requested");
  ESP_LOGD(TAG, "Exit reparse: %s", YESNO(exit_reparse));
  ESP_LOGD(TAG, "URL: %s", this->tft_url_.c_str());

  if (this->is_updating_) {
    ESP_LOGD(TAG, "Currently updating");
    return false;
  }

  if (!network::is_connected()) {
    ESP_LOGD(TAG, "network is not connected");
    return false;
  }

  this->is_updating_ = true;

  if (exit_reparse) {
    ESP_LOGD(TAG, "Exiting Nextion reparse mode");
    if (!this->set_protocol_reparse_mode(false)) {
      ESP_LOGW(TAG, "Failed to request Nextion to exit reparse mode");
      return false;
    }
  }

  // Check if baud rate is supported
  this->original_baud_rate_ = this->parent_->get_baud_rate();
  static const std::vector<uint32_t> SUPPORTED_BAUD_RATES = {2400,   4800,   9600,   19200,  31250,  38400, 57600,
                                                             115200, 230400, 250000, 256000, 512000, 921600};
  if (std::find(SUPPORTED_BAUD_RATES.begin(), SUPPORTED_BAUD_RATES.end(), baud_rate) == SUPPORTED_BAUD_RATES.end()) {
    baud_rate = this->original_baud_rate_;
  }
  ESP_LOGD(TAG, "Baud rate: %" PRIu32, baud_rate);

  HTTPClient http;
  http.setTimeout(15000);  // Yes 15 seconds.... Helps 8266s along
  bool begin_status = false;
#ifdef USE_ESP32
  begin_status = http.begin(this->tft_url_.c_str());
#endif
#ifdef USE_ESP8266
#if USE_ARDUINO_VERSION_CODE >= VERSION_CODE(2, 7, 0)
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
#elif USE_ARDUINO_VERSION_CODE >= VERSION_CODE(2, 6, 0)
  http.setFollowRedirects(true);
#endif
#if USE_ARDUINO_VERSION_CODE >= VERSION_CODE(2, 6, 0)
  http.setRedirectLimit(3);
#endif
  begin_status = http.begin(*this->get_wifi_client_(), this->tft_url_.c_str());
#endif

  if (!begin_status) {
    this->is_updating_ = false;
    ESP_LOGD(TAG, "Connection failed");
    ExternalRAMAllocator<uint8_t> allocator(ExternalRAMAllocator<uint8_t>::ALLOW_FAILURE);
    allocator.deallocate(this->transfer_buffer_, this->transfer_buffer_size_);
    return false;
  } else {
    ESP_LOGD(TAG, "Connected");
  }

  http.addHeader("Range", "bytes=0-255");
  const char *header_names[] = {"Content-Range"};
  http.collectHeaders(header_names, 1);
  ESP_LOGD(TAG, "Requesting URL: %s", this->tft_url_.c_str());

  http.setReuse(true);
  // try up to 5 times. DNS sometimes needs a second try or so
  int tries = 1;
  int code = http.GET();
  delay(100);  // NOLINT

  App.feed_wdt();
  while (code != 200 && code != 206 && tries <= 5) {
    ESP_LOGW(TAG, "HTTP Request failed; URL: %s; Error: %s, retrying (%d/5)", this->tft_url_.c_str(),
             HTTPClient::errorToString(code).c_str(), tries);

    delay(250);  // NOLINT
    App.feed_wdt();
    code = http.GET();
    ++tries;
  }

  if ((code != 200 && code != 206) || tries > 5) {
    return this->upload_end_(false);
  }

  String content_range_string = http.header("Content-Range");
  content_range_string.remove(0, 12);
  this->content_length_ = content_range_string.toInt();
  this->tft_size_ = content_length_;
  http.end();

  if (this->content_length_ < 4096) {
    ESP_LOGE(TAG, "Failed to get file size");
    return this->upload_end_(false);
  }

  ESP_LOGD(TAG, "Updating Nextion %s...", this->device_model_.c_str());
  // The Nextion will ignore the update command if it is sleeping

  this->send_command_("sleep=0");
  this->set_backlight_brightness(1.0);
  delay(250);  // NOLINT

  App.feed_wdt();

  char command[128];
  // Tells the Nextion the content length of the tft file and baud rate it will be sent at
  // Once the Nextion accepts the command it will wait until the file is successfully uploaded
  // If it fails for any reason a power cycle of the display will be needed
  sprintf(command, "whmi-wris %d,%d,1", this->content_length_, baud_rate);

  // Clear serial receive buffer
  uint8_t d;
  while (this->available()) {
    this->read_byte(&d);
  };

  this->send_command_(command);

  if (baud_rate != this->original_baud_rate_) {
    ESP_LOGD(TAG, "Changing baud rate from %" PRIu32 " to %" PRIu32 " bps", this->original_baud_rate_, baud_rate);
    this->parent_->set_baud_rate(baud_rate);
    this->parent_->load_settings();
  }

  App.feed_wdt();

  std::string response;
  ESP_LOGD(TAG, "Waiting for upgrade response");
  this->recv_ret_string_(response, 2000, true);  // This can take some time to return

  // The Nextion display will, if it's ready to accept data, send a 0x05 byte.
  ESP_LOGD(TAG, "Upgrade response is [%s] - %zu bytes",
           format_hex_pretty(reinterpret_cast<const uint8_t *>(response.data()), response.size()).c_str(),
           response.length());

  for (size_t i = 0; i < response.length(); i++) {
    ESP_LOGD(TAG, "Available %d : 0x%02X", i, response[i]);
  }

  if (response.find(0x05) != std::string::npos) {
    ESP_LOGD(TAG, "preparation for tft update done");
  } else {
    ESP_LOGD(TAG, "preparation for tft update failed %d \"%s\"", response[0], response.c_str());
    return this->upload_end_(false);
  }

  // Nextion wants 4096 bytes at a time. Make chunk_size a multiple of 4096
#ifdef USE_ESP32
  uint32_t chunk_size = 8192;
  if (heap_caps_get_free_size(MALLOC_CAP_SPIRAM) > 0) {
    chunk_size = this->content_length_;
  } else {
    if (ESP.getFreeHeap() > 81920) {  // Ensure some FreeHeap to other things and limit chunk size
      chunk_size = ESP.getFreeHeap() - 65536;
      chunk_size = int(chunk_size / 4096) * 4096;
      chunk_size = chunk_size > 65536 ? 65536 : chunk_size;
    } else if (ESP.getFreeHeap() < 32768) {
      chunk_size = 4096;
    }
  }
#else
  // NOLINTNEXTLINE(readability-static-accessed-through-instance)
  uint32_t chunk_size = ESP.getFreeHeap() < 16384 ? 4096 : 8192;
#endif

  if (this->transfer_buffer_ == nullptr) {
    ExternalRAMAllocator<uint8_t> allocator(ExternalRAMAllocator<uint8_t>::ALLOW_FAILURE);
    // NOLINTNEXTLINE(readability-static-accessed-through-instance)
    ESP_LOGD(TAG, "Allocating buffer size %d, Heap size is %u", chunk_size, ESP.getFreeHeap());
    this->transfer_buffer_ = allocator.allocate(chunk_size);
    if (this->transfer_buffer_ == nullptr) {  // Try a smaller size
      ESP_LOGD(TAG, "Could not allocate buffer size: %d trying 4096 instead", chunk_size);
      chunk_size = 4096;
      ESP_LOGD(TAG, "Allocating %d buffer", chunk_size);
      this->transfer_buffer_ = allocator.allocate(chunk_size);

      if (!this->transfer_buffer_)
        return this->upload_end_(false);
    }

    this->transfer_buffer_size_ = chunk_size;
  }

  // NOLINTNEXTLINE(readability-static-accessed-through-instance)
  ESP_LOGD(TAG, "Updating tft from \"%s\" with a file size of %d using %zu chunksize, Heap Size %d",
           this->tft_url_.c_str(), this->content_length_, this->transfer_buffer_size_, ESP.getFreeHeap());

  int result = 0;
  while (this->content_length_ > 0) {
    result = this->upload_by_chunks_(&http, result);
    if (result < 0) {
      ESP_LOGD(TAG, "Error updating Nextion!");
      return this->upload_end_(false);
    }
    App.feed_wdt();
    // NOLINTNEXTLINE(readability-static-accessed-through-instance)
    ESP_LOGD(TAG, "Heap Size %d, Bytes left %d", ESP.getFreeHeap(), this->content_length_);
  }
  ESP_LOGD(TAG, "Successfully updated Nextion!");

  return this->upload_end_(true);
}

bool Nextion::upload_end_(bool successful) {
  this->is_updating_ = false;

  uint32_t baud_rate = this->parent_->get_baud_rate();
  if (baud_rate != this->original_baud_rate_) {
    ESP_LOGD(TAG, "Changing baud rate back from %" PRIu32 " to %" PRIu32 " bps", baud_rate, this->original_baud_rate_);
    this->parent_->set_baud_rate(this->original_baud_rate_);
    this->parent_->load_settings();
  }

  ESP_LOGD(TAG, "Restarting Nextion");
  this->soft_reset();
  if (successful) {
    delay(1500);  // NOLINT
    ESP_LOGD(TAG, "Restarting esphome");
    ESP.restart();  // NOLINT(readability-static-accessed-through-instance)
  }
  return successful;
}

#ifdef USE_ESP8266
WiFiClient *Nextion::get_wifi_client_() {
  if (this->tft_url_.compare(0, 6, "https:") == 0) {
    if (this->wifi_client_secure_ == nullptr) {
      // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
      this->wifi_client_secure_ = new BearSSL::WiFiClientSecure();
      this->wifi_client_secure_->setInsecure();
      this->wifi_client_secure_->setBufferSizes(512, 512);
    }
    return this->wifi_client_secure_;
  }

  if (this->wifi_client_ == nullptr) {
    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
    this->wifi_client_ = new WiFiClient();
  }
  return this->wifi_client_;
}
#endif
}  // namespace nextion
}  // namespace esphome

#endif  // USE_NEXTION_TFT_UPLOAD
#endif  // ARDUINO
