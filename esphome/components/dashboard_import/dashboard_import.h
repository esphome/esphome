#pragma once

#include <string>

namespace esphome {
namespace dashboard_import {

std::string get_dashboard_import_config();
void set_dashboard_import_config(std::string config);

}  // namespace dashboard_import
}  // namespace esphome
