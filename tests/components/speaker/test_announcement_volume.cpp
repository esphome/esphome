#include <gtest/gtest.h>

#include <vector>

#include "esphome/components/speaker/announcement_volume.h"

namespace esphome::speaker::testing {

// Records every volume the speakers are asked to switch to
class AnnouncementVolumeTest : public ::testing::Test {
 protected:
  auto apply_() {
    return [this](float volume) { this->applied_.push_back(volume); };
  }

  AnnouncementVolume volume_;
  std::vector<float> applied_;
};

TEST_F(AnnouncementVolumeTest, NotConfiguredNeverBegins) {
  EXPECT_FALSE(this->volume_.is_configured());
  this->volume_.begin(false, this->apply_());
  EXPECT_FALSE(this->volume_.is_active());
  this->volume_.end(0.2f, this->apply_());
  EXPECT_TRUE(this->applied_.empty());
}

TEST_F(AnnouncementVolumeTest, AnnouncementFromIdleSwitchesAndRestores) {
  this->volume_.set_volume(0.6f, this->apply_());
  EXPECT_TRUE(this->applied_.empty());
  EXPECT_TRUE(this->volume_.is_configured());
  EXPECT_FLOAT_EQ(this->volume_.get_volume(), 0.6f);

  this->volume_.begin(false, this->apply_());
  EXPECT_TRUE(this->volume_.is_active());
  this->volume_.end(0.2f, this->apply_());
  EXPECT_FALSE(this->volume_.is_active());
  // Already restored, so a second end has nothing to apply
  this->volume_.end(0.2f, this->apply_());
  EXPECT_EQ(this->applied_, (std::vector<float>{0.6f, 0.2f}));
}

TEST_F(AnnouncementVolumeTest, AnnouncementOverOtherAudioKeepsMediaVolume) {
  this->volume_.set_volume(0.6f, this->apply_());
  this->volume_.begin(true, this->apply_());
  EXPECT_FALSE(this->volume_.is_active());
  this->volume_.end(0.2f, this->apply_());
  EXPECT_TRUE(this->applied_.empty());
}

TEST_F(AnnouncementVolumeTest, SecondAnnouncementKeepsAnnouncementVolume) {
  this->volume_.set_volume(0.6f, this->apply_());
  this->volume_.begin(false, this->apply_());
  // The running announcement is the other audio; the volume is already applied, so nothing to send again
  this->volume_.begin(true, this->apply_());
  EXPECT_TRUE(this->volume_.is_active());
  EXPECT_EQ(this->applied_, (std::vector<float>{0.6f}));
}

TEST_F(AnnouncementVolumeTest, SetWhileActiveAppliesRightAway) {
  this->volume_.set_volume(0.6f, this->apply_());
  this->volume_.begin(false, this->apply_());
  this->volume_.set_volume(0.3f, this->apply_());
  EXPECT_FLOAT_EQ(this->volume_.get_volume(), 0.3f);
  EXPECT_EQ(this->applied_, (std::vector<float>{0.6f, 0.3f}));
}

TEST_F(AnnouncementVolumeTest, MediaVolumeChangeCancelsUntilNextAnnouncement) {
  this->volume_.set_volume(0.6f, this->apply_());
  this->volume_.begin(false, this->apply_());
  this->volume_.cancel();
  EXPECT_FALSE(this->volume_.is_active());
  // The new media volume was already sent, so the end of the announcement has nothing to restore
  this->volume_.end(0.2f, this->apply_());
  // A new announcement changes the volume while the old one still plays, which counts as other audio
  this->volume_.begin(true, this->apply_());
  this->volume_.begin(false, this->apply_());
  EXPECT_EQ(this->applied_, (std::vector<float>{0.6f, 0.6f}));
}

TEST_F(AnnouncementVolumeTest, SetWhileInactiveWaitsForNextAnnouncement) {
  this->volume_.set_volume(0.6f, this->apply_());
  this->volume_.set_volume(0.4f, this->apply_());
  EXPECT_TRUE(this->applied_.empty());
  this->volume_.begin(false, this->apply_());
  EXPECT_EQ(this->applied_, (std::vector<float>{0.4f}));
}

TEST_F(AnnouncementVolumeTest, EndRestoresTheGivenMediaVolume) {
  this->volume_.set_volume(0.6f, this->apply_());
  this->volume_.begin(false, this->apply_());
  // The media volume may differ from the one before the announcement, e.g. after a restore from flash
  this->volume_.end(0.35f, this->apply_());
  EXPECT_EQ(this->applied_, (std::vector<float>{0.6f, 0.35f}));
}

TEST_F(AnnouncementVolumeTest, AnnouncementQueuedBehindMediaSwitchesWhenItStarts) {
  this->volume_.set_volume(0.6f, this->apply_());
  this->volume_.item_started(false, false, 0.2f, this->apply_());
  EXPECT_TRUE(this->applied_.empty());
  // The media before it has finished, so nothing else plays
  this->volume_.item_started(true, false, 0.2f, this->apply_());
  EXPECT_TRUE(this->volume_.is_active());
  EXPECT_EQ(this->applied_, (std::vector<float>{0.6f}));
}

TEST_F(AnnouncementVolumeTest, MediaQueuedBehindAnnouncementRestoresWhenItStarts) {
  this->volume_.set_volume(0.6f, this->apply_());
  this->volume_.item_started(true, false, 0.2f, this->apply_());
  this->volume_.item_started(false, false, 0.2f, this->apply_());
  EXPECT_FALSE(this->volume_.is_active());
  EXPECT_EQ(this->applied_, (std::vector<float>{0.6f, 0.2f}));
}

TEST_F(AnnouncementVolumeTest, QueuedAnnouncementKeepsVolumeChangeOfThePreviousOne) {
  this->volume_.set_volume(0.6f, this->apply_());
  this->volume_.item_started(true, false, 0.2f, this->apply_());
  this->volume_.cancel();
  this->volume_.item_started(true, false, 0.3f, this->apply_());
  EXPECT_FALSE(this->volume_.is_active());
  EXPECT_EQ(this->applied_, (std::vector<float>{0.6f}));
}

TEST_F(AnnouncementVolumeTest, QueuedAnnouncementOverMediaKeepsMediaVolume) {
  this->volume_.set_volume(0.6f, this->apply_());
  this->volume_.item_started(true, true, 0.2f, this->apply_());
  // Still over media, or the media stopped while the first one played; both keep the media volume
  this->volume_.item_started(true, false, 0.2f, this->apply_());
  EXPECT_FALSE(this->volume_.is_active());
  EXPECT_TRUE(this->applied_.empty());
}

TEST_F(AnnouncementVolumeTest, PlaylistFinishedRestoresAndStartsFresh) {
  this->volume_.set_volume(0.6f, this->apply_());
  this->volume_.item_started(true, false, 0.2f, this->apply_());
  this->volume_.cancel();
  this->volume_.playlist_finished(0.2f, this->apply_());
  // A new playlist uses the announcement volume again, even after a volume change in the previous one
  this->volume_.item_started(true, false, 0.2f, this->apply_());
  EXPECT_TRUE(this->volume_.is_active());
  this->volume_.playlist_finished(0.2f, this->apply_());
  EXPECT_FALSE(this->volume_.is_active());
  EXPECT_EQ(this->applied_, (std::vector<float>{0.6f, 0.6f, 0.2f}));
}

}  // namespace esphome::speaker::testing
