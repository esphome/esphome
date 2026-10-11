#pragma once
#include <functional>
#include "Helpers/Platform.h"
namespace dudanov::midea {
enum AutoconfStatus { AUTOCONF_PROGRESS, AUTOCONF_OK };
class ApplianceBase {
 public:
  template<typename F> static void setLogger(F) {}
  void setStream(Stream *) {}
  void addOnStateCallback(std::function<void()>) {}
  void setPeriod(uint32_t) {}
  void setTimeout(uint32_t) {}
  void setNumAttempts(uint32_t) {}
  void setBeeper(bool) {}
  void setAutoconf(bool) {}
  void setup() {}
  void loop() {}
  AutoconfStatus getAutoconfStatus() const { return AUTOCONF_OK; }
};
}  // namespace dudanov::midea
