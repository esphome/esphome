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

int Nextion::upload_by_chunks_(esp_http_client_handle_t http_client, uint32_t &range_start) {
  uint32_t range_size = this->tft_size_ - range_start;
  ESP_LOGV(TAG, "Free heap: %" PRIu32, esp_get_free_heap_size());
  uint32_t range_end = ((upload_first_chunk_sent_ or this->tft_size_ < 4096) ? this->tft_size_ : 4096) - 1;
  ESP_LOGD(TAG, "Range start: %" PRIu32, range_start);
  if (range_size <= 0 or range_end <= range_start) {
    ESP_LOGD(TAG, "Range end: %" PRIu32, range_end);
    ESP_LOGD(TAG, "Range size: %" PRIu32, range_size);
    ESP_LOGE(TAG, "Invalid range");
    return -1;
  }

  char range_header[32];
  sprintf(range_header, "bytes=%" PRIu32 "-%" PRIu32, range_start, range_end);
  ESP_LOGV(TAG, "Requesting range: %s", range_header);
  esp_http_client_set_header(http_client, "Range", range_header);
  ESP_LOGV(TAG, "Opening HTTP connetion");
  esp_err_t err;
  if ((err = esp_http_client_open(http_client, 0)) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
    return -1;
  }

  ESP_LOGV(TAG, "Fetch content length");
  const int chunk_size = esp_http_client_fetch_headers(http_client);
  ESP_LOGV(TAG, "content_length = %d", chunk_size);
  if (chunk_size <= 0) {
    ESP_LOGE(TAG, "Failed to get chunk's content length: %d", chunk_size);
    return -1;
  }

  // Allocate the buffer dynamically
  ExternalRAMAllocator<uint8_t> allocator(ExternalRAMAllocator<uint8_t>::ALLOW_FAILURE);
  uint8_t *buffer = allocator.allocate(4096);
  if (!buffer) {
    ESP_LOGE(TAG, "Failed to allocate upload buffer");
    return -1;
  }

  std::string recv_string;
  while (true) {
    App.feed_wdt();
    const uint16_t buffer_size =
        this->content_length_ < 4096 ? this->content_length_ : 4096;  // Limits buffer to the remaining data
    ESP_LOGV(TAG, "Fetching %" PRIu16 " bytes from HTTP", buffer_size);
    uint16_t read_len = 0;
    int partial_read_len = 0;
    uint8_t retries = 0;
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
      ESP_LOGE(TAG, "Failed to read full package, received only %" PRIu16 " of %" PRIu16 " bytes", read_len,
               buffer_size);
      // Deallocate buffer
      allocator.deallocate(buffer, 4096);
      buffer = nullptr;
      return -1;
    }
    ESP_LOGV(TAG, "%d bytes fetched, writing it to UART", read_len);
    if (read_len > 0) {
      recv_string.clear();
      this->write_array(buffer, buffer_size);
      App.feed_wdt();
      this->recv_ret_string_(recv_string, upload_first_chunk_sent_ ? 500 : 5000, true);
      this->content_length_ -= read_len;
      const float upload_percentage = 100.0f * (this->tft_size_ - this->content_length_) / this->tft_size_;
#ifdef USE_PSRAM
      ESP_LOGD(
          TAG,
          "Uploaded %0.2f%%, remaining %" PRIu32 " bytes, free heap: %" PRIu32 " (DRAM) + %" PRIu32 " (PSRAM) bytes",
          upload_percentage, this->content_length_, static_cast<uint32_t>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
          static_cast<uint32_t>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)));
#else
      ESP_LOGD(TAG, "Uploaded %0.2f%%, remaining %" PRIu32 " bytes, free heap: %" PRIu32 " bytes", upload_percentage,
               this->content_length_, static_cast<uint32_t>(esp_get_free_heap_size()));
#endif
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
        // Deallocate buffer
        allocator.deallocate(buffer, 4096);
        buffer = nullptr;
        return range_end + 1;
      } else if (recv_string[0] != 0x05 and recv_string[0] != 0x08) {  // 0x05 == "ok"
        ESP_LOGE(TAG, "Invalid response from Nextion: [%s]",
                 format_hex_pretty(reinterpret_cast<const uint8_t *>(recv_string.data()), recv_string.size()).c_str());
        // Deallocate buffer
        allocator.deallocate(buffer, 4096);
        buffer = nullptr;
        return -1;
      }

      recv_string.clear();
    } else if (read_len == 0) {
      ESP_LOGV(TAG, "End of HTTP response reached");
      break;  // Exit the loop if there is no more data to read
    } else {
      ESP_LOGE(TAG, "Failed to read from HTTP client, error code: %" PRIu16, read_len);
      break;  // Exit the loop on error
    }
  }
  range_start = range_end + 1;
  // Deallocate buffer
  allocator.deallocate(buffer, 4096);
  buffer = nullptr;
  return range_end + 1;
}

