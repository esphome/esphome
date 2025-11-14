#ifdef USE_HOST

#define USE_HTTP_REQUEST_HOST_H
#include <curl/curl.h>
#include "http_request_host.h"

#include <format>
#include "esphome/components/network/util.h"
#include "esphome/components/watchdog/watchdog.h"

#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome {
namespace http_request {

static const char *const TAG = "http_request.host";

static size_t body_callback(char *ptr, size_t size, size_t nmemb, void *userdata) {
  auto container_body = static_cast<std::vector<uint8_t> *>(userdata);
  auto p = reinterpret_cast<const uint8_t *>(ptr);

  container_body->insert(container_body->end(), p, p + nmemb);
  return nmemb;
}

static void handle_multi_info(CURLM *multi) {
  struct CURLMsg *m;
  do {
    int msgq = 0;
    m = curl_multi_info_read(multi, &msgq);
    if (m && (m->msg == CURLMSG_DONE)) {
      CURL *e = m->easy_handle;
      if (m->data.result != CURLE_OK) {
        char *url = NULL;
        CURLcode rc = curl_easy_getinfo(e, CURLINFO_EFFECTIVE_URL, &url);
        assert(rc == CURLE_OK);
        ESP_LOGE(TAG, "HTTP Request failed; URL: %s, error: %s", url, curl_easy_strerror(m->data.result));
      }
      /* m->data.result holds the error code for the transfer */
      curl_multi_remove_handle(multi, e);
    }
  } while (m);
}

std::shared_ptr<HttpContainer> HttpRequestHost::perform(const std::string &url, const std::string &method,
                                                        const std::string &body,
                                                        const std::list<Header> &request_headers,
                                                        const std::set<std::string> &response_headers) {
  if (!network::is_connected()) {
    this->status_momentary_error("failed", 1000);
    ESP_LOGW(TAG, "HTTP Request failed; Not connected to network");
    return nullptr;
  }

  CURLM *multi = curl_multi_init();
  CURL *handle = curl_easy_init();
  assert(multi);
  assert(handle);

  CURLcode rc;

  rc = curl_easy_setopt(handle, CURLOPT_PROTOCOLS_STR, "http,https");
  assert(rc == CURLE_OK);

  rc = curl_easy_setopt(handle, CURLOPT_VERBOSE, this->verbose_ ? 1L : 0L);
  assert(rc == CURLE_OK);

  rc = curl_easy_setopt(handle, CURLOPT_URL, url.c_str());
  assert(rc == CURLE_OK);

  curl_slist *headers_slist = nullptr;
  for (const auto &[name, value] : request_headers) {
    headers_slist = curl_slist_append(headers_slist, std::format("{}: {}", name, value).c_str());
    assert(headers_slist != nullptr);
  }
  rc = curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headers_slist);
  assert(rc == CURLE_OK);

  rc = curl_easy_setopt(handle, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
  assert(rc == CURLE_OK);

  rc = curl_easy_setopt(handle, CURLOPT_COPYPOSTFIELDS, body.c_str());
  assert(rc == CURLE_OK);

  rc = curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, method.c_str());
  assert(rc == CURLE_OK);

  rc = curl_easy_setopt(handle, CURLOPT_USERAGENT, this->useragent_);
  assert(rc == CURLE_OK);

  if (this->follow_redirects_) {
    rc = curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 2L /* CURLFOLLOW_OBEYCODE */);
    assert(rc == CURLE_OK);

    rc = curl_easy_setopt(handle, CURLOPT_MAXREDIRS, static_cast<long>(this->redirect_limit_));
    assert(rc == CURLE_OK);
  }

  if (this->ca_path_ != nullptr) {
    rc = curl_easy_setopt(handle, CURLOPT_CAPATH, this->ca_path_);
    assert(rc == CURLE_OK);
  }

  CURLMcode mc = curl_multi_add_handle(multi, handle);
  assert(mc == CURLM_OK);

  std::shared_ptr<HttpContainerHost> container = std::make_shared<HttpContainerHost>(multi, handle, headers_slist);
  container->set_parent(this);
  container->start_ms = millis();

#ifdef USE_HTTP_REQUEST_RESPONSE
  rc = curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, body_callback);
  assert(rc == CURLE_OK);

  rc = curl_easy_setopt(handle, CURLOPT_WRITEDATA, static_cast<void *>(&container->response_body_));
  assert(rc == CURLE_OK);
