#include "alpha3_number.h"

#if defined(USE_ESP32) && defined(USE_NUMBER)
namespace esphome::alpha3 {

void Alpha3Number::control(float value) { this->parent_->request_setpoint(this->type_, value); }

}  // namespace esphome::alpha3
#endif
