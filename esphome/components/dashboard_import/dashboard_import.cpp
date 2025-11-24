#include "dashboard_import.h"

namespace esphome {
namespace dashboard_import {

static const char *g_package_import_url = "";  // NOLINT

const char *get_package_import_url() { return g_package_import_url; }
void set_package_import_url(const char *url) { g_package_import_url = url; }

}  // namespace dashboard_import
}  // namespace esphome
