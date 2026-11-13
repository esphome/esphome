#include "esphome/components/vbus/vbus.h"

// Realtek SDK headers define SUCCESS before the controller header is parsed.
#ifndef SUCCESS
#define SUCCESS 0  // NOLINT: Reproduce the Realtek SDK macro at the header boundary.
#define VBUS_TEST_UNDEF_SUCCESS
#endif
#include "esphome/components/vbus/resol_deltasol_cs_plus.h"
#ifdef VBUS_TEST_UNDEF_SUCCESS
#undef SUCCESS
#undef VBUS_TEST_UNDEF_SUCCESS
#endif

#include <gtest/gtest.h>

#ifdef USE_VBUS_CS_PLUS_WRITE
namespace esphome::vbus::testing {
TEST(VBusSDKMacroTest, SettingWriteStatusesDoNotCollideWithSuccessMacro) {
  EXPECT_EQ(static_cast<uint8_t>(SettingWriteStatus::SETTING_WRITE_STATUS_SUCCESS), 3);
}
}  // namespace esphome::vbus::testing
#endif
