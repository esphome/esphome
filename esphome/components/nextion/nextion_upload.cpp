#include "nextion.h"

#ifdef USE_NEXTION_TFT_UPLOAD

namespace esphome {
namespace nextion {

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

    case Nextion::TFTUploadResult::HTTP_ERROR_FAILED_TO_GET_CONTENT_LENGTH:
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