#else
  rc = curl_easy_setopt(handle, CURLOPT_NOBODY, 1L);
  assert(rc == CURLE_OK);
#endif

  // this is a no-op on host platform, but we keep it here for consistency
  watchdog::WatchdogManager wdm(this->get_watchdog_timeout());

  int running_handles;
  long response_code;
  long redirect_count = 0L;
  long last_redirect_count = 0L;
  char *current_url = nullptr;

  while (true) {
    CURLMcode mc = curl_multi_perform(multi, &running_handles);
    assert(mc == CURLM_OK);

    handle_multi_info(multi);

    rc = curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &response_code);
    assert(rc == CURLE_OK);

    rc = curl_easy_getinfo(handle, CURLINFO_EFFECTIVE_URL, &current_url);
    assert(rc == CURLE_OK);

    rc = curl_easy_getinfo(handle, CURLINFO_REDIRECT_COUNT, &redirect_count);
    assert(rc == CURLE_OK);

    bool is_redirect = (response_code >= HTTP_STATUS_MULTIPLE_CHOICES) && (response_code < HTTP_STATUS_BAD_REQUEST);

    // block until we received the status code of our final request
    if (response_code != 0L && (!this->follow_redirects_ || !is_redirect)) {
      break;
    }

    if (running_handles == 0) {
      // transfer finished but no response code -> error
      break;
    }

    if (is_redirect && redirect_count != last_redirect_count) {
      ESP_LOGI(TAG, "redirect #%ld with status code %ld to %s", redirect_count, response_code, current_url);
      last_redirect_count = redirect_count;
    }

    mc = curl_multi_poll(multi, NULL, 0, 1000, NULL);
    assert(mc == CURLM_OK);
  }

  if (response_code == 0) {
    // error was logged in handle_multi_info()
    container->end();
    this->status_momentary_error("failed", 1000);
    return nullptr;
  }

  container->status_code = static_cast<int>(response_code);
  if (!is_success(container->status_code)) {
    // error was logged in handle_multi_info()
    ESP_LOGE(TAG, "HTTP Request failed; URL: %s; Code: %ld", current_url, response_code);
    this->status_momentary_error("failed", 1000);
    // Still return the container, so it can be used to get the status code and error message
  }

  curl_off_t cl;
  rc = curl_easy_getinfo(handle, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &cl);
  assert(rc == CURLE_OK);
  if (cl != -1) {
    container->content_length = static_cast<size_t>(cl);
#ifdef USE_HTTP_REQUEST_RESPONSE
    container->response_body_.reserve(container->content_length);
#endif
  }

  struct curl_header *prev = NULL;
  struct curl_header *h;

  while ((h = curl_easy_nextheader(handle, CURLH_HEADER, -1, prev)) != NULL) {
    ESP_LOGD(TAG, "Header: %s: %s", h->name, h->value);
    std::string header_name{h->name};
    auto lower_name = str_lower_case(header_name);
    if (response_headers.find(lower_name) != response_headers.end()) {
      container->response_headers_[lower_name].emplace_back(h->value);
    }
    prev = h;
  }

  container->duration_ms = millis() - container->start_ms;

  return container;
}

int HttpContainerHost::read(uint8_t *buf, size_t max_len) {
  int running_handles;
  CURLMcode mc = curl_multi_perform(this->multi_, &running_handles);
  assert(mc == CURLM_OK);

  handle_multi_info(this->multi_);

  auto bytes_remaining = this->response_body_.size() - this->bytes_read_;
  auto read_len = std::min(max_len, bytes_remaining);
  memcpy(buf, this->response_body_.data() + this->bytes_read_, read_len);
  this->bytes_read_ += read_len;
  return read_len;
}

void HttpContainerHost::end() {
  if (this->handle_) {
    curl_easy_cleanup(this->handle_);
    this->handle_ = nullptr;
  }

  if (this->headers_slist_) {
    curl_slist_free_all(this->headers_slist_);
    this->headers_slist_ = nullptr;
  }

  if (this->multi_) {
    CURLMcode mc = curl_multi_cleanup(this->multi_);
    assert(mc == CURLM_OK);
    this->multi_ = nullptr;
  }

  this->response_body_ = std::vector<uint8_t>();
  this->bytes_read_ = 0;
}

}  // namespace http_request
}  // namespace esphome

#endif  // USE_HOST
