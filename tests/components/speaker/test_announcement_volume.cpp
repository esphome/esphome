#include <gtest/gtest.h>

#include "esphome/components/speaker/announcement_volume.h"

namespace esphome::speaker::testing {

TEST(AnnouncementVolume, NotConfiguredNeverBegins) {
  AnnouncementVolume volume;
  EXPECT_FALSE(volume.is_configured());
  EXPECT_FALSE(volume.begin(false));
  EXPECT_FALSE(volume.is_active());
  EXPECT_FALSE(volume.end());
}

TEST(AnnouncementVolume, AnnouncementFromIdleSwitchesAndRestores) {
  AnnouncementVolume volume;
  EXPECT_FALSE(volume.set_volume(0.6f));
  EXPECT_TRUE(volume.is_configured());
  EXPECT_FLOAT_EQ(volume.get_volume(), 0.6f);

  EXPECT_TRUE(volume.begin(false));
  EXPECT_TRUE(volume.is_active());
  EXPECT_TRUE(volume.end());
  EXPECT_FALSE(volume.is_active());
  // Already restored, so a second end has nothing to apply
  EXPECT_FALSE(volume.end());
}

TEST(AnnouncementVolume, AnnouncementOverOtherAudioKeepsMediaVolume) {
  AnnouncementVolume volume;
  volume.set_volume(0.6f);
  EXPECT_FALSE(volume.begin(true));
  EXPECT_FALSE(volume.is_active());
  EXPECT_FALSE(volume.end());
}

TEST(AnnouncementVolume, SecondAnnouncementKeepsAnnouncementVolume) {
  AnnouncementVolume volume;
  volume.set_volume(0.6f);
  ASSERT_TRUE(volume.begin(false));
  // The running announcement is the other audio; the volume is already applied, so nothing to send again
  EXPECT_FALSE(volume.begin(true));
  EXPECT_TRUE(volume.is_active());
}

TEST(AnnouncementVolume, SetWhileActiveAppliesRightAway) {
  AnnouncementVolume volume;
  volume.set_volume(0.6f);
  ASSERT_TRUE(volume.begin(false));
  EXPECT_TRUE(volume.set_volume(0.3f));
  EXPECT_FLOAT_EQ(volume.get_volume(), 0.3f);
}

TEST(AnnouncementVolume, MediaVolumeChangeCancelsUntilNextAnnouncement) {
  AnnouncementVolume volume;
  volume.set_volume(0.6f);
  ASSERT_TRUE(volume.begin(false));
  volume.cancel();
  EXPECT_FALSE(volume.is_active());
  // The new media volume was already sent, so the end of the announcement has nothing to restore
  EXPECT_FALSE(volume.end());
  // A new announcement changes the volume while the old one still plays, which counts as other audio
  EXPECT_FALSE(volume.begin(true));
  EXPECT_TRUE(volume.begin(false));
}

TEST(AnnouncementVolume, SetWhileInactiveWaitsForNextAnnouncement) {
  AnnouncementVolume volume;
  EXPECT_FALSE(volume.set_volume(0.6f));
  EXPECT_FALSE(volume.set_volume(0.4f));
  ASSERT_TRUE(volume.begin(false));
  EXPECT_FLOAT_EQ(volume.get_volume(), 0.4f);
}

}  // namespace esphome::speaker::testing
