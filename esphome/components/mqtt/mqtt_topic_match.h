#pragma once

namespace esphome::mqtt {

/** Check if the message topic matches the given subscription topic.
 *
 * A loop, not recursion: the ESP32's windowed calls cannot reuse the frame for a tail call, so a recursive walk took
 * a stack frame per character and a long topic overflowed the loop task's stack.
 *
 * INFO: MQTT spec mandates that topics must not be empty and must be valid NULL-terminated UTF-8 strings.
 *
 * @param message The message topic that was received from the MQTT server. Note: this must not contain
 *                wildcard characters as mandated by the MQTT spec.
 * @param subscription The subscription topic we are matching against.
 * @return true if the subscription topic matches the message topic, false otherwise.
 */
inline bool topic_match(const char *message, const char *subscription) {
  // Wildcards at the first level do not match topics that begin with '$'
  const bool is_normal = *message != '\0' && *message != '$';
  bool past_separator = false;
  while (true) {
    // Reached end of both strings at the same time, this means we have a successful match
    if (*message == '\0' && *subscription == '\0')
      return true;
    // Either the message or the subscribe are at the end. This means they don't match.
    if (*message == '\0' || *subscription == '\0')
      return false;

    const bool do_wildcards = is_normal || past_separator;
    if (*subscription == '+' && do_wildcards) {
      // single level wildcard: consume + from subscription and the level from the message
      subscription++;
      while (*message != '\0' && *message != '/')
        message++;
      // after this, both pointers point to a '/' or to the end of the string
      past_separator = true;
      continue;
    }
    if (*subscription == '#' && do_wildcards) {
      // multilevel wildcard - MQTT mandates that this must be at end of subscribe topic
      return true;
    }

    // this handles '/' and normal characters at the same time.
    if (*message != *subscription)
      return false;
    past_separator = past_separator || *subscription == '/';
    subscription++;
    message++;
  }
}

}  // namespace esphome::mqtt
