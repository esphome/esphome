#include "at_reply.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <strings.h>

namespace esphome::modem {

// Commands whose arguments are PINs or passwords, which must never reach the log
static constexpr const char *const SECRET_COMMANDS[] = {"+CPIN=", "+CPIN2=", "+CLCK=", "+CPWD="};

bool collect_reply(char *buf, size_t size, size_t &len, const char *data, size_t data_len) {
  const size_t capacity = size - 1;
  if (data_len >= len && memcmp(data, buf, len) == 0) {
    len = 0;
  }
  bool dropped = false;
  if (data_len >= capacity) {
    memcpy(buf, data + data_len - capacity, capacity);
    len = capacity;
    dropped = true;
  } else {
    if (len + data_len > capacity) {
      const size_t drop = len + data_len - capacity;
      memmove(buf, buf + drop, len - drop);
      len -= drop;
      dropped = true;
    }
    memcpy(buf + len, data, data_len);
    len += data_len;
  }
  buf[len] = '\0';
  return dropped;
}

ReplyEnd reply_end(const char *data, size_t len, bool skip_first) {
  const char *end = data + len;
  const char *line = data;
  if (skip_first) {
    const auto *newline = static_cast<const char *>(memchr(data, '\n', len));
    line = newline != nullptr ? newline + 1 : end;
  }
  while (line < end) {
    const auto *newline = static_cast<const char *>(memchr(line, '\n', end - line));
    if (newline == nullptr) {
      break;  // the line has not ended yet
    }
    size_t size = newline - line;
    if (size != 0 && line[size - 1] == '\r') {
      size--;
    }
    if (size == 2 && memcmp(line, "OK", 2) == 0) {
      return ReplyEnd::REPLY_END_OK;
    }
    if ((size == 5 && memcmp(line, "ERROR", 5) == 0) || (size >= 11 && memcmp(line, "+CME ERROR:", 11) == 0) ||
        (size >= 11 && memcmp(line, "+CMS ERROR:", 11) == 0)) {
      return ReplyEnd::REPLY_END_ERROR;
    }
    line = newline + 1;
  }
  return ReplyEnd::REPLY_END_NONE;
}

size_t flatten_reply(char *reply, size_t len) {
  for (size_t i = 0; i < len; i++) {
    if (reply[i] == '\r' || reply[i] == '\n') {
      reply[i] = ' ';
    }
  }
  while (len != 0 && reply[len - 1] == ' ') {
    len--;
  }
  if (len >= 2 && memcmp(reply + len - 2, "OK", 2) == 0 && (len == 2 || reply[len - 3] == ' ')) {
    len -= 2;
    while (len != 0 && reply[len - 1] == ' ') {
      len--;
    }
  }
  size_t start = 0;
  while (start < len && reply[start] == ' ') {
    start++;
  }
  len -= start;
  memmove(reply, reply + start, len);
  reply[len] = '\0';
  return len;
}

int registration_from_reply(const char *reply, const char *prefix) {
  const size_t prefix_len = strlen(prefix);
  for (const char *line = strstr(reply, prefix); line != nullptr; line = strstr(line + 1, prefix)) {
    const char *start = line + prefix_len;
    char *end = nullptr;
    strtol(start, &end, 10);
    if (end == start || *end != ',') {
      continue;
    }
    start = end + 1;
    const auto stat = static_cast<int32_t>(strtol(start, &end, 10));
    if (end != start) {
      return stat;
    }
  }
  return REGISTRATION_UNKNOWN;
}

bool reply_names_unsupported(const char *reply) {
  static constexpr char PREFIX[] = "+CME ERROR:";
  static constexpr char TEXT[] = "operation not supported";
  const char *code = strstr(reply, PREFIX);
  if (code == nullptr) {
    return false;
  }
  code += sizeof(PREFIX) - 1;
  while (*code == ' ') {
    code++;
  }
  return strncasecmp(code, TEXT, sizeof(TEXT) - 1) == 0 || (code[0] == '4' && !isdigit(code[1]));
}

const char *sim_status(char *reply) {
  static constexpr char PREFIX[] = "+CPIN:";
  char *status = strstr(reply, PREFIX);
  if (status == nullptr) {
    return nullptr;
  }
  status += sizeof(PREFIX) - 1;
  while (*status == ' ') {
    status++;
  }
  // A URC in the same reply follows on its own line, which flatten_reply() turned into two spaces
  char *end = strstr(status, "  ");
  if (end != nullptr) {
    *end = '\0';
  }
  return status;
}

size_t find_secret_args(const char *text, size_t len) {
  for (size_t i = 0; i < len; i++) {
    if (text[i] != '+') {
      continue;
    }
    for (const char *secret : SECRET_COMMANDS) {
      const size_t size = strlen(secret);
      if (len - i >= size && strncasecmp(text + i, secret, size) == 0) {
        return i + size;
      }
    }
  }
  return 0;
}

const char *loggable_command(const char *cmd, char *buf, size_t size) {
  const size_t args = find_secret_args(cmd, strlen(cmd));
  if (args == 0) {
    return cmd;
  }
  snprintf(buf, size, "%.*s<hidden>", static_cast<int>(args), cmd);
  return buf;
}

size_t hide_secret_args(char *reply, size_t len) {
  size_t from = 0;
  while (const size_t args = find_secret_args(reply + from, len - from)) {
    const size_t start = from + args;
    size_t end = start;
    bool quoted = false;
    while (end < len && (quoted || reply[end] != ' ')) {
      quoted ^= reply[end] == '"';
      end++;
    }
    memmove(reply + start, reply + end, len - end);
    len -= end - start;
    from = start;
  }
  reply[len] = '\0';
  return len;
}

}  // namespace esphome::modem
