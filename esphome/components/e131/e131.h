#pragma once
#include "esphome/core/defines.h"
#ifdef USE_NETWORK
#include "esphome/components/socket/socket.h"
#include "esphome/core/component.h"

#include <cinttypes>
#include <memory>
#include <vector>

namespace esphome::e131 {

class E131AddressableLightEffect;

enum E131ListenMethod { E131_MULTICAST, E131_UNICAST };

const int E131_MAX_PROPERTY_VALUES_COUNT = 513;

struct E131Packet {
  uint16_t count;
  uint8_t values[E131_MAX_PROPERTY_VALUES_COUNT];
};

#ifdef USE_E131_ARBITRATION
// Source arbitration, ANSI E1.31-2025: options bits (6.2.6), priority range (6.2.3), network data loss timeout
// (6.7.1, annex A) and the out-of-sequence window (6.7.2).
static constexpr uint8_t E131_OPTION_PREVIEW_DATA = 0x80;
static constexpr uint8_t E131_OPTION_STREAM_TERMINATED = 0x40;
static constexpr uint8_t E131_MAX_PRIORITY = 200;
static constexpr uint32_t E131_NETWORK_DATA_LOSS_TIMEOUT_MS = 2500;
static constexpr int8_t E131_SEQUENCE_WINDOW = -20;
static constexpr size_t E131_CID_SIZE = 16;
static constexpr size_t E131_SOURCE_NAME_SIZE = 64;

// Framing layer fields of a received data packet; the pointers point into the receive buffer.
struct E131SourceInfo {
  const uint8_t *cid;
  const uint8_t *source_name;
  uint8_t priority;
  uint8_t sequence;
  uint8_t options;
};

// The source whose data a universe shows, and the best priority among the sources it currently keeps out (the
// runner-up), so that a handoff goes to the best remaining source rather than to whichever packet arrives first.
struct E131Source {
  uint8_t cid[E131_CID_SIZE];
  uint32_t last_packet_ms;
  uint32_t runner_up_ms;
  uint8_t priority;
  uint8_t sequence;
  uint8_t runner_up_priority;
  bool active;
  bool has_runner_up;
};

enum class E131Decision : uint8_t { E131_DECISION_IGNORE, E131_DECISION_APPLY, E131_DECISION_TAKE_OVER };
#endif

struct UniverseConsumer {
  uint16_t universe;
  uint16_t consumers;
#ifdef USE_E131_ARBITRATION
  E131Source source;
#endif
};

class E131Component final : public esphome::Component {
 public:
  E131Component();
  ~E131Component();

  void setup() override;
  void loop() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  void add_effect(E131AddressableLightEffect *light_effect);
  void remove_effect(E131AddressableLightEffect *light_effect);

  void set_method(E131ListenMethod listen_method) { this->listen_method_ = listen_method; }

 protected:
  bool packet_(const uint8_t *data, size_t len, int &universe, E131Packet &packet);
  bool process_(int universe, const E131Packet &packet);
#ifdef USE_E131_ARBITRATION
  void source_info_(const uint8_t *data, E131SourceInfo &info);
  E131Decision arbitrate_(int universe, const E131SourceInfo &info);
  void log_source_taking_over_(int universe, const E131SourceInfo &info);
  void log_source_leaving_(int universe, const uint8_t *cid, const char *reason);
#endif
  void join_igmp_groups_();
  void join_universe_(uint16_t universe);
  UniverseConsumer *find_universe_(int universe);
  void join_(int universe);
  void leave_(int universe);

  E131ListenMethod listen_method_{E131_MULTICAST};
  bool igmp_join_pending_{false};
  std::unique_ptr<socket::UDPSocket> socket_;
  std::vector<E131AddressableLightEffect *> light_effects_;
  std::vector<UniverseConsumer> universe_consumers_;
#if defined(USE_E131_ARBITRATION) && defined(ESPHOME_LOG_HAS_DEBUG)
  // The source last logged as taking over, so that a change of source logs one line, not one per universe.
  uint8_t logged_source_cid_[E131_CID_SIZE]{};
  bool has_logged_source_{false};
#endif
};

}  // namespace esphome::e131

#endif
