#pragma once

namespace esphome::mqtt {

/// True if the message topic matches the subscription, with + and # wildcards.
/// A loop on purpose: recursing per character overflowed the ESP32 loop task's stack on long topics.
inline bool topic_match(const char *message, const char *subscription) {
  // First-level wildcards do not match topics that begin with '$'.
  bool wildcards = *message != '$';
  while (true) {
    if (wildcards && *subscription == '#')
      return true;  // # also matches zero remaining levels (for example, a/b/# matches a/b).

    if (wildcards && *subscription == '+') {
      subscription++;
      while (*message != '\0' && *message != '/')
        message++;
    } else {
      while (*subscription != '\0' && *subscription != '/') {
        if (*message != *subscription)
          return false;
        message++;
        subscription++;
      }
    }

    if (*subscription == '\0')
      return *message == '\0';

    // Both strings must end a level at the same separator. The sole exception is
    // a trailing /# filter, whose # matches zero levels after the parent level.
    if (*message == '\0')
      return subscription[0] == '/' && subscription[1] == '#' && subscription[2] == '\0';
    if (*message != '/')
      return false;

    message++;
    subscription++;
    wildcards = true;
  }
}

}  // namespace esphome::mqtt
