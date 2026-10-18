#include <gtest/gtest.h>

#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "esphome/components/api/api_frame_helper_plaintext.h"
#include "esphome/components/api/api_server.h"
#include "esphome/components/voice_assistant/voice_assistant.h"
#include "esphome/core/application.h"

// PCM capture is outside these response tests. Fail if a scenario starts using it.
namespace esphome::microphone {
void MicrophoneSource::start() { ADD_FAILURE() << "Unexpected microphone capture"; }
void MicrophoneSource::stop() {}
void MicrophoneSource::process_audio_(const std::vector<uint8_t> &, std::vector<uint8_t> &) {
  ADD_FAILURE() << "Unexpected microphone PCM";
}
audio::AudioStreamInfo MicrophoneSource::get_audio_stream_info() { return {16, 1, 16000}; }
}  // namespace esphome::microphone

namespace esphome::voice_assistant::testing {

class TestMicrophone : public microphone::Microphone {
 public:
  void start() override { this->state_ = microphone::STATE_RUNNING; }
  void stop() override { this->state_ = microphone::STATE_STOPPED; }
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
    this->voice_.set_media_player(&this->player_);
    this->voice_.setup();
  }

  void TearDown() override {
    this->voice_.request_stop();
    this->flush_();
    // Numeric timeout IDs are local to VoiceAssistant.
    for (uint32_t id = 0; id < 4; ++id) {
      App.scheduler.cancel_timeout(&this->voice_, id);
      App.scheduler.cancel_interval(&this->voice_, id);
    }
    this->client_.reset();
    this->peer_.reset();
    global_voice_assistant = this->previous_voice_;
    api::global_api_server = this->previous_server_;
  }

  void flush_() {
    App.scheduler.process_to_add();
    App.scheduler.call(millis());
  }

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
  microphone::MicrophoneSource mic_source_{&this->mic_, 16, 1, false};
  TestMediaPlayer player_;
  VoiceAssistant voice_;
  api::APIServer server_;
  std::unique_ptr<api::APIConnection> client_;
  std::unique_ptr<api::APIPlaintextFrameHelper> peer_;
};

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
  this->start_streamed_response_();
  this->player_.report(media_player::MEDIA_PLAYER_STATE_ANNOUNCING);
  this->voice_.loop();
  this->voice_.request_stop();
  this->voice_.request_stop();
  this->voice_.loop();
  EXPECT_FALSE(this->voice_.is_running());
  EXPECT_EQ(this->completions_(), std::vector<bool>({false}));
  EXPECT_EQ(this->player_.stop_requests, 1);
  App.scheduler.call(millis() + 5000);
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

}  // namespace esphome::voice_assistant::testing
