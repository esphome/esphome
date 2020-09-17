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
public:
    void set_payload( const payload_t &payload, payload_t::const_iterator &it) override
    {
        std::copy_n( it, sizeof(value_), (uint8_t*)&value_);
    }

    const T &get_value() const 
    {
        return value_;
    }

protected:
    T value_;

};

template<typename... Ts>
class WifiNowTemplatePayloadSetter<std::string, Ts...>
    : public WifiNowPayloadSetter
{
public:
    void set_payload( const payload_t &payload, payload_t::const_iterator &it) override
    {
        size_t size;
        std::copy( it, it + sizeof(size), (uint8_t*)&size);
        value_.resize(0);
        value_.insert( value_.begin(), it, it + size);
    }
    const std::string &get_value() const 
    {
        return value_;
    }

protected:
    std::string  value_;

};

template<typename... Ts>
class WifiNowTemplatePayloadSetter<std::vector<uint8_t>, Ts...>
    : public WifiNowPayloadSetter
{
public:
    void set_payload( const payload_t &payload, payload_t::const_iterator &it) override
    {
        size_t size;
        std::copy( it, it + sizeof(size), (uint8_t*)&size);
        value_.resize(0);
        value_.insert( value_.begin(), it, it + size);
    }
    const std::vector<uint8_t> &get_value() const 
    {
        return value_;
    }

protected:
    std::vector<uint8_t> value_;

};

template<typename... Ts>
class WifiNowTemplatePayloadSetter<bool, Ts...>
    : public WifiNowPayloadSetter
{
public:
    void set_payload( const payload_t &payload, payload_t::const_iterator &it) override
    {
        std::copy( it, it + sizeof(value_), (uint8_t*)&value_);
    }
    const bool &get_value() const 
    {
        return value_;
    }

protected:
    bool value_;

};

template<typename... Ts>
class WifiNowPayloadPayloadSetter
    : public WifiNowPayloadSetter
{
public:
    void set_payload( const payload_t &payload, payload_t::const_iterator &it) override
    {
        value_.resize(0);
        value_.insert( value_.begin(), it, payload.cend());
    }

    const payload_t &get_value() const 
    {
        return value_;
    }

protected:
    payload_t value_;

};

}  // namespace wifi_now
}  // namespace esphome
