#pragma once

#include "esphome/core/progmem.h"

namespace esphome::dashboard_import {

ProgmemStr get_package_import_url();
void set_package_import_url(ProgmemStr url);

}  // namespace esphome::dashboard_import
