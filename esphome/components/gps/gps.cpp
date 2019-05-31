#include "gps.h"
#include "esphome/core/log.h"

namespace esphome {
namespace gps {

static const char *TAG = "gps";

TinyGPSPlus &GPSListener::get_tiny_gps() { return this->parent_->get_tiny_gps(); }

}  // namespace gps
}  // namespace esphome
