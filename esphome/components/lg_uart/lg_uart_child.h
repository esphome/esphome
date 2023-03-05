#pragma once

#include "esphome/core/helpers.h"
#include "esphome/components/lg_uart/lg_hub.h"

namespace esphome {
namespace lg_uart {

// Forward declare LGUartHub
class LGUartHub;

class LGUartClient : public Parented<LGUartHub> {
 public:
  virtual void on_reply_packet(std::vector<uint8_t> *pkt) = 0;
  void set_encoding_base(int base) { this->encoding_base_ = base; };
  int get_encoding_base() { return this->encoding_base_; };

 protected:
  friend LGUartHub;

  // Two chars + termination
  char cmd_str_[3] = {0};

  // Keep track of base 16 or base 10 decoding on reply packets
  int encoding_base_ = 10;

  virtual std::string describe() = 0;
};

}  // namespace lg_uart
}  // namespace esphome
