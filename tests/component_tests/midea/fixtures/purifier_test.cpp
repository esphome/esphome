#include <cstdio>
#include <cstring>
#include "esphome/core/defines.h"
#include "esphome/components/midea/air_conditioner.h"
#include "esphome/components/switch/switch.h"

#include "esphome/components/midea/purifier_switch.h"
using Child = esphome::midea::ac::PurifierSwitch;

// Keep unrelated climate conversion methods out of this bridge-only host test.
namespace esphome::midea::ac {
void AirConditioner::dump_config() {}
void AirConditioner::on_status_change() {}
void AirConditioner::control(const ClimateCall &) {}
ClimateTraits AirConditioner::traits() { return {}; }
}  // namespace esphome::midea::ac

int main(int argc, char **argv) {
  esphome::midea::ac::AirConditioner parent;
  Child child(&parent, 30000);
  auto &transport = *dudanov::midea::ac::AirConditioner::instance;
  parent.set_purifier_switch(&child);
  if (argc != 2)
    return 2;
  if (strcmp(argv[1], "feedback") == 0) {
    parent.setup();
    parent.loop();
    parent.dump_config();
    if (transport.requests != 0 || child.has_state()) {
      puts("startup must neither request nor publish a purifier state");
      return 1;
    }
    transport.report(false);
    if (!child.has_state() || child.state) {
      puts("first OFF feedback did not reach the child switch");
      return 1;
    }
    transport.report(true);
    if (!child.state) {
      puts("handset ON feedback did not reach the child switch");
      return 1;
    }
    transport.report(false);
    if (child.state || transport.requests != 0) {
      puts("feedback must not issue writes");
      return 1;
    }
    puts("feedback passed");
    return 0;
  }
  if (strcmp(argv[1], "accepted") == 0 || strcmp(argv[1], "rejected") == 0) {
    transport.accepted = strcmp(argv[1], "accepted") == 0;
    child.turn_on();
    if (transport.requests != 1 || !transport.requested || transport.max_age != 30000) {
      puts("explicit ON did not forward the configured freshness bound");
      return 1;
    }
    if (child.has_state()) {
      puts("request acceptance is not confirmed state");
      return 1;
    }
    transport.report(false);
    child.turn_on();
    if (child.state || child.publications != 1) {
      puts("requests must not publish optimistically");
      return 1;
    }
    transport.report(true);
    child.turn_off();
    if (transport.requests != 3 || transport.requested || !child.state || child.publications != 2) {
      puts("explicit OFF must wait for feedback");
      return 1;
    }
    transport.report(false);
    if (child.state) {
      puts("confirmed OFF was not published");
      return 1;
    }
    puts("commands passed");
    return 0;
  }
  return 2;
}
