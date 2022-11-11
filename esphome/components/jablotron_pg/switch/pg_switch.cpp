#include "pg_switch.h"
#include "esphome/components/jablotron/jablotron_component.h"

namespace esphome::jablotron_pg {

void PGSwitch::register_parent(jablotron::JablotronComponent &parent) { parent.register_pg(this); }

void PGSwitch::write_state(bool state) {
  std::string command = state ? "PGON " : "PGOFF ";
  command += this->get_index_string();
  this->get_parent_jablotron()->queue_request_access_code(std::move(command), this->get_access_code());
}

void PGSwitch::set_state(bool state) { this->publish_state(state); }

}  // namespace esphome::jablotron_pg
