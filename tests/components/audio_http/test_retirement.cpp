#ifdef USE_HOST
#include <gtest/gtest.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

#include "esphome/core/application.h"
#include "esphome/core/hal.h"
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
  std::atomic<bool> natural_end{false};
  std::atomic<unsigned> transport_closed{0};
};
HttpFixtureState &fixture() {
  static HttpFixtureState state;
  return state;
}
class FixtureHttpClient : public micro_decoder::HttpClient {
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
    return 0;
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

#ifdef AUDIO_HTTP_TEST_COUNT_THREADS
#include <pthread.h>

// GNU ld --wrap requires these ABI symbols; C++ identifiers stay ordinary names.
extern "C" int real_pthread_create(pthread_t *, const pthread_attr_t *, void *(*)(void *),
                                   void *) asm("__real_pthread_create");
extern "C" int real_pthread_join(pthread_t, void **) asm("__real_pthread_join");
extern "C" int wrapped_pthread_create(pthread_t *, const pthread_attr_t *, void *(*)(void *),
                                      void *) asm("__wrap_pthread_create");
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
#endif
namespace micro_decoder {
std::unique_ptr<HttpClient> create_http_client() {
  return std::make_unique<esphome::audio_http::testing::FixtureHttpClient>();
}
}  // namespace micro_decoder

namespace esphome::audio_http::testing {
std::vector<uint8_t> fixture_bytes(const char *name) {
  std::ifstream input(std::string(AUDIO_HTTP_TEST_FIXTURES) + "/" + name + ".bin", std::ios::binary);
  return {std::istreambuf_iterator<char>(input), {}};
}
// Workers created and joined before an IDLE report; only counted where the linker supports --wrap
void expect_workers_joined([[maybe_unused]] const std::pair<unsigned, unsigned> &at_idle,
                           [[maybe_unused]] unsigned workers) {
#ifdef AUDIO_HTTP_TEST_COUNT_THREADS
  EXPECT_EQ(at_idle, (std::pair<unsigned, unsigned>{workers, workers}));
#endif
}
template<typename Predicate> bool pump_until(AudioHTTPMediaSource &source, Predicate predicate) {
  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (!predicate() && std::chrono::steady_clock::now() < deadline) {
    source.call();
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
      this->at_idle.emplace_back(fixture().threads_created.load(), fixture().threads_joined.load());
      if (this->at_idle.size() == 1 && this->restart_source != nullptr)
        this->restart_accepted = this->restart_source->play_uri("http://fixture.test/next.flac");
    }
  }
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
    fixture().transport_closed.store(0);
    fixture().threads_created.store(0);
    fixture().threads_joined.store(0);
  }
};
// One playback ends naturally; with two, the IDLE listener starts the second from inside its report.
// Without a persistent ring buffer, stop() frees it before the restart allocates a new one.
class HttpPlaybacks : public HttpRetirement, public ::testing::WithParamInterface<std::tuple<unsigned, bool>> {};
TEST_P(HttpPlaybacks, WorkersJoinBeforeEachIdle) {
  using namespace esphome;
  const auto [playbacks, persistent] = GetParam();
  RetirementListener listener;
  audio_http::AudioHTTPMediaSource source;
  if (playbacks > 1)
    listener.restart_source = &source;
  source.media_source::MediaSource::set_listener(&listener);
  source.set_persistent_ring_buffer(persistent);
  source.set_task_stack_in_psram(true);
  source.call();
  ASSERT_TRUE(source.play_uri("http://fixture.test/first.flac"));
  bool ended = pump_until(source, [&] { return listener.at_idle.size() == playbacks; });
  // Capture before explicit cleanup, which must not hide retained workers on the baseline.
  const auto at_idle = listener.at_idle;
  source.handle_command(media_source::MediaSourceCommand::STOP);
  source.call();
  ASSERT_TRUE(ended);
  EXPECT_EQ(listener.restart_accepted, playbacks > 1);
  ASSERT_EQ(at_idle.size(), playbacks);
  const auto reference = fixture_bytes("reference.pcm");
  std::vector<uint8_t> expected;
  for (unsigned i = 0; i < playbacks; ++i) {
    expect_workers_joined(at_idle[i], 2 * (i + 1));
    expected.insert(expected.end(), reference.begin(), reference.end());
  }
  EXPECT_EQ(listener.pcm, expected);
  EXPECT_EQ(fixture().transport_closed.load(), playbacks);
  EXPECT_EQ(source.get_state(), media_source::MediaSourceState::IDLE);
  EXPECT_TRUE(source.is_idle());
}
INSTANTIATE_TEST_SUITE_P(OneAndTwo, HttpPlaybacks, ::testing::Combine(::testing::Values(1u, 2u), ::testing::Bool()));

TEST_F(HttpRetirement, ExplicitStopPublishesIdleOnceAfterJoiningWorkers) {
  using namespace esphome;
  fixture().natural_end.store(false);
  RetirementListener listener;
  audio_http::AudioHTTPMediaSource source;
  source.media_source::MediaSource::set_listener(&listener);
  source.call();
  ASSERT_TRUE(source.play_uri("http://fixture.test/stream.flac"));
  ASSERT_TRUE(pump_until(source, [&] { return source.get_state() == media_source::MediaSourceState::PLAYING; }));

  source.handle_command(media_source::MediaSourceCommand::STOP);
  EXPECT_TRUE(listener.at_idle.empty());
  source.call();
  source.handle_command(media_source::MediaSourceCommand::STOP);
  source.call();
  ASSERT_EQ(listener.at_idle.size(), 1u);
  expect_workers_joined(listener.at_idle.front(), 2);
  EXPECT_TRUE(source.is_idle());
}

}  // namespace esphome::audio_http::testing
#endif
