#include "nextion.h"

#ifdef USE_ESP_IDF
#ifdef USE_NEXTION_TFT_UPLOAD

#include "esphome/core/application.h"
#include "esphome/core/defines.h"
#include "esphome/core/util.h"
#include "esphome/core/log.h"
#include "esphome/components/network/util.h"

#include <esp_heap_caps.h>
#include <esp_http_client.h>
#include <cinttypes>

namespace esphome {
namespace nextion {
static const char *const TAG = "nextion.upload.idf";

// Followed guide
// https://unofficialnextion.com/t/nextion-upload-protocol-v1-2-the-fast-one/1044/2

int Nextion::upload_range(const std::string &url, int range_start) {
  ESP_LOGVV(TAG, "url: %s", url.c_str());
  uint range_size = this->tft_size_ - range_start;
  ESP_LOGVV(TAG, "tft_size_: %i", this->tft_size_);
  ESP_LOGV(TAG, "Available heap: %u", esp_get_free_heap_size());
  int range_end = (range_start == 0) ? std::min(this->tft_size_, 16383) : this->tft_size_;
  if (range_size <= 0 or range_end <= range_start) {
    ESP_LOGE(TAG, "Invalid range");
    ESP_LOGD(TAG, "Range start: %i", range_start);
    ESP_LOGD(TAG, "Range end: %i", range_end);
    ESP_LOGD(TAG, "Range size: %i", range_size);
    return -1;
  }

  esp_http_client_config_t config = {
      .url = url.c_str(),
      .cert_pem = nullptr,
  };
  esp_http_client_handle_t client = esp_http_client_init(&config);

  char range_header[64];
  sprintf(range_header, "bytes=%d-%d", range_start, range_end);
  ESP_LOGV(TAG, "Requesting range: %s", range_header);
  esp_http_client_set_header(client, "Range", range_header);
  ESP_LOGVV(TAG, "Available heap: %u", esp_get_free_heap_size());

  ESP_LOGV(TAG, "Opening http connetion");
  esp_err_t err;
  if ((err = esp_http_client_open(client, 0)) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
    esp_http_client_cleanup(client);
    return -1;
  }

  ESP_LOGV(TAG, "Fetch content length");
  int content_length = esp_http_client_fetch_headers(client);
  ESP_LOGV(TAG, "content_length = %d", content_length);
  if (content_length <= 0) {
    ESP_LOGE(TAG, "Failed to get content length: %d", content_length);
    esp_http_client_cleanup(client);
    return -1;
  }

  int total_read_len = 0, read_len;

  ESP_LOGV(TAG, "Allocate buffer");
  uint8_t *buffer = new uint8_t[4096];
  std::string recv_string;
  if (buffer == nullptr) {
    ESP_LOGE(TAG, "Failed to allocate memory for buffer");
    ESP_LOGV(TAG, "Available heap: %u", esp_get_free_heap_size());
  } else {
    ESP_LOGV(TAG, "Memory for buffer allocated successfully");

    while (true) {
      App.feed_wdt();
      ESP_LOGVV(TAG, "Available heap: %u", esp_get_free_heap_size());
      int read_len = esp_http_client_read(client, reinterpret_cast<char *>(buffer), 4096);
      ESP_LOGVV(TAG, "Read %d bytes from HTTP client, writing to UART", read_len);
      if (read_len > 0) {
        this->write_array(buffer, read_len);
        ESP_LOGVV(TAG, "Write to UART successful");
        this->recv_ret_string_(recv_string, 5000, true);
        this->content_length_ -= read_len;
        ESP_LOGD(TAG, "Uploaded %0.2f %%, remaining %d bytes",
                 100.0 * (this->tft_size_ - this->content_length_) / this->tft_size_, this->content_length_);
        if (recv_string[0] != 0x05) {  // 0x05 == "ok"
          ESP_LOGD(
              TAG, "recv_string [%s]",
              format_hex_pretty(reinterpret_cast<const uint8_t *>(recv_string.data()), recv_string.size()).c_str());
        }
        // handle partial upload request
        if (recv_string[0] == 0x08 && recv_string.size() == 5) {
          uint32_t result = 0;
          for (int j = 0; j < 4; ++j) {
            result += static_cast<uint8_t>(recv_string[j + 1]) << (8 * j);
          }
          if (result > 0) {
            ESP_LOGI(TAG, "Nextion reported new range %" PRIu32, result);
            this->content_length_ = this->tft_size_ - result;
            // Deallocate the buffer when done
            delete[] buffer;
            ESP_LOGVV(TAG, "Memory for buffer deallocated");
            esp_http_client_cleanup(client);
            esp_http_client_close(client);
            return result;
          }
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

    // Deallocate the buffer when done
    delete[] buffer;
    ESP_LOGVV(TAG, "Memory for buffer deallocated");
  }
  esp_http_client_cleanup(client);
  esp_http_client_close(client);
  return range_end + 1;
}

bool Nextion::upload_tft() {
  ESP_LOGD(TAG, "Nextion TFT upload requested");
  ESP_LOGD(TAG, "url: %s", this->tft_url_.c_str());

  if (this->is_updating_) {
    ESP_LOGW(TAG, "Currently updating");
    return false;
  }

  if (!network::is_connected()) {
    ESP_LOGE(TAG, "Network is not connected");
    return false;
  }

  this->is_updating_ = true;

  // Define the configuration for the HTTP client
  ESP_LOGV(TAG, "Establishing connection to HTTP server");
  ESP_LOGVV(TAG, "Available heap: %u", esp_get_free_heap_size());
  esp_http_client_config_t config = {
      .url = this->tft_url_.c_str(),
      .cert_pem = nullptr,
      .method = HTTP_METHOD_HEAD,
      .timeout_ms = 15000,
  };

  // Initialize the HTTP client with the configuration
  ESP_LOGV(TAG, "Initializing HTTP client");
  ESP_LOGV(TAG, "Available heap: %u", esp_get_free_heap_size());
  esp_http_client_handle_t http = esp_http_client_init(&config);
  if (!http) {
    ESP_LOGE(TAG, "Failed to initialize HTTP client.");
    return this->upload_end(false);
  }

  // Perform the HTTP request
  ESP_LOGV(TAG, "Check if the client could connect");
  ESP_LOGV(TAG, "Available heap: %u", esp_get_free_heap_size());
  esp_err_t err = esp_http_client_perform(http);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(err));
    esp_http_client_cleanup(http);
    return this->upload_end(false);
  }

  // Check the HTTP Status Code
  int status_code = esp_http_client_get_status_code(http);
  ESP_LOGV(TAG, "HTTP Status Code: %d", status_code);
  size_t tft_file_size = esp_http_client_get_content_length(http);
  ESP_LOGD(TAG, "TFT file size: %zu", tft_file_size);

  if (tft_file_size < 4096) {
    ESP_LOGE(TAG, "File size check failed. Size: %zu", tft_file_size);
    esp_http_client_cleanup(http);
    return this->upload_end(false);
  } else {
    ESP_LOGV(TAG, "File size check passed. Proceeding...");
  }
  this->content_length_ = tft_file_size;
  this->tft_size_ = tft_file_size;

  ESP_LOGD(TAG, "Updating Nextion");
  // The Nextion will ignore the update command if it is sleeping

  this->send_command_("sleep=0");
  this->set_backlight_brightness(1.0);
  vTaskDelay(pdMS_TO_TICKS(250));  // NOLINT

  App.feed_wdt();
  char command[128];
  // Tells the Nextion the content length of the tft file and baud rate it will be sent at
  // Once the Nextion accepts the command it will wait until the file is successfully uploaded
  // If it fails for any reason a power cycle of the display will be needed
  sprintf(command, "whmi-wris %d,%" PRIu32 ",1", this->content_length_, this->parent_->get_baud_rate());

  // Clear serial receive buffer
  uint8_t d;
  while (this->available()) {
    this->read_byte(&d);
  };

  this->send_command_(command);

  std::string response;
  ESP_LOGV(TAG, "Waiting for upgrade response");
  this->recv_ret_string_(response, 2048, true);  // This can take some time to return

  // The Nextion display will, if it's ready to accept data, send a 0x05 byte.
  ESP_LOGD(TAG, "Upgrade response is [%s]",
           format_hex_pretty(reinterpret_cast<const uint8_t *>(response.data()), response.size()).c_str());

  if (response.find(0x05) != std::string::npos) {
    ESP_LOGV(TAG, "Preparation for tft update done");
  } else {
    ESP_LOGE(TAG, "Preparation for tft update failed %d \"%s\"", response[0], response.c_str());
    esp_http_client_cleanup(http);
    return this->upload_end(false);
  }

  ESP_LOGD(TAG, "Updating tft from \"%s\" with a file size of %d, Heap Size %" PRIu32, this->tft_url_.c_str(),
           content_length_, esp_get_free_heap_size());

  ESP_LOGV(TAG, "Starting transfer by chunks loop");
  int result = 0;
  while (content_length_ > 0) {
    result = upload_range(this->tft_url_.c_str(), result);
    if (result < 0) {
      ESP_LOGE(TAG, "Error updating Nextion!");
      esp_http_client_cleanup(http);
      return this->upload_end(false);
    }
    App.feed_wdt();
    ESP_LOGV(TAG, "Heap Size %" PRIu32 ", Bytes left %d", esp_get_free_heap_size(), content_length_);
  }

  ESP_LOGD(TAG, "Successfully updated Nextion!");

  ESP_LOGD(TAG, "Close HTTP connection");
  esp_http_client_close(http);
  esp_http_client_cleanup(http);
  return upload_end(true);
}

bool Nextion::upload_end(bool successful) {
  this->is_updating_ = false;
  ESP_LOGD(TAG, "Restarting Nextion");
  this->soft_reset();
  vTaskDelay(pdMS_TO_TICKS(1500));  // NOLINT
  if (successful) {
    ESP_LOGD(TAG, "Restarting esphome");
    esp_restart();  // NOLINT(readability-static-accessed-through-instance)
  }
  return successful;
}

}  // namespace nextion
}  // namespace esphome

#endif  // USE_NEXTION_TFT_UPLOAD
#endif  // USE_ESP_IDF
