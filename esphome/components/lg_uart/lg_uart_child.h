#pragma once

#include "esphome/core/helpers.h"

namespace esphome {
namespace lg_uart {

// Forward declare LGUartHub
class LGUartHub;

class LGUartClient : public Parented<LGUartHub> {
 public:
 protected:
  friend LGUartHub;
};

}  // namespace lg_uart
}  // namespace esphome
