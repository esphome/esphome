#pragma once

#include <utility>

#include "esphome/core/defines.h"
#include "esphome/core/automation.h"
#include "esphome/core/base_automation.h"
#include "esphome/core/component.h"

#include "simple_types.h"
#include "component.h"
#include "payload_setter.h"

namespace esphome {
namespace wifi_now {

class WifiNowReceiveTrigger
    : public Trigger<>
    , public Component
{
public:
    WifiNowReceiveTrigger( WifiNowComponent *component);

    float get_setup_priority() const override;
    void setup() override;

    void set_peer(WifiNowPeer *peer);
    void set_servicekey(const servicekey_t servicekey);
    void set_payload_setters( const std::vector<WifiNowPayloadSetter*> &payload_setters);

protected:
    bool recieve_packet(WifiNowPacket &packet);

    WifiNowComponent *component_;
    WifiNowPeer *peer_{nullptr};
    servicekey_t servicekey_{};
    std::vector<WifiNowPayloadSetter*> payload_setters_;

};

} // namespace esphome
} // namespace wifi_now
