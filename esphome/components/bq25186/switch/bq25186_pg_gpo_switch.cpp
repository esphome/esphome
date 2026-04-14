#include "bq25186_pg_gpo_switch.h"

namespace esphome::bq25186 {

#ifdef USE_BQ25186_PG_GPO_SWITCH
void BQ25186PgGpoSwitch::write_state(bool state) {
  if (this->parent_->write_pg_gpo_level(state)) {
    this->publish_state(state);
  }
}
#endif

}  // namespace esphome::bq25186
