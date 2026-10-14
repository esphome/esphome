#include <gtest/gtest.h>

#include <string>

#include "esphome/components/rtttl/rtttl.h"

namespace esphome::rtttl::testing {

// Reads the parser state the tests check; Rtttl is final, so this is a friend rather than a subclass.
class RtttlParserAccess {
 public:
  static uint8_t denominator(const Rtttl &p) { return p.default_note_denominator_; }
  static uint8_t octave(const Rtttl &p) { return p.default_octave_; }
  static uint16_t wholenote(const Rtttl &p) { return p.wholenote_duration_; }
  static uint16_t note_duration(const Rtttl &p) { return p.note_duration_; }
  static uint32_t freq(const Rtttl &p) { return p.output_freq_; }
  static size_t position(const Rtttl &p) { return p.position_; }
  static char at(const Rtttl &p, size_t pos) { return p.song_at_(pos); }
  static void set_running(Rtttl &p) { p.state_ = State::RUNNING; }
};

using Access = RtttlParserAccess;

static const char SONG[] = "test:d=8,o=5,b=100:c";

TEST(RtttlParser, StaticSongParsesTheHeader) {
  Rtttl player;
  player.play(StaticSong{SONG});
  EXPECT_EQ(Access::denominator(player), 8);
  EXPECT_EQ(Access::octave(player), 5);
  EXPECT_EQ(Access::wholenote(player), 60 * 1000 * 4 / 100);
  // Positioned on the first note after the second ':'
  EXPECT_EQ(Access::position(player), 19u);
  EXPECT_EQ(Access::at(player, Access::position(player)), 'c');
}

TEST(RtttlParser, OwnedSongParsesTheSameAndOutlivesTheCaller) {
  Rtttl player;
  {
    std::string song(SONG);
    player.play(song);
  }
  EXPECT_EQ(Access::denominator(player), 8);
  EXPECT_EQ(Access::octave(player), 5);
  EXPECT_EQ(Access::at(player, 0), 't');
  EXPECT_EQ(Access::at(player, Access::position(player)), 'c');
  EXPECT_EQ(Access::at(player, 100), '\0');
}

TEST(RtttlParser, ControlOrderDoesNotMatter) {
  for (const char *song : {"a:b=100,o=5,d=8:c", "a:o=5,b=100,d=8:c", "a:d=8,b=100,o=5:c"}) {
    Rtttl player;
    player.play(StaticSong{song});
    EXPECT_EQ(Access::denominator(player), 8) << song;
    EXPECT_EQ(Access::octave(player), 5) << song;
    EXPECT_EQ(Access::wholenote(player), 2400) << song;
  }
}

TEST(RtttlParser, MissingControlsUseDefaults) {
  Rtttl player;
  player.play(StaticSong{"a::c"});
  EXPECT_EQ(Access::denominator(player), DEFAULT_NOTE_DENOMINATOR);
  EXPECT_EQ(Access::octave(player), DEFAULT_OCTAVE);
  EXPECT_EQ(Access::wholenote(player), 60 * 1000 * 4 / 63);
}

TEST(RtttlParser, NotesReadFromStaticStorage) {
  Rtttl player;
  player.play(StaticSong{"t:d=4,o=5,b=120:8c,p"});
  Access::set_running(player);
  player.loop();
  // C5 at 523 Hz for an eighth of a 2000 ms whole note
  EXPECT_EQ(Access::freq(player), 523u);
  EXPECT_EQ(Access::note_duration(player), 250);
  player.loop();
  // A pause for a default quarter note
  EXPECT_EQ(Access::freq(player), 0u);
  EXPECT_EQ(Access::note_duration(player), 500);
  player.loop();
  // End of the song
  EXPECT_EQ(Access::at(player, Access::position(player)), '\0');
}

TEST(RtttlParser, RefusesANewSongWhilePlaying) {
  Rtttl player;
  player.play(StaticSong{"first:d=8:c"});
  Access::set_running(player);
  player.play(std::string("second:d=16:c"));
  EXPECT_EQ(Access::denominator(player), 8);
  EXPECT_EQ(Access::at(player, 0), 'f');
}

TEST(RtttlParser, ClampsASongLongerThanTheLengthField) {
  Rtttl player;
  player.play(std::string(70000, 'a'));
  EXPECT_EQ(Access::at(player, 65534), 'a');
  EXPECT_EQ(Access::at(player, 65535), '\0');
  EXPECT_EQ(Access::note_duration(player), 0);
}

TEST(RtttlParser, RejectsASongWithoutAName) {
  Rtttl player;
  player.play(StaticSong{"no colon here"});
  EXPECT_EQ(Access::note_duration(player), 0);
  EXPECT_EQ(Access::position(player), 13u);
}

}  // namespace esphome::rtttl::testing
