#pragma once

#include <string>
#include <vector>

#include "esphome/core/component.h"

#include "simple_types.h"
#include "component.h"

namespace esphome {
namespace wifi_now {

template<typename... Ts>
class WifiNowPayloadGetter
{
public:
    WifiNowPayloadGetter() {}
    virtual void get_payload(payload_t &payload, Ts... x) = 0;
};

template<typename T, typename... Ts>
class WifiNowTemplatePayloadGetter
    : public WifiNowPayloadGetter<Ts...>
{
    TEMPLATABLE_VALUE(T, value);

public:
    void get_payload(payload_t &payload, Ts... x) override
    {
        T value = this->value_.value(x...);
        payload.insert( payload.end(), (const uint8_t*)&value, ((const uint8_t*)&value) + 4);
    }
};

template<typename... Ts>
class WifiNowTemplatePayloadGetter<std::string, Ts...>
    : public WifiNowPayloadGetter<Ts...>
{
    TEMPLATABLE_VALUE(std::string, value);

public:
    void get_payload(payload_t &payload, Ts... x)
    {
        std::string value = this->value_.value(x...);
        size_t size = value.size();
        payload.insert( payload.end(), (const uint8_t*)&size, ((const uint8_t*)&size) + sizeof(size));
        payload.insert( payload.end(), value.cbegin(), value.cend());
    };
};

template<typename... Ts>
class WifiNowTemplatePayloadGetter<std::vector<uint8_t>, Ts...>
    : public WifiNowPayloadGetter<Ts...>
{
    TEMPLATABLE_VALUE(std::vector<uint8_t>, value);

public:
    void get_payload(payload_t &payload, Ts... x)
    {
        std::vector<uint8_t> value = this->value_.value(x...);
        size_t size = value.size();
        payload.insert( payload.end(), (const uint8_t*)&size, ((const uint8_t*)&size) + sizeof(size));
        payload.insert( payload.end(), value.cbegin(), value.cend());
    };
};

template<typename... Ts>
class WifiNowTemplatePayloadGetter<bool, Ts...>
    : public WifiNowPayloadGetter<Ts...>
{
    TEMPLATABLE_VALUE(bool, value);

public:
    void get_payload(payload_t &payload, Ts... x)
    {
        bool value = this->value_.value(x...) ? 1 : 0;
        payload.insert( payload.end(), (const uint8_t*)&value, ((const uint8_t*)&value) + sizeof(value));
    };
};

template<typename... Ts>
class WifiNowPayloadPayloadGetter
    : public WifiNowPayloadGetter<Ts...>
{
    TEMPLATABLE_VALUE(payload_t, value);

public:
    void get_payload(payload_t &payload, Ts... x)
    {
        payload_t value = this->value_.value(x...);
        payload.insert( payload.end(), value.cbegin(), value.cend());
    };
};

}  // namespace wifi_now
}  // namespace esphome
