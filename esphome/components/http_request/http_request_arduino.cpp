#include "http_request_arduino.h"

#if defined(USE_ARDUINO) && !defined(USE_ESP32)

#include "esphome/components/network/util.h"
#include "esphome/components/watchdog/watchdog.h"

#include "esphome/core/application.h"
#include "esphome/core/defines.h"
#include "esphome/core/log.h"

namespace esphome::http_request {

static const char *const TAG = "http_request.arduino";

std::shared_ptr<HttpContainer> HttpRequestArduino::perform(const std::string &url, const std::string &method,
                                                           const std::string &body,
                                                           const std::list<Header> &request_headers,
                                                           const std::set<std::string> &collect_headers) {
  if (!network::is_connected()) {
    this->status_momentary_error("failed", 1000);
    ESP_LOGW(TAG, "HTTP Request failed; Not connected to network");
    return nullptr;
  }

  std::shared_ptr<HttpContainerArduino> container = std::make_shared<HttpContainerArduino>();
  container->set_parent(this);

  const uint32_t start = millis();

  bool secure = url.find("https:") != std::string::npos;
  container->set_secure(secure);

  watchdog::WatchdogManager wdm(this->get_watchdog_timeout());

  if (this->follow_redirects_) {
    container->client_.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
    container->client_.setRedirectLimit(this->redirect_limit_);
  } else {
    container->client_.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
  }

#if defined(USE_ESP8266)
  std::unique_ptr<WiFiClient> stream_ptr;
#ifdef USE_HTTP_REQUEST_ESP8266_HTTPS
  if (secure) {
    ESP_LOGV(TAG, "ESP8266 HTTPS connection with WiFiClientSecure");
    stream_ptr = std::make_unique<WiFiClientSecure>();
    WiFiClientSecure *secure_client = static_cast<WiFiClientSecure *>(stream_ptr.get());
    secure_client->setBufferSizes(512, 512);
    secure_client->setInsecure();
  } else {
    stream_ptr = std::make_unique<WiFiClient>();
  }
#else
  ESP_LOGV(TAG, "ESP8266 HTTP connection with WiFiClient");
  if (secure) {
    ESP_LOGE(TAG, "Can't use HTTPS connection with esp8266_disable_ssl_support");
    return nullptr;
  }
  stream_ptr = std::make_unique<WiFiClient>();
#endif  // USE_HTTP_REQUEST_ESP8266_HTTPS

#if USE_ARDUINO_VERSION_CODE >= VERSION_CODE(3, 1, 0)  // && USE_ARDUINO_VERSION_CODE < VERSION_CODE(?, ?, ?)
  if (!secure) {
    ESP_LOGW(TAG, "Using HTTP on Arduino version >= 3.1 is **very** slow. Consider setting framework version to 3.0.2 "
                  "in your YAML, or use HTTPS");
  }
#endif  // USE_ARDUINO_VERSION_CODE
  bool status = container->client_.begin(*stream_ptr, url.c_str());

#elif defined(USE_RP2040)
  if (secure) {
    container->client_.setInsecure();
  }
  bool status = container->client_.begin(url.c_str());
#endif

  App.feed_wdt();

  if (!status) {
    ESP_LOGW(TAG, "HTTP Request failed; URL: %s", url.c_str());
    container->end();
    this->status_momentary_error("failed", 1000);
    return nullptr;
  }

  container->client_.setReuse(true);
  container->client_.setTimeout(this->timeout_);

  if (this->useragent_ != nullptr) {
    container->client_.setUserAgent(this->useragent_);
  }
  for (const auto &header : request_headers) {
    container->client_.addHeader(header.name.c_str(), header.value.c_str(), false, true);
  }

  // returned needed headers must be collected before the requests
  const char *header_keys[collect_headers.size()];
  int index = 0;
  for (auto const &header_name : collect_headers) {
    header_keys[index++] = header_name.c_str();
  }
  container->client_.collectHeaders(header_keys, index);

  App.feed_wdt();
  container->status_code = container->client_.sendRequest(method.c_str(), body.c_str());
  App.feed_wdt();
  if (container->status_code < 0) {
    ESP_LOGW(TAG, "HTTP Request failed; URL: %s; Error: %s", url.c_str(),
             HTTPClient::errorToString(container->status_code).c_str());
    this->status_momentary_error("failed", 1000);
    container->end();
    return nullptr;
  }

  if (!is_success(container->status_code)) {
    ESP_LOGE(TAG, "HTTP Request failed; URL: %s; Code: %d", url.c_str(), container->status_code);
    this->status_momentary_error("failed", 1000);
    // Still return the container, so it can be used to get the status code and error message
  }

  container->response_headers_ = {};
  auto header_count = container->client_.headers();
  for (int i = 0; i < header_count; i++) {
    const std::string header_name = str_lower_case(container->client_.headerName(i).c_str());
    if (collect_headers.count(header_name) > 0) {
      std::string header_value = container->client_.header(i).c_str();
      ESP_LOGD(TAG, "Received response header, name: %s, value: %s", header_name.c_str(), header_value.c_str());
      container->response_headers_[header_name].push_back(header_value);
    }
  }

  // HTTPClient::getSize() returns -1 for chunked transfer encoding (no Content-Length).
  // When cast to size_t, -1 becomes SIZE_MAX (4294967295 on 32-bit).
  // The read() method uses a chunked transfer encoding decoder (read_chunked_) to strip
  // chunk framing and deliver only decoded content. When the final 0-size chunk is received,
  // is_chunked_ is cleared and content_length is set to the actual decoded size, so
  // is_read_complete() returns true and callers exit their read loops correctly.
  int content_length = container->client_.getSize();
  ESP_LOGD(TAG, "Content-Length: %d", content_length);
  container->content_length = (size_t) content_length;
  // -1 (SIZE_MAX when cast to size_t) means chunked transfer encoding
  container->set_chunked(content_length == -1);
  container->duration_ms = millis() - start;

  return container;
}

// Arduino HTTP read implementation
//
// WARNING: Return values differ from BSD sockets! See http_request.h for full documentation.
//
// Arduino's WiFiClient is inherently non-blocking - available() returns 0 when
// no data is ready. We use connected() to distinguish "no data yet" from
// "connection closed".
//
// WiFiClient behavior:
//   available() > 0: data ready to read
//   available() == 0 && connected(): no data yet, still connected
//   available() == 0 && !connected(): connection closed
//
// We normalize to HttpContainer::read() contract (NOT BSD socket semantics!):
//   > 0: bytes read
//   0: no data yet, retry            <-- NOTE: 0 means retry, NOT EOF!
//   < 0: error/connection closed     <-- connection closed returns -1, not 0
//
// For chunked transfer encoding, read_chunked_() decodes chunk framing and delivers
// only the payload data. When the final 0-size chunk is received, it clears is_chunked_
// and sets content_length = bytes_read_ so is_read_complete() returns true.
int HttpContainerArduino::read(uint8_t *buf, size_t max_len) {
  const uint32_t start = millis();
  watchdog::WatchdogManager wdm(this->parent_->get_watchdog_timeout());

  WiFiClient *stream_ptr = this->client_.getStreamPtr();
  if (stream_ptr == nullptr) {
    ESP_LOGE(TAG, "Stream pointer vanished!");
    return HTTP_ERROR_CONNECTION_CLOSED;
  }

  if (this->is_chunked_) {
    int result = this->read_chunked_(buf, max_len, stream_ptr);
    this->duration_ms += (millis() - start);
    if (result > 0) {
      return result;
    }
    // result <= 0: check for completion or errors
    if (this->is_read_complete()) {
      return 0;  // Chunked transfer complete (final 0-size chunk received)
    }
    if (result < 0) {
      return result;  // Stream error during chunk decoding
    }
    // read_chunked_ returned 0: no data was available (available() was 0).
    // This happens when the TCP buffer is empty - either more data is in flight,
    // or the connection dropped. Arduino's connected() returns false only when
    // both the remote has closed AND the receive buffer is empty, so any buffered
    // data is fully drained before we report the drop.
    if (!stream_ptr->connected()) {
      return HTTP_ERROR_CONNECTION_CLOSED;
    }
    return 0;  // No data yet, caller should retry
  }

  // Non-chunked path
  int available_data = stream_ptr->available();
  size_t remaining = (this->content_length > 0) ? (this->content_length - this->bytes_read_) : max_len;
  int bufsize = std::min(max_len, std::min(remaining, (size_t) available_data));

  if (bufsize == 0) {
    this->duration_ms += (millis() - start);
    if (this->is_read_complete()) {
      return 0;  // All content read successfully
    }
    if (!stream_ptr->connected()) {
      return HTTP_ERROR_CONNECTION_CLOSED;
    }
    return 0;  // No data yet, caller should retry
  }

  App.feed_wdt();
  int read_len = stream_ptr->readBytes(buf, bufsize);
  this->bytes_read_ += read_len;

  this->duration_ms += (millis() - start);

  return read_len;
}

void HttpContainerArduino::chunk_header_complete_() {
  if (this->chunk_remaining_ == 0) {
    this->chunk_state_ = ChunkedState::CHUNK_TRAILER;
    this->chunk_remaining_ = 1;  // repurpose as at-start-of-line flag
  } else {
    this->chunk_state_ = ChunkedState::CHUNK_DATA;
  }
}

// Chunked transfer encoding decoder
//
// On Arduino, getStreamPtr() returns raw TCP data. For chunked responses, this includes
// chunk framing (size headers, CRLF delimiters) mixed with payload data. This decoder
// strips the framing and delivers only decoded content to the caller.
//
// Chunk format (RFC 9112 Section 7.1):
//   <hex-size>[;extension]\r\n
//   <data bytes>\r\n
//   ...
//   0\r\n
//   [trailer-field\r\n]*
//   \r\n
//
// Non-blocking: only processes bytes already in the TCP receive buffer.
// State (chunk_state_, chunk_remaining_) is preserved between calls, so partial
// chunk headers or split \r\n sequences resume correctly on the next call.
// Framing bytes (hex sizes, \r\n) may be consumed without producing output;
// the caller sees 0 and retries via the normal read timeout logic.
//
// WiFiClient::read() returns -1 on error despite available() > 0 (connection reset
// between check and read). On any stream error (c < 0 or readBytes <= 0), we return
// already-decoded data if any; otherwise HTTP_ERROR_CONNECTION_CLOSED. The error
// will surface again on the next call since the stream stays broken.
//
// Returns: > 0 decoded bytes, 0 no data available, < 0 error
int HttpContainerArduino::read_chunked_(uint8_t *buf, size_t max_len, WiFiClient *stream) {
  int total_decoded = 0;

  while (total_decoded < (int) max_len && this->chunk_state_ != ChunkedState::COMPLETE) {
    // Non-blocking: only process what's already buffered
    if (stream->available() == 0)
      break;

    // CHUNK_DATA reads multiple bytes; handle before the single-byte switch
    if (this->chunk_state_ == ChunkedState::CHUNK_DATA) {
      // Only read what's available, what fits in buf, and what remains in this chunk
      size_t to_read =
          std::min({max_len - (size_t) total_decoded, this->chunk_remaining_, (size_t) stream->available()});
      if (to_read == 0)
        break;
      App.feed_wdt();
      int read_len = stream->readBytes(buf + total_decoded, to_read);
      if (read_len <= 0)
        return total_decoded > 0 ? total_decoded : HTTP_ERROR_CONNECTION_CLOSED;
      total_decoded += read_len;
      this->chunk_remaining_ -= read_len;
      this->bytes_read_ += read_len;
      if (this->chunk_remaining_ == 0)
        this->chunk_state_ = ChunkedState::CHUNK_DATA_TRAIL;
      continue;
    }

    // All other states consume a single byte
    int c = stream->read();
    if (c < 0)
      return total_decoded > 0 ? total_decoded : HTTP_ERROR_CONNECTION_CLOSED;

    switch (this->chunk_state_) {
      // Parse hex chunk size, one byte at a time: "<hex>[;ext]\r\n"
      // Note: if no hex digits are parsed (e.g., bare \r\n), chunk_remaining_ stays 0
      // and is treated as the final chunk. This is intentionally lenient — on embedded
      // devices, rejecting malformed framing is less useful than terminating cleanly.
      // Overflow of chunk_remaining_ from extremely long hex strings (>8 digits on
      // 32-bit) is not checked; >4GB chunks are unrealistic on embedded targets and
      // would simply cause fewer bytes to be read from that chunk.
      case ChunkedState::CHUNK_HEADER:
        if (c == '\n') {
          // \n terminates the size line; chunk_remaining_ == 0 means last chunk
          this->chunk_header_complete_();
        } else {
          uint8_t hex = parse_hex_char(c);
          if (hex != INVALID_HEX_CHAR) {
            this->chunk_remaining_ = (this->chunk_remaining_ << 4) | hex;
          } else if (c != '\r') {
            this->chunk_state_ = ChunkedState::CHUNK_HEADER_EXT;  // ';' starts extension, skip to \n
          }
        }
        break;

      // Skip chunk extension bytes until \n (e.g., ";name=value\r\n")
      case ChunkedState::CHUNK_HEADER_EXT:
        if (c == '\n') {
          this->chunk_header_complete_();
        }
        break;

      // Consume \r\n trailing each chunk's data
      case ChunkedState::CHUNK_DATA_TRAIL:
        if (c == '\n') {
          this->chunk_state_ = ChunkedState::CHUNK_HEADER;
          this->chunk_remaining_ = 0;  // reset for next chunk's hex accumulation
        }
        // else: \r is consumed silently, next iteration gets \n
        break;

      // Consume optional trailer headers and terminating empty line after final chunk.
      // Per RFC 9112 Section 7.1: "0\r\n" is followed by optional "field\r\n" lines
      // and a final "\r\n". chunk_remaining_ is repurposed as a flag: 1 = at start
      // of line (may be the empty terminator), 0 = mid-line (reading a trailer field).
      case ChunkedState::CHUNK_TRAILER:
        if (c == '\n') {
          if (this->chunk_remaining_ != 0) {
            this->chunk_state_ = ChunkedState::COMPLETE;  // Empty line terminates trailers
          } else {
            this->chunk_remaining_ = 1;  // End of trailer field, at start of next line
          }
        } else if (c != '\r') {
          this->chunk_remaining_ = 0;  // Non-CRLF char: reading a trailer field
        }
        // \r doesn't change the flag — it's part of \r\n line endings
        break;

      default:
        break;
    }

    if (this->chunk_state_ == ChunkedState::COMPLETE) {
      // Clear chunked flag and set content_length to actual decoded size so
      // is_read_complete() returns true and callers exit their read loops
      this->is_chunked_ = false;
      this->content_length = this->bytes_read_;
    }
  }

  return total_decoded;
}

void HttpContainerArduino::end() {
  watchdog::WatchdogManager wdm(this->parent_->get_watchdog_timeout());
  this->client_.end();
}

}  // namespace esphome::http_request

#endif  // USE_ARDUINO && !USE_ESP32
