#include <gtest/gtest.h>

#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "esphome/components/api/api_frame_helper_plaintext.h"
#include "esphome/components/api/api_server.h"
#include "esphome/components/voice_assistant/voice_assistant.h"
#include "esphome/core/application.h"
#include "esphome/core/base_automation.h"

// Capture state is simulated only for direct-speaker tests. PCM conversion is out of scope.
namespace esphome::microphone {
void MicrophoneSource::start() {
  this->enabled_ = true;
  this->mic_->start();
}
void MicrophoneSource::stop() {
  this->enabled_ = false;
  this->mic_->stop();
}
void MicrophoneSource::process_audio_(const std::vector<uint8_t> &, std::vector<uint8_t> &) {
  ADD_FAILURE() << "Unexpected microphone PCM";
}
audio::AudioStreamInfo MicrophoneSource::get_audio_stream_info() { return {16, 1, 16000}; }
}  // namespace esphome::microphone

namespace esphome::voice_assistant::testing {

class TestMicrophone : public microphone::Microphone {
 public:
  unsigned starts{0};
  unsigned stops{0};
  void start() override {
    ++this->starts;
    this->state_ = microphone::STATE_RUNNING;
  }
  void stop() override {
    ++this->stops;
    this->state_ = microphone::STATE_STOPPED;
  }
};

class TestMediaPlayer : public media_player::MediaPlayer {
 public:
  int play_requests{0};
  int stop_requests{0};
  void report(media_player::MediaPlayerState state) {
    this->state = state;
    this->publish_state();
  }
  media_player::MediaPlayerTraits get_traits() override { return {}; }

 protected:
  void control(const media_player::MediaPlayerCall &call) override {
    if (call.get_media_url().has_value())
      ++this->play_requests;
    if (call.get_command() == media_player::MEDIA_PLAYER_COMMAND_STOP) {
      ++this->stop_requests;
      this->report(media_player::MEDIA_PLAYER_STATE_IDLE);
    }
  }
};

class VoiceAssistantResponseTest : public ::testing::Test {
 protected:
  void SetUp() override {
    int listener = ::socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GE(listener, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    ASSERT_EQ(::bind(listener, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)), 0);
    socklen_t size = sizeof(addr);
    ASSERT_EQ(::getsockname(listener, reinterpret_cast<sockaddr *>(&addr), &size), 0);
    ASSERT_EQ(::listen(listener, 1), 0);
    int peer = ::socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GE(peer, 0);
    ASSERT_EQ(::connect(peer, reinterpret_cast<sockaddr *>(&addr), size), 0);
    int client = ::accept(listener, nullptr, nullptr);
    ::close(listener);
    ASSERT_GE(client, 0);
    this->peer_ = std::make_unique<api::APIPlaintextFrameHelper>(std::make_unique<socket::Socket>(peer));
    ASSERT_EQ(this->peer_->init(), api::APIError::OK);
    this->client_ = std::make_unique<api::APIConnection>(std::make_unique<socket::Socket>(client), &this->server_);
    this->client_->start();
    this->voice_.client_subscription(this->client_.get(), true);
    this->voice_.set_microphone_source(&this->mic_source_);
    this->voice_.set_use_wake_word(false);
    this->voice_.set_noise_suppression_level(0);
    this->voice_.set_auto_gain(0);
    this->voice_.set_volume_multiplier(1.0f);
    this->voice_.set_conversation_timeout(300000);
    this->configure_output();
    this->voice_.setup();
    this->intent_automation_.add_action(&this->intent_action_);
    this->error_automation_.add_action(&this->error_action_);
  }

  void TearDown() override {
    this->voice_.request_stop();
    this->flush_();
    // Numeric timeout IDs are local to VoiceAssistant.
    for (uint32_t id = 0; id < 4; ++id) {
      App.scheduler.cancel_timeout(&this->voice_, id);
      App.scheduler.cancel_interval(&this->voice_, id);
    }
    EXPECT_EQ(this->mic_.starts, this->expected_microphone_starts_);
    this->client_.reset();
    this->peer_.reset();
    global_voice_assistant = this->previous_voice_;
    api::global_api_server = this->previous_server_;
  }

  void flush_() {
    App.scheduler.process_to_add();
    App.scheduler.call(millis());
  }

