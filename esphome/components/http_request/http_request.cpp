#include "http_request.h"

#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include <cinttypes>

namespace esphome::http_request {

ESPHOME_LOG_TAG(TAG, "http_request");

void HttpRequestComponent::dump_config() {
  ESP_LOGCONFIG(TAG,
                "HTTP Request:\n"
                "  Timeout: %" PRIu32 "ms\n"
                "  User-Agent: %s\n"
                "  Follow redirects: %s\n"
                "  Redirect limit: %d\n"
                "  Max concurrent requests: %u",
                this->timeout_, this->useragent_, YESNO(this->follow_redirects_), this->redirect_limit_,
                this->max_concurrent_);
  if (this->watchdog_timeout_ > 0) {
    ESP_LOGCONFIG(TAG, "  Watchdog Timeout: %" PRIu32 "ms", this->watchdog_timeout_);
  }
}

std::string HttpContainer::get_response_header(const std::string &header_name) {
  auto lower = str_lower_case(header_name);  // NOLINT
  for (const auto &entry : this->response_headers_) {
    if (entry.name == lower) {
      ESP_LOGV(TAG, "Header with name %s found with value %s", lower.c_str(), entry.value.c_str());
      return entry.value;
    }
  }
  return "";
}

void PendingRequest::free_buffer() {
  if (this->buf != nullptr)
    RAMAllocator<uint8_t>().deallocate(this->buf, this->buf_size);
  this->buf = nullptr;
}

void HttpRequestComponent::setup() {
  // An on_boot automation may already have queued a request
  if (this->requests_ == nullptr)
    this->disable_loop();
}

void HttpRequestComponent::loop() {
  if (!this->process_())
    this->disable_loop();
}

void HttpRequestComponent::on_safe_shutdown() {
  // Called before any on_shutdown(). Drop the requests already made, so that they do not hold up the reboot;
  // only the ones that on_shutdown automations make are sent.
  this->shutting_down_ = true;
  this->busy_++;
  unsigned dropped = 0;
  for (PendingRequest *request = this->requests_; request != nullptr; request = request->next) {
    if (request->finished || request->cancelled)
      continue;
    dropped++;
    request->cancelled = true;
    // As script.stop would, so that a script that was waiting for it can run again from on_shutdown
    request->handler(request, RequestEvent::REQUEST_EVENT_ABANDON);
  }
  this->busy_--;
  if (dropped != 0) {
    ESP_LOGW(TAG, "Shutting down; %u requests dropped", dropped);
  }
  if (this->busy_ == 0)
    this->remove_dead_();
}

void HttpRequestComponent::on_shutdown() {
  // Shutdown runs no loop(), so finish the requests here, blocking as they did before. A forced reboot calls
  // only this, so it also finishes the requests made before it.
  this->shutting_down_ = true;
  this->drain_();
}

void HttpRequestComponent::submit(PendingRequest *request) {
  this->busy_++;
  uint8_t running = 0;
  uint8_t waiting = 0;
  PendingRequest **link = &this->requests_;
  for (; *link != nullptr; link = &(*link)->next) {
    const PendingRequest *other = *link;
    if (other->finished || other->cancelled)
      continue;
    if (other->container == nullptr) {
      waiting++;
    } else {
      running++;
    }
  }
  *link = request;
  this->enable_loop();
  if (waiting >= MAX_QUEUED) {
    ESP_LOGW(TAG, "Too many requests waiting; request dropped");
    request->failed = true;
    this->finish_(request);
  } else if (waiting == 0 && running < this->max_concurrent_) {
    // Start at once; one that cannot start is reported at once
    this->begin_request_(request);
    if (request->failed)
      this->finish_(request);
  }
  this->busy_--;
  if (this->busy_ != 0)
    return;
  if (this->shutting_down_) {
    this->drain_();
  } else {
    this->remove_dead_();
  }
}

void HttpRequestComponent::cancel(void *owner) {
  for (PendingRequest *request = this->requests_; request != nullptr; request = request->next) {
    if (request->owner == owner)
      request->cancelled = true;
  }
  if (this->busy_ == 0)
    this->remove_dead_();
}

bool HttpRequestComponent::process_() {
  this->busy_++;
  // The first max_concurrent_ live requests run; a request finished here frees its slot on the next pass
  uint8_t slots = this->max_concurrent_;
  for (PendingRequest *request = this->requests_; request != nullptr && slots != 0; request = request->next) {
    if (request->finished || request->cancelled)
      continue;
    slots--;
    if (request->container == nullptr)
      this->begin_request_(request);
    if (!request->cancelled && (request->failed || this->advance_(request)))
      this->finish_(request);
  }
  this->busy_--;
  if (this->busy_ == 0)
    this->remove_dead_();
  return this->requests_ != nullptr;
}

void HttpRequestComponent::drain_() {
  // A call further up holds request pointers, and runs the new requests itself
  if (this->busy_ != 0)
    return;
  while (this->process_()) {
    App.feed_wdt();
    delay(1);
  }
}

void HttpRequestComponent::begin_request_(PendingRequest *request) {
  request->start_time = millis();
  request->handler(request, RequestEvent::REQUEST_EVENT_START);
  // Stopped from the request's own lambdas: it is removed without being polled
  if (!request->cancelled && request->container == nullptr)
    request->failed = true;
}

bool HttpRequestComponent::advance_(PendingRequest *request) {
  if (!request->reading) {
    auto progress = request->container->poll();
    if (progress == HttpRequestProgress::HTTP_REQUEST_PROGRESS_PENDING)
      return false;
    if (progress == HttpRequestProgress::HTTP_REQUEST_PROGRESS_FAILED) {
      request->failed = true;
      return true;
    }
    if (!request->capture)
      return true;
    request->buf = RAMAllocator<uint8_t>().allocate(request->buf_size);
    if (request->buf == nullptr) {
      // As before, on_response still fires, with an empty body
      ESP_LOGW(TAG, "Could not allocate %zu bytes for the response body", request->buf_size);
      return true;
    }
    request->reading = true;
    // millis(), not the loop time: poll() may have blocked earlier in this pass
    request->last_data_time = millis();
  }
  // NOTE: HttpContainer::read() has non-BSD socket semantics - see http_request.h
  // millis(), not the loop time: the budget is shorter than one pass of the loop
  const uint32_t start = millis();
  while (request->read_index < request->buf_size) {
    int read_or_error =
        request->container->read_available(request->buf + request->read_index, request->buf_size - request->read_index);
    const uint32_t now = millis();
    // As before, a body that is cut off still fires on_response with what was read
    if (read_or_error < 0) {
      ESP_LOGW(TAG, "Response body read failed after %zu bytes", request->read_index);
      return true;
    }
    if (read_or_error == 0) {
      if (request->container->is_read_complete())
        return true;
      if (now - request->last_data_time < request->timeout)
        return false;
      ESP_LOGW(TAG, "Response body timed out after %zu bytes", request->read_index);
      return true;
    }
    request->read_index += read_or_error;
    request->last_data_time = now;
    if (now - start >= READ_BUDGET_MS)
      return false;
  }
  // As before, on_response still fires, with the part that fitted
  if (!request->container->is_read_complete()) {
    ESP_LOGW(TAG, "Response body is larger than max_response_buffer_size; kept the first %zu bytes", request->buf_size);
  }
  return true;
}

void HttpRequestComponent::finish_(PendingRequest *request) {
  request->finished = true;
  // The body was read over several passes, so time it from the start, as a blocking read would
  if (request->reading && request->container != nullptr)
    request->container->duration_ms = millis() - request->start_time;
  request->handler(request, RequestEvent::REQUEST_EVENT_FINISH);
}

void HttpRequestComponent::remove_dead_() {
  PendingRequest **link = &this->requests_;
  while (PendingRequest *request = *link) {
    if (!request->finished && !request->cancelled) {
      link = &request->next;
      continue;
    }
    *link = request->next;
    if (request->container != nullptr)
      request->container->end();
    request->free_buffer();
    request->handler(request, RequestEvent::REQUEST_EVENT_DESTROY);
  }
}

}  // namespace esphome::http_request
