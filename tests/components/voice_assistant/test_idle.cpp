#include <gtest/gtest.h>

#include <netinet/in.h>
#include <span>
#include <sys/socket.h>
#include <unistd.h>

#include "esphome/components/api/api_frame_helper_plaintext.h"
#include "esphome/components/api/api_server.h"
#include "esphome/components/voice_assistant/voice_assistant.h"
#include "esphome/core/application.h"
#include "esphome/core/base_automation.h"

// Only PCM capture is doubled. The complete VA, scheduler, Automation and API connection are production code.
namespace esphome::microphone {
void MicrophoneSource::start() { this->enabled_ = true; this->mic_->start(); }
void MicrophoneSource::stop() { this->enabled_ = false; this->mic_->stop(); }
void MicrophoneSource::process_audio_(std::span<const uint8_t>, const audio::AudioStreamInfo &,
                                      std::vector<uint8_t> &) {
  ADD_FAILURE() << "Unexpected PCM in a lifecycle test";
}
audio::AudioStreamInfo MicrophoneSource::get_audio_stream_info() { return {16, 1, 16000}; }
}

namespace esphome::voice_assistant::testing {
class IdleMicrophone : public microphone::Microphone {
 public:
  unsigned starts{0};
  void start() override { ++this->starts; this->state_ = microphone::STATE_RUNNING; }
  void stop() override { this->state_ = microphone::STATE_STOPPED; }
};

class VoiceAssistantIdleTest : public ::testing::Test {
 protected:
  void SetUp() override {
    int listener = ::socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GE(listener, 0);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    ASSERT_EQ(::bind(listener, reinterpret_cast<sockaddr *>(&address), sizeof(address)), 0);
    socklen_t size = sizeof(address);
    ASSERT_EQ(::getsockname(listener, reinterpret_cast<sockaddr *>(&address), &size), 0);
    ASSERT_EQ(::listen(listener, 1), 0);
    int peer = ::socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GE(peer, 0);
    ASSERT_EQ(::connect(peer, reinterpret_cast<sockaddr *>(&address), size), 0);
    int client = ::accept(listener, nullptr, nullptr);
    ::close(listener);
    ASSERT_GE(client, 0);
    this->peer_ = std::make_unique<api::APIPlaintextFrameHelper>(std::make_unique<socket::Socket>(peer));
    ASSERT_EQ(this->peer_->init(), api::APIError::OK);
    this->client_ = std::make_unique<api::APIConnection>(std::make_unique<socket::Socket>(client), &this->server_);
    this->client_->start();
    this->voice_.set_microphone_source(&this->source_);
    this->voice_.set_use_wake_word(false);
    this->voice_.set_noise_suppression_level(0);
    this->voice_.set_auto_gain(0);
    this->voice_.set_volume_multiplier(1.0f);
    this->voice_.set_conversation_timeout(300000);
    this->idle_automation_.add_action(&this->idle_action_);
    this->end_automation_.add_action(&this->end_action_);
    this->voice_.client_subscription(this->client_.get(), true);
    this->voice_.setup();
  }
  void TearDown() override {
    this->restart_once_ = false;
    this->voice_.client_subscription(this->client_.get(), false);
    this->voice_.request_stop();
    this->pump(4);
    for (uint32_t id = 0; id <= 5; ++id) {
      App.scheduler.cancel_timeout(&this->voice_, id);
      App.scheduler.cancel_interval(&this->voice_, id);
    }
    this->client_.reset();
    this->peer_.reset();
    global_voice_assistant = this->previous_voice_;
    api::global_api_server = this->previous_server_;
  }
  void flush() {
    App.scheduler.process_to_add();
    App.scheduler.call(millis());
  }
  void pump(unsigned count) {
    for (unsigned step = 0; step < count; ++step) {
      this->voice_.loop();
      this->flush();
    }
  }
  void event(api::enums::VoiceAssistantEvent type) {
    api::VoiceAssistantEventResponse message;
    message.event_type = type;
    this->voice_.on_event(message);
  }
  void begin_request(bool continuous = false) {
    this->voice_.request_start(continuous, false);
    this->pump(3);
    this->voice_.start_streaming();
    this->event(api::enums::VOICE_ASSISTANT_RUN_START);
  }
  void await_response() {
    this->event(api::enums::VOICE_ASSISTANT_STT_VAD_END);
    this->pump(2);
    ASSERT_TRUE(this->microphone_.is_stopped());
    ASSERT_TRUE(this->voice_.is_running());
  }
  unsigned start_requests() {
    unsigned starts = 0;
    api::ReadPacketBuffer packet;
    while (this->peer_->read_packet(&packet) == api::APIError::OK) {
      if (packet.type == api::VoiceAssistantRequest::MESSAGE_TYPE && packet.data_len >= 2 &&
          packet.data[0] == 0x08 && packet.data[1] == 1) {
        ++starts;
      }
    }
    return starts;
  }

