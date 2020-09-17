#pragma once

#include <vector>
#include <functional>

#include "esphome/core/defines.h"
#include "esphome/core/component.h"

#include "simple_types.h"
#include "peer.h"
#include "packet.h"

namespace esphome {
namespace wifi_now {


class WifiNowComponent
    : public Component
{
public:
    WifiNowComponent();

    float get_setup_priority() const override;
    float get_loop_priority() const override;
    void setup() override;
    void dump_config() override;
    void loop() override;

    void set_channel(uint8_t channel);
    uint8_t get_channel() const;
    void set_aeskey(const aeskey_t &aeskey);
    const optional<aeskey_t> &get_aeskey() const;
    void set_peer(WifiNowPeer *peer);
    void add_peer(WifiNowPeer *peer);
    const std::vector<WifiNowPeer*> &get_peers() const;

    void send( const WifiNowPacket &packet, std::function<void(bool)> &&callback = nullptr);
    void register_receive_callback(std::function<bool(WifiNowPacket&)> &&callback);
    void register_priorized_receive_callback(std::function<bool(WifiNowPacket&)> &&callback);


protected:
    uint8_t channel_;
    optional<aeskey_t> aeskey_;
    std::vector<WifiNowPeer*> peers_;
};

} // namespace wifi_now
} // namespace esphome

