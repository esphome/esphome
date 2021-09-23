#include "dashboard_import.h"

namespace esphome {
namespace dashboard_import {

static std::string g_package_import_url;  // NOLINT

std::string get_package_import_url() { return g_package_import_url; }
void set_package_import_url(std::string url) { g_package_import_url = std::move(url); }

}  // namespace dashboard_import
}  // namespace esphome
