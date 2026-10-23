#include <gtest/gtest.h>

#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "esphome/components/http_request/http_request.h"
#include "esphome/core/base_automation.h"

namespace esphome::http_request::testing {

// A response that stays pending for a number of polls, then serves its body in fixed chunks
class FakeContainer : public HttpContainer {
 public:
  HttpRequestProgress poll() override {
    this->polls++;
    if (this->pending_polls > 0) {
      this->pending_polls--;
      return HttpRequestProgress::HTTP_REQUEST_PROGRESS_PENDING;
    }
    return this->result;
  }
  int read(uint8_t *buf, size_t max_len) override { return this->read_available(buf, max_len); }
  int read_available(uint8_t *buf, size_t max_len) override {
    if (this->stall)
      return 0;
    size_t len = std::min({max_len, this->chunk, this->body.size() - this->bytes_read_});
    std::memcpy(buf, this->body.data() + this->bytes_read_, len);
    this->bytes_read_ += len;
    return static_cast<int>(len);
  }
  void end() override { this->ends++; }

  int pending_polls{0};
  HttpRequestProgress result{HttpRequestProgress::HTTP_REQUEST_PROGRESS_DONE};
  std::string body;
  size_t chunk{512};
  bool stall{false};
  int polls{0};
  int ends{0};
};

class FakeHttpRequest : public HttpRequestComponent {
 public:
  std::shared_ptr<HttpContainer> begin(const char *url, const char *method, std::string body,
                                       const std::vector<Header> &request_headers,
                                       const std::vector<std::string> &lower_case_collect_headers,
                                       uint32_t timeout_ms) override {
    this->begins++;
    if (this->refuse)
      return nullptr;
    auto container = std::make_shared<FakeContainer>();
    container->status_code = 200;
    container->pending_polls = this->pending_polls;
    container->body = this->response_body;
    container->content_length = this->response_body.size();
    this->containers.push_back(container);
    return container;
  }

  int begins{0};
  bool refuse{false};
  int pending_polls{2};
  std::string response_body;
  std::vector<std::shared_ptr<FakeContainer>> containers;

 protected:
  std::shared_ptr<HttpContainer> perform(const char *url, const char *method, const std::string &body,
                                         const std::vector<Header> &request_headers,
                                         const std::vector<std::string> &lower_case_collect_headers,
                                         uint32_t timeout_ms) override {
    return nullptr;
  }
};

class CountAction : public Action<> {
 public:
  explicit CountAction(int *count) : count_(count) {}

 protected:
  void play() override { (*this->count_)++; }
  int *count_;
};

class HttpRequestSendActionTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->action_.set_url(std::string("http://example.invalid/"));
    this->action_.set_method([]() -> const char * { return "GET"; });
    this->action_.set_max_response_buffer_size(1024);
    this->automation_.add_actions({&this->action_, &this->next_});
  }

  // One pass of the main loop
  void loop() {
    LoopBlockingGuard guard{nullptr, nullptr, millis()};
    this->action_.loop();
  }

  void run_until_idle() {
    for (int i = 0; i != 100 && this->automation_.is_running(); i++)
      this->loop();
  }

  void capture_response() {
    this->action_.set_capture_response([]() { return true; });
    this->with_body_.add_action(&this->store_body_);
  }

  FakeHttpRequest http_;
  HttpRequestSendAction<> action_{&this->http_};
  int next_count_{0};
  CountAction next_{&this->next_count_};
  Trigger<> trigger_;
  Automation<> automation_{&this->trigger_};

  int responses_{0};
  int errors_{0};
  std::string body_;
  Automation<std::shared_ptr<HttpContainer>> on_response_{this->action_.get_success_trigger()};
  LambdaAction<std::shared_ptr<HttpContainer>> count_response_{
      [this](const std::shared_ptr<HttpContainer> &) { this->responses_++; }};
  Automation<> on_error_{this->action_.get_error_trigger()};
  CountAction count_error_{&this->errors_};
  Automation<std::shared_ptr<HttpContainer>, std::string &> with_body_{
      this->action_.get_success_trigger_with_response()};
  LambdaAction<std::shared_ptr<HttpContainer>, std::string &> store_body_{
      [this](const std::shared_ptr<HttpContainer> &, std::string &body) {
        this->responses_++;
        this->body_ = body;
      }};

 public:
  HttpRequestSendActionTest() {
    this->on_response_.add_action(&this->count_response_);
    this->on_error_.add_action(&this->count_error_);
  }
};

TEST_F(HttpRequestSendActionTest, ChainWaitsForTheResponse) {
  this->trigger_.trigger();
  EXPECT_EQ(this->http_.begins, 1);
  EXPECT_TRUE(this->automation_.is_running());
  EXPECT_EQ(this->next_count_, 0);

  this->loop();
  this->loop();
  EXPECT_EQ(this->next_count_, 0);
  EXPECT_EQ(this->responses_, 0);

  this->loop();
  EXPECT_EQ(this->responses_, 1);
  EXPECT_EQ(this->errors_, 0);
  EXPECT_EQ(this->next_count_, 1);
  EXPECT_EQ(this->http_.containers[0]->ends, 1);
  EXPECT_FALSE(this->automation_.is_running());
}

TEST_F(HttpRequestSendActionTest, RequestThatCannotStartFiresOnErrorAtOnce) {
  this->http_.refuse = true;
  this->trigger_.trigger();
  EXPECT_EQ(this->errors_, 1);
  EXPECT_EQ(this->responses_, 0);
  EXPECT_EQ(this->next_count_, 1);
  EXPECT_FALSE(this->automation_.is_running());
}

