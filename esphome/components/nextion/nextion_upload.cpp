#include "nextion.h"

#ifdef USE_NEXTION_TFT_UPLOAD

#include "esphome/core/application.h"
#include "esphome/core/defines.h"
#include "esphome/core/util.h"
#include "esphome/core/log.h"
#include "esphome/components/network/util.h"
#include <cinttypes>

#ifdef USE_ESP32
#include <esp_heap_caps.h>
#endif
#ifdef USE_ESP_IDF
#include <esp_http_client.h>
#endif

namespace esphome {
namespace nextion {
#ifdef ARDUINO
static const char *const TAG = "nextion.upload.arduino";
#elif defined(USE_ESP_IDF)
static const char *const TAG = "nextion.upload.idf";
#endif  // ARDUINO vs USE_ESP_IDF

// Followed guide
// https://unofficialnextion.com/t/nextion-upload-protocol-v1-2-the-fast-one/1044/2

const char *Nextion::tft_upload_result_to_string(Nextion::TFTUploadResult result) {
  switch (result) {
    case Nextion::TFTUploadResult::OK:
      return "Upload successful";

    case Nextion::TFTUploadResult::UPLOAD_IN_PROGRESS:
      return "Another upload is already in progress";

    case Nextion::TFTUploadResult::NETWORK_ERROR_NOT_CONNECTED:
      return "Network is not connected";

    case Nextion::TFTUploadResult::HTTP_ERROR_CONNECTION_FAILED:
      return "Connection to HTTP server failed";

    case Nextion::TFTUploadResult::HTTP_ERROR_RESPONSE_SERVER:
      return "HTTP server error response";

    case Nextion::TFTUploadResult::HTTP_ERROR_RESPONSE_NOT_FOUND:
      return "The requested resource was not found on the HTTP server";

    case Nextion::TFTUploadResult::HTTP_ERROR_RESPONSE_CLIENT:
      return "HTTP error - resource not found";

    case Nextion::TFTUploadResult::HTTP_ERROR_RESPONSE_REDIRECTION:
      return "HTTP redirection error response";

    case Nextion::TFTUploadResult::HTTP_ERROR_RESPONSE_OTHER:
      return "HTTP other error response";

    case Nextion::TFTUploadResult::HTTP_ERROR_INVALID_SERVER_HEADER:
      return "HTTP server provided an invalid header";

    case Nextion::TFTUploadResult::HTTP_ERROR_CLIENT_INITIALIZATION:
      return "Failed to initialize HTTP client";

    case Nextion::TFTUploadResult::HTTP_ERROR_KEEP_ALIVE:
      return "HTTP failed to setup a persistent connection";

    case Nextion::TFTUploadResult::HTTP_ERROR_REQUEST_FAILED:
      return "HTTP request failed";

    case Nextion::TFTUploadResult::HTTP_ERROR_INVALID_FILE_SIZE:
      return "The downloaded file size did not match the expected size";

    case Nextion::TFTUploadResult::HTTP_ERROR_FAILED_TO_FETCH_FULL_PACKAGE:
      return "Failed to fetch full package from HTTP server";

    case Nextion::TFTUploadResult::HTTP_ERROR_FAILED_TO_OPEN_CONNECTION:
      return "Failed to open connection to HTTP server";

    case Nextion::TFTUploadResult::HTTP_ERROR_FAILED_TO_GET_CONTENT_LENGHT:
      return "Failed to get content length from HTTP server";

    case Nextion::TFTUploadResult::HTTP_ERROR_SET_METHOD_FAILED:
      return "Failed to set HTTP method";

    // Nextion Errors
    case Nextion::TFTUploadResult::NEXTION_ERROR_PREPARATION_FAILED:
      return "Preparation for TFT upload failed";

    case Nextion::TFTUploadResult::NEXTION_ERROR_INVALID_RESPONSE:
      return "Invalid response from Nextion";

    case Nextion::TFTUploadResult::NEXTION_ERROR_EXIT_REPARSE_NOT_SENT:
      return "Failed to send an exit reparse command to Nextion";

    // Process Errors
    case Nextion::TFTUploadResult::PROCESS_ERROR_INVALID_RANGE:
      return "Invalid range requested";

    // Memory Errors
    case Nextion::TFTUploadResult::MEMORY_ERROR_FAILED_TO_ALLOCATE:
      return "Failed to allocate memory";

    default:
      return "Unknown error";
  }
}

uint32_t Nextion::get_free_heap_() {
#ifdef ARDUINO
#ifdef ESP32
  return ESP.getFreeHeap();
#elif defined(USE_ESP8266)
  return EspClass::getFreeHeap();
#endif  // ESP32 vs USE_ESP8266
#elif defined(USE_ESP_IDF)
  return esp_get_free_heap_size();
#endif  // ARDUINO vs USE_ESP_IDF
}

Nextion::TFTUploadResult Nextion::handle_http_response_code_(int code) {
  if (code >= 500) {
    return Nextion::TFTUploadResult::HTTP_ERROR_RESPONSE_SERVER;
  } else if (code == 404) {  // This is a special case as many times local server are used
    return Nextion::TFTUploadResult::HTTP_ERROR_RESPONSE_NOT_FOUND;
  } else if (code >= 400) {
    return Nextion::TFTUploadResult::HTTP_ERROR_RESPONSE_CLIENT;
  } else if (code >= 300) {
    return Nextion::TFTUploadResult::HTTP_ERROR_RESPONSE_REDIRECTION;
  } else if (code != 200 && code != 206) {
    return Nextion::TFTUploadResult::HTTP_ERROR_RESPONSE_OTHER;
  }
  return Nextion::TFTUploadResult::OK;
}

#ifdef ARDUINO
Nextion::TFTUploadResult Nextion::upload_by_chunks_(HTTPClient &http_client, int &range_start) {
#elif defined(USE_ESP_IDF)
Nextion::TFTUploadResult Nextion::upload_by_chunks_(esp_http_client_handle_t http_client, int &range_start) {
#endif  // ARDUINO vs USE_ESP_IDF
  uint range_size = this->tft_size_ - range_start;
  ESP_LOGV(TAG, "Free heap: %" PRIu32, this->get_free_heap_());
  int range_end = ((upload_first_chunk_sent_ or this->tft_size_ < 4096) ? this->tft_size_ : 4096) - 1;
  ESP_LOGD(TAG, "Range start: %i", range_start);
  if (range_size <= 0 or range_end <= range_start) {
    ESP_LOGD(TAG, "Range end: %i", range_end);
    ESP_LOGD(TAG, "Range size: %i", range_size);
    ESP_LOGE(TAG, "Invalid range");
    return Nextion::TFTUploadResult::PROCESS_ERROR_INVALID_RANGE;
  }

  char range_header[64];
  sprintf(range_header, "bytes=%d-%d", range_start, range_end);
  ESP_LOGV(TAG, "Requesting range: %s", range_header);
#ifdef ARDUINO
  http_client.addHeader("Range", range_header);
  int code = http_client.GET();
  if (code != HTTP_CODE_OK and code != HTTP_CODE_PARTIAL_CONTENT) {
    ESP_LOGW(TAG, "HTTP Request failed; Error: %s", HTTPClient::errorToString(code).c_str());
    return Nextion::TFTUploadResult::HTTP_ERROR_REQUEST_FAILED;
  }
#elif defined(USE_ESP_IDF)
  esp_http_client_set_header(http_client, "Range", range_header);
  ESP_LOGV(TAG, "Opening HTTP connetion");
  esp_err_t err;
  if ((err = esp_http_client_open(http_client, 0)) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
    return Nextion::TFTUploadResult::HTTP_ERROR_FAILED_TO_OPEN_CONNECTION;
  }
#endif  // ARDUINO vs USE_ESP_IDF

  ESP_LOGV(TAG, "Fetch content length");
#ifdef ARDUINO
  int content_length = range_end - range_start;
#elif defined(USE_ESP_IDF)
  int content_length = esp_http_client_fetch_headers(http_client);
#endif  // ARDUINO vs USE_ESP_IDF
  ESP_LOGV(TAG, "content_length = %d", content_length);
  if (content_length <= 0) {
    ESP_LOGE(TAG, "Failed to get content length: %d", content_length);
    return Nextion::TFTUploadResult::HTTP_ERROR_FAILED_TO_GET_CONTENT_LENGHT;
  }

  std::string recv_string;
  while (true) {
    App.feed_wdt();
    int buffer_size = std::min(this->content_length_, 4096);  // Limits buffer to the remaining data
    std::vector<uint8_t> buffer(buffer_size);                 // Initialize buffer with the specified size
    ESP_LOGVV(TAG, "Fetching %d bytes from HTTP", buffer_size);
    int read_len = 0;
    int partial_read_len = 0;
#ifdef ARDUINO
    uint32_t start_time = millis();
    const uint32_t timeout = 5000;
    while (read_len < buffer_size && millis() - start_time < timeout) {
      if (http_client.getStreamPtr()->available() > 0) {
        partial_read_len = http_client.getStreamPtr()->readBytes(reinterpret_cast<char *>(buffer.data()) + read_len,
                                                                 buffer_size - read_len);
        read_len += partial_read_len;
        if (partial_read_len > 0) {
          App.feed_wdt();
          delay(2);
        }
      }
    }
#elif defined(USE_ESP_IDF)
    int retries = 0;
    // Attempt to read the chunk with retries.
    while (retries < 5 && read_len < buffer_size) {
      partial_read_len =
          esp_http_client_read(http_client, reinterpret_cast<char *>(buffer.data()) + read_len, buffer_size - read_len);
      if (partial_read_len > 0) {
        read_len += partial_read_len;  // Accumulate the total read length.
        // Reset retries on successful read.
        retries = 0;
      } else {
        // If no data was read, increment retries.
        retries++;
        vTaskDelay(pdMS_TO_TICKS(2));  // NOLINT
      }
      App.feed_wdt();  // Feed the watchdog timer.
    }
#endif  // ARDUINO vs USE_ESP_IDF
    if (read_len != buffer_size) {
      // Did not receive the full package within the timeout period
      ESP_LOGE(TAG, "Failed to read full package, received only %d of %d bytes", read_len, buffer_size);
      return Nextion::TFTUploadResult::HTTP_ERROR_FAILED_TO_FETCH_FULL_PACKAGE;
    }
    ESP_LOGVV(TAG, "%d bytes fetched, writing it to UART", read_len);
    if (read_len > 0) {
      recv_string.clear();
      this->write_array(buffer);
      App.feed_wdt();
      this->recv_ret_string_(recv_string, upload_first_chunk_sent_ ? 500 : 5000, true);
      this->content_length_ -= read_len;
      ESP_LOGD(TAG, "Uploaded %0.2f %%, remaining %d bytes, free heap: %" PRIu32 " bytes",
               100.0 * (this->tft_size_ - this->content_length_) / this->tft_size_, this->content_length_,
               this->get_free_heap_());
      upload_first_chunk_sent_ = true;
      if (recv_string[0] == 0x08 && recv_string.size() == 5) {  // handle partial upload request
        ESP_LOGD(TAG, "recv_string [%s]",
                 format_hex_pretty(reinterpret_cast<const uint8_t *>(recv_string.data()), recv_string.size()).c_str());
        uint32_t result = 0;
        for (int j = 0; j < 4; ++j) {
          result += static_cast<uint8_t>(recv_string[j + 1]) << (8 * j);
        }
        if (result > 0) {
          ESP_LOGI(TAG, "Nextion reported new range %" PRIu32, result);
          this->content_length_ = this->tft_size_ - result;
          range_start = result;
        } else {
          range_start = range_end + 1;
        }
        return Nextion::TFTUploadResult::OK;
      } else if (recv_string[0] != 0x05 and recv_string[0] != 0x08) {  // 0x05 == "ok"
        ESP_LOGE(TAG, "Invalid response from Nextion: [%s]",
                 format_hex_pretty(reinterpret_cast<const uint8_t *>(recv_string.data()), recv_string.size()).c_str());
        return Nextion::TFTUploadResult::NEXTION_ERROR_INVALID_RESPONSE;
      }

      recv_string.clear();
    } else if (read_len == 0) {
      ESP_LOGV(TAG, "End of HTTP response reached");
      break;  // Exit the loop if there is no more data to read
    } else {
      ESP_LOGE(TAG, "Failed to read from HTTP client, error code: %d", read_len);
      break;  // Exit the loop on error
    }
  }
  range_start = range_end + 1;
  return Nextion::TFTUploadResult::OK;
}

Nextion::TFTUploadResult Nextion::upload_tft(bool exit_reparse) {
  ESP_LOGD(TAG, "Nextion TFT upload requested");
  ESP_LOGD(TAG, "URL: %s", this->tft_url_.c_str());

  if (this->is_updating_) {
    ESP_LOGW(TAG, "Currently uploading");
    return Nextion::TFTUploadResult::UPLOAD_IN_PROGRESS;
  }

  if (!network::is_connected()) {
    ESP_LOGE(TAG, "Network is not connected");
    return Nextion::TFTUploadResult::NETWORK_ERROR_NOT_CONNECTED;
  }

  this->is_updating_ = true;

  if (exit_reparse) {
    ESP_LOGD(TAG, "Exiting Nextion reparse mode");
    if (!this->set_protocol_reparse_mode(false)) {
      ESP_LOGW(TAG, "Failed to request Nextion to exit reparse mode");
      return Nextion::TFTUploadResult::NEXTION_ERROR_EXIT_REPARSE_NOT_SENT;
    }
  }

  // Define the configuration for the HTTP client
  ESP_LOGV(TAG, "Initializing HTTP client");
  ESP_LOGV(TAG, "Free heap: %" PRIu32, this->get_free_heap_());
#ifdef ARDUINO
  HTTPClient http_client;
  http_client.setTimeout(15000);  // Yes 15 seconds.... Helps 8266s along
  bool begin_status = false;
#ifdef USE_ESP32
  begin_status = http_client.begin(this->tft_url_.c_str());
#endif
#ifdef USE_ESP8266
#if USE_ARDUINO_VERSION_CODE >= VERSION_CODE(2, 7, 0)
  http_client.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
#elif USE_ARDUINO_VERSION_CODE >= VERSION_CODE(2, 6, 0)
  http_client.setFollowRedirects(true);
#endif
#if USE_ARDUINO_VERSION_CODE >= VERSION_CODE(2, 6, 0)
  http_client.setRedirectLimit(3);
#endif
  begin_status = http_client.begin(*this->get_wifi_client_(), this->tft_url_.c_str());
#endif  // USE_ESP8266
  if (!begin_status) {
    this->is_updating_ = false;
    ESP_LOGD(TAG, "Connection failed");
    return Nextion::TFTUploadResult::HTTP_ERROR_CONNECTION_FAILED;
  } else {
    ESP_LOGD(TAG, "Connected");
  }
  http_client.addHeader("Range", "bytes=0-255");
  const char *header_names[] = {"Content-Range"};
  http_client.collectHeaders(header_names, 1);
  ESP_LOGD(TAG, "Requesting URL: %s", this->tft_url_.c_str());
  http_client.setReuse(true);
  // try up to 5 times. DNS sometimes needs a second try or so
  int tries = 1;
  int code = http_client.GET();
  delay(100);  // NOLINT

  App.feed_wdt();
  while (code != 200 && code != 206 && tries <= 5) {
    ESP_LOGW(TAG, "HTTP Request failed; URL: %s; Error: %s, retrying (%d/5)", this->tft_url_.c_str(),
             HTTPClient::errorToString(code).c_str(), tries);

    delay(250);  // NOLINT
    App.feed_wdt();
    code = http_client.GET();
    ++tries;
  }

  TFTUploadResult response_error = this->handle_http_response_code_(code);
  if (response_error != Nextion::TFTUploadResult::OK) {
    return this->upload_end_(response_error);
  }

  String content_range_string = http_client.header("Content-Range");
  content_range_string.remove(0, 12);
  this->tft_size_ = content_range_string.toInt();

#elif defined(USE_ESP_IDF)
  esp_http_client_config_t config = {
      .url = this->tft_url_.c_str(),
      .cert_pem = nullptr,
      .method = HTTP_METHOD_HEAD,
      .timeout_ms = 15000,
      .disable_auto_redirect = false,
      .max_redirection_count = 10,
  };
  // Initialize the HTTP client with the configuration
  esp_http_client_handle_t http_client = esp_http_client_init(&config);
  if (!http_client) {
    ESP_LOGE(TAG, "Failed to initialize HTTP client.");
    return this->upload_end_(Nextion::TFTUploadResult::HTTP_ERROR_CLIENT_INITIALIZATION);
  }

  esp_err_t err = esp_http_client_set_header(http_client, "Connection", "keep-alive");
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "HTTP set header failed: %s", esp_err_to_name(err));
    esp_http_client_cleanup(http_client);
    return this->upload_end_(Nextion::TFTUploadResult::HTTP_ERROR_KEEP_ALIVE);
  }

