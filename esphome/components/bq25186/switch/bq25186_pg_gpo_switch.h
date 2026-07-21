#pragma once

#include "esphome/components/switch/switch.h"
#include "../bq25186.h"

namespace esphome::bq25186 {

#ifdef USE_BQ25186_PG_GPO_SWITCH
class BQ25186PgGpoSwitch : public switch_::Switch, public Parented<BQ25186Component> {
 protected:
  void write_state(bool state) override;
};
#endif

}  // namespace esphome::bq25186
