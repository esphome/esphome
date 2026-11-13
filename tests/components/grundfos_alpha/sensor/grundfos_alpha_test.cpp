#include <gtest/gtest.h>

#include <cmath>

#include "../common.h"

namespace esphome::grundfos_alpha::testing {

TEST_F(GrundfosAlphaTest, IdentifiesThenChainsBothObjects) {
  this->read_all_();
  EXPECT_FLOAT_EQ(this->flow_.state, 3600.0f / 4096.0f);  // m³/h
  EXPECT_FLOAT_EQ(this->head_.state, 4.0f);               // 39216 Pa at 9804 Pa/m
  EXPECT_FLOAT_EQ(this->medium_temperature_.state, 25.5f);
  EXPECT_FLOAT_EQ(this->voltage_.state, 230.0f);
  EXPECT_FLOAT_EQ(this->current_.state, 0.125f);
  EXPECT_FLOAT_EQ(this->power_.state, 19.5f);  // dc_power_consumption, not power_consumption
  EXPECT_FLOAT_EQ(this->speed_.state, 3000.0f);
  EXPECT_TRUE(this->next_request_().empty());
}

TEST_F(GrundfosAlphaTest, IdentifiesOnlyOnce) {
  this->read_all_();
  this->pump_.update();
  EXPECT_EQ(this->next_request_(), FRAME_PROCESS);
}

TEST_F(GrundfosAlphaTest, ReadsOnlyObjectsWithSensors) {
  this->pump_.set_flow_sensor(nullptr);
  this->pump_.set_head_sensor(nullptr);
  this->pump_.set_medium_temperature_sensor(nullptr);
  this->pump_.update();
  ASSERT_EQ(this->next_request_(), FRAME_IDENTITY);
  this->reply_(REPLY_IDENTITY_ALPHA3);
  ASSERT_EQ(this->next_request_(), FRAME_MOTOR);
  this->reply_(REPLY_MOTOR);
  EXPECT_FLOAT_EQ(this->power_.state, 19.5f);
  EXPECT_TRUE(this->next_request_().empty());
}

TEST_F(GrundfosAlphaTest, UnsupportedModelReadsNothing) {
  this->pump_.update();
  ASSERT_EQ(this->next_request_(), FRAME_IDENTITY);
  this->reply_(REPLY_IDENTITY_ALPHA15);
  EXPECT_TRUE(this->next_request_().empty());
  EXPECT_TRUE(this->pump_.status_has_warning());
  this->pump_.update();
  EXPECT_TRUE(this->next_request_().empty());
  EXPECT_FALSE(this->flow_.has_state());
  EXPECT_FALSE(this->power_.has_state());
}

TEST_F(GrundfosAlphaTest, IdentityErrorIsRetried) {
  this->pump_.update();
  ASSERT_EQ(this->next_request_(), FRAME_IDENTITY);
  this->reply_(REPLY_IDENTITY_UNKNOWN_ID);
  EXPECT_TRUE(this->next_request_().empty());
  EXPECT_TRUE(this->pump_.status_has_warning());
  this->pump_.update();
  EXPECT_EQ(this->next_request_(), FRAME_IDENTITY);
}

TEST_F(GrundfosAlphaTest, ShortIdentityIsRetriedAndWarningClearsOnSuccess) {
  this->pump_.update();
  ASSERT_EQ(this->next_request_(), FRAME_IDENTITY);
  this->reply_(REPLY_IDENTITY_SHORT);
  EXPECT_TRUE(this->next_request_().empty());
  EXPECT_TRUE(this->pump_.status_has_warning());
  this->pump_.update();
  ASSERT_EQ(this->next_request_(), FRAME_IDENTITY);
  this->reply_(REPLY_IDENTITY_SHORT);
  EXPECT_TRUE(this->next_request_().empty());
  this->pump_.update();
  ASSERT_EQ(this->next_request_(), FRAME_IDENTITY);
  this->reply_(REPLY_IDENTITY_ALPHA3);
  EXPECT_FALSE(this->pump_.status_has_warning());
  EXPECT_EQ(this->next_request_(), FRAME_PROCESS);
}

TEST_F(GrundfosAlphaTest, ResponseWithoutOpenRequestIsIgnored) {
  // Before the identity is known
  this->pump_.on_response({genibus::GenibusError::GENIBUS_ERROR_NONE, {}});
  EXPECT_TRUE(this->next_request_().empty());
  // After the chain has ended
  this->read_all_();
  ASSERT_TRUE(this->next_request_().empty());
  this->pump_.on_response({genibus::GenibusError::GENIBUS_ERROR_NONE, {}});
  EXPECT_TRUE(this->next_request_().empty());
  EXPECT_FLOAT_EQ(this->power_.state, 19.5f);
  EXPECT_FALSE(this->pump_.status_has_warning());
}

TEST_F(GrundfosAlphaTest, UnexpectedVersionIsNotDecoded) {
  this->pump_.update();
  ASSERT_EQ(this->next_request_(), FRAME_IDENTITY);
  this->reply_(REPLY_IDENTITY_ALPHA3);
  ASSERT_EQ(this->next_request_(), FRAME_PROCESS);
  this->reply_(REPLY_PROCESS);
  ASSERT_EQ(this->next_request_(), FRAME_MOTOR);
  this->reply_(REPLY_MOTOR_V2);
  EXPECT_FLOAT_EQ(this->head_.state, 4.0f);
  EXPECT_TRUE(this->power_.has_state());
  EXPECT_TRUE(std::isnan(this->power_.state));
  EXPECT_TRUE(std::isnan(this->voltage_.state));
  EXPECT_TRUE(std::isnan(this->speed_.state));
}

TEST_F(GrundfosAlphaTest, ErrorAckGivesNanAndChainGoesOn) {
  this->pump_.update();
  ASSERT_EQ(this->next_request_(), FRAME_IDENTITY);
  this->reply_(REPLY_IDENTITY_ALPHA3);
  ASSERT_EQ(this->next_request_(), FRAME_PROCESS);
  this->reply_(REPLY_PROCESS_ID_UNKNOWN);
  EXPECT_TRUE(std::isnan(this->flow_.state));
  EXPECT_TRUE(std::isnan(this->head_.state));
  ASSERT_EQ(this->next_request_(), FRAME_MOTOR);
  this->reply_(REPLY_MOTOR);
  EXPECT_FLOAT_EQ(this->power_.state, 19.5f);
}

TEST_F(GrundfosAlphaTest, MissingObjectGivesNanAndIsSkippedUntilReconnect) {
  this->pump_.update();
  ASSERT_EQ(this->next_request_(), FRAME_IDENTITY);
  this->reply_(REPLY_IDENTITY_ALPHA3);
  ASSERT_EQ(this->next_request_(), FRAME_PROCESS);
  this->reply_(REPLY_OBJECT_MISSING);
  EXPECT_TRUE(this->flow_.has_state());
  EXPECT_TRUE(std::isnan(this->flow_.state));
  EXPECT_TRUE(std::isnan(this->head_.state));
  EXPECT_TRUE(std::isnan(this->medium_temperature_.state));
  ASSERT_EQ(this->next_request_(), FRAME_MOTOR);
  this->reply_(REPLY_MOTOR);
  EXPECT_FLOAT_EQ(this->voltage_.state, 230.0f);
  EXPECT_FALSE(this->pump_.status_has_warning());
  // The next update asks only for the object the pump has
  this->pump_.update();
  ASSERT_EQ(this->next_request_(), FRAME_MOTOR);
  this->reply_(REPLY_MOTOR);
  EXPECT_TRUE(this->next_request_().empty());
  // A new connection may be to a pump with other firmware
  this->bus_.lose_connection();
  this->bus_.establish_connection();
  EXPECT_EQ(this->next_request_(), FRAME_PROCESS);
}

TEST_F(GrundfosAlphaTest, TimeoutGivesNanAndChainGoesOn) {
  this->read_all_();
  this->pump_.update();
  ASSERT_EQ(this->next_request_(), FRAME_PROCESS);
  this->run_(TIMEOUT_MS);
  EXPECT_TRUE(std::isnan(this->flow_.state));
  EXPECT_FLOAT_EQ(this->power_.state, 19.5f);
  this->now_ += 250;  // quiet period after the timeout
  EXPECT_EQ(this->next_request_(), FRAME_MOTOR);
}

TEST_F(GrundfosAlphaTest, SkipsUpdateWhileBusy) {
  this->pump_.update();
  ASSERT_EQ(this->next_request_(), FRAME_IDENTITY);
  this->pump_.update();
  EXPECT_TRUE(this->next_request_().empty());
}

TEST_F(GrundfosAlphaTest, NotConnectedSkipsQuietly) {
  this->bus_.connected = false;
  this->pump_.update();
  EXPECT_TRUE(this->next_request_().empty());
  EXPECT_FALSE(this->flow_.has_state());
  EXPECT_FALSE(this->bus_.status_has_warning());
  EXPECT_FALSE(this->pump_.status_has_warning());
}

TEST_F(GrundfosAlphaTest, DisconnectBetweenPollsGivesNanAtOnce) {
  this->read_all_();
  this->bus_.lose_connection();
  // No update() needed: the values turn unknown when the connection drops
  for (auto *sensor : {&this->flow_, &this->head_, &this->medium_temperature_, &this->voltage_, &this->current_,
                       &this->power_, &this->speed_})
    EXPECT_TRUE(std::isnan(sensor->state));
  EXPECT_FALSE(this->pump_.status_has_warning());
}

TEST_F(GrundfosAlphaTest, DisconnectDuringRequestGivesNanAndRecovers) {
  this->read_all_();
  this->pump_.update();
  ASSERT_EQ(this->next_request_(), FRAME_PROCESS);
  this->bus_.lose_connection();
  EXPECT_TRUE(std::isnan(this->flow_.state));
  EXPECT_TRUE(std::isnan(this->power_.state));
  this->bus_.connected = true;
  this->pump_.update();
  ASSERT_EQ(this->next_request_(), FRAME_PROCESS);  // identity stays known
  this->reply_(REPLY_PROCESS);
  EXPECT_FLOAT_EQ(this->head_.state, 4.0f);
}

TEST_F(GrundfosAlphaTest, FirstConnectionIdentifiesAtOnce) {
  this->bus_.connected = false;
  this->bus_.establish_connection();
  EXPECT_EQ(this->next_request_(), FRAME_IDENTITY);
}

TEST_F(GrundfosAlphaTest, ReconnectReadsAtOnce) {
  this->read_all_();
  this->bus_.lose_connection();
  EXPECT_TRUE(std::isnan(this->flow_.state));
  this->bus_.establish_connection();
  ASSERT_EQ(this->next_request_(), FRAME_PROCESS);  // identity stays known
  this->reply_(REPLY_PROCESS);
  EXPECT_FLOAT_EQ(this->head_.state, 4.0f);
}

}  // namespace esphome::grundfos_alpha::testing
