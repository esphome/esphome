#include "espectre_select.h"

#ifdef USE_ESPECTRE

namespace esphome::espectre {

void TrafficModeSelect::control(size_t index) { this->parent_->request_traffic_generator_mode(this->option_at(index)); }

}  // namespace esphome::espectre

#endif  // USE_ESPECTRE