  // Perform the HTTP request
  ESP_LOGV(TAG, "Check if the client could connect");
  ESP_LOGV(TAG, "Free heap: %" PRIu32, this->get_free_heap_());
  err = esp_http_client_perform(http_client);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(err));
    esp_http_client_cleanup(http_client);
    return this->upload_end_(Nextion::TFTUploadResult::HTTP_ERROR_REQUEST_FAILED);
  }

  // Check the HTTP Status Code
  ESP_LOGV(TAG, "Check the HTTP Status Code");
  ESP_LOGV(TAG, "Free heap: %" PRIu32, this->get_free_heap_());
  int status_code = esp_http_client_get_status_code(http_client);
  TFTUploadResult response_error = this->handle_http_response_code_(status_code);
  if (response_error != Nextion::TFTUploadResult::OK) {
    return this->upload_end_(response_error);
  }

  this->tft_size_ = esp_http_client_get_content_length(http_client);
#endif  // ARDUINO vs USE_ESP_IDF

  ESP_LOGD(TAG, "TFT file size: %zu bytes", this->tft_size_);
  if (this->tft_size_ < 4096) {
    ESP_LOGE(TAG, "File size check failed.");
    ESP_LOGD(TAG, "Close HTTP connection");
#ifdef ARDUINO
    http_client.end();
#elif defined(USE_ESP_IDF)
    esp_http_client_close(http_client);
    esp_http_client_cleanup(http_client);
#endif  // ARDUINO vs USE_ESP_IDF
    ESP_LOGV(TAG, "Connection closed");
    return this->upload_end_(Nextion::TFTUploadResult::HTTP_ERROR_INVALID_FILE_SIZE);
  } else {
    ESP_LOGV(TAG, "File size check passed. Proceeding...");
  }
  this->content_length_ = this->tft_size_;

  ESP_LOGD(TAG, "Uploading Nextion");

  // The Nextion will ignore the upload command if it is sleeping
  ESP_LOGV(TAG, "Wake-up Nextion");
  ESP_LOGV(TAG, "Free heap: %" PRIu32, this->get_free_heap_());
  this->send_command_("sleep=0");
  this->set_backlight_brightness(1.0);

  App.feed_wdt();
  char command[128];
  // Tells the Nextion the content length of the tft file and baud rate it will be sent at
  // Once the Nextion accepts the command it will wait until the file is successfully uploaded
  // If it fails for any reason a power cycle of the display will be needed
  sprintf(command, "whmi-wris %d,%" PRIu32 ",1", this->content_length_, this->parent_->get_baud_rate());

  // Clear serial receive buffer
  ESP_LOGV(TAG, "Clear serial receive buffer");
  ESP_LOGV(TAG, "Free heap: %" PRIu32, this->get_free_heap_());
  uint8_t d;
  while (this->available()) {
    this->read_byte(&d);
  };

  ESP_LOGV(TAG, "Send upload instruction: %s", command);
  ESP_LOGV(TAG, "Free heap: %" PRIu32, this->get_free_heap_());
  this->send_command_(command);

  std::string response;
  ESP_LOGV(TAG, "Waiting for upgrade response");
  this->recv_ret_string_(response, 2000, true);  // This can take some time to return

  // The Nextion display will, if it's ready to accept data, send a 0x05 byte.
  ESP_LOGD(TAG, "Upgrade response is [%s] - %zu byte(s)",
           format_hex_pretty(reinterpret_cast<const uint8_t *>(response.data()), response.size()).c_str(),
           response.length());
  ESP_LOGV(TAG, "Free heap: %" PRIu32, this->get_free_heap_());

  if (response.find(0x05) != std::string::npos) {
    ESP_LOGV(TAG, "Preparation for TFT upload done");
  } else {
    ESP_LOGE(TAG, "Preparation for TFT upload failed %d \"%s\"", response[0], response.c_str());
    ESP_LOGD(TAG, "Close HTTP connection");
#ifdef ARDUINO
    http_client.end();
#elif defined(USE_ESP_IDF)
    esp_http_client_close(http_client);
    esp_http_client_cleanup(http_client);
#endif  // ARDUINO vs USE_ESP_IDF
    ESP_LOGV(TAG, "Connection closed");
    return this->upload_end_(Nextion::TFTUploadResult::NEXTION_ERROR_PREPARATION_FAILED);
  }

