#pragma once

#include "esphome/core/defines.h"
#ifdef USE_BQ25186_PG_GPO

#include "bq25186.h"
#include "esphome/components/switch/switch.h"

namespace esphome::bq25186 {

class BQ25186PgGpoSwitch final : public switch_::Switch, public Parented<BQ25186Component> {
 protected:
  void write_state(bool state) override {
    if (this->parent_->write_pg_gpo_level(state))
      this->publish_state(state);
  }
};

}  // namespace esphome::bq25186

#endif  // USE_BQ25186_PG_GPO
