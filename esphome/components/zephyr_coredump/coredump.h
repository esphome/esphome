#pragma once
#include "esphome/core/component.h"

namespace esphome::zephyr_coredump {

/// The tag the dump is logged under, so a client can check the level in force for it
inline constexpr const char *CRASH_LOG_TAG = "zephyr_coredump";

/// Whether a dump from before this boot is waiting to be printed.
bool coredump_pending();
/// A log reader is present: serial at boot, USB CDC when the port opens, an API or MQTT client subscribing.
/// The stored dump is printed from the start and erased once it has gone out; a print already under way
/// carries on.
void print_coredump();
/// The reader went away (the USB port was closed); the print stops and the dump stays for the next one.
/// Readers are not told apart: closing the USB port also stops a print an API or MQTT client is receiving,
/// and a client that disconnects does not stop one, the same as the other platforms' crash records.
void stop_coredump();

class Coredump : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

 protected:
  friend bool coredump_pending();
  friend void print_coredump();
  friend void stop_coredump();
  /// Start printing after a short delay, one line per loop so the USB CDC transmit buffer and the main loop keep up
  void start_print_();
  void stop_print_();
  void finish_(int err);

  uint32_t offset_{0};
  uint32_t total_{0};
  uint32_t last_line_ms_{0};
  int verify_{0};  // result of verifying the stored dump, 1 when it is good
  bool stored_{false};
  bool printing_{false};
};

}  // namespace esphome::zephyr_coredump
