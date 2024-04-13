#include "nextion.h"

#ifdef USE_NEXTION_TFT_UPLOAD
#ifdef USE_ESP_IDF

#include "esphome/core/application.h"
#include "esphome/core/defines.h"
#include "esphome/core/util.h"
#include "esphome/core/log.h"
#include "esphome/components/network/util.h"
#include <cinttypes>
#include <esp_heap_caps.h>
#include <esp_http_client.h>

namespace esphome {
namespace nextion {
static const char *const TAG = "nextion.upload.idf";

// Followed guide
// https://unofficialnextion.com/t/nextion-upload-protocol-v1-2-the-fast-one/1044/2

Nextion::TFTUploadResult Nextion::upload_by_chunks_(esp_http_client_handle_t http_client, int &range_start, uint8_t *buffer) {
  uint range_size = this->tft_size_ - range_start;
  ESP_LOGV(TAG, "Free heap: %" PRIu32, esp_get_free_heap_size());
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
  esp_http_client_set_header(http_client, "Range", range_header);
  ESP_LOGV(TAG, "Opening HTTP connetion");
  esp_err_t err;
  if ((err = esp_http_client_open(http_client, 0)) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
    return Nextion::TFTUploadResult::HTTP_ERROR_FAILED_TO_OPEN_CONNECTION;
  }

  ESP_LOGV(TAG, "Fetch content length");
  int content_length = esp_http_client_fetch_headers(http_client);
  ESP_LOGV(TAG, "content_length = %d", content_length);
  if (content_length <= 0) {
    ESP_LOGE(TAG, "Failed to get content length: %d", content_length);
    return Nextion::TFTUploadResult::HTTP_ERROR_FAILED_TO_GET_CONTENT_LENGTH;
  }

  std::string recv_string;
  while (true) {
    App.feed_wdt();
    int buffer_size = std::min(this->content_length_, 4096);  // Limits buffer to the remaining data
    ESP_LOGVV(TAG, "Fetching %d bytes from HTTP", buffer_size);
    int read_len = 0;
    int partial_read_len = 0;
    int retries = 0;
    // Attempt to read the chunk with retries.
    while (retries < 5 && read_len < buffer_size) {
      partial_read_len =
          esp_http_client_read(http_client, reinterpret_cast<char *>(buffer) + read_len, buffer_size - read_len);
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
    if (read_len != buffer_size) {
      // Did not receive the full package within the timeout period
      ESP_LOGE(TAG, "Failed to read full package, received only %d of %d bytes", read_len, buffer_size);
      return Nextion::TFTUploadResult::HTTP_ERROR_FAILED_TO_FETCH_FULL_PACKAGE;
    }
    ESP_LOGVV(TAG, "%d bytes fetched, writing it to UART", read_len);
    if (read_len > 0) {
      recv_string.clear();
      this->write_array(buffer, buffer_size);
      App.feed_wdt();
      this->recv_ret_string_(recv_string, upload_first_chunk_sent_ ? 500 : 5000, true);
      this->content_length_ -= read_len;
      ESP_LOGD(TAG, "Uploaded %0.2f %%, remaining %d bytes, free heap: %" PRIu32 " bytes",
               100.0 * (this->tft_size_ - this->content_length_) / this->tft_size_, this->content_length_,
               esp_get_free_heap_size());
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

Nextion::TFTUploadResult Nextion::upload_tft(uint32_t baud_rate, bool exit_reparse) {
  ESP_LOGD(TAG, "Nextion TFT upload requested");
  ESP_LOGD(TAG, "Exit reparse: %s", YESNO(exit_reparse));
  ESP_LOGD(TAG, "URL: %s", this->tft_url_.c_str());

  if (this->is_updating_) {
    ESP_LOGW(TAG, "Currently uploading");
    return Nextion::TFTUploadResult::UPLOAD_IN_PROGRESS;
  }

  if (!network::is_connected()) {
    ESP_LOGE(TAG, "Network is not connected");
    return Nextion::TFTUploadResult::NETWORK_ERROR_NOT_CONNECTED;
  }

  // Allocate the buffer dynamically
  uint8_t *buffer = (uint8_t*)heap_caps_malloc(4096, MALLOC_CAP_8BIT);
  if (!buffer) {
    ESP_LOGE(TAG, "Failed to allocate upload buffer");
    return Nextion::TFTUploadResult::MEMORY_ERROR_FAILED_TO_ALLOCATE;
  }

  this->is_updating_ = true;

  // Check if baud rate is supported
  this->original_baud_rate_ = this->parent_->get_baud_rate();
  static const std::vector<uint32_t> SUPPORTED_BAUD_RATES = {2400,   4800,   9600,   19200,  31250,  38400, 57600,
                                                             115200, 230400, 250000, 256000, 512000, 921600};
  if (std::find(SUPPORTED_BAUD_RATES.begin(), SUPPORTED_BAUD_RATES.end(), baud_rate) == SUPPORTED_BAUD_RATES.end()) {
    baud_rate = this->original_baud_rate_;
  }
  ESP_LOGD(TAG, "Baud rate: %" PRIu32, baud_rate);

  if (exit_reparse) {
    ESP_LOGD(TAG, "Exiting Nextion reparse mode");
    if (!this->set_protocol_reparse_mode(false)) {
      ESP_LOGW(TAG, "Failed to request Nextion to exit reparse mode");
      heap_caps_free(buffer);
      return Nextion::TFTUploadResult::NEXTION_ERROR_EXIT_REPARSE_NOT_SENT;
    }
  }

  // Define the configuration for the HTTP client
  ESP_LOGV(TAG, "Initializing HTTP client");
  ESP_LOGV(TAG, "Free heap: %" PRIu32, esp_get_free_heap_size());
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
    heap_caps_free(buffer);
    return this->upload_end_(Nextion::TFTUploadResult::HTTP_ERROR_CLIENT_INITIALIZATION);
  }

  esp_err_t err = esp_http_client_set_header(http_client, "Connection", "keep-alive");
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "HTTP set header failed: %s", esp_err_to_name(err));
    esp_http_client_cleanup(http_client);
    heap_caps_free(buffer);
    return this->upload_end_(Nextion::TFTUploadResult::HTTP_ERROR_KEEP_ALIVE);
  }

  // Perform the HTTP request
  ESP_LOGV(TAG, "Check if the client could connect");
  ESP_LOGV(TAG, "Free heap: %" PRIu32, esp_get_free_heap_size());
  err = esp_http_client_perform(http_client);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(err));
    esp_http_client_cleanup(http_client);
    heap_caps_free(buffer);
    return this->upload_end_(Nextion::TFTUploadResult::HTTP_ERROR_REQUEST_FAILED);
  }

