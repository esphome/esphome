#include "receive_trigger.h"

namespace esphome {
namespace wifi_now {

static const char *TAG = "wifi_now.receive_trigger";

WifiNowReceiveTrigger::WifiNowReceiveTrigger( WifiNowComponent *component)
    : component_( component)
{
}

float WifiNowReceiveTrigger::get_setup_priority() const
{
    return setup_priority::DATA;
}

void WifiNowReceiveTrigger::setup()
{
    component_->register_receive_callback([this] (WifiNowPacket & packet) -> bool { return this->recieve_packet(packet);});
}

void WifiNowReceiveTrigger::set_peer(WifiNowPeer *peer)
{
    peer_ = peer;
}

void WifiNowReceiveTrigger::set_servicekey(const servicekey_t servicekey)
{
    servicekey_ = servicekey;
}

void WifiNowReceiveTrigger::set_payload_setters( const std::vector<WifiNowPayloadSetter*> &payload_setters)
{
    payload_setters_ = payload_setters;
}

bool WifiNowReceiveTrigger::recieve_packet(WifiNowPacket &packet)
{
    if( peer_)
    {
        if( peer_->get_bssid() != packet.get_bssid())
        {
            return false;
        }
    }
    if( servicekey_ == servicekey_t())
    {
        auto &payload = packet.get_packetdata();
        auto it = payload.cbegin();
        for( auto setter : payload_setters_)
        {
            setter->set_payload( payload, it);
        }
    }
    else
    {
        if( servicekey_ != packet.get_servicekey())
        {
            return false;
        }

        auto &payload = packet.get_payload();
        auto it = payload.cbegin();
        for( auto setter : payload_setters_)
        {
            setter->set_payload( payload, it);
        }
    }
    trigger();

    return true;
}


} // namespace esphome
} // namespace wifi_now
