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

// AT+CEREG? and AT+CREG? <stat> values (3GPP TS 27.007)
static constexpr int REGISTRATION_NOT_SEARCHING = 0;
static constexpr int REGISTRATION_HOME = 1;
static constexpr int REGISTRATION_SEARCHING = 2;
static constexpr int REGISTRATION_DENIED = 3;
static constexpr int REGISTRATION_REPORTED_UNKNOWN = 4;
static constexpr int REGISTRATION_ROAMING = 5;
static constexpr int REGISTRATION_SMS_ONLY_HOME = 6;
static constexpr int REGISTRATION_SMS_ONLY_ROAMING = 7;
static constexpr int REGISTRATION_EMERGENCY_ONLY = 8;
static constexpr int REGISTRATION_CSFB_NOT_PREFERRED_HOME = 9;
static constexpr int REGISTRATION_CSFB_NOT_PREFERRED_ROAMING = 10;
// Results beyond the 3GPP values
static constexpr int REGISTRATION_UNKNOWN = -1;      // no readable answer
static constexpr int REGISTRATION_UNSUPPORTED = -2;  // the query is not supported
static constexpr int REGISTRATION_NO_ANSWER = -3;    // the query timed out

/// Registered on a network, so the data attach can be requested. SMS only and "CSFB not preferred" limit the
/// circuit switched side, not the packet data attach; emergency only (8) is not a registration.
inline bool is_registered(int registration) {
  return registration == REGISTRATION_HOME || registration == REGISTRATION_ROAMING ||
         (registration >= REGISTRATION_SMS_ONLY_HOME && registration <= REGISTRATION_SMS_ONLY_ROAMING) ||
         (registration >= REGISTRATION_CSFB_NOT_PREFERRED_HOME &&
          registration <= REGISTRATION_CSFB_NOT_PREFERRED_ROAMING);
}

/// Add data to the reply in buf (size includes the terminator, len is the current length). With whole, data is
/// everything received so far (UART) and replaces the reply; otherwise it is one new CMUX frame and is appended. A
/// reply longer than the buffer keeps its newest bytes, where the final line is. Returns true when bytes were dropped.
bool collect_reply(char *buf, size_t size, size_t &len, const char *data, size_t data_len, bool whole);

/// How the reply ends. Only a complete final result line counts, so ERROR inside a payload line does not; a +CME ERROR
/// line counts even without its reason, which a dropped CMUX frame can take away. With
/// skip_first, the first line is a cut-off fragment and is ignored.
ReplyEnd reply_end(const char *data, size_t len, bool skip_first);

/// Puts a raw reply on one line for the log and drops the final OK; returns the new length
size_t flatten_reply(char *reply, size_t len);

/// The <stat> after the first comma of the "<n>,<stat>" line starting with prefix (+CREG: and friends), or
/// REGISTRATION_UNKNOWN; a one-field line is an unsolicited notification and is skipped
int registration_from_reply(const char *reply, const char *prefix);

/// The state that tells most from the AT+CEREG? (LTE) and AT+CREG? (2G and 3G) results
int merge_registration(int lte, int legacy);

/// Whether an error reply says the command is not supported (+CME ERROR: 4 or its verbose text)
bool reply_names_unsupported(const char *reply);

/// The SIM status in a flattened AT+CPIN? reply, cut before any following line, or nullptr
const char *sim_status(char *reply);

}  // namespace esphome::modem
