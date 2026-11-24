#pragma once

#include "types.h"

namespace esphome::canopen {

// SDO reply callbacks for the generated actions; an automation that isn't configured is nullptr
template<void (*ON_SUCCESS)(), void (*ON_ERROR)(const SDOResult &, const uint32_t &)>
void sdo_write_reply(void * /*context*/, const SDOReply &reply) {
  if (reply.ok()) {
    if constexpr (ON_SUCCESS != nullptr)
      ON_SUCCESS();
  } else if constexpr (ON_ERROR != nullptr) {
    ON_ERROR(reply.result, reply.abort_code);
  }
}

template<void (*ON_VALUE)(const uint32_t &), void (*ON_ERROR)(const SDOResult &, const uint32_t &)>
void sdo_read_reply(void * /*context*/, const SDOReply &reply) {
  if (reply.ok()) {
    if constexpr (ON_VALUE != nullptr)
      ON_VALUE(reply.value);
  } else if constexpr (ON_ERROR != nullptr) {
    ON_ERROR(reply.result, reply.abort_code);
  }
}

}  // namespace esphome::canopen
