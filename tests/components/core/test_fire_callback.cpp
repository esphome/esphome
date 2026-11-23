#include <gtest/gtest.h>
#include <string>
#include <type_traits>
#include <vector>

#include "esphome/core/automation.h"
#include "esphome/core/string_ref.h"

namespace esphome::core::testing {

namespace {

std::vector<float> values;       // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
std::vector<std::string> texts;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
std::vector<int> counts;         // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

void on_value(const float &x) { values.push_back(x); }
void on_text(const std::string &text, const int &count) {
  texts.push_back(text);
  counts.push_back(count);
}

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
  values.clear();
  FireCallback<on_value, ValueBase> callback(10);
  callback.check(5.0f);
  callback.check(12.0f);
  EXPECT_EQ(values, std::vector<float>{12.0f});
}

TEST(FireCallback, PassesSeveralArgumentsConverted) {
  texts.clear();
  counts.clear();
  FireCallback<on_text, TextBase> callback;
  callback.announce();
  EXPECT_EQ(texts, std::vector<std::string>{"hello"});
  EXPECT_EQ(counts, std::vector<int>{2});
}

// A buffer passed by reference and small value types are accepted
static_assert(std::is_abstract_v<FiringComponent<const std::vector<uint8_t> &, StringRef, float>>);

// The leaf adds no data to its base
static_assert(sizeof(FireCallback<on_value, ValueBase>) == sizeof(ValueBase));

}  // namespace esphome::core::testing
