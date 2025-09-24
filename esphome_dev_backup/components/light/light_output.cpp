#include "light_output.h"
#include "transformers.h"

namespace esphome {
namespace light {

std::unique_ptr<LightTransformer> LightOutput::create_default_transition() {
  return make_unique<LightTransitionTransformer>();
}

}  // namespace light
}  // namespace esphome
