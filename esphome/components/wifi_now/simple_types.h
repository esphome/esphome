#pragma once

#include <array>
#include <vector>

namespace esphome {
namespace wifi_now {

using bssid_t = std::array<uint8_t, 6>;
using aeskey_t = std::array<uint8_t, 16>;
using packetdata_t = std::vector<uint8_t>;
using payload_t = std::vector<uint8_t>;

using servicekey_t = std::array<uint8_t, 8>;

using packet_t = struct
{
    uint8_t servicekey[8];
    uint8_t payload[0];
};

enum WifiNowBinarySensorEvent : uint8_t
{
    SWITCH_OFF = 0,
    SWITCH_ON = 1,
    PRESS,
    RELEASE,
    CLICK,
    DOUBLE_CLICK,
    MULTI_CLICK1,
    MULTI_CLICK2,
    MULTI_CLICK3,
    MULTI_CLICK4,
    MULTI_CLICK5,
    MULTI_CLICK6,
    MULTI_CLICK7,
    MULTI_CLICK8,
    MULTI_CLICK9,
    MULTI_CLICK10,
    MULTI_CLICK11,
    MULTI_CLICK12,
    MULTI_CLICK13,
    MULTI_CLICK14,
    MULTI_CLICK15,
    MULTI_CLICK16,
    MULTI_CLICK17,
    MULTI_CLICK18,
    MULTI_CLICK19,
    MULTI_CLICK20,
    COUNT = MULTI_CLICK20 + 1,
};

} // namespace wifi_now
} // namespace esphome
