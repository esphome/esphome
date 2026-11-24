#pragma once

#include <type_traits>

#include "types.h"

namespace esphome::canopen {

// SDO reply callbacks for the generated actions; an automation that isn't configured is nullptr
template<auto ON_SUCCESS, auto ON_ERROR> void sdo_write_reply(void * /*context*/, const SDOReply &reply) {
  if (reply.ok()) {
    if constexpr (!std::is_null_pointer_v<decltype(ON_SUCCESS)>)
      ON_SUCCESS();
  } else if constexpr (!std::is_null_pointer_v<decltype(ON_ERROR)>) {
    ON_ERROR(reply.result, reply.abort_code);
  }
}

template<auto ON_VALUE, auto ON_ERROR> void sdo_read_reply(void * /*context*/, const SDOReply &reply) {
  if (reply.ok()) {
    if constexpr (!std::is_null_pointer_v<decltype(ON_VALUE)>)
      ON_VALUE(reply.value);
  } else if constexpr (!std::is_null_pointer_v<decltype(ON_ERROR)>) {
    ON_ERROR(reply.result, reply.abort_code);
  }
}

}  // namespace esphome::canopen
