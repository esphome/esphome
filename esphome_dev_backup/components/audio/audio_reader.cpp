#include "audio_reader.h"

#ifdef USE_ESP_IDF

#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

#if CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
#include "esp_crt_bundle.h"
#endif

namespace esphome {
namespace audio {

static const uint32_t READ_WRITE_TIMEOUT_MS = 20;

static const uint32_t CONNECTION_TIMEOUT_MS = 5000;

// The number of times the http read times out with no data before throwing an error
static const uint32_t ERROR_COUNT_NO_DATA_READ_TIMEOUT = 100;

static const size_t HTTP_STREAM_BUFFER_SIZE = 2048;

static const uint8_t MAX_REDIRECTION = 5;

// Some common HTTP status codes - borrowed from http_request component accessed 20241224
enum HttpStatus {
  HTTP_STATUS_OK = 200,
  HTTP_STATUS_NO_CONTENT = 204,
  HTTP_STATUS_PARTIAL_CONTENT = 206,

  /* 3xx - Redirection */
  HTTP_STATUS_MULTIPLE_CHOICES = 300,
  HTTP_STATUS_MOVED_PERMANENTLY = 301,
  HTTP_STATUS_FOUND = 302,
  HTTP_STATUS_SEE_OTHER = 303,
  HTTP_STATUS_NOT_MODIFIED = 304,
  HTTP_STATUS_TEMPORARY_REDIRECT = 307,
  HTTP_STATUS_PERMANENT_REDIRECT = 308,

  /* 4XX - CLIENT ERROR */
  HTTP_STATUS_BAD_REQUEST = 400,
  HTTP_STATUS_UNAUTHORIZED = 401,
  HTTP_STATUS_FORBIDDEN = 403,
  HTTP_STATUS_NOT_FOUND = 404,
  HTTP_STATUS_METHOD_NOT_ALLOWED = 405,
  HTTP_STATUS_NOT_ACCEPTABLE = 406,
  HTTP_STATUS_LENGTH_REQUIRED = 411,