  virtual void configure_output() { this->voice_.set_media_player(&this->player_); }

  void event_(api::enums::VoiceAssistantEvent event, const char *key = nullptr, const char *value = nullptr) {
    api::VoiceAssistantEventResponse msg;
    msg.event_type = event;
    if (key != nullptr) {
      api::VoiceAssistantEventData arg;
      arg.name = StringRef(key);
      arg.value = StringRef(value);
      msg.data.push_back(arg);
    }
    this->voice_.on_event(msg);
  }

  void start_streamed_response_() {
    this->event_(api::enums::VOICE_ASSISTANT_RUN_START, "url", "http://example.test/tts.wav");
    this->event_(api::enums::VOICE_ASSISTANT_INTENT_PROGRESS, "tts_start_streaming", "1");
    this->flush_();
    ASSERT_EQ(this->player_.play_requests, 1);
  }

  void await_response_before_microphone_stop_() {
    ++this->expected_microphone_starts_;
    this->voice_.request_start(false, false);
    this->voice_.loop();
    this->voice_.loop();
    this->voice_.loop();
    this->voice_.start_streaming();
    this->event_(api::enums::VOICE_ASSISTANT_RUN_START, "url", "http://example.test/tts.wav");
    this->event_(api::enums::VOICE_ASSISTANT_STT_VAD_END);
    ASSERT_TRUE(this->mic_.is_running());
    ASSERT_EQ(this->mic_.stops, 0);
  }

  std::vector<bool> completions_() {
    std::vector<bool> result;
    api::ReadPacketBuffer buffer;
    while (this->peer_->read_packet(&buffer) == api::APIError::OK) {
      if (buffer.type == api::VoiceAssistantAnnounceFinished::MESSAGE_TYPE) {
        // Protobuf field 1 is the success boolean; false uses the default (empty payload).
        if (buffer.data_len == 0) {
          result.push_back(false);
        } else {
          EXPECT_EQ(buffer.data_len, 2);
          EXPECT_EQ(buffer.data[0], 0x08);
          EXPECT_EQ(buffer.data[1], 1);
          result.push_back(true);
        }
      }
    }
    return result;
  }

