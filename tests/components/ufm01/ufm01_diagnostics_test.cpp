#include "common.h"

namespace esphome::ufm01::testing {

#ifdef USE_TEXT_SENSOR
TEST_F(UFM01Test, ActiveFramePublishesDeviceIdOnce) {
  this->attach_diagnostic_text_sensors();
  auto frame = make_active_frame();
  this->mock_uart_.enqueue(std::vector<uint8_t>(frame.begin(), frame.end()));

  EXPECT_TRUE(this->ufm01_.process_active_stream());
  EXPECT_TRUE(this->ufm01_.device_id_published());
  EXPECT_STREQ(this->device_id_text_sensor_.state.c_str(), "2307140001");

  this->mock_uart_.enqueue(std::vector<uint8_t>(frame.begin(), frame.end()));
  EXPECT_TRUE(this->ufm01_.process_active_stream());
  EXPECT_STREQ(this->device_id_text_sensor_.state.c_str(), "2307140001");
}

TEST_F(UFM01Test, SoftwareVersionResponseDecoded) {
  this->attach_diagnostic_text_sensors();
  this->ufm01_.start_software_version_read();
  auto response = make_software_version_response();
  this->mock_uart_.enqueue(std::vector<uint8_t>(response.begin(), response.end()));

  EXPECT_EQ(this->ufm01_.continue_software_version_read(),
            SoftwareVersionReadResult::SOFTWARE_VERSION_READ_RESULT_SUCCESS);
  EXPECT_TRUE(this->ufm01_.software_version_published());
  EXPECT_STREQ(this->software_version_text_sensor_.state.c_str(), "23380315");
}

TEST_F(UFM01Test, SoftwareVersionReadTimeoutFails) {
  this->attach_diagnostic_text_sensors();
  this->ufm01_.start_software_version_read();
  this->ufm01_.set_software_version_start_ms(millis() - 2000);

  EXPECT_EQ(this->ufm01_.continue_software_version_read(),
            SoftwareVersionReadResult::SOFTWARE_VERSION_READ_RESULT_FAILURE);
  EXPECT_FALSE(this->ufm01_.software_version_published());
}

TEST_F(UFM01Test, StartupReadsSoftwareVersionBeforeActiveMode) {
  this->attach_diagnostic_text_sensors();
  this->ufm01_.prepare_post_reset_wait_phase();

  this->ufm01_.loop_startup();

  EXPECT_EQ(this->ufm01_.startup_phase(), StartupPhase::SOFTWARE_VERSION_WAIT_REPLY);
  ASSERT_EQ(this->mock_uart_.written_data.size(), 7u);
  EXPECT_EQ(this->mock_uart_.written_data[3], 0x5E);

  auto response = make_software_version_response();
  this->mock_uart_.enqueue(std::vector<uint8_t>(response.begin(), response.end()));
  this->mock_uart_.written_data.clear();
  this->ufm01_.loop_startup();

  EXPECT_EQ(this->ufm01_.startup_phase(), StartupPhase::ACTIVE_WAIT_FRAME);
  ASSERT_EQ(this->mock_uart_.written_data.size(), 7u);
  EXPECT_EQ(this->mock_uart_.written_data[3], 0x5C);  // set active mode
  EXPECT_TRUE(this->ufm01_.software_version_published());
  EXPECT_STREQ(this->software_version_text_sensor_.state.c_str(), "23380315");
}
#endif

}  // namespace esphome::ufm01::testing
