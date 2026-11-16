#ifdef USE_HOST
#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <fstream>
#include <iterator>
#include <pthread.h>
#include <thread>
#include <vector>

#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/components/audio/audio.h"
#include "esphome/components/media_source/media_source.h"
#include "platform/http_client.h"

// Enable only this existing ESP adapter translation unit; the workers remain native pthreads.
#define USE_ESP32
// NOLINTNEXTLINE(bugprone-suspicious-include): exercise production code with the host SDK boundary.
#include "esphome/components/audio_http/audio_http_media_source.cpp"
#undef USE_ESP32

namespace esphome::audio_http::testing {
struct HttpFixtureState {
  std::atomic<unsigned> threads_created{0}, threads_joined{0};
  std::vector<uint8_t> body;
  std::atomic<bool> natural_end{false}, reader_fault{false};
  std::atomic<unsigned> transport_errors{0}, transport_closed{0};
};
HttpFixtureState &fixture() {
  static HttpFixtureState state;
  return state;
}
class FaultHttpClient : public micro_decoder::HttpClient {
 public:
  bool open(const micro_decoder::HttpRequest &) override { return true; }
  const micro_decoder::HttpResponse &response_info() const override { return this->response_; }
  int read(uint8_t *data, size_t capacity) override {
    if (this->position_ < fixture().body.size()) {
      size_t length = std::min(capacity, fixture().body.size() - this->position_);
      std::memcpy(data, fixture().body.data() + this->position_, length);
      this->position_ += length;
      return static_cast<int>(length);
    }
    if (!fixture().reader_fault.load())
      return 0;
    ++fixture().transport_errors;
    return -1;
  }
  bool is_complete() const override { return fixture().natural_end.load() && this->position_ == fixture().body.size(); }
  void close() override {
    if (!this->closed_) {
      this->closed_ = true;
      ++fixture().transport_closed;
    }
  }

 private:
  micro_decoder::HttpResponse response_{200, "audio/flac"};
  size_t position_{0};
  bool closed_{false};
};

}  // namespace esphome::audio_http::testing

// GNU ld --wrap requires these ABI symbols; C++ identifiers stay ordinary names.
extern "C" int real_pthread_create(pthread_t *, const pthread_attr_t *, void *(*)(void *), void *)
    asm("__real_pthread_create");
extern "C" int real_pthread_join(pthread_t, void **) asm("__real_pthread_join");
extern "C" int wrapped_pthread_create(pthread_t *, const pthread_attr_t *, void *(*)(void *), void *)
    asm("__wrap_pthread_create");
extern "C" int wrapped_pthread_join(pthread_t, void **) asm("__wrap_pthread_join");
extern "C" int wrapped_pthread_create(pthread_t *thread, const pthread_attr_t *attributes, void *(*entry)(void *),
                                       void *argument) {
  int result = real_pthread_create(thread, attributes, entry, argument);
  if (result == 0)
    ++esphome::audio_http::testing::fixture().threads_created;
  return result;
}
extern "C" int wrapped_pthread_join(pthread_t thread, void **value) {
  int result = real_pthread_join(thread, value);
  if (result == 0)
    ++esphome::audio_http::testing::fixture().threads_joined;
  return result;
}
namespace micro_decoder {
std::unique_ptr<HttpClient> create_http_client() {
  return std::make_unique<esphome::audio_http::testing::FaultHttpClient>();
}
}  // namespace micro_decoder