  VoiceAssistant *previous_voice_{global_voice_assistant};
  api::APIServer *previous_server_{api::global_api_server};
  TestMicrophone mic_;
  unsigned expected_microphone_starts_{0};
  microphone::MicrophoneSource mic_source_{&this->mic_, 16, 1, false};
  TestMediaPlayer player_;
  VoiceAssistant voice_;
  std::function<void(std::string)> intent_hook_;
  LambdaAction<std::string> intent_action_{[this](const std::string &url) {
    if (this->intent_hook_)
      this->intent_hook_(url);
  }};
  Automation<std::string> intent_automation_{this->voice_.get_intent_progress_trigger()};
  std::vector<std::string> errors_;
  LambdaAction<std::string, std::string> error_action_{
      [this](const std::string &code, const std::string &) { this->errors_.push_back(code); }};
  Automation<std::string, std::string> error_automation_{this->voice_.get_error_trigger()};
  api::APIServer server_;
  std::unique_ptr<api::APIConnection> client_;
  std::unique_ptr<api::APIPlaintextFrameHelper> peer_;
};

class TestSpeaker : public speaker::Speaker {
 public:
  std::vector<uint8_t> pcm;
  size_t play(const uint8_t *data, size_t length) override {
    this->pcm.insert(this->pcm.end(), data, data + length);
    return length;
  }
  void start() override { this->state_ = speaker::STATE_RUNNING; }
  void stop() override { this->state_ = speaker::STATE_STOPPED; }
  bool has_buffered_data() const override { return false; }
};

class VoiceAssistantSpeakerResponseTest : public VoiceAssistantResponseTest {
 protected:
  void TearDown() override {
    this->speaker_.stop();
    this->event_(api::enums::VOICE_ASSISTANT_TTS_STREAM_END);
    for (unsigned i = 0; i < 3; ++i)
      this->voice_.loop();
    VoiceAssistantResponseTest::TearDown();
  }
  void configure_output() override { this->voice_.set_speaker(&this->speaker_); }
  void start_speaker_response_() {
    ++this->expected_microphone_starts_;
    this->voice_.request_start(false, false);
    this->voice_.loop();
    this->voice_.loop();
    this->voice_.loop();
    this->voice_.start_streaming();
    this->event_(api::enums::VOICE_ASSISTANT_STT_VAD_END);
    this->voice_.loop();
    this->voice_.loop();
    this->event_(api::enums::VOICE_ASSISTANT_TTS_START, "text", "Test response");
    this->event_(api::enums::VOICE_ASSISTANT_TTS_END, "url", "http://example.test/tts.wav");
    this->flush_();
  }
  void send_pcm_() {
    const uint8_t pcm[]{1, 2, 3, 4, 5, 6};
    api::VoiceAssistantAudio msg;
    msg.data = pcm;
    msg.data_len = sizeof(pcm);
    this->voice_.on_audio(msg);
  }
  TestSpeaker speaker_;
};

TEST_F(VoiceAssistantSpeakerResponseTest, DirectSpeakerStreamEndAndReuseWithoutMediaPlayer) {
  for (unsigned i = 0; i < 2; ++i) {
    this->speaker_.pcm.clear();
    this->start_speaker_response_();
    this->event_(api::enums::VOICE_ASSISTANT_TTS_STREAM_START);
    this->send_pcm_();
    this->event_(api::enums::VOICE_ASSISTANT_TTS_STREAM_END);
    this->voice_.loop();
    this->speaker_.stop();  // The sink has consumed its output.
    this->voice_.loop();
    EXPECT_FALSE(this->voice_.is_running());
    EXPECT_EQ(this->speaker_.pcm, std::vector<uint8_t>({1, 2, 3, 4, 5, 6}));
    EXPECT_TRUE(this->completions_().empty());
  }
  EXPECT_EQ(this->player_.play_requests, 0);
  EXPECT_TRUE(this->errors_.empty());
}

TEST_F(VoiceAssistantSpeakerResponseTest, DirectSpeakerKeepsConfiguredLegacyTimeout) {
  this->voice_.set_tts_playback_start_timeout(20);
  this->start_speaker_response_();
  this->send_pcm_();
  this->voice_.loop();
  this->speaker_.stop();
  delay(30);
  this->flush_();
  this->voice_.loop();
  EXPECT_FALSE(this->voice_.is_running());
  EXPECT_EQ(this->completions_(), std::vector<bool>({true}));
  EXPECT_EQ(this->speaker_.pcm, std::vector<uint8_t>({1, 2, 3, 4, 5, 6}));
  EXPECT_EQ(this->player_.play_requests, 0);
  EXPECT_TRUE(this->errors_.empty());
}

TEST_F(VoiceAssistantResponseTest, LateTtsEndDoesNotReopenFinishedResponse) {
  this->start_streamed_response_();
  this->player_.report(media_player::MEDIA_PLAYER_STATE_ANNOUNCING);
  this->player_.report(media_player::MEDIA_PLAYER_STATE_IDLE);
  this->voice_.loop();
  this->voice_.loop();
  ASSERT_FALSE(this->voice_.is_running());
  EXPECT_EQ(this->completions_(), std::vector<bool>({true}));

  this->event_(api::enums::VOICE_ASSISTANT_TTS_END, "url", "http://example.test/tts.wav");
  this->flush_();
  this->voice_.loop();
  EXPECT_FALSE(this->voice_.is_running());
  EXPECT_EQ(this->player_.play_requests, 1);
  EXPECT_TRUE(this->completions_().empty());
}

TEST_F(VoiceAssistantResponseTest, StopPendingResponseReportsFailureOnce) {
  this->start_streamed_response_();
  this->voice_.request_stop();
  this->voice_.request_stop();
  this->voice_.loop();
  EXPECT_FALSE(this->voice_.is_running());
  EXPECT_EQ(this->completions_(), std::vector<bool>({false}));
  EXPECT_EQ(this->player_.stop_requests, 1);

  this->event_(api::enums::VOICE_ASSISTANT_TTS_END, "url", "http://example.test/tts.wav");
  this->flush_();
  this->voice_.loop();
  EXPECT_FALSE(this->voice_.is_running());
  EXPECT_EQ(this->player_.play_requests, 1);
  EXPECT_TRUE(this->completions_().empty());
}

TEST_F(VoiceAssistantResponseTest, StopPlayingResponseReportsFailureOnce) {
  this->voice_.set_tts_playback_start_timeout(20);
  this->start_streamed_response_();
  this->player_.report(media_player::MEDIA_PLAYER_STATE_ANNOUNCING);
  this->voice_.loop();
  this->voice_.request_stop();
  this->voice_.request_stop();
  this->voice_.loop();
  EXPECT_FALSE(this->voice_.is_running());
  EXPECT_EQ(this->completions_(), std::vector<bool>({false}));
  EXPECT_EQ(this->player_.stop_requests, 1);
  delay(30);
  this->flush_();
  EXPECT_TRUE(this->completions_().empty());
}

TEST_F(VoiceAssistantResponseTest, StopBeforeDeferredTtsEndDoesNotStartPlayback) {
  this->event_(api::enums::VOICE_ASSISTANT_TTS_END, "url", "http://example.test/tts.wav");
  this->voice_.request_stop();
  this->flush_();
  this->voice_.loop();
  EXPECT_FALSE(this->voice_.is_running());
  EXPECT_EQ(this->player_.play_requests, 0);
  EXPECT_EQ(this->completions_(), std::vector<bool>({false}));
}

TEST_F(VoiceAssistantResponseTest, StopBeforeStreamingUrlDoesNotStartLatePlayback) {
  this->event_(api::enums::VOICE_ASSISTANT_RUN_START, "url", "http://example.test/tts.wav");
  this->voice_.request_stop();
  this->event_(api::enums::VOICE_ASSISTANT_INTENT_PROGRESS, "tts_start_streaming", "1");
  this->event_(api::enums::VOICE_ASSISTANT_TTS_START, "text", "Cancelled reply");
  this->event_(api::enums::VOICE_ASSISTANT_TTS_END, "url", "http://example.test/tts.wav");
  this->flush_();
  EXPECT_FALSE(this->voice_.is_running());
  EXPECT_EQ(this->player_.play_requests, 0);
}

TEST_F(VoiceAssistantResponseTest, NormalNonStreamingReplyCompletesOnce) {
  this->event_(api::enums::VOICE_ASSISTANT_TTS_END, "url", "http://example.test/tts.wav");
  this->flush_();
  ASSERT_EQ(this->player_.play_requests, 1);
  this->player_.report(media_player::MEDIA_PLAYER_STATE_ANNOUNCING);
  this->voice_.loop();
  EXPECT_TRUE(this->voice_.is_running());
  EXPECT_TRUE(this->completions_().empty());
  this->player_.report(media_player::MEDIA_PLAYER_STATE_IDLE);
  this->voice_.loop();
  this->voice_.loop();
  EXPECT_FALSE(this->voice_.is_running());
  EXPECT_EQ(this->completions_(), std::vector<bool>({true}));
  this->voice_.request_stop();
  EXPECT_TRUE(this->completions_().empty());
}

TEST_F(VoiceAssistantResponseTest, NewAnnouncementWorksAfterStop) {
  api::VoiceAssistantAnnounceRequest msg;
  msg.media_id = StringRef::from_lit("http://example.test/announce.wav");
  this->voice_.on_announce(msg);
  this->voice_.request_stop();
  this->voice_.loop();
  EXPECT_EQ(this->completions_(), std::vector<bool>({false}));
  this->voice_.on_announce(msg);
  this->player_.report(media_player::MEDIA_PLAYER_STATE_ANNOUNCING);
  this->player_.report(media_player::MEDIA_PLAYER_STATE_IDLE);
  this->voice_.loop();
  this->voice_.loop();
  EXPECT_FALSE(this->voice_.is_running());
  EXPECT_EQ(this->completions_(), std::vector<bool>({true}));
  EXPECT_EQ(this->player_.play_requests, 2);
}

TEST_F(VoiceAssistantResponseTest, StopAnnouncementWhileMicrophoneIsStopping) {
  this->voice_.set_continuous(true);
  api::VoiceAssistantAnnounceRequest msg;
  msg.media_id = StringRef::from_lit("http://example.test/announce.wav");
  this->voice_.on_announce(msg);
  this->voice_.request_stop();
  this->voice_.request_stop();
  this->voice_.loop();
  EXPECT_FALSE(this->voice_.is_running());
  EXPECT_EQ(this->player_.stop_requests, 1);
  EXPECT_EQ(this->completions_(), std::vector<bool>({false}));
}

TEST_F(VoiceAssistantResponseTest, TtsEventsBeforeMicrophoneStopDoNotSkipStoppingIt) {
  this->await_response_before_microphone_stop_();
  this->event_(api::enums::VOICE_ASSISTANT_INTENT_PROGRESS, "tts_start_streaming", "1");
  this->event_(api::enums::VOICE_ASSISTANT_TTS_END, "url", "http://example.test/tts.wav");
  this->flush_();
  ASSERT_EQ(this->player_.play_requests, 1);
  EXPECT_TRUE(this->mic_.is_running());
  this->voice_.loop();
  EXPECT_EQ(this->mic_.stops, 1);
  EXPECT_TRUE(this->mic_.is_stopped());
  this->voice_.loop();
  this->player_.report(media_player::MEDIA_PLAYER_STATE_ANNOUNCING);
  this->player_.report(media_player::MEDIA_PLAYER_STATE_IDLE);
  this->voice_.loop();
  this->voice_.loop();
  EXPECT_FALSE(this->voice_.is_running());
  EXPECT_EQ(this->completions_(), std::vector<bool>({true}));
}

TEST_F(VoiceAssistantResponseTest, StopBeforeMicrophoneStopCancelsResponseOnce) {
  this->await_response_before_microphone_stop_();
  this->event_(api::enums::VOICE_ASSISTANT_INTENT_PROGRESS, "tts_start_streaming", "1");
  this->flush_();
  this->voice_.request_stop();
  this->voice_.request_stop();
  EXPECT_EQ(this->player_.stop_requests, 1);
  EXPECT_EQ(this->completions_(), std::vector<bool>({false}));
  EXPECT_TRUE(this->mic_.is_running());
  this->voice_.loop();
  this->voice_.loop();
  this->voice_.loop();
  EXPECT_EQ(this->mic_.stops, 1);
  EXPECT_TRUE(this->mic_.is_stopped());
  EXPECT_FALSE(this->voice_.is_running());
  this->event_(api::enums::VOICE_ASSISTANT_TTS_END, "url", "http://example.test/tts.wav");
  this->flush_();
  this->voice_.loop();
  EXPECT_EQ(this->player_.play_requests, 1);
  EXPECT_TRUE(this->completions_().empty());
}

TEST_F(VoiceAssistantResponseTest, CompletedResponseBeforeMicrophoneStopKeepsSuccess) {
  this->await_response_before_microphone_stop_();
  this->event_(api::enums::VOICE_ASSISTANT_INTENT_PROGRESS, "tts_start_streaming", "1");
  this->flush_();
  this->player_.report(media_player::MEDIA_PLAYER_STATE_ANNOUNCING);
  this->player_.report(media_player::MEDIA_PLAYER_STATE_IDLE);
  this->voice_.request_stop();
  this->voice_.request_stop();
  EXPECT_EQ(this->player_.stop_requests, 0);
  EXPECT_EQ(this->completions_(), std::vector<bool>({true}));
  this->voice_.loop();
  this->voice_.loop();
  this->voice_.loop();
  EXPECT_EQ(this->mic_.stops, 1);
  EXPECT_TRUE(this->mic_.is_stopped());
  EXPECT_FALSE(this->voice_.is_running());
  EXPECT_TRUE(this->completions_().empty());
}

TEST_F(VoiceAssistantResponseTest, StopAfterPlayerCompletionKeepsSuccessfulResult) {
  this->start_streamed_response_();
  this->player_.report(media_player::MEDIA_PLAYER_STATE_ANNOUNCING);
  this->player_.report(media_player::MEDIA_PLAYER_STATE_IDLE);
  this->voice_.request_stop();
  this->voice_.loop();
  EXPECT_FALSE(this->voice_.is_running());
  EXPECT_EQ(this->player_.stop_requests, 0);
  EXPECT_EQ(this->completions_(), std::vector<bool>({true}));
}

TEST_F(VoiceAssistantResponseTest, IntentProgressPreparesPlayerBeforeStreamingStarts) {
  int hooks = 0;
  this->intent_hook_ = [this, &hooks](const std::string &url) {
    ++hooks;
    EXPECT_EQ(url, "http://example.test/tts.wav");
    EXPECT_EQ(this->player_.play_requests, 0);
  };
  this->start_streamed_response_();
  EXPECT_EQ(hooks, 1);
}

TEST_F(VoiceAssistantResponseTest, StopFromIntentProgressPreventsStreamingPlayback) {
  this->intent_hook_ = [this](const std::string &) { this->voice_.request_stop(); };
  this->event_(api::enums::VOICE_ASSISTANT_RUN_START, "url", "http://example.test/tts.wav");
  this->event_(api::enums::VOICE_ASSISTANT_INTENT_PROGRESS, "tts_start_streaming", "1");
  this->flush_();
  this->voice_.loop();
  EXPECT_EQ(this->player_.play_requests, 0);
  EXPECT_FALSE(this->voice_.is_running());
  EXPECT_EQ(this->completions_(), std::vector<bool>({false}));
}

TEST_F(VoiceAssistantResponseTest, IntentProgressWithoutAudioStillRunsItsHook) {
  int hooks = 0;
  this->intent_hook_ = [&hooks](const std::string &url) {
    ++hooks;
    EXPECT_TRUE(url.empty());
  };
  this->event_(api::enums::VOICE_ASSISTANT_INTENT_PROGRESS);
  this->flush_();
  EXPECT_EQ(hooks, 1);
  EXPECT_EQ(this->player_.play_requests, 0);
}

TEST_F(VoiceAssistantResponseTest, PlaybackStartTimeoutReportsFailureAndRejectsLateEvents) {
  this->voice_.set_tts_playback_start_timeout(20);
  this->start_streamed_response_();
  // The host scheduler uses its native 64-bit clock, not call()'s timestamp.
  delay(30);
  this->flush_();
  this->flush_();
  this->voice_.loop();
  EXPECT_EQ(this->completions_(), std::vector<bool>({false}));
  EXPECT_EQ(this->player_.stop_requests, 1);
  EXPECT_EQ(this->errors_, std::vector<std::string>({"tts-start-timeout"}));
  EXPECT_FALSE(this->voice_.is_running());
  this->event_(api::enums::VOICE_ASSISTANT_TTS_END, "url", "http://example.test/tts.wav");
  this->flush_();
  this->voice_.loop();
  EXPECT_FALSE(this->voice_.is_running());
  EXPECT_EQ(this->player_.play_requests, 1);
  EXPECT_TRUE(this->completions_().empty());
}

TEST_F(VoiceAssistantResponseTest, ErrorBeforeDeferredTtsEndPreventsPlayback) {
  this->event_(api::enums::VOICE_ASSISTANT_TTS_END, "url", "http://example.test/tts.wav");
  this->event_(api::enums::VOICE_ASSISTANT_ERROR, "code", "tts-failed");
  this->flush_();
  EXPECT_EQ(this->player_.play_requests, 0);
  EXPECT_EQ(this->errors_, std::vector<std::string>({"tts-failed"}));
}

TEST_F(VoiceAssistantResponseTest, ErrorRejectsLateStreamingUrl) {
  this->event_(api::enums::VOICE_ASSISTANT_RUN_START, "url", "http://example.test/tts.wav");
  this->event_(api::enums::VOICE_ASSISTANT_ERROR, "code", "tts-failed");
  this->event_(api::enums::VOICE_ASSISTANT_INTENT_PROGRESS, "tts_start_streaming", "1");
  this->flush_();
  EXPECT_EQ(this->player_.play_requests, 0);
}

TEST_F(VoiceAssistantResponseTest, LateTtsStartDoesNotTriggerFinishedResponse) {
  this->start_streamed_response_();
  this->player_.report(media_player::MEDIA_PLAYER_STATE_ANNOUNCING);
  this->player_.report(media_player::MEDIA_PLAYER_STATE_IDLE);
  this->voice_.loop();
  this->voice_.loop();
  int hooks = 0;
  LambdaAction<std::string> action([&hooks](const std::string &) { ++hooks; });
  Automation<std::string> automation(this->voice_.get_tts_start_trigger());
  automation.add_action(&action);
  this->event_(api::enums::VOICE_ASSISTANT_TTS_START, "text", "Late response text");
  this->flush_();
  EXPECT_EQ(hooks, 0);
  EXPECT_FALSE(this->voice_.is_running());
}

}  // namespace esphome::voice_assistant::testing
