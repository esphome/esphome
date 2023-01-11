#include "min_max_sliding_window.h"

namespace esphome {
namespace minmax {

void MinMaxSlidingWindow::add_tail(float value) {
    while (!this->max_deque_.empty() && value > this->max_deque_.back()) {
        this->max_deque_.pop_back();
    }
    max_deque_.push_back(value);

    while (!this->min_deque_.empty() && value < this->min_deque_.back()) {
        this->min_deque_.pop_back();
    }
    this->min_deque_.push_back(value);
}

void MinMaxSlidingWindow::remove_head(float value) {
    if (!this->max_deque_.empty()) {
        if (value == this->max_deque_.front()) {
            this->max_deque_.pop_front();
        }
    }

    if (!this->min_deque_.empty()) {
        if (value == this->min_deque_.front()) {
            this->min_deque_.pop_front();
        }
    }
}

}  // namespace minmax
}  // namespace esphome