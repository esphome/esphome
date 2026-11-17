#include "common.h"

namespace esphome::ufm01::testing {

TEST_F(UFM01Test, ActiveFramePublishesMeterIdOnce) {
  this->attach_diagnostic_text_sensors();
  auto frame = make_active_frame();
  this->mock_uart_.enqueue(frame);

  EXPECT_TRUE(this->ufm01_.process_active_stream());
  EXPECT_TRUE(this->ufm01_.meter_id_published());
  EXPECT_STREQ(this->meter_id_text_sensor_.state.c_str(), "2307140001");

  this->mock_uart_.enqueue(frame);
  EXPECT_TRUE(this->ufm01_.process_active_stream());
  EXPECT_STREQ(this->meter_id_text_sensor_.state.c_str(), "2307140001");
}

#ifdef USE_SENSOR
TEST_F(UFM01Test, PassiveFrameWithIdUsesDocumentedMeasurementOffsets) {
  this->attach_diagnostic_text_sensors();
  this->attach_flow_and_temperature_sensors();
  this->ufm01_.start_passive_read();
  this->expect_written(READ_SENSOR_DATA_WITH_ID);

  this->mock_uart_.enqueue(make_passive_with_id_frame());

  EXPECT_EQ(this->ufm01_.continue_passive_read(), ReadResult::READ_RESULT_SUCCESS);
  EXPECT_STREQ(this->meter_id_text_sensor_.state.c_str(), "2307140001");
  EXPECT_NEAR(this->flow_sensor_.get_state(), 0.01234f, 0.000001f);
  EXPECT_NEAR(this->temperature_sensor_.get_state(), 23.5f, 0.001f);
}
#endif

TEST_F(UFM01Test, InvalidPassiveFrameWithIdIsRejected) {
  this->attach_diagnostic_text_sensors();
  this->ufm01_.start_passive_read();
  auto frame = make_passive_with_id_frame();
  frame[37] ^= 0xFF;
  this->mock_uart_.enqueue(frame);

  EXPECT_EQ(this->ufm01_.continue_passive_read(), ReadResult::READ_RESULT_FAILURE);
  EXPECT_FALSE(this->ufm01_.meter_id_published());
}

TEST_F(UFM01Test, SoftwareVersionResponseDecoded) {
  this->attach_diagnostic_text_sensors();
  this->ufm01_.start_software_version_read();
  this->mock_uart_.enqueue(make_software_version_response());

  EXPECT_EQ(this->ufm01_.continue_software_version_read(), ReadResult::READ_RESULT_SUCCESS);
  EXPECT_TRUE(this->ufm01_.software_version_published());
  EXPECT_STREQ(this->software_version_text_sensor_.state.c_str(), "23380315");
}

TEST_F(UFM01Test, SoftwareVersionReadSkipsLeftoverStreamBytes) {
  this->attach_diagnostic_text_sensors();
  this->ufm01_.start_software_version_read();
  // Tail of an active frame that was still on the wire when the command was sent
  auto frame = make_active_frame();
  this->mock_uart_.enqueue(std::vector<uint8_t>(frame.begin() + 20, frame.end()));
  this->mock_uart_.enqueue(make_software_version_response());

  EXPECT_EQ(this->ufm01_.continue_software_version_read(), ReadResult::READ_RESULT_SUCCESS);
  EXPECT_STREQ(this->software_version_text_sensor_.state.c_str(), "23380315");
}

TEST_F(UFM01Test, InvalidSoftwareVersionBcdFails) {
  this->attach_diagnostic_text_sensors();
  this->ufm01_.start_software_version_read();
  auto response = make_software_version_response();
  response[2] = 0xFA;
  response[5] = checksum(response, 1, 5);
  this->mock_uart_.enqueue(response);

  EXPECT_EQ(this->ufm01_.continue_software_version_read(), ReadResult::READ_RESULT_FAILURE);
  EXPECT_FALSE(this->ufm01_.software_version_published());
}

TEST_F(UFM01Test, SoftwareVersionReadTimeoutFails) {
  this->attach_diagnostic_text_sensors();
  this->ufm01_.start_software_version_read();
  this->advance(1900);
  EXPECT_EQ(this->ufm01_.continue_software_version_read(), ReadResult::READ_RESULT_PENDING);

  this->advance(100);

  EXPECT_EQ(this->ufm01_.continue_software_version_read(), ReadResult::READ_RESULT_FAILURE);
  EXPECT_FALSE(this->ufm01_.software_version_published());
}

TEST_F(UFM01Test, StartupReadsSoftwareVersionBeforeActiveMode) {
  this->attach_diagnostic_text_sensors();
  this->ufm01_.begin_post_reset_wait();
  this->advance(3000);

  this->ufm01_.loop_startup();

  EXPECT_EQ(this->ufm01_.startup_phase(), StartupPhase::SOFTWARE_VERSION_WAIT_REPLY);
  this->expect_written(GET_SOFTWARE_VERSION);

  this->mock_uart_.enqueue(make_software_version_response());
  this->mock_uart_.written_data.clear();
  this->ufm01_.loop_startup();

  EXPECT_EQ(this->ufm01_.startup_phase(), StartupPhase::ACTIVE_WAIT_FRAME);
  this->expect_written(ACTIVE_MODE);
  EXPECT_TRUE(this->ufm01_.software_version_published());
  EXPECT_STREQ(this->software_version_text_sensor_.state.c_str(), "23380315");
}

TEST_F(UFM01Test, AlreadyActiveStartupRequestsSoftwareVersion) {
  this->attach_diagnostic_text_sensors();
  this->mock_uart_.enqueue(make_active_frame());
  this->ufm01_.init_wait_phase();

  this->ufm01_.loop_startup();

  EXPECT_EQ(this->ufm01_.operating_mode(), OperatingMode::ACTIVE_STREAM);
  EXPECT_TRUE(this->ufm01_.meter_id_published());
  EXPECT_TRUE(this->mock_uart_.written_data.empty());

  this->ufm01_.loop_active_stream();

  this->expect_written(GET_SOFTWARE_VERSION);
}

TEST_F(UFM01Test, ActiveStreamRetriesSoftwareVersionRead) {
  this->attach_diagnostic_text_sensors();
  this->ufm01_.set_operating_mode(OperatingMode::ACTIVE_STREAM);
  this->ufm01_.set_last_software_version_attempt_ms(this->now_);
  this->advance(29000);
  this->ufm01_.loop_active_stream();
  EXPECT_TRUE(this->mock_uart_.written_data.empty());

  this->advance(2000);
  this->ufm01_.loop_active_stream();

  this->expect_written(GET_SOFTWARE_VERSION);
}

TEST_F(UFM01Test, ActiveStreamGivesUpSoftwareVersionAfterRepeatedFailures) {
  this->attach_diagnostic_text_sensors();
  this->ufm01_.set_operating_mode(OperatingMode::ACTIVE_STREAM);
  for (int attempt = 0; attempt < 3; ++attempt) {
    this->ufm01_.loop_active_stream();
    this->expect_written(GET_SOFTWARE_VERSION);
    this->mock_uart_.written_data.clear();
    this->advance(2000);
    this->ufm01_.loop_active_stream();
    this->advance(30000);
  }

  this->ufm01_.loop_active_stream();

  EXPECT_TRUE(this->mock_uart_.written_data.empty());
  EXPECT_FALSE(this->ufm01_.software_version_published());
}

TEST_F(UFM01Test, InvalidMeterIdLeavesSensorUnknown) {
  this->attach_diagnostic_text_sensors();
  auto frame = make_active_frame();
  frame[2] = 0xAB;
  frame[30] = checksum(frame, 0, 30);
  this->mock_uart_.enqueue(frame);

  EXPECT_TRUE(this->ufm01_.process_active_stream());

  EXPECT_FALSE(this->ufm01_.meter_id_published());
  EXPECT_FALSE(this->meter_id_text_sensor_.has_state());
}

}  // namespace esphome::ufm01::testing
