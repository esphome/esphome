#ifdef USE_ESP32

#include "camera_web_server.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/util.h"

#include <cstdlib>
#include <esp_http_server.h>
#include <utility>

namespace esphome::esp32_camera_web_server {

static const uint32_t IMAGE_REQUEST_TIMEOUT = 5000;
// How often streaming_handler_ reports its throughput.
static const uint32_t STREAM_STATS_INTERVAL = 5000;
static const char *const TAG = "esp32_camera_web_server";

#define PART_BOUNDARY "123456789000000000000987654321"
#define CONTENT_TYPE "image/jpeg"
#define CONTENT_LENGTH "Content-Length"

static const char *const STREAM_HEADER = "HTTP/1.0 200 OK\r\n"
                                         "Access-Control-Allow-Origin: *\r\n"
                                         "Connection: close\r\n"
                                         "Content-Type: multipart/x-mixed-replace;boundary=" PART_BOUNDARY "\r\n"
                                         "\r\n"
                                         "--" PART_BOUNDARY "\r\n";
static const char *const STREAM_ERROR = "Content-Type: text/plain\r\n"
                                        "\r\n"
                                        "No frames send.\r\n"
                                        "--" PART_BOUNDARY "\r\n";
static const char *const STREAM_PART = "Content-Type: " CONTENT_TYPE "\r\n" CONTENT_LENGTH ": %u\r\n\r\n";
static const char *const STREAM_BOUNDARY = "\r\n"
                                           "--" PART_BOUNDARY "\r\n";

CameraWebServer::CameraWebServer() {}

CameraWebServer::~CameraWebServer() {}

void CameraWebServer::setup() {
  if (!camera::Camera::instance() || camera::Camera::instance()->is_failed()) {
    this->mark_failed();
    return;
  }

  this->semaphore_ = xSemaphoreCreateBinary();

  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = this->port_;
  config.ctrl_port = this->port_;
  config.max_open_sockets = 1;
  config.backlog_conn = 2;
  config.lru_purge_enable = true;

  if (httpd_start(&this->httpd_, &config) != ESP_OK) {
    mark_failed();
    return;
  }

  httpd_uri_t uri = {
      .uri = "/",
      .method = HTTP_GET,
      .handler = [](struct httpd_req *req) { return ((CameraWebServer *) req->user_ctx)->handler_(req); },
      .user_ctx = this};

  httpd_register_uri_handler(this->httpd_, &uri);

  camera::Camera::instance()->add_listener(this);
}

void CameraWebServer::on_camera_image(const std::shared_ptr<camera::CameraImage> &image) {
  if (this->running_ && image->was_requested_by(camera::WEB_REQUESTER)) {
    this->image_ = image;
    xSemaphoreGive(this->semaphore_);
  }
}

void CameraWebServer::on_shutdown() {
  this->running_ = false;
  this->image_ = nullptr;
  httpd_stop(this->httpd_);
  this->httpd_ = nullptr;
  vSemaphoreDelete(this->semaphore_);
  this->semaphore_ = nullptr;
}

void CameraWebServer::dump_config() {
  ESP_LOGCONFIG(TAG,
                "ESP32 Camera Web Server:\n"
                "  Port: %d",
                this->port_);
  if (this->mode_ == STREAM) {
    ESP_LOGCONFIG(TAG, "  Mode: stream");
  } else {
    ESP_LOGCONFIG(TAG, "  Mode: snapshot");
  }

  if (this->is_failed()) {
    ESP_LOGE(TAG, "  Setup Failed");
  }
}

float CameraWebServer::get_setup_priority() const { return setup_priority::LATE; }

void CameraWebServer::loop() {
  if (!this->running_) {
    this->image_ = nullptr;
  }
}

std::shared_ptr<esphome::camera::CameraImage> CameraWebServer::wait_for_image_() {
  std::shared_ptr<esphome::camera::CameraImage> image;
  image.swap(this->image_);

  if (image)
    return image;

  // Keep waiting until a frame really shows up, rather than trusting a single
  // take() to mean one is there.
  //
  // on_camera_image() gives the semaphore for every frame it accepts, but the
  // swap above hands frames out without taking it, so as soon as the camera is
  // faster than this task for one frame the (binary) semaphore is left
  // signalled by a frame that has already been consumed. The next take() then
  // returns immediately with nothing to swap in, and the caller reports a lost
  // frame and closes the stream -- after an arbitrary number of good frames,
  // which is exactly when the camera happens to fall behind for one iteration.
  //
  // running_ is re-checked on every pass so a shutdown or a client that went
  // away is noticed straight away instead of after the full timeout.
  const uint32_t start = millis();
  while (this->running_) {
    uint32_t elapsed = millis() - start;
    if (elapsed >= IMAGE_REQUEST_TIMEOUT)
      break;
    xSemaphoreTake(this->semaphore_, pdMS_TO_TICKS(IMAGE_REQUEST_TIMEOUT - elapsed));
    image.swap(this->image_);
    if (image)
      break;
  }

  return image;
}

esp_err_t CameraWebServer::handler_(struct httpd_req *req) {
  esp_err_t res = ESP_FAIL;

  this->image_ = nullptr;
  this->running_ = true;

  switch (this->mode_) {
    case STREAM:
      res = this->streaming_handler_(req);
      break;

    case SNAPSHOT:
      res = this->snapshot_handler_(req);
      break;
  }

  this->running_ = false;
  this->image_ = nullptr;
  return res;
}

static esp_err_t httpd_send_all(httpd_req_t *r, const char *buf, size_t buf_len) {
  int ret;

  while (buf_len > 0) {
    ret = httpd_send(r, buf, buf_len);
    if (ret < 0) {
      return ESP_FAIL;
    }
    buf += ret;
    buf_len -= ret;
  }
  return ESP_OK;
}

esp_err_t CameraWebServer::streaming_handler_(struct httpd_req *req) {
  esp_err_t res = ESP_OK;
  char part_buf[64];

  // This manually constructs HTTP response to avoid chunked encoding
  // which is not supported by some clients

  res = httpd_send_all(req, STREAM_HEADER, strlen(STREAM_HEADER));
  if (res != ESP_OK) {
    ESP_LOGW(TAG, "STREAM: failed to set HTTP header");
    return res;
  }

  uint32_t frames = 0;
  // Frame statistics are aggregated over STREAM_STATS_INTERVAL rather than
  // logged per frame. A line per frame comes out of this (non-main) task tens
  // of times a second, and formatting and buffering it costs more than the
  // stream it is reporting on.
  uint32_t stats_since = millis();
  uint32_t stats_frames = 0;
  uint32_t stats_bytes = 0;

  camera::Camera::instance()->start_stream(esphome::camera::WEB_REQUESTER);

  while (res == ESP_OK && this->running_) {
    auto image = this->wait_for_image_();

    if (!image) {
      // A shutdown is not a lost frame: wait_for_image_() returns empty as soon
      // as running_ clears, and the loop condition below ends the stream anyway.
      if (this->running_)
        ESP_LOGW(TAG, "STREAM: failed to acquire frame");
      res = ESP_FAIL;
    }
    if (res == ESP_OK) {
      size_t hlen = snprintf(part_buf, 64, STREAM_PART, image->get_data_length());
      res = httpd_send_all(req, part_buf, hlen);
    }
    if (res == ESP_OK) {
      res = httpd_send_all(req, (const char *) image->get_data_buffer(), image->get_data_length());
    }
    if (res == ESP_OK) {
      res = httpd_send_all(req, STREAM_BOUNDARY, strlen(STREAM_BOUNDARY));
    }
    if (res == ESP_OK) {
      frames++;
      stats_frames++;
      stats_bytes += image->get_data_length();
      uint32_t elapsed = millis() - stats_since;
      if (elapsed >= STREAM_STATS_INTERVAL) {
        ESP_LOGD(TAG, "MJPG: %.1ffps, %" PRIu32 "B/frame (%" PRIu32 " frames)", stats_frames * 1000.0f / elapsed,
                 stats_bytes / stats_frames, stats_frames);
        stats_since = millis();
        stats_frames = 0;
        stats_bytes = 0;
      }
    }
  }

  // Report whatever did not fill a whole interval, so a stream that only ran for
  // a second or two still says what it managed rather than nothing at all.
  if (stats_frames > 0) {
    uint32_t elapsed = millis() - stats_since;
    if (elapsed == 0)
      elapsed = 1;
    ESP_LOGD(TAG, "MJPG: %.1ffps, %" PRIu32 "B/frame (%" PRIu32 " frames)", stats_frames * 1000.0f / elapsed,
             stats_bytes / stats_frames, stats_frames);
  }

  if (!frames) {
    res = httpd_send_all(req, STREAM_ERROR, strlen(STREAM_ERROR));
  }

  camera::Camera::instance()->stop_stream(esphome::camera::WEB_REQUESTER);

  ESP_LOGI(TAG, "STREAM: closed. Frames: %" PRIu32, frames);

  return res;
}

esp_err_t CameraWebServer::snapshot_handler_(struct httpd_req *req) {
  esp_err_t res = ESP_OK;

  camera::Camera::instance()->request_image(esphome::camera::WEB_REQUESTER);

  auto image = this->wait_for_image_();

  if (!image) {
    ESP_LOGW(TAG, "SNAPSHOT: failed to acquire frame");
    httpd_resp_send_500(req);
    res = ESP_FAIL;
    return res;
  }

  res = httpd_resp_set_type(req, CONTENT_TYPE);
  if (res != ESP_OK) {
    ESP_LOGW(TAG, "SNAPSHOT: failed to set HTTP response type");
    return res;
  }

  httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");

  if (res == ESP_OK) {
    res = httpd_resp_send(req, (const char *) image->get_data_buffer(), image->get_data_length());
  }
  return res;
}

}  // namespace esphome::esp32_camera_web_server

#endif  // USE_ESP32
