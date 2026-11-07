#include "interval.h"

namespace esphome::interval {

void IntervalBase::start_after_delay_(uint32_t startup_delay) {
  this->stop_poller();
  this->set_timeout(startup_delay, [this] { this->start_poller(); });
}

}  // namespace esphome::interval
