#pragma once

#include "esphome/core/helpers.h"
#include "esphome/components/lg_uart/lg_hub.h"

namespace esphome {
namespace lg_uart {

// Forward declare LGUartHub
class LGUartHub;

class LGUartClient : public Parented<LGUartHub> {
 public:
  virtual void on_reply_packet(uint8_t pkt[]) = 0;

 protected:
  friend LGUartHub;
  virtual std::string describe() = 0;
  virtual void set_encoding_base(int base) = 0;
  virtual int get_encoding_base() = 0;
};

}  // namespace lg_uart
}  // namespace esphome
