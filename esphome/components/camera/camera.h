#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/helpers.h"

#include "buffer.h"
#include "camera_incremental_context.h"

namespace esphome {
namespace camera {

#define SUB_CAMERA(name) \
 protected: \
  camera::Camera *name##_camera_{nullptr}; \
\
 public: \
  void set_##name##_camera(camera::Camera *camera) { this->name##_camera_ = camera; }

/** Different sources for filtering.
 *  IDLE: Camera requests to send an image to the API.
 *  API_REQUESTER: API requests a new image.
 *  WEB_REQUESTER: ESP32 web server request an image. Ignored by API.
 */
enum CameraRequester : uint8_t { IDLE, API_REQUESTER, WEB_REQUESTER };

/// Enumeration of different pixel formats.
enum PixelFormat : uint8_t {
  PIXEL_FORMAT_GRAYSCALE = 0,  ///< 8-bit grayscale.
  PIXEL_FORMAT_RGB565,         ///< 16-bit RGB (5-6-5).
  PIXEL_FORMAT_BGR888,         ///< RGB pixel data in 8-bit format, stored as B, G, R (1 byte each).
};

/// Returns string name for a given PixelFormat.
inline const char *to_string(PixelFormat format) {
  switch (format) {
    case PIXEL_FORMAT_GRAYSCALE:
      return "PIXEL_FORMAT_GRAYSCALE";
    case PIXEL_FORMAT_RGB565:
      return "PIXEL_FORMAT_RGB565";
    case PIXEL_FORMAT_BGR888:
      return "PIXEL_FORMAT_BGR888";
  }
  return "PIXEL_FORMAT_UNKNOWN";
}
/** Abstract camera image base class.
 *  Encapsulates the JPEG encoded data and it is shared among
 *  all connected clients.
 */
class CameraImage {
 public:
  virtual uint8_t *get_data_buffer() = 0;
  virtual size_t get_data_length() = 0;
  virtual bool was_requested_by(CameraRequester requester) const = 0;
  virtual ~CameraImage() {}
};

/** Abstract image reader base class.
 *  Keeps track of the data offset of the camera image and
 *  how many bytes are remaining to read. When the image
 *  is returned, the shared_ptr is reset and the camera can
 *  reuse the memory of the camera image.
 */
class CameraImageReader {
 public:
  virtual void set_image(std::shared_ptr<CameraImage> image) = 0;
  virtual size_t available() const = 0;
  virtual uint8_t *peek_data_buffer() = 0;
  virtual void consume_data(size_t consumed) = 0;
  virtual void return_image() = 0;
  virtual ~CameraImageReader() {}
};

/** Struct that encapsulates the image data for the CameraImageTrigger */
struct CameraImageData {
  uint8_t *data;
  size_t length;
};

/// Specification of a caputured camera image.
/// This struct defines the format and size details for images captured
/// or processed by a camera component.
struct CameraImageSpec {
  uint16_t width;
  uint16_t height;
  PixelFormat format;
  size_t bytes_per_pixel() {
    switch (format) {
      case PIXEL_FORMAT_GRAYSCALE:
        return 1;
      case PIXEL_FORMAT_RGB565:
        return 2;
      case PIXEL_FORMAT_BGR888:
        return 3;
    }

    return 1;
  }
  size_t bytes_per_row() { return bytes_per_pixel() * width; }
  size_t bytes_per_image() { return bytes_per_pixel() * width * height; }
};

/** Abstract camera base class. Collaborates with API.
 *  1) API server starts and installs callback (add_image_callback)
 *     which is called by the camera when a new image is available.
 *  2) New API client connects and creates a new image reader (create_image_reader).
 *  3) API connection receives protobuf CameraImageRequest and calls request_image.
 *  3.a) API connection receives protobuf CameraImageRequest and calls start_stream.
 *  4) Camera implementation provides JPEG data in the CameraImage and calls callback.
 *  5) API connection sets the image in the image reader.
 *  6) API connection consumes data from the image reader and returns the image when finished.
 *  7.a) Camera captures a new image and continues with 4) until start_stream is called.
 */
class Camera : public EntityBase, public Component {
 public:
  Camera();
  // Camera implementation invokes callback to capture a new image.
  virtual void add_capture_callback(
      std::function<void(Buffer &, CameraImageSpec, CameraIncrementalContext &)> &&callback);
  // Camera implementation invokes callback to render overlays on the captured image.
  virtual void add_overlay_callback(
      std::function<void(Buffer &, CameraImageSpec, CameraIncrementalContext &)> &&callback);
  // Camera implementation invokes callback to publish a new image.
  virtual void add_image_callback(std::function<void(std::shared_ptr<CameraImage>)> &&callback);
  // Camera implementation invokes callback when start_stream is called.
  virtual void add_stream_start_callback(std::function<void()> &&callback);
  // Camera implementation invokes callback when stop_stream is called.
  virtual void add_stream_stop_callback(std::function<void()> &&callback);
  /// Returns a new camera image reader that keeps track of the JPEG data in the camera image.
  virtual CameraImageReader *create_image_reader() = 0;
  // Connection, camera or web server requests one new JPEG image.
  virtual void request_image(CameraRequester requester) = 0;
  // Connection, camera or web server requests a stream of images.
  virtual void start_stream(CameraRequester requester) = 0;
  // Connection or web server stops the previously started stream.
  virtual void stop_stream(CameraRequester requester) = 0;
  virtual ~Camera() {}
  /// The singleton instance of the camera implementation.
  static Camera *instance();

