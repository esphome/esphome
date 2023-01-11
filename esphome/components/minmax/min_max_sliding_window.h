#include <deque>

namespace esphome {
namespace minmax {

class MinMaxSlidingWindow {
 public:
  void add_tail(float value);
  void remove_head(float value);

  float get_minimum() const { return min_deque_.front(); };
  float get_maximum() const { return max_deque_.front(); };

 protected:
  std::deque<float> min_deque_;
  std::deque<float> max_deque_;
};

}  // namespace minmax
}  // namespace esphome
