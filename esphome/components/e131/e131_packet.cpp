#ifdef USE_ARDUINO

#include "e131.h"
#include "esphome/core/log.h"
#include "esphome/core/util.h"
#include "esphome/components/network/ip_address.h"
#include <cstring>

#include <lwip/init.h>
#include <lwip/ip_addr.h>
#include <lwip/ip4_addr.h>
#include <lwip/igmp.h>

namespace esphome {
namespace e131 {

static const char *const TAG = "e131";

static const uint8_t ACN_ID[12] = {0x41, 0x53, 0x43, 0x2d, 0x45, 0x31, 0x2e, 0x31, 0x37, 0x00, 0x00, 0x00};
static const uint32_t VECTOR_ROOT = 4;
static const uint32_t VECTOR_FRAME = 2;
static const uint8_t VECTOR_DMP = 2;

// E1.31 Packet Structure
union E131RawPacket {
  struct {
    // Root Layer
    uint16_t preamble_size;
    uint16_t postamble_size;
    uint8_t acn_id[12];
    uint16_t root_flength;
    uint32_t root_vector;
    uint8_t cid[16];

    // Frame Layer
    uint16_t frame_flength;
    uint32_t frame_vector;
    uint8_t source_name[64];
    uint8_t priority;
    uint16_t reserved;
    uint8_t sequence_number;
    uint8_t options;
    uint16_t universe;

    // DMP Layer
    uint16_t dmp_flength;
    uint8_t dmp_vector;
    uint8_t type;
    uint16_t first_address;
    uint16_t address_increment;
    uint16_t property_value_count;
    uint8_t property_values[E131_MAX_PROPERTY_VALUES_COUNT];
  } __attribute__((packed));

  uint8_t raw[638];
};

// We need to have at least one `1` value
// Get the offset of `property_values[1]`
const size_t E131_MIN_PACKET_SIZE = reinterpret_cast<size_t>(&((E131RawPacket *) nullptr)->property_values[1]);

bool E131Component::join_igmp_groups_() {
  if (listen_method_ != E131_MULTICAST)
    return false;
  if (!udp_)
    return false;

  for (auto universe : universe_consumers_) {
    if (!universe.second)
      continue;

    ip4_addr_t multicast_addr = {static_cast<uint32_t>(
        network::IPAddress(239, 255, ((universe.first >> 8) & 0xff), ((universe.first >> 0) & 0xff)))};

    auto err = igmp_joingroup(IP4_ADDR_ANY4, &multicast_addr);

    if (err) {
      ESP_LOGW(TAG, "IGMP join for %d universe of E1.31 failed. Multicast might not work.", universe.first);
    }
  }

  return true;
}

void E131Component::join_(int universe) {
  // store only latest received packet for the given universe
  auto consumers = ++universe_consumers_[universe];

  if (consumers > 1) {
    return;  // we already joined before
  }

  if (join_igmp_groups_()) {
    ESP_LOGD(TAG, "Joined %d universe for E1.31.", universe);
  }
}

void E131Component::leave_(int universe) {
  auto consumers = --universe_consumers_[universe];

  if (consumers > 0) {
    return;  // we have other consumers of the given universe
  }

  if (listen_method_ == E131_MULTICAST) {
    ip4_addr_t multicast_addr = {
        static_cast<uint32_t>(network::IPAddress(239, 255, ((universe >> 8) & 0xff), ((universe >> 0) & 0xff)))};

    igmp_leavegroup(IP4_ADDR_ANY4, &multicast_addr);
  }

  ESP_LOGD(TAG, "Left %d universe for E1.31.", universe);
}

bool E131Component::packet_(const std::vector<uint8_t> &data, int &universe, E131Packet &packet) {
  if (data.size() < E131_MIN_PACKET_SIZE)
    return false;

  auto *sbuff = reinterpret_cast<const E131RawPacket *>(&data[0]);

  if (memcmp(sbuff->acn_id, ACN_ID, sizeof(sbuff->acn_id)) != 0)
    return false;
  if (htonl(sbuff->root_vector) != VECTOR_ROOT)
    return false;
  if (htonl(sbuff->frame_vector) != VECTOR_FRAME)
    return false;
  if (sbuff->dmp_vector != VECTOR_DMP)
    return false;
  if (sbuff->property_values[0] != 0)
    return false;

  universe = htons(sbuff->universe);
  packet.count = htons(sbuff->property_value_count);
  if (packet.count > E131_MAX_PROPERTY_VALUES_COUNT)
    return false;

  memcpy(packet.values, sbuff->property_values, packet.count);
  return true;
}

}  // namespace e131
}  // namespace esphome

#endif  // USE_ARDUINO
