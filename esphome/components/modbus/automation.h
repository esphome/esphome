#pragma once

#include "esphome/components/modbus/modbus.h"
#include "esphome/core/automation.h"

#include <span>
#include <vector>

namespace esphome::modbus {

// Fire-and-forget: send a raw PDU (function code + data) to `address` on this hub. The hub prepends the
// device address and appends the CRC. No device is attached, so any reply is dropped - correct for a
// broadcast write (address 0, which the spec leaves unanswered) and for pokes where the reply does not
// matter. For request/response (handle the reply), use the modbus_client component instead.
template<typename... Ts> class ModbusSendAction final : public Action<Ts...>, public Parented<ModbusClientHub> {
 public:
  TEMPLATABLE_VALUE(uint8_t, address)
  TEMPLATABLE_VALUE(std::vector<uint8_t>, pdu)
  void play(Ts... x) override {
    auto pdu = this->pdu_.value(x...);
    if (!pdu.empty())
      this->parent_->send_pdu(this->address_.value(x...), std::span<const uint8_t>(pdu));
  }
};

}  // namespace esphome::modbus