 protected:
  CallbackManager<void(camera::Buffer &, CameraImageSpec, CameraIncrementalContext &)> image_capture_callback_{};
  CallbackManager<void(camera::Buffer &, CameraImageSpec, CameraIncrementalContext &)> overlay_callback_{};
  CallbackManager<void(std::shared_ptr<camera::CameraImage>)> new_image_callback_{};
  CallbackManager<void()> stream_start_callback_{};
  CallbackManager<void()> stream_stop_callback_{};
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
  static Camera *global_camera;
};

/** Class that installs a camera callback which is triggered
 *  every time a new image can be captured.
 */
class CameraCaptureImageTrigger : public Trigger<CameraImageData, CameraImageSpec, CameraIncrementalContext &> {
 public:
  explicit CameraCaptureImageTrigger(Camera *camera) {
    camera->add_capture_callback([this](Buffer &image, const CameraImageSpec &spec, CameraIncrementalContext &context) {
      CameraImageData camera_image_data{};
      camera_image_data.length = image.get_data_length();
      camera_image_data.data = image.get_data_buffer();
      this->trigger(camera_image_data, spec, context);
    });
  }
};

/** Class that installs a overlay callback which is triggered
 *  every time to overlay graphics on the captured image.
 */
class CameraOverlayTrigger : public Trigger<CameraImageData, CameraImageSpec, CameraIncrementalContext &> {
 public:
  explicit CameraOverlayTrigger(Camera *camera) {
    camera->add_overlay_callback(
        [this](camera::Buffer &image, const CameraImageSpec &spec, CameraIncrementalContext &context) {
          CameraImageData camera_image_data{};
          camera_image_data.length = image.get_data_length();
          camera_image_data.data = image.get_data_buffer();
          this->trigger(camera_image_data, spec, context);
        });
  }
};

/** Class that installs a camera callback which is triggered
 *  every time a new jpeg encoded image is available.
 */
class CameraImageTrigger : public Trigger<CameraImageData> {
 public:
  explicit CameraImageTrigger(Camera *camera) {
    camera->add_image_callback([this](const std::shared_ptr<camera::CameraImage> &image) {
      CameraImageData camera_image_data{};
      camera_image_data.length = image->get_data_length();
      camera_image_data.data = image->get_data_buffer();
      this->trigger(camera_image_data);
    });
  }
};

/** Class that installs a camera callback which is triggered
 *  every time a new image stream is requested.
 */
class CameraStreamStartTrigger : public Trigger<> {
 public:
  explicit CameraStreamStartTrigger(Camera *camera) {
    camera->add_stream_start_callback([this]() { this->trigger(); });
  }
};

/** Class that installs a camera callback which is triggered
 *  every time a image stream is stopped.
 */
class CameraStreamStopTrigger : public Trigger<> {
 public:
  explicit CameraStreamStopTrigger(Camera *camera) {
    camera->add_stream_stop_callback([this]() { this->trigger(); });
  }
};

}  // namespace camera
}  // namespace esphome