TEST_F(HttpRequestSendActionTest, FailedRequestFiresOnError) {
  this->trigger_.trigger();
  this->http_.containers[0]->result = HttpRequestProgress::HTTP_REQUEST_PROGRESS_FAILED;
  this->run_until_idle();
  EXPECT_EQ(this->errors_, 1);
  EXPECT_EQ(this->responses_, 0);
  EXPECT_EQ(this->next_count_, 1);
  EXPECT_EQ(this->http_.containers[0]->ends, 1);
}

TEST_F(HttpRequestSendActionTest, SecondTriggerWaitsForTheFirstRequest) {
  this->trigger_.trigger();
  this->trigger_.trigger();
  EXPECT_EQ(this->http_.begins, 1);

  this->loop();
  this->loop();
  EXPECT_EQ(this->http_.begins, 1);

  // The first finishes and the second starts in the same pass
  this->loop();
  EXPECT_EQ(this->responses_, 1);
  EXPECT_EQ(this->http_.begins, 2);

  this->run_until_idle();
  EXPECT_EQ(this->responses_, 2);
  EXPECT_EQ(this->next_count_, 2);
}

TEST_F(HttpRequestSendActionTest, TriggersBeyondTheQueueAreDropped) {
  for (int i = 0; i != 7; i++)
    this->trigger_.trigger();
  // One running, four waiting, two dropped with on_error
  EXPECT_EQ(this->http_.begins, 1);
  EXPECT_EQ(this->errors_, 2);
  EXPECT_EQ(this->next_count_, 2);

  this->run_until_idle();
  EXPECT_EQ(this->http_.begins, 5);
  EXPECT_EQ(this->responses_, 5);
  EXPECT_EQ(this->next_count_, 7);
}

TEST_F(HttpRequestSendActionTest, WaitingRequestThatCannotStartFiresOnError) {
  this->trigger_.trigger();
  this->trigger_.trigger();
  this->http_.refuse = true;
  this->run_until_idle();
  EXPECT_EQ(this->responses_, 1);
  EXPECT_EQ(this->errors_, 1);
  EXPECT_EQ(this->next_count_, 2);
}

TEST_F(HttpRequestSendActionTest, StopEndsRequestsWithoutTriggers) {
  this->trigger_.trigger();
  this->trigger_.trigger();
  this->automation_.stop();
  EXPECT_FALSE(this->automation_.is_running());
  EXPECT_EQ(this->http_.containers[0]->ends, 1);

  this->loop();
  this->loop();
  this->loop();
  EXPECT_EQ(this->http_.begins, 1);
  EXPECT_EQ(this->responses_, 0);
  EXPECT_EQ(this->errors_, 0);
  EXPECT_EQ(this->next_count_, 0);

  // The action is usable again after a stop
  this->trigger_.trigger();
  this->run_until_idle();
  EXPECT_EQ(this->responses_, 1);
  EXPECT_EQ(this->next_count_, 1);
}

TEST_F(HttpRequestSendActionTest, StopFromOnResponseDropsTheRest) {
  LambdaAction<std::shared_ptr<HttpContainer>> stop{
      [this](const std::shared_ptr<HttpContainer> &) { this->automation_.stop(); }};
  this->on_response_.add_action(&stop);
  this->trigger_.trigger();
  this->trigger_.trigger();
  this->run_until_idle();
  EXPECT_EQ(this->responses_, 1);
  EXPECT_EQ(this->http_.begins, 1);
  EXPECT_EQ(this->next_count_, 0);
  EXPECT_FALSE(this->automation_.is_running());
}

TEST_F(HttpRequestSendActionTest, TriggerFromOnResponseStartsAtOnce) {
  int retriggers = 0;
  LambdaAction<std::shared_ptr<HttpContainer>> again{[this, &retriggers](const std::shared_ptr<HttpContainer> &) {
    if (retriggers++ == 0)
      this->trigger_.trigger();
  }};
  this->on_response_.add_action(&again);
  this->trigger_.trigger();
  this->loop();
  this->loop();
  this->loop();
  EXPECT_EQ(this->http_.begins, 2);

  this->run_until_idle();
  EXPECT_EQ(this->responses_, 2);
  EXPECT_EQ(this->next_count_, 2);
}

TEST_F(HttpRequestSendActionTest, CapturedBodyIsReadAcrossPasses) {
  this->capture_response();
  this->http_.pending_polls = 0;
  this->http_.response_body = std::string(900, 'a');
  this->trigger_.trigger();
  this->http_.containers[0]->chunk = 100;
  this->run_until_idle();
  EXPECT_EQ(this->responses_, 1);
  EXPECT_EQ(this->body_, std::string(900, 'a'));
  EXPECT_EQ(this->next_count_, 1);
}

TEST_F(HttpRequestSendActionTest, CapturedBodyStopsAtTheBufferSize) {
  this->capture_response();
  this->action_.set_max_response_buffer_size(300);
  this->http_.response_body = std::string(1000, 'b');
  this->trigger_.trigger();
  this->run_until_idle();
  EXPECT_EQ(this->body_, std::string(300, 'b'));
}

TEST_F(HttpRequestSendActionTest, StalledBodyEndsAtTheTimeout) {
  this->capture_response();
  this->action_.set_timeout([]() -> uint32_t { return 20; });
  this->http_.response_body = std::string(1000, 'c');
  this->trigger_.trigger();
  this->http_.containers[0]->stall = true;
  this->loop();
  this->loop();
  this->loop();
  EXPECT_TRUE(this->automation_.is_running());

  delay(30);
  this->loop();
  EXPECT_FALSE(this->automation_.is_running());
  EXPECT_EQ(this->responses_, 1);
  EXPECT_TRUE(this->body_.empty());
}

}  // namespace esphome::http_request::testing
