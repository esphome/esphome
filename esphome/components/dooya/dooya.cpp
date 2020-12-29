#include "endstop_cover.h"
#include "esphome/core/log.h"

namespace esphome {
namespace dooya {

static const char *TAG = "dooya.cover";

using namespace esphome::cover;

CoverTraits DooyaCover::get_traits() {
  auto traits = CoverTraits();
  traits.set_supports_position(true);
  return traits;
}



}  // namespace dooya
}  // namespace esphome
