#pragma once

namespace esphome::mqtt {

/// True if the message topic matches the subscription, with + and # wildcards.
/// A loop on purpose: recursing per character overflowed the ESP32 loop task's stack on long topics.
inline bool topic_match(const char *message, const char *subscription) {
  // First-level wildcards do not match topics that begin with '$'
  bool wildcards = *message != '$';
  while (*message != '\0' && *subscription != '\0') {
    if (wildcards && *subscription == '#')
      return true;
    if (wildcards && *subscription == '+') {
      subscription++;
      while (*message != '\0' && *message != '/')
        message++;
      continue;
    }
    if (*message != *subscription)
      return false;
    wildcards |= *subscription == '/';
    message++;
    subscription++;
  }
  return *message == *subscription;  // a match only if both ended
}

}  // namespace esphome::mqtt