  /* 5xx - Server Error */
  HTTP_STATUS_INTERNAL_ERROR = 500
};

AudioReader::~AudioReader() { this->cleanup_connection_(); }

esp_err_t AudioReader::add_sink(const std::weak_ptr<RingBuffer> &output_ring_buffer) {
  if (current_audio_file_ != nullptr) {
    // A transfer buffer isn't ncessary for a local file
    this->file_ring_buffer_ = output_ring_buffer.lock();
    return ESP_OK;
  }

  if (this->output_transfer_buffer_ != nullptr) {
    this->output_transfer_buffer_->set_sink(output_ring_buffer);
    return ESP_OK;
  }

  return ESP_ERR_INVALID_STATE;
}

esp_err_t AudioReader::start(AudioFile *audio_file, AudioFileType &file_type) {
  file_type = AudioFileType::NONE;

  this->current_audio_file_ = audio_file;

  this->file_current_ = audio_file->data;
  file_type = audio_file->file_type;

  return ESP_OK;
}

esp_err_t AudioReader::start(const std::string &uri, AudioFileType &file_type) {
  file_type = AudioFileType::NONE;

  this->cleanup_connection_();

  if (uri.empty()) {
    return ESP_ERR_INVALID_ARG;
  }

  esp_http_client_config_t client_config = {};

  client_config.url = uri.c_str();
  client_config.cert_pem = nullptr;
  client_config.disable_auto_redirect = false;
  client_config.max_redirection_count = 10;
  client_config.event_handler = http_event_handler;
  client_config.user_data = this;
  client_config.buffer_size = HTTP_STREAM_BUFFER_SIZE;
  client_config.keep_alive_enable = true;
  client_config.timeout_ms = CONNECTION_TIMEOUT_MS;  // Shouldn't trigger watchdog resets if caller runs in a task

#if CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
  if (uri.find("https:") != std::string::npos) {
    client_config.crt_bundle_attach = esp_crt_bundle_attach;
  }
#endif

  this->client_ = esp_http_client_init(&client_config);

  if (this->client_ == nullptr) {
    return ESP_FAIL;
  }

  esp_err_t err = esp_http_client_open(this->client_, 0);

  if (err != ESP_OK) {
    this->cleanup_connection_();
    return err;
  }

  int64_t header_length = esp_http_client_fetch_headers(this->client_);
  if (header_length < 0) {
    this->cleanup_connection_();
    return ESP_FAIL;
  }

  int status_code = esp_http_client_get_status_code(this->client_);

  if ((status_code < HTTP_STATUS_OK) || (status_code > HTTP_STATUS_PERMANENT_REDIRECT)) {
    this->cleanup_connection_();
    return ESP_FAIL;
  }

  ssize_t redirect_count = 0;

  while ((esp_http_client_set_redirection(this->client_) == ESP_OK) && (redirect_count < MAX_REDIRECTION)) {
    err = esp_http_client_open(this->client_, 0);
    if (err != ESP_OK) {
      this->cleanup_connection_();
      return ESP_FAIL;
    }

    header_length = esp_http_client_fetch_headers(this->client_);
    if (header_length < 0) {
      this->cleanup_connection_();
      return ESP_FAIL;
    }

    status_code = esp_http_client_get_status_code(this->client_);

    if ((status_code < HTTP_STATUS_OK) || (status_code > HTTP_STATUS_PERMANENT_REDIRECT)) {
      this->cleanup_connection_();
      return ESP_FAIL;
    }

    ++redirect_count;
  }

  if (this->audio_file_type_ == AudioFileType::NONE) {
    // Failed to determine the file type from the header, fallback to using the url
    char url[500];
    err = esp_http_client_get_url(this->client_, url, 500);
    if (err != ESP_OK) {
      this->cleanup_connection_();
      return err;
    }

    std::string url_string = str_lower_case(url);

    if (str_endswith(url_string, ".wav")) {
      file_type = AudioFileType::WAV;
    }
#ifdef USE_AUDIO_MP3_SUPPORT
    else if (str_endswith(url_string, ".mp3")) {
      file_type = AudioFileType::MP3;
    }
#endif
#ifdef USE_AUDIO_FLAC_SUPPORT
    else if (str_endswith(url_string, ".flac")) {
      file_type = AudioFileType::FLAC;
    }
#endif
    else {
      file_type = AudioFileType::NONE;
      this->cleanup_connection_();
      return ESP_ERR_NOT_SUPPORTED;
    }
  } else {
    file_type = this->audio_file_type_;
  }

  this->last_data_read_ms_ = millis();

  this->output_transfer_buffer_ = AudioSinkTransferBuffer::create(this->buffer_size_);
  if (this->output_transfer_buffer_ == nullptr) {
    return ESP_ERR_NO_MEM;
  }

  return ESP_OK;
}

AudioReaderState AudioReader::read() {
  if (this->client_ != nullptr) {
    return this->http_read_();
  } else if (this->current_audio_file_ != nullptr) {
    return this->file_read_();
  }

  return AudioReaderState::FAILED;
}

AudioFileType AudioReader::get_audio_type(const char *content_type) {
#ifdef USE_AUDIO_MP3_SUPPORT
  if (strcasecmp(content_type, "mp3") == 0 || strcasecmp(content_type, "audio/mp3") == 0 ||
      strcasecmp(content_type, "audio/mpeg") == 0) {
    return AudioFileType::MP3;
  }
#endif
  if (strcasecmp(content_type, "audio/wav") == 0) {
    return AudioFileType::WAV;
  }
#ifdef USE_AUDIO_FLAC_SUPPORT
  if (strcasecmp(content_type, "audio/flac") == 0 || strcasecmp(content_type, "audio/x-flac") == 0) {
    return AudioFileType::FLAC;
  }
#endif
  return AudioFileType::NONE;
}

esp_err_t AudioReader::http_event_handler(esp_http_client_event_t *evt) {
  // Based on https://github.com/maroc81/WeatherLily/tree/main/main/net accessed 20241224
  AudioReader *this_reader = (AudioReader *) evt->user_data;

  switch (evt->event_id) {
    case HTTP_EVENT_ON_HEADER:
      if (strcasecmp(evt->header_key, "Content-Type") == 0) {
        this_reader->audio_file_type_ = get_audio_type(evt->header_value);
      }
      break;
    default:
      break;
  }
  return ESP_OK;
}

AudioReaderState AudioReader::file_read_() {
  size_t remaining_bytes = this->current_audio_file_->length - (this->file_current_ - this->current_audio_file_->data);
  if (remaining_bytes > 0) {
    size_t bytes_written = this->file_ring_buffer_->write_without_replacement(this->file_current_, remaining_bytes,
                                                                              pdMS_TO_TICKS(READ_WRITE_TIMEOUT_MS));
    this->file_current_ += bytes_written;

    return AudioReaderState::READING;
  }

  return AudioReaderState::FINISHED;
}

AudioReaderState AudioReader::http_read_() {
  this->output_transfer_buffer_->transfer_data_to_sink(pdMS_TO_TICKS(READ_WRITE_TIMEOUT_MS), false);

  if (esp_http_client_is_complete_data_received(this->client_)) {
    if (this->output_transfer_buffer_->available() == 0) {
      this->cleanup_connection_();
      return AudioReaderState::FINISHED;
    }
  } else if (this->output_transfer_buffer_->free() > 0) {
    size_t bytes_to_read = this->output_transfer_buffer_->free();
    int received_len =
        esp_http_client_read(this->client_, (char *) this->output_transfer_buffer_->get_buffer_end(), bytes_to_read);

    if (received_len > 0) {
      this->output_transfer_buffer_->increase_buffer_length(received_len);
      this->last_data_read_ms_ = millis();
    } else if (received_len < 0) {
      // HTTP read error
      this->cleanup_connection_();
      return AudioReaderState::FAILED;
    } else {
      if (bytes_to_read > 0) {
        // Read timed out
        if ((millis() - this->last_data_read_ms_) > CONNECTION_TIMEOUT_MS) {
          this->cleanup_connection_();
          return AudioReaderState::FAILED;
        }

        delay(READ_WRITE_TIMEOUT_MS);
      }
    }
  }

  return AudioReaderState::READING;
}

void AudioReader::cleanup_connection_() {
  if (this->client_ != nullptr) {
    esp_http_client_close(this->client_);
    esp_http_client_cleanup(this->client_);
    this->client_ = nullptr;
  }
}

}  // namespace audio
}  // namespace esphome

#endif
