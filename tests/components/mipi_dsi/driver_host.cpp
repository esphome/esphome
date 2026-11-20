#if defined(USE_HOST) && defined(MIPI_TEST_IDF_MAJOR)
#define MIPI_DSI_HOST_TEST
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

// Exercise the complete production driver without changing unrelated host components.
#define USE_ESP32_VARIANT_ESP32P4
// NOLINTNEXTLINE(bugprone-suspicious-include): use the real driver with the host SDK boundary.
#include "esphome/components/mipi_dsi/mipi_dsi.cpp"
#undef USE_ESP32_VARIANT_ESP32P4
#undef MIPI_DSI_HOST_TEST
#endif
