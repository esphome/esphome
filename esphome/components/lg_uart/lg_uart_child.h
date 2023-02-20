#pragma once

#include "esphome/core/helpers.h"

namespace esphome {
namespace lg_uart {

// Forward declare LGUartHub
class LGUartHub;

class LGUartClient : public Parented<LGUartHub> {
 public:
  virtual void on_reply_packet(char cmd_str) = 0;

 protected:
  friend LGUartHub;
  virtual std::string describe() = 0;
};

}  // namespace lg_uart
}  // namespace esphome
