#pragma once

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <algorithm> 
#include <assert.h>
#include <memory.h>
#include <vector>

#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif
#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

namespace esphome {
namespace wmbus {

  uint16_t packetSize(uint8_t t_L);
  uint16_t byteSize(uint16_t t_packetSize);

}
}