  VoiceAssistant *previous_voice_{global_voice_assistant};
  api::APIServer *previous_server_{api::global_api_server};
  IdleMicrophone microphone_;
  microphone::MicrophoneSource source_{&this->microphone_, 16, 1, false};
  VoiceAssistant voice_;
  unsigned idle_count_{0}, end_count_{0};
  bool restart_once_{false};
  LambdaAction<> idle_action_{[this]() {
    ++this->idle_count_;
    if (this->restart_once_ && this->idle_count_ == 1)
      this->voice_.request_start(false, false);
  }};
  Automation<> idle_automation_{this->voice_.get_idle_trigger()};
  LambdaAction<> end_action_{[this]() { ++this->end_count_; }};
  Automation<> end_automation_{this->voice_.get_end_trigger()};
  api::APIServer server_;
  std::unique_ptr<api::APIConnection> client_;
  std::unique_ptr<api::APIPlaintextFrameHelper> peer_;
};

TEST_F(VoiceAssistantIdleTest, InitialAndRepeatedIdleDoNotInventACompletion) {
  this->pump(3);
  EXPECT_EQ(this->idle_count_, 0u);
  EXPECT_EQ(this->end_count_, 0u);
}

TEST_F(VoiceAssistantIdleTest, NoncontinuousRunEndReportsLocalIdleOnce) {
  this->begin_request();
  this->await_response();
  this->event(api::enums::VOICE_ASSISTANT_RUN_END);
  EXPECT_FALSE(this->voice_.is_running());
  EXPECT_EQ(this->idle_count_, 1u);
  this->pump(3);
  EXPECT_EQ(this->idle_count_, 1u);
  EXPECT_EQ(this->end_count_, 1u);
}

TEST_F(VoiceAssistantIdleTest, IdleAutomationCanStartTheNextRequest) {
  this->begin_request();
  ASSERT_EQ(this->start_requests(), 1u);
  this->await_response();
  this->restart_once_ = true;
  this->event(api::enums::VOICE_ASSISTANT_RUN_END);
  this->pump(3);
  EXPECT_EQ(this->idle_count_, 1u);
  EXPECT_EQ(this->microphone_.starts, 2u);
  EXPECT_EQ(this->start_requests(), 1u);
  EXPECT_TRUE(this->voice_.is_running());
}

TEST_F(VoiceAssistantIdleTest, ContinuousIdleRetainsItsExistingLoopTiming) {
  this->begin_request(true);
  this->await_response();
  this->event(api::enums::VOICE_ASSISTANT_RUN_END);
  EXPECT_EQ(this->idle_count_, 0u);
  this->pump(1);
  EXPECT_EQ(this->idle_count_, 1u);
  this->pump(3);
  EXPECT_EQ(this->idle_count_, 1u);
  EXPECT_EQ(this->microphone_.starts, 2u);
}

TEST_F(VoiceAssistantIdleTest, ExplicitStopReportsIdleIndependentlyOfDeferredRunEnd) {
  this->begin_request();
  this->voice_.request_stop();
  this->pump(2);
  EXPECT_FALSE(this->voice_.is_running());
  EXPECT_EQ(this->idle_count_, 1u);
  EXPECT_EQ(this->end_count_, 0u);
  this->event(api::enums::VOICE_ASSISTANT_RUN_END);
  this->flush();
  EXPECT_EQ(this->idle_count_, 1u);
  EXPECT_EQ(this->end_count_, 1u);
}
}  // namespace esphome::voice_assistant::testing
