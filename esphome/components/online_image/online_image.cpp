#include "online_image.h"
#include "esphome/components/runtime_image/image_decoder.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <algorithm>
#include <cstdio>

ESPHOME_LOG_TAG(TAG, "online_image");
static const char *const CONTENT_TYPE_HEADER_NAME = "content-type";
static const char *const ETAG_HEADER_NAME = "etag";
static const char *const IF_NONE_MATCH_HEADER_NAME = "if-none-match";
static const char *const LAST_MODIFIED_HEADER_NAME = "last-modified";
static const char *const IF_MODIFIED_SINCE_HEADER_NAME = "if-modified-since";

namespace esphome::online_image {

OnlineImage::OnlineImage(const std::string &url, int width, int height, runtime_image::ImageFormat format,
                         image::ImageType type, image::Transparency transparency, image::Image *placeholder,
                         uint32_t buffer_size, bool is_big_endian)
    : RuntimeImage(format, type, transparency, placeholder, is_big_endian, width, height),
      download_buffer_(buffer_size),
      download_buffer_initial_size_(buffer_size) {
  this->set_url(url);
}

bool OnlineImage::validate_url_(const std::string &url) {
  if (url.empty()) {
    ESP_LOGE(TAG, "URL is empty");
    return false;
  }
  if (url.length() > 2048) {
    ESP_LOGE(TAG, "URL is too long");
    return false;
  }
  if (!url.starts_with("http://") && !url.starts_with("https://")) {
    ESP_LOGE(TAG, "URL must start with http:// or https://");
    return false;
  }
  return true;
}

void OnlineImage::update() {
  if (this->is_decoding()) {
    ESP_LOGW(TAG, "Image already being updated.");
    return;
  }

  if (!this->validate_url_(this->url_)) {
    ESP_LOGE(TAG, "Invalid URL: %s", this->url_.c_str());
    this->download_error_callback_.call();
    return;
  }

  // A failed allocation at boot, or while growing the buffer for a JPEG, leaves no download buffer
  if (this->download_buffer_.size() == 0 && this->download_buffer_.resize(this->download_buffer_initial_size_) == 0) {
    // resize() logged the failed allocation
    this->download_error_callback_.call();
    return;
  }

  ESP_LOGD(TAG, "Updating image from %s", this->url_.c_str());

  std::vector<http_request::Header> headers;

  // Add caching headers if we have them
  if (!this->etag_.empty()) {
    headers.push_back({IF_NONE_MATCH_HEADER_NAME, this->etag_});
  }
  if (!this->last_modified_.empty()) {
    headers.push_back({IF_MODIFIED_SINCE_HEADER_NAME, this->last_modified_});
  }

  runtime_image::ImageFormat format = this->get_format();
  // Accept: "<mime>,*/*;q=0.8"; 32 covers the longest MIME type plus the suffix
  char accept_header[32];
  snprintf(accept_header, sizeof(accept_header), "%s,*/*;q=0.8", runtime_image::get_mime_type_for_format(format));
  headers.push_back({"Accept", accept_header});

  // User headers last so they can override any of the above
  for (auto &header : this->request_headers_) {
    headers.push_back(http_request::Header{header.first, header.second.value()});
  }

  this->downloader_ =
      this->parent_->get(this->url_, headers, {ETAG_HEADER_NAME, LAST_MODIFIED_HEADER_NAME, CONTENT_TYPE_HEADER_NAME});
  if (this->downloader_ == nullptr) {
    ESP_LOGE(TAG, "Download failed.");
    this->end_connection_();
    this->download_error_callback_.call();
    return;
  }

  int http_code = this->downloader_->status_code;
  if (http_code == HTTP_CODE_NOT_MODIFIED) {
    // Image hasn't changed on server. Skip download.
    ESP_LOGI(TAG, "Server returned HTTP 304 (Not Modified). Download skipped.");
    this->end_connection_();
    this->download_finished_callback_.call(true);
    return;
  }
  if (http_code != HTTP_CODE_OK) {
    ESP_LOGE(TAG, "HTTP result: %d", http_code);
    this->end_connection_();
    this->download_error_callback_.call();
    return;
  }

  ESP_LOGD(TAG, "Starting download");
  size_t total_size = this->downloader_->content_length;
  ESP_LOGV(TAG, "Content-Length: %zu", total_size);

  if (format == runtime_image::AUTO) {
    // Try to auto-detect format from Content-Type header
    auto content_type = this->downloader_->get_response_header(CONTENT_TYPE_HEADER_NAME);
    ESP_LOGV(TAG, "Content-Type: %s", content_type.c_str());
    auto mime_format = esphome::runtime_image::get_format_for_mime_type(content_type.c_str());
    if (mime_format.has_value()) {
      format = *mime_format;
    } else {
      if (content_type.empty()) {
        ESP_LOGE(TAG, "Server sent no Content-Type header; cannot determine image format. Set `format:` explicitly");
      } else if (str_contains_ignore_case(content_type.c_str(), "image/")) {
        ESP_LOGE(TAG, "Image format '%s' not supported.", content_type.c_str());
      } else {
        ESP_LOGE(TAG, "Server did not return an image (Content-Type: '%s')", content_type.c_str());
      }
      this->end_connection_();
      this->download_error_callback_.call();
      return;
    }
  }
  ESP_LOGD(TAG, "Using image format: %s", LOG_STR_ARG(runtime_image::get_format_name(format)));

  // Initialize decoder with the known format
  if (!this->begin_decode(total_size, format)) {
    ESP_LOGE(TAG, "Failed to initialize decoder for format %s", LOG_STR_ARG(runtime_image::get_format_name(format)));
    this->end_connection_();
    this->download_error_callback_.call();
    return;
  }

  // JPEG requires the complete image in the download buffer before decoding
  if (format == runtime_image::JPEG && total_size > this->download_buffer_.size() &&
      this->download_buffer_.resize(total_size) == 0) {
    // resize() logged the failed allocation; the next update() allocates the configured buffer again
    this->end_connection_();
    this->download_error_callback_.call();
    return;
  }

  ESP_LOGI(TAG, "Downloading image (Size: %zu)", total_size);
  this->start_time_ = millis();
  this->last_data_time_ = this->start_time_;
  this->enable_loop();
}

void OnlineImage::loop() {
  if (!this->is_decoding()) {
    // Not decoding at the moment => nothing to do.
    this->disable_loop();
    return;
  }

  if (!this->downloader_) {
    ESP_LOGE(TAG, "Downloader not instantiated; cannot download");
    this->end_connection_();
    this->download_error_callback_.call();
    return;
  }

  // Check if download is complete — use decoder's format-specific completion check
  // to handle both known content-length and chunked transfer encoding
  if (this->is_decode_finished() || (this->downloader_->content_length > 0 &&
                                     this->downloader_->get_bytes_read() >= this->downloader_->content_length &&
                                     this->download_buffer_.unread() == 0)) {
    this->finish_download_();
    return;
  }

  // Download and decode more data
  size_t available = this->download_buffer_.free_capacity();
  if (available > 0) {
    // Download in chunks to avoid blocking
    available = std::min(available, this->download_buffer_initial_size_);
    auto len = this->downloader_->read(this->download_buffer_.append(), available);

    if (len > 0) {
      this->last_data_time_ = App.get_loop_component_start_time();
      this->download_buffer_.write(len);

      // Feed data to decoder
      auto consumed = this->feed_data(this->download_buffer_.data(), this->download_buffer_.unread());

      if (consumed < 0) {
        ESP_LOGE(TAG, "Error decoding image: %s", esphome::runtime_image::decode_error_to_string(consumed));
        this->end_connection_();
        this->download_error_callback_.call();
        return;
      }

      if (consumed > 0) {
        this->download_buffer_.read(consumed);
      }
    } else if (len < 0) {
      ESP_LOGE(TAG, "Error downloading image: %d", len);
      this->end_connection_();
      this->download_error_callback_.call();
      return;
    } else if (this->downloader_->is_read_complete()) {
      // The response has ended, so data left in the buffer can never be completed
      size_t unread = this->download_buffer_.unread();
      if (unread == 0 && this->downloader_->get_bytes_read() > 0) {
        this->finish_download_();
        return;
      }
      if (unread == 0) {
        ESP_LOGE(TAG, "Server sent an empty response");
      } else {
        ESP_LOGE(TAG, "Download ended with %zu bytes the decoder could not use", unread);
      }
      this->end_connection_();
      this->download_error_callback_.call();
      return;
    } else if (App.get_loop_component_start_time() - this->last_data_time_ >= this->parent_->get_timeout()) {
      // ESP-IDF keeps returning 0 after the server closes the connection early
      ESP_LOGE(TAG, "No data received for %" PRIu32 " ms", this->parent_->get_timeout());
      this->end_connection_();
      this->download_error_callback_.call();
      return;
    }
  } else {
    // Buffer is full, need to decode some data first
    auto consumed = this->feed_data(this->download_buffer_.data(), this->download_buffer_.unread());
    if (consumed > 0) {
      this->download_buffer_.read(consumed);
    } else if (consumed < 0) {
      ESP_LOGE(TAG, "Decode error with full buffer: %d", consumed);
      this->end_connection_();
      this->download_error_callback_.call();
      return;
    } else if (App.get_loop_component_start_time() - this->last_data_time_ >= this->parent_->get_timeout()) {
      // The decoder needs more data than the buffer can hold, so it can never make progress. The
      // no-data timeout gives a decoder that only had to wait the chance to show otherwise.
      ESP_LOGE(TAG, "Image does not fit in the %zu byte download buffer; increase buffer_size",
               this->download_buffer_.size());
      this->end_connection_();
      this->download_error_callback_.call();
      return;
    }
  }
}

void OnlineImage::finish_download_() {
  // Must be read before end_decode() ends the decoding session
  bool decoder_finished = this->is_decode_finished();
  this->end_decode();

  ESP_LOGD(TAG, "Image fully downloaded, %zu bytes in %" PRIu32 " ms", this->downloader_->get_bytes_read(),
           millis() - this->start_time_);

  // Only cache an image the decoder confirmed as complete; otherwise a cut-off image would be kept by 304 responses
  if (decoder_finished) {
    this->etag_ = this->downloader_->get_response_header(ETAG_HEADER_NAME);
    this->last_modified_ = this->downloader_->get_response_header(LAST_MODIFIED_HEADER_NAME);
  } else {
    ESP_LOGD(TAG, "Decoder could not confirm the image is complete; not caching it");
    this->etag_.clear();
    this->last_modified_.clear();
  }

  this->download_finished_callback_.call(false);
  this->end_connection_();
}

void OnlineImage::end_connection_() {
  // Abort any in-progress decode; the decoder object is kept warm for the next decode.
  // Use RuntimeImage::release() directly to avoid recursion with OnlineImage::release().
  if (this->is_decoding()) {
    RuntimeImage::release();
  }
  if (this->downloader_) {
    this->downloader_->end();
    this->downloader_ = nullptr;
  }
  this->download_buffer_.reset();
  this->disable_loop();
}

void OnlineImage::release() {
  // Clear cache headers
  this->etag_ = "";
  this->last_modified_ = "";

  // End any active connection
  this->end_connection_();

  // Call parent's release to free the image buffer
  RuntimeImage::release();
}

}  // namespace esphome::online_image
