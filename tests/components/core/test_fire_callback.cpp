#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "esphome/core/base_automation.h"

namespace esphome::core::testing {

namespace {

std::vector<std::string> fired;

void on_value(const float &x) { fired.push_back("value " + std::to_string(static_cast<int>(x))); }
void on_text(const std::string &text, const int &count) { fired.push_back(text + " " + std::to_string(count)); }

// Bases with their own logic, compiled once; they only know fire()
class ValueBase : public FiringComponent<float> {
 public:
  explicit ValueBase(int threshold) : threshold_(threshold) {}
  void check(float x) {
    if (x > this->threshold_)
      this->fire(x);
  }

 protected:
  int threshold_;
};

class TextBase : public FiringComponent<const char *, int> {
 public:
  void announce() { this->fire("hello", 2); }
};

}  // namespace

TEST(FireCallback, FiresOnlyWhenTheBaseDecides) {
  fired.clear();
  FireCallback<on_value, ValueBase> callback(10);
  callback.check(5.0f);
  callback.check(12.0f);
  EXPECT_EQ(fired, std::vector<std::string>{"value 12"});
}

TEST(FireCallback, PassesSeveralArgumentsConverted) {
  fired.clear();
  FireCallback<on_text, TextBase> callback;
  callback.announce();
  EXPECT_EQ(fired, std::vector<std::string>{"hello 2"});
}

// The leaf adds no data to its base
static_assert(sizeof(FireCallback<on_value, ValueBase>) == sizeof(ValueBase));

}  // namespace esphome::core::testing
