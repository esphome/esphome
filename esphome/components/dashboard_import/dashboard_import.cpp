#include "dashboard_import.h"

namespace esphome {
namespace dashboard_import {

static std::string g_import_config;  // NOLINT

std::string get_dashboard_import_config() {
  return g_import_config;
}
void set_dashboard_import_config(std::string config) {
  g_import_config = std::move(config);
}

}  // namespace dashboard_import
}  // namespace esphome
