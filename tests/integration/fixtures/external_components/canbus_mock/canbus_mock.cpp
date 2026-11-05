#include "canbus_mock.h"

namespace esphome::canbus_mock {

canbus::Error MockCanbus::send_message(canbus::CanFrame *frame) {
  if (this->count_ == QUEUE_SIZE)
    return canbus::ERROR_ALLTXBUSY;
  this->queue_[(this->head_ + this->count_) % QUEUE_SIZE] = *frame;
  this->count_++;
  return canbus::ERROR_OK;
}

canbus::Error MockCanbus::read_message(canbus::CanFrame *frame) {
  if (this->count_ == 0)
    return canbus::ERROR_NOMSG;
  *frame = this->queue_[this->head_];
  this->head_ = (this->head_ + 1) % QUEUE_SIZE;
  this->count_--;
  return canbus::ERROR_OK;
}

}  // namespace esphome::canbus_mock
