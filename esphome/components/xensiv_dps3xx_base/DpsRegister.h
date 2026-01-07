#pragma once

#ifndef DPSREGISTER_H_INCLUDED
#define DPSREGISTER_H_INCLUDED

#include <stdint.h>

namespace esphome {
namespace xensiv_dps3xx_base {

typedef struct {
  uint8_t regAddress;
  uint8_t mask;
  uint8_t shift;
} RegMask_t;

typedef struct {
  uint8_t regAddress;
  uint8_t length;
} RegBlock_t;

}  // namespace xensiv_dps3xx_base
}  // namespace esphome

#endif
