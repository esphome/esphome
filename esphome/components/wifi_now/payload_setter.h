#pragma once

#include <string>
#include <vector>

#include "esphome/core/component.h"

#include "simple_types.h"
#include "component.h"

namespace esphome {
namespace wifi_now {

class WifiNowPayloadSetter
{

public:
    WifiNowPayloadSetter() {};
    virtual void set_payload( const payload_t &payload,  payload_t::const_iterator &it) = 0;
};

template<typename T, typename... Ts>
class WifiNowTemplatePayloadSetter
    : public WifiNowPayloadSetter
{
    T value;

public:
    void set_payload( const payload_t &payload, payload_t::const_iterator &it) override
    {
        std::copy_n( it, sizeof(value), (uint8_t*)&value);
    }

    const T &get_value() const 
    {
        return value;
    }
};

template<typename... Ts>
class WifiNowTemplatePayloadSetter<std::string, Ts...>
    : public WifiNowPayloadSetter
{
    std::string  value;

public:
    void set_payload( const payload_t &payload, payload_t::const_iterator &it) override
    {
        size_t size;
        std::copy( it, it + sizeof(size), (uint8_t*)&size);
        value.resize(0);
        value.insert( value.begin(), it, it + size);
    }
    const std::string &get_value() const 
    {
        return value;
    }
};

template<typename... Ts>
class WifiNowTemplatePayloadSetter<std::vector<uint8_t>, Ts...>
    : public WifiNowPayloadSetter
{
    std::vector<uint8_t> value;

public:
    void set_payload( const payload_t &payload, payload_t::const_iterator &it) override
    {
        size_t size;
        std::copy( it, it + sizeof(size), (uint8_t*)&size);
        value.resize(0);
        value.insert( value.begin(), it, it + size);
    }
    const std::vector<uint8_t> &get_value() const 
    {
        return value;
    }
};

template<typename... Ts>
class WifiNowTemplatePayloadSetter<bool, Ts...>
    : public WifiNowPayloadSetter
{
    bool value;

public:
    void set_payload( const payload_t &payload, payload_t::const_iterator &it) override
    {
        std::copy( it, it + sizeof(value), (uint8_t*)&value);
    }
    const bool &get_value() const 
    {
        return value;
    }
};

template<typename... Ts>
class WifiNowPayloadPayloadSetter
    : public WifiNowPayloadSetter
{
    payload_t value;

public:
    void set_payload( const payload_t &payload, payload_t::const_iterator &it) override
    {
        value.resize(0);
        value.insert( value.begin(), it, payload.cend());
    }

    const payload_t &get_value() const 
    {
        return value;
    }
};

}  // namespace wifi_now
}  // namespace esphome
