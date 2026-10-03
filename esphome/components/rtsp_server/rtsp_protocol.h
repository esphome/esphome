#pragma once

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

namespace esphome::rtsp_server {

enum class RtspMethod : uint8_t {
  RTSP_METHOD_OPTIONS,
  RTSP_METHOD_DESCRIBE,
  RTSP_METHOD_SETUP,
  RTSP_METHOD_PLAY,
  RTSP_METHOD_TEARDOWN,
  RTSP_METHOD_UNKNOWN,
};

struct RtspRequestView {
  RtspMethod method{RtspMethod::RTSP_METHOD_UNKNOWN};
  int cseq{1};
  const char *data{nullptr};
  size_t length{0};
};

inline bool ascii_equal_case_insensitive(char lhs, char rhs) {
  return std::tolower(static_cast<unsigned char>(lhs)) == std::tolower(static_cast<unsigned char>(rhs));
}

inline const char *find_case_insensitive(const char *data, size_t length, const char *needle) {
  if (data == nullptr || needle == nullptr) {
    return nullptr;
  }
  const size_t needle_length = std::strlen(needle);
  if (needle_length == 0) {
    return data;
  }
  if (needle_length > length) {
    return nullptr;
  }
  for (size_t offset = 0; offset + needle_length <= length; offset++) {
    bool matches = true;
    for (size_t index = 0; index < needle_length; index++) {
      if (!ascii_equal_case_insensitive(data[offset + index], needle[index])) {
        matches = false;
        break;
      }
    }
    if (matches) {
      return data + offset;
    }
  }
  return nullptr;
}

inline bool starts_with_token(const char *data, size_t length, const char *token) {
  const size_t token_length = std::strlen(token);
  if (data == nullptr || token_length > length || std::memcmp(data, token, token_length) != 0) {
    return false;
  }
  return token_length == length || data[token_length] == ' ' || data[token_length] == '\t';
}

inline RtspMethod parse_rtsp_method(const char *data, size_t length) {
  if (starts_with_token(data, length, "OPTIONS")) {
    return RtspMethod::RTSP_METHOD_OPTIONS;
  }
  if (starts_with_token(data, length, "DESCRIBE")) {
    return RtspMethod::RTSP_METHOD_DESCRIBE;
  }
  if (starts_with_token(data, length, "SETUP")) {
    return RtspMethod::RTSP_METHOD_SETUP;
  }
  if (starts_with_token(data, length, "PLAY")) {
    return RtspMethod::RTSP_METHOD_PLAY;
  }
  if (starts_with_token(data, length, "TEARDOWN")) {
    return RtspMethod::RTSP_METHOD_TEARDOWN;
  }
  return RtspMethod::RTSP_METHOD_UNKNOWN;
}

inline int parse_positive_int(const char *data, const char *end, int fallback) {
  if (data == nullptr || end == nullptr || data >= end) {
    return fallback;
  }
  while (data < end && (*data == ' ' || *data == '\t')) {
    data++;
  }
  if (data >= end || !std::isdigit(static_cast<unsigned char>(*data))) {
    return fallback;
  }
  uint32_t value = 0;
  while (data < end && std::isdigit(static_cast<unsigned char>(*data))) {
    const uint32_t digit = static_cast<uint32_t>(*data - '0');
    if (value > (static_cast<uint32_t>(std::numeric_limits<int>::max()) - digit) / 10U) {
      return fallback;
    }
    value = value * 10U + digit;
    data++;
  }
  return value == 0 ? fallback : static_cast<int>(value);
}

inline RtspRequestView parse_rtsp_request(const char *data, size_t length, int default_cseq = 1) {
  RtspRequestView request{parse_rtsp_method(data, length), default_cseq, data, length};
  const char *header = find_case_insensitive(data, length, "CSeq:");
  if (header != nullptr) {
    const char *value = header + 5;
    const char *end = data + length;
    request.cseq = parse_positive_int(value, end, default_cseq);
  }
  return request;
}

inline size_t rtsp_request_length(const char *data, size_t length) {
  if (data == nullptr || length < 4U) {
    return 0;
  }
  for (size_t index = 0; index + 4U <= length; index++) {
    if (data[index] == '\r' && data[index + 1U] == '\n' && data[index + 2U] == '\r' && data[index + 3U] == '\n') {
      return index + 4U;
    }
  }
  return 0;
}

inline bool rtsp_transport_requests_tcp(const char *request, size_t length) {
  return find_case_insensitive(request, length, "RTP/AVP/TCP") != nullptr ||
         find_case_insensitive(request, length, "interleaved=") != nullptr;
}

inline bool secure_token_equal(const char *lhs, size_t lhs_length, const char *rhs, size_t rhs_length) {
  const size_t maximum = lhs_length > rhs_length ? lhs_length : rhs_length;
  uint8_t difference = static_cast<uint8_t>(lhs_length != rhs_length);
  for (size_t index = 0; index < maximum; index++) {
    const uint8_t left = index < lhs_length ? static_cast<uint8_t>(lhs[index]) : 0U;
    const uint8_t right = index < rhs_length ? static_cast<uint8_t>(rhs[index]) : 0U;
    difference |= left ^ right;
  }
  return difference == 0;
}

}  // namespace esphome::rtsp_server
