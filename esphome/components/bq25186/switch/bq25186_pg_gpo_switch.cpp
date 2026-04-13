#include "bq25186_pg_gpo_switch.h"

namespace esphome::bq25186 {

void BQ25186PgGpoSwitch::write_state(bool state) {
  if (this->parent_->write_pg_gpo_level(state)) {
    this->publish_state(state);
  }
}

}  // namespace esphome::bq25186