#ifdef USE_ESP_IDF
  ESP_LOGV(TAG, "Change the method to GET before starting the download");
  esp_err_t set_method_result = esp_http_client_set_method(http_client, HTTP_METHOD_GET);
  if (set_method_result != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set HTTP method to GET: %s", esp_err_to_name(set_method_result));
    return Nextion::TFTUploadResult::HTTP_ERROR_SET_METHOD_FAILED;
  }
#endif  // USE_ESP_IDF

  ESP_LOGD(TAG, "Uploading TFT to Nextion:");
  ESP_LOGD(TAG, "  URL: %s", this->tft_url_.c_str());
  ESP_LOGD(TAG, "  File size: %d bytes", this->content_length_);
  ESP_LOGD(TAG, "  Free heap: %" PRIu32, this->get_free_heap_());

  // Proceed with the content download as before

  ESP_LOGV(TAG, "Starting transfer by chunks loop");

  int position = 0;
  while (this->content_length_ > 0) {
    Nextion::TFTUploadResult upload_result = upload_by_chunks_(http_client, position);
    if (upload_result != Nextion::TFTUploadResult::OK) {
      ESP_LOGE(TAG, "Error uploading TFT to Nextion!");
      ESP_LOGD(TAG, "Close HTTP connection");
#ifdef ARDUINO
      http_client.end();
#elif defined(USE_ESP_IDF)
      esp_http_client_close(http_client);
      esp_http_client_cleanup(http_client);
#endif  // ARDUINO vs USE_ESP_IDF
      ESP_LOGV(TAG, "Connection closed");
      return this->upload_end_(upload_result);
    }
    App.feed_wdt();
    ESP_LOGV(TAG, "Free heap: %" PRIu32 ", Bytes left: %d", this->get_free_heap_(), this->content_length_);
  }

  ESP_LOGD(TAG, "Successfully uploaded TFT to Nextion!");

  ESP_LOGD(TAG, "Close HTTP connection");
