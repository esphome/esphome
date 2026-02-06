#pragma once

#include "http_request.h"

#if defined(USE_ARDUINO) && !defined(USE_ESP32)

#if defined(USE_RP2040)
#include <HTTPClient.h>
#include <WiFiClient.h>
#endif
#ifdef USE_ESP8266
#include <ESP8266HTTPClient.h>
#ifdef USE_HTTP_REQUEST_ESP8266_HTTPS
#include <WiFiClientSecure.h>
#endif
#endif

namespace esphome::http_request {

class HttpRequestArduino;

/// State machine for decoding chunked transfer encoding on Arduino
enum class ChunkedState : uint8_t {
  CHUNK_HEADER,      ///< Reading hex digits of chunk size
  CHUNK_HEADER_EXT,  ///< Skipping chunk extensions until \n
  CHUNK_DATA,        ///< Reading chunk data bytes
  CHUNK_DATA_TRAIL,  ///< Skipping \r\n after chunk data
  CHUNK_TRAILER,     ///< Consuming trailer headers after final 0-size chunk
  COMPLETE,          ///< Finished: final chunk and trailers consumed
};

class HttpContainerArduino : public HttpContainer {
 public:
  int read(uint8_t *buf, size_t max_len) override;
  void end() override;

 protected:
  friend class HttpRequestArduino;
  HTTPClient client_{};

  /// Decode chunked transfer encoding from the raw stream
  int read_chunked_(uint8_t *buf, size_t max_len, WiFiClient *stream);
  /// Transition from chunk header to data or trailer based on parsed size
  void chunk_header_complete_();
  ChunkedState chunk_state_{ChunkedState::CHUNK_HEADER};
  size_t chunk_remaining_{0};  ///< Bytes remaining in current chunk
};

class HttpRequestArduino : public HttpRequestComponent {
 protected:
  std::shared_ptr<HttpContainer> perform(const std::string &url, const std::string &method, const std::string &body,
                                         const std::list<Header> &request_headers,
                                         const std::set<std::string> &collect_headers) override;
};

}  // namespace esphome::http_request

#endif  // USE_ARDUINO && !USE_ESP32
