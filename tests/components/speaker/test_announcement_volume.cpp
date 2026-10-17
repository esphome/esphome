#include <gtest/gtest.h>

#include <vector>

#include "esphome/components/speaker/announcement_volume.h"

namespace esphome::speaker::testing {

// Records every volume the speakers are asked to switch to
class AnnouncementVolumeTest : public ::testing::Test {
 protected:
  auto apply() {
    return [this](float volume) { this->applied_.push_back(volume); };
  }

  AnnouncementVolume volume_;
  std::vector<float> applied_;
};

TEST_F(AnnouncementVolumeTest, NotConfiguredNeverBegins) {
  EXPECT_FALSE(this->volume_.is_configured());
  this->volume_.begin(false, this->apply());
  EXPECT_FALSE(this->volume_.is_active());
  this->volume_.end(0.2f, this->apply());
  EXPECT_TRUE(this->applied_.empty());
}

TEST_F(AnnouncementVolumeTest, AnnouncementFromIdleSwitchesAndRestores) {
  this->volume_.set_volume(0.6f, this->apply());
  EXPECT_TRUE(this->applied_.empty());
  EXPECT_TRUE(this->volume_.is_configured());
  EXPECT_FLOAT_EQ(this->volume_.get_volume(), 0.6f);

  this->volume_.begin(false, this->apply());
  EXPECT_TRUE(this->volume_.is_active());
  this->volume_.end(0.2f, this->apply());
  EXPECT_FALSE(this->volume_.is_active());
  // Already restored, so a second end has nothing to apply
  this->volume_.end(0.2f, this->apply());
  EXPECT_EQ(this->applied_, (std::vector<float>{0.6f, 0.2f}));
}

TEST_F(AnnouncementVolumeTest, AnnouncementOverOtherAudioKeepsMediaVolume) {
  this->volume_.set_volume(0.6f, this->apply());
  this->volume_.begin(true, this->apply());
  EXPECT_FALSE(this->volume_.is_active());
  this->volume_.end(0.2f, this->apply());
  EXPECT_TRUE(this->applied_.empty());
}

TEST_F(AnnouncementVolumeTest, SecondAnnouncementKeepsAnnouncementVolume) {
  this->volume_.set_volume(0.6f, this->apply());
  this->volume_.begin(false, this->apply());
  // The running announcement is the other audio; the volume is already applied, so nothing to send again
  this->volume_.begin(true, this->apply());
  EXPECT_TRUE(this->volume_.is_active());
  EXPECT_EQ(this->applied_, (std::vector<float>{0.6f}));
}

TEST_F(AnnouncementVolumeTest, SetWhileActiveAppliesRightAway) {
  this->volume_.set_volume(0.6f, this->apply());
  this->volume_.begin(false, this->apply());
  this->volume_.set_volume(0.3f, this->apply());
  EXPECT_FLOAT_EQ(this->volume_.get_volume(), 0.3f);
  EXPECT_EQ(this->applied_, (std::vector<float>{0.6f, 0.3f}));
}

TEST_F(AnnouncementVolumeTest, MediaVolumeChangeCancelsUntilNextAnnouncement) {
  this->volume_.set_volume(0.6f, this->apply());
  this->volume_.begin(false, this->apply());
  this->volume_.cancel();
  EXPECT_FALSE(this->volume_.is_active());
  // The new media volume was already sent, so the end of the announcement has nothing to restore
  this->volume_.end(0.2f, this->apply());
  // A new announcement changes the volume while the old one still plays, which counts as other audio
  this->volume_.begin(true, this->apply());
  this->volume_.begin(false, this->apply());
  EXPECT_EQ(this->applied_, (std::vector<float>{0.6f, 0.6f}));
}

TEST_F(AnnouncementVolumeTest, SetWhileInactiveWaitsForNextAnnouncement) {
  this->volume_.set_volume(0.6f, this->apply());
  this->volume_.set_volume(0.4f, this->apply());
  EXPECT_TRUE(this->applied_.empty());
  this->volume_.begin(false, this->apply());
  EXPECT_EQ(this->applied_, (std::vector<float>{0.4f}));
}

TEST_F(AnnouncementVolumeTest, EndRestoresTheGivenMediaVolume) {
  this->volume_.set_volume(0.6f, this->apply());
  this->volume_.begin(false, this->apply());
  // The media volume may differ from the one before the announcement, e.g. after a restore from flash
  this->volume_.end(0.35f, this->apply());
  EXPECT_EQ(this->applied_, (std::vector<float>{0.6f, 0.35f}));
}

}  // namespace esphome::speaker::testing