bool Nextion::upload_tft(uint32_t baud_rate, bool exit_reparse) {
  ESP_LOGD(TAG, "Nextion TFT upload requested");
  ESP_LOGD(TAG, "Exit reparse: %s", YESNO(exit_reparse));
  ESP_LOGD(TAG, "URL: %s", this->tft_url_.c_str());

  if (this->is_updating_) {
    ESP_LOGW(TAG, "Currently uploading");
    return false;
  }

  if (!network::is_connected()) {
    ESP_LOGE(TAG, "Network is not connected");
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
    return this->upload_end_(false);
  }

  esp_err_t err = esp_http_client_set_header(http_client, "Connection", "keep-alive");
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "HTTP set header failed: %s", esp_err_to_name(err));
    esp_http_client_cleanup(http_client);
    return this->upload_end_(false);
  }

  // Perform the HTTP request
  ESP_LOGV(TAG, "Check if the client could connect");
  ESP_LOGV(TAG, "Free heap: %" PRIu32, esp_get_free_heap_size());
  err = esp_http_client_perform(http_client);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(err));
    esp_http_client_cleanup(http_client);
    return this->upload_end_(false);
  }

  // Check the HTTP Status Code
  ESP_LOGV(TAG, "Check the HTTP Status Code");
  ESP_LOGV(TAG, "Free heap: %" PRIu32, esp_get_free_heap_size());
  int status_code = esp_http_client_get_status_code(http_client);
  if (status_code != 200 && status_code != 206) {
    return this->upload_end_(false);
  }

  this->tft_size_ = esp_http_client_get_content_length(http_client);

  ESP_LOGD(TAG, "TFT file size: %zu bytes", this->tft_size_);
  if (this->tft_size_ < 4096 || this->tft_size_ > 134217728) {
    ESP_LOGE(TAG, "File size check failed.");
    ESP_LOGD(TAG, "Close HTTP connection");
    esp_http_client_close(http_client);
    esp_http_client_cleanup(http_client);
    ESP_LOGV(TAG, "Connection closed");
    return this->upload_end_(false);
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
    this->parent_->load_settings();
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
    return this->upload_end_(false);
  }

  ESP_LOGV(TAG, "Change the method to GET before starting the download");
  esp_err_t set_method_result = esp_http_client_set_method(http_client, HTTP_METHOD_GET);
  if (set_method_result != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set HTTP method to GET: %s", esp_err_to_name(set_method_result));
    return this->upload_end_(false);
  }

  ESP_LOGD(TAG, "Uploading TFT to Nextion:");
  ESP_LOGD(TAG, "  URL: %s", this->tft_url_.c_str());
  ESP_LOGD(TAG, "  File size: %" PRIu32 " bytes", this->content_length_);
  ESP_LOGD(TAG, "  Free heap: %" PRIu32, esp_get_free_heap_size());

  // Proceed with the content download as before

  ESP_LOGV(TAG, "Starting transfer by chunks loop");

  uint32_t position = 0;
  while (this->content_length_ > 0) {
    int upload_result = upload_by_chunks_(http_client, position);
    if (upload_result < 0) {
      ESP_LOGE(TAG, "Error uploading TFT to Nextion!");
      ESP_LOGD(TAG, "Close HTTP connection");
      esp_http_client_close(http_client);
      esp_http_client_cleanup(http_client);
      ESP_LOGV(TAG, "Connection closed");
      return this->upload_end_(false);
    }
    App.feed_wdt();
    ESP_LOGV(TAG, "Free heap: %" PRIu32 ", Bytes left: %" PRIu32, esp_get_free_heap_size(), this->content_length_);
  }

  ESP_LOGD(TAG, "Successfully uploaded TFT to Nextion!");

  ESP_LOGD(TAG, "Close HTTP connection");
  esp_http_client_close(http_client);
  esp_http_client_cleanup(http_client);
  ESP_LOGV(TAG, "Connection closed");
  return this->upload_end_(true);
}

bool Nextion::upload_end_(bool successful) {
  ESP_LOGD(TAG, "Nextion TFT upload finished: %s", YESNO(successful));
  this->is_updating_ = false;
  this->ignore_is_setup_ = false;

  uint32_t baud_rate = this->parent_->get_baud_rate();
  if (baud_rate != this->original_baud_rate_) {
    ESP_LOGD(TAG, "Changing baud rate back from %" PRIu32 " to %" PRIu32 " bps", baud_rate, this->original_baud_rate_);
    this->parent_->set_baud_rate(this->original_baud_rate_);
    this->parent_->load_settings();
  }

  if (successful) {
    ESP_LOGD(TAG, "Restarting ESPHome");
    delay(1500);  // NOLINT
    arch_restart();
  } else {
    ESP_LOGE(TAG, "Nextion TFT upload failed");
  }
  return successful;
}

}  // namespace nextion
}  // namespace esphome

#endif  // USE_ESP_IDF
#endif  // USE_NEXTION_TFT_UPLOAD
