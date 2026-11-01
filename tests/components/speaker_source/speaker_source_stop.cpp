#ifdef USE_HOST
#include <gtest/gtest.h>

#include <cstring>
#include <string>
#include <vector>

#include "esphome/components/host/preferences.h"
#include "esphome/components/media_player/media_player.h"
#include "esphome/components/media_source/media_source.h"
#include "esphome/core/application.h"
#include "esphome/core/automation.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/preferences.h"

// Compile the ESP32-only player in this translation unit against the FreeRTOS queue double in __sdk.
#define USE_ESP32
// NOLINTNEXTLINE(bugprone-suspicious-include): exercise production code with the host FreeRTOS double.
#include "esphome/components/speaker_source/speaker_source_media_player.cpp"

namespace esphome::speaker_source::testing {

using media_player::MediaPlayerCommand;
using media_source::MediaSourceCommand;
using media_source::MediaSourceState;

/// A source that records what the player asks of it; the test decides when it reports a state.
class FakeSource : public media_source::MediaSource {
 public:
  bool play_uri(const std::string &uri) override {
    this->played.push_back(uri);
    return true;
  }
  void handle_command(MediaSourceCommand command) override {
    this->commands.push_back(command);
    if (command == MediaSourceCommand::STOP && this->idle_on_stop)
      this->set_state_(MediaSourceState::IDLE);
  }
  bool can_handle(const std::string &uri) const override { return true; }
  bool has_internal_playlist() const override { return this->internal_playlist; }
  void report(MediaSourceState state) { this->set_state_(state); }

  std::vector<std::string> played;
  std::vector<MediaSourceCommand> commands;
  bool idle_on_stop{false};
  bool internal_playlist{false};
};

class FakeSpeaker : public speaker::Speaker {
 public:
  size_t play(const uint8_t *data, size_t length) override { return length; }
  void start() override { this->state_ = speaker::STATE_RUNNING; }
  void stop() override { this->state_ = speaker::STATE_STOPPED; }
  bool has_buffered_data() const override { return false; }
};

class SpeakerSourceStop : public ::testing::Test {
 protected:
  void SetUp() override {
    host::setup_preferences();
    queue_test::sent.clear();
    this->player_.set_volume_increment(0.05f);
    this->player_.set_volume_initial(0.5f);
    this->player_.set_volume_min(0.0f);
    this->player_.set_volume_max(1.0f);
    this->player_.set_speaker(MEDIA_PIPELINE, &this->speaker_);
    this->player_.add_media_source(MEDIA_PIPELINE, &this->source_);
    this->player_.call();  // setup()
  }
  // Runs the callbacks the player deferred while it is still alive
  void TearDown() override { App.scheduler.call(millis()); }

  /// Runs the player's loop enough times to drain the commands it queues for itself.
  void run_loop() {
    for (int i = 0; i < 4; i++)
      this->player_.call();
  }
  void play(const std::string &uri) {
    this->player_.make_call().set_media_url(uri).perform();
    this->run_loop();
  }
  void enqueue(const std::string &uri) {
    this->player_.make_call().set_media_url(uri).set_command(media_player::MEDIA_PLAYER_COMMAND_ENQUEUE).perform();
    this->run_loop();
  }
  void command(MediaPlayerCommand command) {
    this->player_.make_call().set_command(command).perform();
    this->run_loop();
  }
  void report(MediaSourceState state) {
    this->source_.report(state);
    this->run_loop();
  }
  /// How many times the player queued a playlist advance for itself.
  size_t playlist_advances() const {
    size_t count = 0;
    for (const auto &item : queue_test::sent) {
      MediaPlayerControlCommand command;
      std::memcpy(&command, item.data(), sizeof(command));
      if (command.type == MediaPlayerControlCommand::PLAYLIST_ADVANCE)
        count++;
    }
    return count;
  }

