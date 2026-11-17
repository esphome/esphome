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

/// Append data to the reply in buf (size includes the terminator, len is the current length). A UART reply grows in
/// place and comes back whole each time, so data starting with the collected text replaces it. A reply longer than
/// the buffer keeps its newest bytes, where the final line is. Returns true when bytes were dropped.
bool collect_reply(char *buf, size_t size, size_t &len, const char *data, size_t data_len);

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

/// Offset just past the '=' of the first command in text that carries a PIN or password, or 0 when there is none
size_t find_secret_args(const char *text, size_t len);

/// cmd as it may be logged: the arguments of a PIN or password command are replaced by <hidden> in buf
const char *loggable_command(const char *cmd, char *buf, size_t size);

/// Drops the arguments of every PIN or password command echoed in a flattened reply; returns the new length
size_t hide_secret_args(char *reply, size_t len);

}  // namespace esphome::modem
