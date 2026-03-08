#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace camera {

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

// Forward declaration
class CameraImage;

/** Listener interface for camera events.
 *
 * Components can implement this interface to receive camera notifications
 * (new images, stream start/stop) without the overhead of std::function callbacks.
 */
class CameraListener {
 public:
  virtual void on_camera_image(const std::shared_ptr<CameraImage> &image) {}
  virtual void on_stream_start() {}
  virtual void on_stream_stop() {}
};

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
 *  1) API server starts and registers as a listener (add_listener)
 *     to receive new images from the camera.
 *  2) New API client connects and creates a new image reader (create_image_reader).
 *  3) API connection receives protobuf CameraImageRequest and calls request_image.
 *  3.a) API connection receives protobuf CameraImageRequest and calls start_stream.
 *  4) Camera implementation provides JPEG data in the CameraImage and notifies listeners.
 *  5) API connection sets the image in the image reader.
 *  6) API connection consumes data from the image reader and returns the image when finished.
 *  7.a) Camera captures a new image and continues with 4) until start_stream is called.
 */
class Camera : public EntityBase, public Component {
 public:
  Camera();
  /// Add a listener to receive camera events
  virtual void add_listener(CameraListener *listener) = 0;
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
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
  static Camera *global_camera;
};

}  // namespace camera
}  // namespace esphome
