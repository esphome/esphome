#pragma once

#include <cstddef>
#include <cstdint>

// Parsers for raw AT replies; free of esp_modem so they build and run in host unit tests
namespace esphome::modem {

enum class ReplyEnd : uint8_t {
  REPLY_END_NONE,   // no complete final result line yet
  REPLY_END_OK,     // an OK line
  REPLY_END_ERROR,  // an ERROR, +CME ERROR: or +CMS ERROR: line
};

// Registration <stat> results beyond the 3GPP values
static constexpr int REGISTRATION_UNKNOWN = -1;  // no readable answer

/// Add data to the reply in buf (size includes the terminator, len is the current length). With whole, data is
/// everything received so far (UART) and replaces the reply; otherwise it is one new CMUX frame and is appended. A
/// reply longer than the buffer keeps its newest bytes, where the final line is. Returns true when bytes were dropped.
bool collect_reply(char *buf, size_t size, size_t &len, const char *data, size_t data_len, bool whole);

/// How the reply ends. Only a complete final result line counts, so ERROR inside a payload line does not. With
/// skip_first, the first line is a cut-off fragment and is ignored.
ReplyEnd reply_end(const char *data, size_t len, bool skip_first);

/// Puts a raw reply on one line for the log and drops the final OK; returns the new length
size_t flatten_reply(char *reply, size_t len);

/// The <stat> after the first comma of the "<n>,<stat>" line starting with prefix (+CREG: and friends), or
/// REGISTRATION_UNKNOWN; a one-field line is an unsolicited notification and is skipped
int registration_from_reply(const char *reply, const char *prefix);

/// Whether an error reply says the command is not supported (+CME ERROR: 4 or its verbose text)
bool reply_names_unsupported(const char *reply);

/// The SIM status in a flattened AT+CPIN? reply, cut before any following line, or nullptr
const char *sim_status(char *reply);

}  // namespace esphome::modem
