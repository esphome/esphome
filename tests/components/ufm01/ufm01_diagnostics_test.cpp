#include "common.h"

namespace esphome::ufm01::testing {

#if defined(USE_UFM01_METER_ID) && defined(USE_UFM01_SOFTWARE_VERSION)
#ifdef USE_UFM01_METER_ID
TEST_F(UFM01Test, ActiveFramePublishesMeterIdOnce) {
  this->attach_diagnostic_text_sensors();
  auto frame = make_active_frame();
  this->mock_uart_.enqueue(std::vector<uint8_t>(frame.begin(), frame.end()));

  EXPECT_TRUE(this->ufm01_.process_active_stream());
  EXPECT_TRUE(this->ufm01_.meter_id_published());
  EXPECT_STREQ(this->meter_id_text_sensor_.state.c_str(), "2307140001");

  this->mock_uart_.enqueue(std::vector<uint8_t>(frame.begin(), frame.end()));
  EXPECT_TRUE(this->ufm01_.process_active_stream());
  EXPECT_STREQ(this->meter_id_text_sensor_.state.c_str(), "2307140001");
}
#endif

#ifdef USE_SENSOR
#ifdef USE_UFM01_METER_ID
TEST_F(UFM01Test, PassiveFrameWithIdUsesDocumentedMeasurementOffsets) {
  this->attach_diagnostic_text_sensors();
  this->attach_flow_and_temperature_sensors();
  this->ufm01_.start_passive_read();
  ASSERT_EQ(this->mock_uart_.written_data.size(), 7u);
  EXPECT_EQ(this->mock_uart_.written_data[4], 0xCB);

  auto frame = make_passive_with_id_frame();
  this->mock_uart_.enqueue(std::vector<uint8_t>(frame.begin(), frame.end()));

  EXPECT_EQ(this->ufm01_.continue_passive_read(), PassiveReadResult::PASSIVE_READ_RESULT_SUCCESS);
  EXPECT_STREQ(this->meter_id_text_sensor_.state.c_str(), "2307140001");
  EXPECT_NEAR(this->flow_sensor_.get_state(), 0.01234f, 0.000001f);
  EXPECT_NEAR(this->temperature_sensor_.get_state(), 23.5f, 0.001f);
}
#endif
#endif

#ifdef USE_UFM01_METER_ID
TEST_F(UFM01Test, InvalidPassiveFrameWithIdIsRejected) {
  this->attach_diagnostic_text_sensors();
  this->ufm01_.start_passive_read();
  auto frame = make_passive_with_id_frame();
  frame[37] ^= 0xFF;
  this->mock_uart_.enqueue(std::vector<uint8_t>(frame.begin(), frame.end()));

  EXPECT_EQ(this->ufm01_.continue_passive_read(), PassiveReadResult::PASSIVE_READ_RESULT_FAILURE);
  EXPECT_FALSE(this->ufm01_.meter_id_published());
}
#endif

#ifdef USE_UFM01_SOFTWARE_VERSION
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

TEST_F(UFM01Test, InvalidSoftwareVersionBcdFails) {
  this->attach_diagnostic_text_sensors();
  this->ufm01_.start_software_version_read();
  auto response = make_software_version_response();
  response[2] = 0xFA;
  response[5] = response[1] + response[2] + response[3] + response[4];
  this->mock_uart_.enqueue(std::vector<uint8_t>(response.begin(), response.end()));

  EXPECT_EQ(this->ufm01_.continue_software_version_read(),
            SoftwareVersionReadResult::SOFTWARE_VERSION_READ_RESULT_FAILURE);
  EXPECT_FALSE(this->ufm01_.software_version_published());
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

TEST_F(UFM01Test, AlreadyActiveStartupRequestsSoftwareVersion) {
  this->attach_diagnostic_text_sensors();
  auto frame = make_active_frame();
  this->mock_uart_.enqueue(std::vector<uint8_t>(frame.begin(), frame.end()));
  this->ufm01_.init_wait_phase();

  this->ufm01_.loop_startup();

  EXPECT_EQ(this->ufm01_.operating_mode(), OperatingMode::STARTUP);
  EXPECT_EQ(this->ufm01_.startup_phase(), StartupPhase::SOFTWARE_VERSION_WAIT_REPLY);
  ASSERT_EQ(this->mock_uart_.written_data.size(), 7u);
  EXPECT_EQ(this->mock_uart_.written_data[3], 0x5E);
#ifdef USE_UFM01_METER_ID
  EXPECT_TRUE(this->ufm01_.meter_id_published());
#endif
}

TEST_F(UFM01Test, ActiveStreamRetriesSoftwareVersionRead) {
  this->attach_diagnostic_text_sensors();
  this->ufm01_.set_operating_mode(OperatingMode::ACTIVE_STREAM);
  this->ufm01_.set_last_software_version_attempt_ms(millis() - 31000);

  this->ufm01_.loop_active_stream();

  ASSERT_EQ(this->mock_uart_.written_data.size(), 7u);
  EXPECT_EQ(this->mock_uart_.written_data[3], 0x5E);
}
#endif
#endif

}  // namespace esphome::ufm01::testing