  FakeSpeaker speaker_;
  FakeSource source_;
  SpeakerSourceMediaPlayer player_;
};

TEST_F(SpeakerSourceStop, StopReachesASourceThatHasNotStartedYet) {
  this->play("http://a");
  ASSERT_EQ(this->source_.played, std::vector<std::string>{"http://a"});
  ASSERT_EQ(this->source_.get_state(), MediaSourceState::IDLE);

  // The source cancels its startup without leaving IDLE, so no state callback follows
  this->command(media_player::MEDIA_PLAYER_COMMAND_STOP);
  EXPECT_EQ(this->source_.commands, std::vector<MediaSourceCommand>{MediaSourceCommand::STOP});
  EXPECT_EQ(this->playlist_advances(), 0);
  EXPECT_EQ(this->player_.state, media_player::MEDIA_PLAYER_STATE_IDLE);

  // Nothing is left over from the stop: the next request plays, and its natural end advances the playlist
  this->play("http://b");
  EXPECT_EQ(this->source_.played, (std::vector<std::string>{"http://a", "http://b"}));
  this->report(MediaSourceState::PLAYING);
  EXPECT_EQ(this->player_.state, media_player::MEDIA_PLAYER_STATE_PLAYING);
  this->report(MediaSourceState::IDLE);
  EXPECT_EQ(this->playlist_advances(), 1);
  EXPECT_EQ(this->player_.state, media_player::MEDIA_PLAYER_STATE_IDLE);
}

TEST_F(SpeakerSourceStop, StopOfAStartingSourceThatGoesIdleLater) {
  this->play("http://a");
  // An error before PLAYING advances the playlist past the source but keeps it pending, so the stop still reaches it
  this->report(MediaSourceState::ERROR);
  EXPECT_EQ(this->playlist_advances(), 1);

  this->command(media_player::MEDIA_PLAYER_COMMAND_STOP);
  EXPECT_EQ(this->source_.commands, std::vector<MediaSourceCommand>{MediaSourceCommand::STOP});

  // Neither the stop nor the IDLE it causes advances the playlist again
  this->report(MediaSourceState::IDLE);
  EXPECT_EQ(this->playlist_advances(), 1);
  EXPECT_EQ(this->source_.played, std::vector<std::string>{"http://a"});
  EXPECT_EQ(this->player_.state, media_player::MEDIA_PLAYER_STATE_IDLE);
}

TEST_F(SpeakerSourceStop, StopOfAPlayingSourceDoesNotAdvance) {
  for (bool idle_on_stop : {false, true}) {
    SCOPED_TRACE(idle_on_stop ? "IDLE during STOP" : "IDLE after STOP");
    queue_test::sent.clear();
    this->source_.commands.clear();
    this->source_.played.clear();
    this->source_.idle_on_stop = idle_on_stop;

    this->play("http://a");
    this->report(MediaSourceState::PLAYING);
    this->enqueue("http://b");
    ASSERT_EQ(this->player_.state, media_player::MEDIA_PLAYER_STATE_PLAYING);

    this->command(media_player::MEDIA_PLAYER_COMMAND_STOP);
    EXPECT_EQ(this->source_.commands, std::vector<MediaSourceCommand>{MediaSourceCommand::STOP});
    if (!idle_on_stop)
      this->report(MediaSourceState::IDLE);
    EXPECT_EQ(this->playlist_advances(), 0);
    EXPECT_EQ(this->source_.played, std::vector<std::string>{"http://a"});
    EXPECT_EQ(this->player_.state, media_player::MEDIA_PLAYER_STATE_IDLE);
  }
}

TEST_F(SpeakerSourceStop, StopOfASourceWithItsOwnPlaylistEndsPlayback) {
  this->source_.internal_playlist = true;
  this->play("http://a");
  this->report(MediaSourceState::PLAYING);

  this->command(media_player::MEDIA_PLAYER_COMMAND_STOP);
  this->report(MediaSourceState::IDLE);
  EXPECT_EQ(this->playlist_advances(), 0);
  EXPECT_EQ(this->player_.state, media_player::MEDIA_PLAYER_STATE_IDLE);
}

TEST_F(SpeakerSourceStop, AnErrorBeforePlayingDoesNotHoldBackLaterItems) {
  this->source_.idle_on_stop = true;
  this->enqueue("http://a");
  ASSERT_EQ(this->source_.played, std::vector<std::string>{"http://a"});

  // The source cannot open the first item and fails before it reports PLAYING
  this->report(MediaSourceState::ERROR);
  EXPECT_EQ(this->playlist_advances(), 1);

  // An item enqueued after the failure plays instead of waiting behind it
  this->enqueue("http://b");
  EXPECT_EQ(this->source_.played, (std::vector<std::string>{"http://a", "http://b"}));
  EXPECT_EQ(this->source_.commands, std::vector<MediaSourceCommand>{MediaSourceCommand::STOP});
  EXPECT_EQ(this->playlist_advances(), 1);
}

TEST_F(SpeakerSourceStop, NaturalEndAdvancesThePlaylist) {
  this->play("http://a");
  this->report(MediaSourceState::PLAYING);
  this->enqueue("http://b");

  this->report(MediaSourceState::IDLE);
  EXPECT_EQ(this->playlist_advances(), 1);
  EXPECT_EQ(this->source_.played, (std::vector<std::string>{"http://a", "http://b"}));
  EXPECT_TRUE(this->source_.commands.empty());
}

}  // namespace esphome::speaker_source::testing
#endif  // USE_HOST
