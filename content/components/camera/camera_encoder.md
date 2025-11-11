---
description: "Instructions for setting up the camera encoder component in ESPHome."
title: "Camera Encoder"
params:
  seo:
    description: Instructions for setting up the camera encoder component in ESPHome.
    image: camera.svg
---

The `camera_encoder` component provides image compression support for software-based cameras or cameras without
internal compression. It allows raw camera frames to be compressed into a format suitable for transmission to API
clients, such as Home Assistant, which expect JPEG-compressed images.

It supports different encoder implementations, such as a ESP32 Camera software JPEG encoder that can be configured with
options like image quality and incremental encoding. These settings make it possible to balance image
quality and performance depending on the use case.

> [!NOTE]
> The default software JPEG encoder enables devices like the ESP32-S3 to stream images.
> It is primarily intended for smallar images due to limited processing power and memory,
> and supports only devices from the ESP32 family.

```yaml
# Example configuration entry
camera_encoder:
```

## Configuration variables

- **type** (*Optional*): `esp32_camera`

## esp32_camera Options

- **quality** (*Optional*, int): Sets JPEG compression quality.
  Valid values range from `1` (lowest quality, highest compression) to `100` (best quality, least compression). Defaults: `80`.

- **buffer_size** (*Optional*, int): Initial size of the output buffer in bytes, used to store the JPEG-encoded image data.
  - Minimum: 1024 bytes
  - Maximum: 2097152 bytes (2 MB), sufficient for ESP32-S3 and ESP32-P4
  - Default: `4096`.

- **buffer_expand_size** (*Optional*, int): Number of bytes to expand the output buffer if it is too small to hold the JPEG-encoded image. A value of `0` disables expansion.
  - Maximum: 2097152 bytes (2 MB), sufficient for ESP32-S3 and ESP32-P4
  - Default: `1024`.

## See Also

- {{< apiref "camera/encoder.h" "camera/encoder.h" >}}
- {{< apiref "camera_encoder/esp32_camera_jpeg_encoder.h" "camera_encoder/esp32_camera_jpeg_encoder.h" >}}