  // Check the HTTP Status Code
  ESP_LOGV(TAG, "Check the HTTP Status Code");
  ESP_LOGV(TAG, "Free heap: %" PRIu32, esp_get_free_heap_size());
  int status_code = esp_http_client_get_status_code(http_client);
  TFTUploadResult response_error = this->handle_http_response_code_(status_code);
  if (response_error != Nextion::TFTUploadResult::OK) {
    heap_caps_free(buffer);
    return this->upload_end_(response_error);
  }

  this->tft_size_ = esp_http_client_get_content_length(http_client);

  ESP_LOGD(TAG, "TFT file size: %zu bytes", this->tft_size_);
  if (this->tft_size_ < 4096) {
    ESP_LOGE(TAG, "File size check failed.");
    ESP_LOGD(TAG, "Close HTTP connection");
    esp_http_client_close(http_client);
    esp_http_client_cleanup(http_client);
    ESP_LOGV(TAG, "Connection closed");
    heap_caps_free(buffer);
    return this->upload_end_(Nextion::TFTUploadResult::HTTP_ERROR_INVALID_FILE_SIZE);
  } else {
    ESP_LOGV(TAG, "File size check passed. Proceeding...");
  }
  this->content_length_ = this->tft_size_;

  ESP_LOGD(TAG, "Uploading Nextion");

  // The Nextion will ignore the upload command if it is sleeping
  ESP_LOGV(TAG, "Wake-up Nextion");
  this->ignore_is_setup_ = true;
  this->send_command_("sleep=0");
  this->send_command_("dim=100");
  vTaskDelay(pdMS_TO_TICKS(250));  // NOLINT
  ESP_LOGV(TAG, "Free heap: %" PRIu32, esp_get_free_heap_size());

  App.feed_wdt();
  char command[128];
  // Tells the Nextion the content length of the tft file and baud rate it will be sent at
  // Once the Nextion accepts the command it will wait until the file is successfully uploaded
  // If it fails for any reason a power cycle of the display will be needed
  sprintf(command, "whmi-wris %d,%" PRIu32 ",1", this->content_length_, baud_rate);

  // Clear serial receive buffer
  ESP_LOGV(TAG, "Clear serial receive buffer");
  this->reset_(false);
  vTaskDelay(pdMS_TO_TICKS(250));  // NOLINT
  ESP_LOGV(TAG, "Free heap: %" PRIu32, esp_get_free_heap_size());

  ESP_LOGV(TAG, "Send upload instruction: %s", command);
  this->send_command_(command);