namespace esphome::audio_http::testing {
std::vector<uint8_t> fixture_bytes(const char *name) {
  std::ifstream input(std::string(AUDIO_HTTP_TEST_FIXTURES) + "/" + name + ".bin", std::ios::binary);
  return {std::istreambuf_iterator<char>(input), {}};
}
template<typename Predicate, typename Pump> bool pump_until(Predicate predicate, Pump pump) {
  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (!predicate() && std::chrono::steady_clock::now() < deadline) {
    pump();
    App.scheduler.call(millis());
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return predicate();
}
class RetirementListener : public esphome::media_source::MediaSourceListener {
 public:
  size_t write_audio(const uint8_t *data, size_t length, uint32_t, const esphome::audio::AudioStreamInfo &) override {
    this->pcm.insert(this->pcm.end(), data, data + length);
    return length;
  }
  void report_state(esphome::media_source::MediaSourceState state) override {
    if (state == esphome::media_source::MediaSourceState::IDLE) {
      this->at_idle.emplace_back(fixture().threads_created.load() - this->created_before,
                                 fixture().threads_joined.load() - this->joined_before);
      if (this->at_idle.size() == 1 && this->restart_source != nullptr)
        this->restart_accepted = this->restart_source->play_uri("http://fixture.test/next.flac");
    }
  }
  const unsigned created_before{fixture().threads_created.load()};
  const unsigned joined_before{fixture().threads_joined.load()};
  std::vector<std::pair<unsigned, unsigned>> at_idle;
  std::vector<uint8_t> pcm;
  esphome::audio_http::AudioHTTPMediaSource *restart_source{nullptr};
  bool restart_accepted{false};
};

class HttpRetirement : public ::testing::Test {
 protected:
  void SetUp() override {
    fixture().body = fixture_bytes("fixture.flac");
    ASSERT_GT(fixture().body.size(), 1024u);
    fixture().natural_end.store(true);
    fixture().reader_fault.store(false);
    fixture().transport_errors.store(0);
    fixture().transport_closed.store(0);
  }
};
TEST_F(HttpRetirement, NaturalEndJoinsRealWorkersBeforePublishingIdle) {
  using namespace esphome;
  ASSERT_GT(fixture().body.size(), 1024u);
  RetirementListener listener;
  audio_http::AudioHTTPMediaSource source;
  source.media_source::MediaSource::set_listener(&listener);
  source.set_persistent_ring_buffer(true);
  source.set_task_stack_in_psram(true);
  source.call();
  ASSERT_TRUE(source.play_uri("http://fixture.test/natural.flac"));
  bool ended = pump_until([&] { return listener.at_idle.size() == 1; }, [&] { source.call(); });
  // Capture before explicit cleanup, which must not hide retained workers on the baseline.
  const auto at_idle = listener.at_idle;
  source.handle_command(media_source::MediaSourceCommand::STOP);
  source.call();
  ASSERT_TRUE(ended);
  ASSERT_EQ(at_idle.size(), 1u);
  EXPECT_EQ(at_idle.front(), (std::pair<unsigned, unsigned>{2, 2}));
  EXPECT_EQ(listener.pcm, fixture_bytes("reference.pcm"));
  EXPECT_EQ(fixture().transport_errors.load(), 0u);
  EXPECT_EQ(fixture().transport_closed.load(), 1u);
  EXPECT_EQ(source.get_state(), media_source::MediaSourceState::IDLE);
  EXPECT_TRUE(source.is_idle());
}

TEST_F(HttpRetirement, IdleListenerCanStartTheNextRealHttpPlayback) {
  using namespace esphome;
  RetirementListener listener;
  audio_http::AudioHTTPMediaSource source;
  listener.restart_source = &source;
  source.media_source::MediaSource::set_listener(&listener);
  source.set_persistent_ring_buffer(true);
  source.call();
  ASSERT_TRUE(source.play_uri("http://fixture.test/first.flac"));
  bool ended = pump_until([&] { return listener.at_idle.size() == 2; }, [&] { source.call(); });
  const auto at_idle = listener.at_idle;
  source.handle_command(media_source::MediaSourceCommand::STOP);
  source.call();
  ASSERT_TRUE(ended);
  ASSERT_TRUE(listener.restart_accepted);
  ASSERT_EQ(at_idle.size(), 2u);
  EXPECT_EQ(at_idle[0], (std::pair<unsigned, unsigned>{2, 2}));
  EXPECT_EQ(at_idle[1], (std::pair<unsigned, unsigned>{4, 4}));
  auto expected = fixture_bytes("reference.pcm");
  const auto second = expected;
  expected.insert(expected.end(), second.begin(), second.end());
  EXPECT_EQ(listener.pcm, expected);
  EXPECT_EQ(fixture().transport_closed.load(), 2u);
  EXPECT_EQ(fixture().transport_errors.load(), 0u);
  EXPECT_TRUE(source.is_idle());
}

}  // namespace esphome::audio_http::testing
#endif
