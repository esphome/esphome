#include "../common.h"

#include <cmath>

namespace esphome::ld6002b::testing {

namespace {

std::vector<uint8_t> area_report(uint16_t type, uint8_t area, const std::array<float, 6> &bounds) {
  std::vector<uint8_t> p;
  for (uint8_t i = 0; i < AREA_COUNT; i++) {
    for (float v : bounds)
      put_f32(p, i == area ? v : 0.0f);
  }
  return tf_frame(type, p);
}

}  // namespace

TEST_F(LD6002BTest, TargetReportPublishesLittleEndianFloats) {
  this->feed_(target_report({{1.25f, -2.5f, 0.75f, -3, 7}}));
  ASSERT_EQ(this->target_count_published_.size(), 1u);
  EXPECT_FLOAT_EQ(this->target_count_published_[0], 1.0f);
  EXPECT_FLOAT_EQ(this->target_x_[0].state, 1.25f);
  EXPECT_FLOAT_EQ(this->target_y_[0].state, -2.5f);
  EXPECT_FLOAT_EQ(this->target_z_[0].state, 0.75f);
  EXPECT_FLOAT_EQ(this->target_dop_[0].state, -3.0f);
  EXPECT_FLOAT_EQ(this->target_cluster_[0].state, 7.0f);
  EXPECT_FALSE(this->target_x_[1].has_state());
}

TEST_F(LD6002BTest, TargetCountIsPublishedOnChangeOnly) {
  this->feed_(target_report({{1.0f, 1.0f, 1.0f, 0, 1}}));
  this->feed_(target_report({{2.0f, 1.0f, 1.0f, 0, 1}}));
  this->feed_(target_report({{2.0f, 1.0f, 1.0f, 0, 1}, {3.0f, 1.0f, 1.0f, 0, 2}}));
  EXPECT_EQ(this->target_count_published_, (std::vector<float>{1.0f, 2.0f}));
  EXPECT_EQ(this->target_x0_published_, (std::vector<float>{1.0f, 2.0f, 2.0f}));
}

TEST_F(LD6002BTest, SlotsFollowClusterIdWhenModuleReorders) {
  this->feed_(target_report({{1.0f, 0.0f, 0.0f, 0, 5}, {2.0f, 0.0f, 0.0f, 0, 9}}));
  this->feed_(target_report({{2.5f, 0.0f, 0.0f, 0, 9}, {1.5f, 0.0f, 0.0f, 0, 5}}));
  EXPECT_FLOAT_EQ(this->target_x_[0].state, 1.5f);
  EXPECT_FLOAT_EQ(this->target_cluster_[0].state, 5.0f);
  EXPECT_FLOAT_EQ(this->target_x_[1].state, 2.5f);
  EXPECT_FLOAT_EQ(this->target_cluster_[1].state, 9.0f);
}

TEST_F(LD6002BTest, TargetLeavingClearsItsSlot) {
  this->feed_(target_report({{1.0f, 0.0f, 0.0f, 0, 5}, {2.0f, 0.0f, 0.0f, 0, 9}}));
  this->feed_(target_report({{1.0f, 0.0f, 0.0f, 0, 5}}));
  EXPECT_TRUE(std::isnan(this->target_x_[1].state));
  EXPECT_TRUE(std::isnan(this->target_cluster_[1].state));
  EXPECT_FLOAT_EQ(this->target_x_[0].state, 1.0f);
}

TEST_F(LD6002BTest, MoreTargetsThanSlotsFillOnlyThree) {
  std::vector<Target> four;
  for (int32_t i = 0; i < 4; i++)
    four.push_back({static_cast<float>(i), 0.0f, 0.0f, 0, i + 1});
  this->feed_(target_report(four));
  EXPECT_FLOAT_EQ(this->target_count_published_.back(), 4.0f);
  EXPECT_FLOAT_EQ(this->target_x_[2].state, 2.0f);
}

TEST_F(LD6002BTest, TargetCountIsCappedAtWhatThePayloadHolds) {
  // Claim 200 targets in a payload that carries one
  std::vector<uint8_t> p = u32_payload(200);
  for (float v : {1.0f, 0.0f, 0.0f})
    put_f32(p, v);
  put_u32(p, 0);
  put_u32(p, 1);
  this->feed_(tf_frame(TYPE_REPORT_TARGET, p));
  EXPECT_EQ(this->target_count_published_, (std::vector<float>{1.0f}));
}

TEST_F(LD6002BTest, PointCloudPublishesCount) {
  std::vector<uint8_t> p = u32_payload(12);
  p.resize(4 + 12 * 16);
  this->feed_(tf_frame(TYPE_REPORT_POINT_CLOUD, p));
  this->feed_(tf_frame(TYPE_REPORT_POINT_CLOUD, p));
  EXPECT_EQ(this->point_count_published_, (std::vector<float>{12.0f}));
}

// Larger than the 1024 byte default buffer, which a configured point_count raises to 4096
TEST_F(LD6002BTest, LargePointCloudFitsTheRaisedBuffer) {
  std::vector<uint8_t> p = u32_payload(150);
  p.resize(4 + 150 * 16);
  this->feed_(tf_frame(TYPE_REPORT_POINT_CLOUD, p));
  EXPECT_EQ(this->point_count_published_, (std::vector<float>{150.0f}));
}

TEST_F(LD6002BTest, InterferenceAreaReportPublishesBounds) {
  this->feed_(area_report(TYPE_REPORT_INTERFERENCE_AREAS, 0, {-1.5f, 1.5f, 0.25f, 3.0f, -0.5f, 2.75f}));
  EXPECT_FLOAT_EQ(this->interference_0_[0].state, -1.5f);
  EXPECT_FLOAT_EQ(this->interference_0_[1].state, 1.5f);
  EXPECT_FLOAT_EQ(this->interference_0_[2].state, 0.25f);
  EXPECT_FLOAT_EQ(this->interference_0_[3].state, 3.0f);
  EXPECT_FLOAT_EQ(this->interference_0_[4].state, -0.5f);
  EXPECT_FLOAT_EQ(this->interference_0_[5].state, 2.75f);
  EXPECT_FALSE(this->detection_1_[0].has_state());
}

TEST_F(LD6002BTest, DetectionAreaReportPublishesTheRightArea) {
  this->feed_(area_report(TYPE_REPORT_DETECTION_AREAS, 1, {-2.0f, 2.0f, 0.5f, 4.0f, 0.0f, 2.5f}));
  EXPECT_FLOAT_EQ(this->detection_1_[0].state, -2.0f);
  EXPECT_FLOAT_EQ(this->detection_1_[3].state, 4.0f);
  EXPECT_FLOAT_EQ(this->detection_1_[5].state, 2.5f);
  EXPECT_FALSE(this->interference_0_[0].has_state());
}

TEST_F(LD6002BTest, ShortAreaReportIsIgnored) {
  auto p = std::vector<uint8_t>(AREA_COUNT * 24 - 1, 0);
  this->feed_(tf_frame(TYPE_REPORT_DETECTION_AREAS, p));
  EXPECT_FALSE(this->detection_1_[0].has_state());
}

}  // namespace esphome::ld6002b::testing