#ifdef ARDUINO
  http_client.end();
#elif defined(USE_ESP_IDF)
  esp_http_client_close(http_client);
  esp_http_client_cleanup(http_client);
#endif  // ARDUINO vs USE_ESP_IDF
  ESP_LOGV(TAG, "Connection closed");
  return upload_end_(Nextion::TFTUploadResult::OK);
}

Nextion::TFTUploadResult Nextion::upload_end_(Nextion::TFTUploadResult upload_results) {
  ESP_LOGD(TAG, "Nextion TFT upload finished: %s", this->tft_upload_result_to_string(upload_results));
  this->is_updating_ = false;
  if (upload_results == Nextion::TFTUploadResult::OK) {
    ESP_LOGD(TAG, "Restarting ESPHome");
#ifdef ARDUINO
    delay(1500);    // NOLINT
    ESP.restart();  // NOLINT(readability-static-accessed-through-instance)
#elif defined(USE_ESP_IDF)
    vTaskDelay(pdMS_TO_TICKS(1500));  // NOLINT
    esp_restart();                    // NOLINT(readability-static-accessed-through-instance)
#endif  // ARDUINO vs USE_ESP_IDF
  } else {
    ESP_LOGE(TAG, "Nextion TFT upload failed");
  }
  return upload_results;
}

}  // namespace nextion
}  // namespace esphome

#endif  // USE_NEXTION_TFT_UPLOAD
