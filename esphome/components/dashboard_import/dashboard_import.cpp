#include "dashboard_import.h"

namespace esphome::dashboard_import {

static const char EMPTY_URL[] PROGMEM = "";                                        // NOLINT
static ProgmemStr g_package_import_url = reinterpret_cast<ProgmemStr>(EMPTY_URL);  // NOLINT

ProgmemStr get_package_import_url() { return g_package_import_url; }
void set_package_import_url(ProgmemStr url) { g_package_import_url = url; }

}  // namespace esphome::dashboard_import
