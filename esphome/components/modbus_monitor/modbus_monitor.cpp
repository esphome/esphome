#include "modbus_monitor.h"
#include "esphome/components/modbus/modbus_definitions.h"
#include "esphome/components/modbus/modbus_helpers.h"

#include <algorithm>

namespace esphome::modbus_monitor {

void RegisterDecoder::on_response(uint8_t address, std::span<const uint8_t> request,
                                  std::span<const uint8_t> response) {
  const auto start = modbus::helpers::client_pdu_start_address(request);
  if (!start.has_value() || response.empty())
    return;
  const uint16_t start_address = *start;

  // The register values, two bytes each from start_address: a read carries them in its reply, a write in
  // its request. Coils pack bits, so neither kind of coil exchange is a register an item can index.
  std::span<const uint8_t> payload;
  switch (static_cast<modbus::FunctionCode>(request[0])) {
    case modbus::FunctionCode::READ_HOLDING_REGISTERS:
    case modbus::FunctionCode::READ_INPUT_REGISTERS:
    case modbus::FunctionCode::READ_WRITE_MULTIPLE_REGISTERS:
      // An exception reply carries an error code where the registers would be.
      if (modbus::helpers::is_function_code_exception(response[0]))
        return;
      payload = modbus::helpers::server_pdu_payload(response);
      // A server hub pairs a peer's reply by address and function code only, so a late reply to an earlier
      // read can arrive paired with a newer one. Its length is the one thing that can give it away: the
      // reply must carry exactly the registers requested (bytes 3-4; for 0x17, the read quantity).
      if (request.size() < 5 || payload.size() != size_t{encode_uint16(request[3], request[4])} * 2)
        return;
      break;
    case modbus::FunctionCode::WRITE_SINGLE_REGISTER:
      // The device confirms by echoing the request. Without that echo the value was only asked for.
      if (request.size() != 5 || !std::ranges::equal(request, response))
        return;
      payload = request.subspan(3, 2);
      break;
    case modbus::FunctionCode::WRITE_MULTIPLE_REGISTERS:
      // The device confirms by repeating the start address and count: the request's first five bytes.
      if (request.size() < 6 || response.size() != 5 || !std::ranges::equal(request.first(5), response))
        return;
      payload = request.subspan(6);  // after the byte count
      break;
    default:
      return;
  }

  for (MonitorItem *item : this->items_) {
    if (item->address != address || item->reg < start_address)
      continue;
    const size_t index = static_cast<size_t>(item->reg - start_address) * 2;
    const size_t size = static_cast<size_t>(item->register_count) * 2;
    if (index + size > payload.size())
      continue;  // this exchange did not reach all of that item's registers
    item->parse_and_publish(payload.subspan(index, size));
  }
}

}  // namespace esphome::modbus_monitor
