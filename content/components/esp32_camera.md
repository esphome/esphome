---
description: "Instructions for setting up the ESP32 Cameras in ESPHome"
title: "ESP32 Camera Component"
params:
  seo:
    description: Instructions for setting up the ESP32 Cameras in ESPHome
    image: camera.svg
---

The `esp32_camera` component allows you to use ESP32-based camera boards in ESPHome that
directly integrate into Home Assistant through the native API.

Requires an {{< docref "/components/i2c" >}} and {{< docref "/components/psram" >}} to be configured.

```yaml
# Example configuration entry
esp32_camera:
  name: My Camera
  external_clock:
    pin: GPIOXX
    frequency: 20MHz
  i2c_id: my_i2c_bus
  data_pins: [GPIOXX, GPIOXX, GPIOXX, GPIOXX, GPIOXX, GPIOXX, GPIOXX, GPIOXX]
  vsync_pin: GPIOXX
  href_pin: GPIOXX
  pixel_clock_pin: GPIOXX
  reset_pin: GPIOXX
  resolution: 640x480
  jpeg_quality: 10
```

## Configuration variables

- **name** (**Required**, string): The name of the camera.
- **icon** (*Optional*, icon): Manually set the icon to use for the camera in the frontend.
- **internal** (*Optional*, boolean): Mark this component as internal. Internal components will
  not be exposed to the frontend (like Home Assistant). Only specifying an `id` without
  a `name` will implicitly set this to true.

- **disabled_by_default** (*Optional*, boolean): If true, then this entity should not be added to any client's frontend,
  (usually Home Assistant) without the user manually enabling it (via the Home Assistant UI).
  Defaults to `false`.

- **entity_category** (*Optional*, string): The category of the entity.
  See <https://developers.home-assistant.io/docs/core/entity/#generic-properties>
  for a list of available options.
  Set to `""` to remove the default entity category.

Connection Options:

- **data_pins** (**Required**, list of pins): The data lanes of the camera, this must be a list
  of 8 GPIO pins.

- **vsync_pin** (**Required**, pin): The pin the VSYNC line of the camera is connected to.
- **href_pin** (**Required**, pin): The pin the HREF line of the camera is connected to.
- **pixel_clock_pin** (**Required**, pin): The pin the pixel clock line of the camera is connected to.
- **external_clock** (**Required**): The configuration of the external clock to drive the camera.

  - **pin** (**Required**, pin): The pin the external clock line is connected to.
  - **frequency** (*Optional*, float): The frequency of the external clock, must be between 10
    and 20MHz. Defaults to `20MHz`.

