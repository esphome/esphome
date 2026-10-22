#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include "esphome/components/http_request/http_request.h"
#include "esphome/components/http_request/ota/ota_http_request.h"
#include "esphome/components/update/update_entity.h"

#ifdef USE_ESP32
#include <freertos/FreeRTOS.h>
#endif

namespace esphome::http_request {

namespace testing {
class HttpRequestUpdateTest;
}  // namespace testing

class HttpRequestUpdate;

// One manifest check: handed from update() to the task and back to the main loop.
struct TaskResult {
  HttpRequestUpdate *update;
  const char *source_url;  // the URL this check reads, fixed before it starts
  update::UpdateInfo info;
  const LogString *error_str{nullptr};  // set on failure, nullptr on success
  bool owns_source_url;                 // the check holds the buffer until it ends, see begin_check_()
};

class HttpRequestUpdate final : public update::UpdateEntity, public PollingComponent, public ota::OTAStateListener {
 public:
  void setup() override;
  void update() override;

  void perform(bool force) override;
  void check() override { this->update(); }

  ~HttpRequestUpdate() override { this->release_source_url_(); }

  /// Codegen only: the config literal lives in flash for the life of the program, so it is kept by pointer.
  void set_source_url_static(const char *source_url) {
    this->release_source_url_();
    this->source_url_ = source_url;
  }
  /// Set the manifest URL at run time; the string is copied, so any storage may be passed.
  void set_source_url(const char *source_url) { this->set_source_url(source_url, strlen(source_url)); }
  void set_source_url(const std::string &source_url) { this->set_source_url(source_url.data(), source_url.size()); }
  void set_source_url(const char *source_url, size_t len);
  const char *get_source_url() const { return this->source_url_; }

  void set_request_parent(HttpRequestComponent *request_parent) { this->request_parent_ = request_parent; }
  void set_ota_parent(OtaHttpRequestComponent *ota_parent) { this->ota_parent_ = ota_parent; }

  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  void on_ota_state(ota::OTAState state, float progress, uint8_t error) override;

 protected:
  HttpRequestComponent *request_parent_;
  OtaHttpRequestComponent *ota_parent_;

  static void update_task(void *params);
#ifdef USE_ESP32
  TaskResult *task_result_{nullptr};  // set while a check runs on the update task
#endif
  uint8_t initial_check_remaining_{0};

 private:
#ifdef USE_HOST
  friend class testing::HttpRequestUpdateTest;
#endif
  void release_source_url_();
  // A check reads the URL it started with until it ends, on the update task or inline through an
  // automation it triggers. Ownership of that buffer moves to the check, so a set_source_url in the
  // meantime cannot free it; end_check_() hands it back or frees it on the main loop.
  void begin_check_(TaskResult *result);
  void end_check_(TaskResult *result);

  bool source_url_owned_{false};  // source_url_ was copied by a run-time setter; sits in the padding above
  const char *source_url_{nullptr};
};

}  // namespace esphome::http_request