  if (baud_rate != this->original_baud_rate_) {
    ESP_LOGD(TAG, "Changing baud rate from %" PRIu32 " to %" PRIu32 " bps", this->original_baud_rate_, baud_rate);
    this->parent_->set_baud_rate(baud_rate);
  }

  std::string response;
  ESP_LOGV(TAG, "Waiting for upgrade response");
  this->recv_ret_string_(response, 5000, true);  // This can take some time to return

  // The Nextion display will, if it's ready to accept data, send a 0x05 byte.
  ESP_LOGD(TAG, "Upgrade response is [%s] - %zu byte(s)",
           format_hex_pretty(reinterpret_cast<const uint8_t *>(response.data()), response.size()).c_str(),
           response.length());
  ESP_LOGV(TAG, "Free heap: %" PRIu32, esp_get_free_heap_size());

  if (response.find(0x05) != std::string::npos) {
    ESP_LOGV(TAG, "Preparation for TFT upload done");
  } else {
    ESP_LOGE(TAG, "Preparation for TFT upload failed %d \"%s\"", response[0], response.c_str());
    ESP_LOGD(TAG, "Close HTTP connection");
    esp_http_client_close(http_client);
    esp_http_client_cleanup(http_client);
    ESP_LOGV(TAG, "Connection closed");
    heap_caps_free(buffer);
    return this->upload_end_(Nextion::TFTUploadResult::NEXTION_ERROR_PREPARATION_FAILED);
  }

  ESP_LOGV(TAG, "Change the method to GET before starting the download");
  esp_err_t set_method_result = esp_http_client_set_method(http_client, HTTP_METHOD_GET);
  if (set_method_result != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set HTTP method to GET: %s", esp_err_to_name(set_method_result));
    heap_caps_free(buffer);
    return Nextion::TFTUploadResult::HTTP_ERROR_SET_METHOD_FAILED;
  }

  ESP_LOGD(TAG, "Uploading TFT to Nextion:");
  ESP_LOGD(TAG, "  URL: %s", this->tft_url_.c_str());
  ESP_LOGD(TAG, "  File size: %d bytes", this->content_length_);
  ESP_LOGD(TAG, "  Free heap: %" PRIu32, esp_get_free_heap_size());

  // Proceed with the content download as before

  ESP_LOGV(TAG, "Starting transfer by chunks loop");

  int position = 0;
  while (this->content_length_ > 0) {
    Nextion::TFTUploadResult upload_result = upload_by_chunks_(http_client, position, buffer);
    if (upload_result != Nextion::TFTUploadResult::OK) {
      ESP_LOGE(TAG, "Error uploading TFT to Nextion!");
      ESP_LOGD(TAG, "Close HTTP connection");
      esp_http_client_close(http_client);
      esp_http_client_cleanup(http_client);
      ESP_LOGV(TAG, "Connection closed");
      heap_caps_free(buffer);
      return this->upload_end_(upload_result);
    }
    App.feed_wdt();
    ESP_LOGV(TAG, "Free heap: %" PRIu32 ", Bytes left: %d", esp_get_free_heap_size(), this->content_length_);
  }

  ESP_LOGD(TAG, "Successfully uploaded TFT to Nextion!");

  ESP_LOGD(TAG, "Close HTTP connection");
  esp_http_client_close(http_client);
  esp_http_client_cleanup(http_client);
  ESP_LOGV(TAG, "Connection closed");
  heap_caps_free(buffer);
  return upload_end_(Nextion::TFTUploadResult::OK);
}

Nextion::TFTUploadResult Nextion::upload_end_(Nextion::TFTUploadResult upload_results) {
  ESP_LOGD(TAG, "Nextion TFT upload finished: %s", this->tft_upload_result_to_string(upload_results));
  this->is_updating_ = false;
  this->ignore_is_setup_ = false;

  uint32_t baud_rate = this->parent_->get_baud_rate();
  if (baud_rate != this->original_baud_rate_) {
    ESP_LOGD(TAG, "Changing baud rate back from %" PRIu32 " to %" PRIu32 " bps", baud_rate, this->original_baud_rate_);
    this->parent_->set_baud_rate(this->original_baud_rate_);
  }

  if (upload_results == Nextion::TFTUploadResult::OK) {
    ESP_LOGD(TAG, "Restarting ESPHome");
    vTaskDelay(pdMS_TO_TICKS(1500));  // NOLINT
    esp_restart();                    // NOLINT(readability-static-accessed-through-instance)
  } else {
    ESP_LOGE(TAG, "Nextion TFT upload failed");
  }
  return upload_results;
}

}  // namespace nextion
}  // namespace esphome

#endif  // USE_ESP_IDF
#endif  // USE_NEXTION_TFT_UPLOAD