- **i2c_id** (**Required**, [ID](/guides/configuration-types#id)): The ID of the [I²C bus](/components/i2c) the camera is connected to.
- **reset_pin** (*Optional*, pin): The ESP pin the reset pin of the camera is connected to.
  If set, this will reset the camera before the ESP boots.

- **power_down_pin** (*Optional*, pin): The ESP pin to power down the camera.
  If set, this will power down the camera while it is inactive.

- **test_pattern** (*Optional*, boolean): When enabled, the camera will show a test pattern
  that can be used to debug connection issues.

Frame Settings:

- **max_framerate** (*Optional*, float): The maximum framerate the camera will generate images at.
  Up to 60Hz is possible (with reduced frame sizes), but beware of overheating. Defaults to `10 fps`.

- **idle_framerate** (*Optional*, float): The framerate to capture images at when no client
  is requesting a full stream. Defaults to `0.1 fps`.

- **frame_buffer_count** (*Optional*, int): The number of frame buffers to use when reading from the camera sensor.
  Must be between 1 and 2. Defaults to `1`.

- **frame_buffer_location** (*Optional*, enum): The memory area used for storing the frame buffers. Defaults to `PSRAM`.
  - `PSRAM`
  - `DRAM`

Image Settings:

- **resolution** (*Optional*, enum): The resolution the camera will capture images at. Higher
  resolutions require more memory, if there's not enough memory you will see an error during startup.

  - `160x120` (QQVGA, 4:3)
  - `176x144` (QCIF, 11:9)
  - `240x176` (HQVGA, 15:11)
  - `320x240` (QVGA, 4:3)
  - `400x296` (CIF, 50:37)
  - `640x480` (VGA, 4:3, default)
  - `800x600` (SVGA, 4:3)
  - `1024x768` (XGA, 4:3)
  - `1280x1024` (SXGA, 5:4)
  - `1600x1200` (UXGA, 4:3)
  - `1920x1080` (FHD, 16:9)
  - `720x1280` (Portrait HD, 9:16)
  - `864x1536` (Portrait 3MP, 9:16)
  - `2048x1536` (QXGA, 4:3)
  - `2560x1440` (QHD, 16:9)
  - `2560x1600` (WQXGA, 8:5)
  - `1080x1920` (Portrait FHD, 9:16)
  - `2560x1920` (QSXGA, 4:3)

- **jpeg_quality** (*Optional*, int): The JPEG quality that the camera should encode images with.
  From 10 (best) to 63 (worst). Defaults to `10`.

- **vertical_flip** (*Optional*, boolean): Whether to flip the image vertically. Defaults to `true`.
- **horizontal_mirror** (*Optional*, boolean): Whether to mirror the image horizontally. Defaults to `true`.
- **contrast** (*Optional*, int): The contrast to apply to the picture, from -2 to 2. Defaults to `0`.
- **brightness** (*Optional*, int): The brightness to apply to the picture, from -2 to 2. Defaults to `0`.
- **saturation** (*Optional*, int): The saturation to apply to the picture, from -2 to 2. Defaults to `0`.
- **special_effect** (*Optional*, enum): The effect to apply to the picture. Defaults to `none` (picture without effect).

  - `none`  : Picture without effect
  - `negative`  : Colors of picture are inverted
  - `grayscale`  : Only luminance of picture is kept
  - `red_tint`  : Picture appear red-tinted
  - `green_tint`  : Picture appear green-tinted
  - `blue_tint`  : Picture appear blue-tinted
  - `sepia`  : Sepia effect is applied to picture

Exposure Settings:

- **aec_mode** (*Optional*, enum): The mode of exposure module. Defaults to `auto` (leave camera to automatically adjust exposure).

  - `manual`  : Exposure can be manually set, with **aec_value** parameter. **ae_level** has no effect here
  - `auto`  : Camera manage exposure automatically. Compensation can be applied, thanks to **ae_level** parameter. **aec_value** has no effect here

- **aec2** (*Optional*, boolean): Whether to enable Auto Exposure Control 2. Seems to change computation method of automatic exposure. Defaults to `false`.
- **ae_level** (*Optional*, int): The auto exposure level to apply to the picture (when **aec_mode** is set to `auto`  ), from -2 to 2. Defaults to `0`.
- **aec_value** (*Optional*, int): The Exposure value to apply to the picture (when **aec_mode** is set to `manual`  ), from 0 to 1200. Defaults to `300`.

Sensor Gain Settings:

- **agc_mode** (*Optional*, enum): The mode of gain control module. Defaults to `auto` (leave camera to automatically adjust sensor gain).

  - `manual`  : Gain can be manually set, with **agc_value** parameter. **agc_gain_ceiling** has no effect here
  - `auto`  : Camera manage sensor gain automatically. Maximum gain can be defined, thanks to **agc_gain_ceiling** parameter. **agc_value** has no effect here

- **agc_value** (*Optional*, int): The gain value to apply to the picture (when **aec_mode** is set to `manual`  ), from 0 to 30. Defaults to `0`.
- **agc_gain_ceiling** (*Optional*, enum): The maximum gain allowed, when **agc_mode** is set to `auto`. This parameter seems act as "ISO" setting. Defaults to `2x`.

  - `2x`  : Camera is less sensitive, picture is clean (without visible noise)
  - `4x`
  - `8x`
  - `16x`
  - `32x`
  - `64x`
  - `128x`  : Camera is more sensitive, but picture contain lot of noise

White Balance Setting:

- **wb_mode** (*Optional*, enum): The mode of white balace module. Defaults to `auto`.

  - `auto`  : Camera choose best white balance setting
  - `sunny`  : White balance sunny mode
  - `cloudy`  : White balance cloudy mode
  - `office`  : White balance office mode
  - `home`  : White balance home mode

Automations:

- **on_stream_start** (*Optional*, [Automation](/automations)): An automation to perform
  when a stream starts.

- **on_stream_stop** (*Optional*, [Automation](/automations)): An automation to perform
  when a stream stops.

- **on_image** (*Optional*, [Automation](/automations)): An automation called when image taken. Image is available as `image` variable of type {{< apistruct "esp32_camera::CameraImageData" "esp32_camera::CameraImageData" >}}.

Test Setting:

- **test_pattern** (*Optional*, boolean): For tests purposes, it's possible to replace picture get from sensor by a test color pattern. Defaults to `false`.

> [!NOTE]
> Camera uses PWM timer #1. If you need PWM (via the `ledc` platform) you need to manually specify
> a channel there (with the `channel: 2`  parameter)

## Configuration examples

**Ai-Thinker Camera**:

> [!WARNING]
> GPIO16 on this board (and possibly other boards below) is connected to onboard PSRAM.
> Using this GPIO for other purposes (eg as a button) will trigger the watchdog.
> Further information on pin notes can be found here: <https://github.com/raphaelbs/esp32-cam-ai-thinker/blob/master/docs/esp32cam-pin-notes.md>

```yaml
# Example configuration entry
i2c:
  - id: camera_i2c
    sda: GPIO26
    scl: GPIO27

esp32_camera:
  external_clock:
    pin: GPIO0
    frequency: 20MHz
  i2c_id: camera_i2c
  data_pins: [GPIO5, GPIO18, GPIO19, GPIO21, GPIO36, GPIO39, GPIO34, GPIO35]
  vsync_pin: GPIO25
  href_pin: GPIO23
  pixel_clock_pin: GPIO22
  power_down_pin: GPIO32

  # Image settings
  name: My Camera
  # ...
```

**M5Stack Camera**:

> [!WARNING]
> This camera board has insufficient cooling and will overheat over time,
> ESPHome does only activate the camera when Home Assistant requests an image, but
> the camera unit can still heat up considerably for some boards.
>
> If the camera is not recognized after a reboot and the unit feels warm, try waiting for
> it to cool down and check again - if that still doesn't work try enabling the test pattern.

```yaml
# Example configuration entry
i2c:
  - id: camera_i2c
    sda: GPIO25
    scl: GPIO23

esp32_camera:
  external_clock:
    pin: GPIO27
    frequency: 20MHz
  i2c_id: camera_i2c
  data_pins: [GPIO17, GPIO35, GPIO34, GPIO5, GPIO39, GPIO18, GPIO36, GPIO19]
  vsync_pin: GPIO22
  href_pin: GPIO26
  pixel_clock_pin: GPIO21
  reset_pin: GPIO15

  # Image settings
  name: My Camera
  # ...
```

**M5Stack Timer Camera X/F**:

```yaml
# Example configuration entry
i2c:
  - id: camera_i2c
    sda: GPIO25
    scl: GPIO23
esp32_camera:
  external_clock:
    pin: GPIO27
    frequency: 20MHz
  i2c_id: camera_i2c
  data_pins: [GPIO32, GPIO35, GPIO34, GPIO5, GPIO39, GPIO18, GPIO36, GPIO19]
  vsync_pin: GPIO22
  href_pin: GPIO26
  pixel_clock_pin: GPIO21
  reset_pin: GPIO15

  # Image settings
  name: My Camera
  # ...
```

**M5Stack M5CameraF New**:

```yaml
# Example configuration entry as per https://docs.m5stack.com/en/unit/m5camera_f_new
i2c:
  - id: camera_i2c
    sda: GPIO22
    scl: GPIO23
esp32_camera:
  external_clock:
    pin: GPIO27
    frequency: 20MHz
  i2c_id: camera_i2c
  data_pins: [GPIO32, GPIO35, GPIO34, GPIO5, GPIO39, GPIO18, GPIO36, GPIO19]
  vsync_pin: GPIO25
  href_pin: GPIO26
  pixel_clock_pin: GPIO21
  reset_pin: GPIO15
```

**Wrover Kit Boards**:

```yaml
# Example configuration entry
i2c:
  - id: camera_i2c
    sda: GPIO26
    scl: GPIO27
esp32_camera:
  external_clock:
    pin: GPIO21
    frequency: 20MHz
  i2c_id: camera_i2c
  data_pins: [GPIO4, GPIO5, GPIO18, GPIO19, GPIO36, GPIO39, GPIO34, GPIO35]
  vsync_pin: GPIO25
  href_pin: GPIO23
  pixel_clock_pin: GPIO22

  # Image settings
  name: My Camera
  # ...
```

**TTGO T-Camera V05**:

```yaml
# Example configuration entry
i2c:
  - id: camera_i2c
    sda: GPIO13
    scl: GPIO12
esp32_camera:
  external_clock:
    pin: GPIO32
    frequency: 20MHz
  i2c_id: camera_i2c
  data_pins: [GPIO5, GPIO14, GPIO4, GPIO15, GPIO18, GPIO23, GPIO36, GPIO39]
  vsync_pin: GPIO27
  href_pin: GPIO25
  pixel_clock_pin: GPIO19
  power_down_pin: GPIO26

  # Image settings
  name: My Camera
  # ...
```

**TTGO T-Camera V162**:

```yaml
# Example configuration entry
i2c:
  - id: camera_i2c
    sda: GPIO18
    scl: GPIO23
esp32_camera:
  external_clock:
    pin: GPIO4
    frequency: 20MHz
  i2c_id: camera_i2c
  data_pins: [GPIO34, GPIO13, GPIO14, GPIO35, GPIO39, GPIO38, GPIO37, GPIO36]
  vsync_pin: GPIO5
  href_pin: GPIO27
  pixel_clock_pin: GPIO25
  jpeg_quality: 10
  vertical_flip: true
  horizontal_mirror: false

  # Image settings
  name: My Camera
  # ...
```

**TTGO T-Camera V17**:

```yaml
# Example configuration entry
i2c:
  - id: camera_i2c
    sda: GPIO13
    scl: GPIO12
esp32_camera:
  external_clock:
    pin: GPIO32
    frequency: 20MHz
  i2c_id: camera_i2c
  data_pins: [GPIO5, GPIO14, GPIO4, GPIO15, GPIO18, GPIO23, GPIO36, GPIO39]
  vsync_pin: GPIO27
  href_pin: GPIO25
  pixel_clock_pin: GPIO19
  # power_down_pin: GPIO26
  vertical_flip: true
  horizontal_mirror: true

  # Image settings
  name: My Camera
  # ...
```

**TTGO T-Journal**:

```yaml
# Example configuration entry
i2c:
  - id: camera_i2c
    sda: GPIO25
    scl: GPIO23
esp32_camera:
  external_clock:
    pin: GPIO27
    frequency: 20MHz
  i2c_id: camera_i2c
  data_pins: [GPIO17, GPIO35, GPIO34, GPIO5, GPIO39, GPIO18, GPIO36, GPIO19]
  vsync_pin: GPIO22
  href_pin: GPIO26
  pixel_clock_pin: GPIO21

  # Image settings
  name: My Camera
  # ...
```

**TTGO-Camera Plus**:

```yaml
# Example configuration entry
i2c:
  - id: camera_i2c
    sda: GPIO18
    scl: GPIO23
esp32_camera:
  external_clock:
    pin: GPIO4
    frequency: 20MHz
  i2c_id: camera_i2c
  data_pins: [GPIO34, GPIO13, GPIO26, GPIO35, GPIO39, GPIO38, GPIO37, GPIO36]
  vsync_pin: GPIO5
  href_pin: GPIO27
  pixel_clock_pin: GPIO25
  vertical_flip: false
  horizontal_mirror: false

  # Image settings
  name: My Camera
  # ...
```

**TTGO-Camera Mini**:

```yaml
# Example configuration entry
i2c:
  - id: camera_i2c
    sda: GPIO13
    scl: GPIO12
esp32_camera:
  external_clock:
    pin: GPIO32
    frequency: 20MHz
  i2c_id: camera_i2c
  data_pins: [GPIO5, GPIO14, GPIO4, GPIO15, GPIO37, GPIO38, GPIO36, GPIO39]
  vsync_pin: GPIO27
  href_pin: GPIO25
  pixel_clock_pin: GPIO19

  # Image settings
  name: My Camera
  # ...
```

**ESP-EYE**:

```yaml
# Example configuration entry
i2c:
  - id: camera_i2c
    sda: GPIO18
    scl: GPIO23
esp32_camera:
  external_clock:
    pin: GPIO4
    frequency: 20MHz
  i2c_id: camera_i2c
  data_pins: [GPIO34, GPIO13, GPIO14, GPIO35, GPIO39, GPIO38, GPIO37, GPIO36]
  vsync_pin: GPIO5
  href_pin: GPIO27
  pixel_clock_pin: GPIO25

  # Image settings
  name: My Camera
  # ...
```

**ESP32S3_EYE** on [Freenove ESP32-S3-DevKitC-1](https://github.com/Freenove/Freenove_ESP32_S3_WROOM_Board):

```yaml
# Example configuration entry
i2c:
  - id: camera_i2c
    sda: GPIO4
    scl: GPIO5
esp32_camera:
  external_clock:
    pin: GPIO15
    frequency: 20MHz
  i2c_id: camera_i2c
  data_pins: [GPIO11, GPIO9, GPIO8, GPIO10, GPIO12, GPIO18, GPIO17, GPIO16]
  vsync_pin: GPIO6
  href_pin: GPIO7
  pixel_clock_pin: GPIO13
  frame_buffer_location: DRAM

  # Image settings
  name: My Camera
  # ...
```

**Seeed Studio XIAO ESP32S3 Sense**:

```yaml
# Example configuration entry
i2c:
  - id: camera_i2c
    sda: GPIO40
    scl: GPIO39
esp32_camera:
  external_clock:
    pin: GPIO10
    frequency: 20MHz
  i2c_id: camera_i2c
  data_pins: [GPIO15, GPIO17, GPIO18, GPIO16, GPIO14, GPIO12, GPIO11, GPIO48]
  vsync_pin: GPIO38
  href_pin: GPIO47
  pixel_clock_pin: GPIO13

  # Image settings
  name: My Camera
  # ...
```

**Waveshare ESP32-S3 ETH + OV2640 camera**:

```yaml
i2c:
  - id: camera_i2c
    sda: GPIO48
    scl: GPIO47
esp32_camera:
  external_clock:
    pin: GPIO3
    frequency: 20MHz
  i2c_id: camera_i2c
  data_pins: [GPIO41, GPIO45, GPIO46, GPIO42, GPIO40, GPIO38, GPIO15, GPIO18]
  vsync_pin: GPIO1
  href_pin: GPIO2
  pixel_clock_pin: GPIO39
  power_down_pin: GPIO8

  # Image settings
  name: My Camera
  # ...
```

## See Also

- {{< apiref "esp32_camera/esp32_camera.h" "esp32_camera/esp32_camera.h" >}}
