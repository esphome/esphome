#pragma once
#include <utility>
#include "Appliance/ApplianceBase.h"
namespace dudanov::midea::ac {
// A controlled transport boundary: protocol validation is tested in MideaUART.
class AirConditioner : public ApplianceBase {
 public:
  AirConditioner() { instance = this; }
  inline static AirConditioner *instance{nullptr};
  std::function<void(bool)> feedback;
  bool accepted{true};
  bool requested{false};
  int requests{0};
  uint32_t max_age{0};
  void addOnPurifierStatusCallback(std::function<void(bool)> callback) { feedback = std::move(callback); }
  bool setPurifier(bool state, uint32_t age) {
    ++requests;
    requested = state;
    max_age = age;
    return accepted;
  }
  void report(bool state) {
    if (feedback)
      feedback(state);
  }
  void setPowerState(bool) {}
};
}  // namespace dudanov::midea::ac
