#pragma once

#include "bedjet_codec.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace bedjet {

// Forward declare BedJetHub
class BedJetHub;

class BedJetClient : public Parented<BedJetHub> {
 public:
  virtual void on_status(const BedjetStatusPacket *data) = 0;
  virtual void on_bedjet_state(bool is_ready) = 0;

 protected:
  friend BedJetHub;
  virtual std::string describe() = 0;
};

}  // namespace bedjet
}  // namespace esphome
