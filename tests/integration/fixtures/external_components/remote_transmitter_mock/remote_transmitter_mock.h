#pragma once

// ============================================================================
// HOST-ONLY TEST COMPONENT — DO NOT COPY TO PRODUCTION CODE
//
// Emulates a non-blocking remote transmitter: a frame "occupies" the
// transmitter for its real duration and the completion callback fires from a
// scheduler timeout, the same way the ESP32 RMT backend reports completion.
// A frame submitted while another is in flight is logged as an overlap; the
// real backends block the main loop in that case.
// ============================================================================

#include "esphome/core/component.h"
#include "esphome/components/remote_base/remote_base.h"

namespace esphome::remote_transmitter_mock {

class MockRemoteTransmitter : public remote_base::RemoteTransmitterBase, public Component {
 public:
  MockRemoteTransmitter() : remote_base::RemoteTransmitterBase(nullptr) {}
  void dump_config() override;

 protected:
  void flush_pending_completion_() override;
  void send_internal(uint32_t send_times, uint32_t send_wait) override;
  void finish_();

  uint32_t seq_{0};  // own count for the log lines, one per frame
  bool busy_{false};
};

}  // namespace esphome::remote_transmitter_mock
