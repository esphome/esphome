# Picture Viewer Component

A sophisticated picture viewer for ESPHome with LVGL integration, supporting dynamic image loading, slideshow functionality, and thumbnails.

## Features

- ✅ **Dynamic Image Loading** - FileManager integration for automatic updates
- ✅ **Multiple JPEG Decoders** - Optimized for each platform:
  - ESP32-S2/S3: `esp_jpeg` (faster than JPEGDec)
  - ESP32-P4: Hardware JPEG decoder
  - Other platforms: JPEGDec library
- ✅ **PSRAM Optimization** - Images stored in PSRAM when available
- ✅ **Pre-loading** - Next image pre-loaded during slideshow for smooth transitions
- ✅ **Slideshow** - Configurable interval with play/pause/stop
- ✅ **Manual Selection** - Navigate with next/previous buttons
- ✅ **Thumbnails** - ESP32-P4 hardware-accelerated thumbnail generation
- ✅ **Fullscreen Mode** - Dynamic resizing
- ✅ **LVGL Integration** - Native canvas and UI support

## Hardware Requirements

- **ESP32-S2/S3/P4** recommended (hardware acceleration)
- **PSRAM** highly recommended for smooth operation
- **SD card or network storage** for photos
- **Display** with LVGL support
- **Optional touchscreen** for UI interaction

## Installation

### Option 1: Copy to ESPHome components directory

```bash
cp -r picture_viewer /path/to/esphome/esphome/components/
```

### Option 2: Use as external component

```yaml
external_components:
  - source:
      type: local
      path: /path/to/picture_viewer
    components: [picture_viewer]
```

## Configuration

### Basic Example

```yaml
storage_host:
  id: storage
  file_managers:
    - id: photo_monitor
      watch_directory: /sd/photos
      patterns:
        - "*.jpg"
        - "*.jpeg"

picture_viewer:
  id: viewer
  file_manager_id: photo_monitor
  canvas_id: photo_canvas
  display_id: main_display
  watch_directory: /sd/photos
  slideshow_interval: 5s
  enable_thumbnails: true
  thumbnail_width: 120
  thumbnail_height: 90
```

### Complete Example

See `examples/picture_viewer_example.yaml` for a full LVGL UI setup with:
- Thumbnail panel (right side)
- Main image canvas (left side)
- Control buttons (play, pause, next, previous, fullscreen)
- Settings page for slideshow configuration
- Home Assistant integration

## API Reference

### Configuration Options

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `file_manager_id` | ID | Required | FileManager to monitor for image changes |
| `canvas_id` | ID | Required | LVGL canvas for displaying images |
| `display_id` | ID | Required | Display component reference |
| `watch_directory` | string | Required | Directory containing photos |
| `slideshow_interval` | time | 5s | Time between slideshow images |
| `enable_thumbnails` | bool | true | Enable thumbnail generation (P4 only) |
| `thumbnail_width` | int | 120 | Thumbnail width in pixels |
| `thumbnail_height` | int | 90 | Thumbnail height in pixels |

### Methods (available in lambdas)

```cpp
// Show specific image
id(viewer)->show_image(5);              // By index
id(viewer)->show_image("/sd/photo.jpg"); // By path

// Navigation
id(viewer)->next_image();
id(viewer)->previous_image();

// Slideshow control
id(viewer)->start_slideshow();
id(viewer)->stop_slideshow();
id(viewer)->pause_slideshow();
id(viewer)->toggle_slideshow();

// Runtime configuration
id(viewer)->set_slideshow_interval_runtime(10000);  // 10 seconds
id(viewer)->set_fullscreen(true);

// Status
size_t count = id(viewer)->get_image_count();
int index = id(viewer)->get_current_index();
auto mode = id(viewer)->get_slideshow_mode();
bool fs = id(viewer)->is_fullscreen();

// Refresh image list
id(viewer)->refresh_images();
```

## Implementation Details

### PSRAM Allocation

Images are automatically allocated in PSRAM when available for better performance:

```cpp
uint8_t *PictureViewer::allocate_image_buffer_(size_t size) {
#ifdef USE_ESP_IDF
  #if CONFIG_SPIRAM
    // Allocate in PSRAM (external RAM)
    uint8_t *buffer = (uint8_t *)heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    if (buffer != nullptr) {
      ESP_LOGD(TAG, "Allocated %zu bytes in PSRAM", size);
      return buffer;
    }
    ESP_LOGW(TAG, "PSRAM allocation failed, falling back to internal RAM");
  #endif
#endif
  // Fallback to regular heap
  uint8_t *buffer = (uint8_t *)malloc(size);
  ESP_LOGD(TAG, "Allocated %zu bytes in heap", size);
  return buffer;
}
```

### Pre-loading Next Image

During slideshow, the next image is pre-loaded in the background:

```cpp
void PictureViewer::preload_next_image_() {
  int next_index = (this->current_index_ + 1) % this->images_.size();

  if (next_index == this->next_image_index_) {
    return;  // Already pre-loaded
  }

  // Free previous pre-loaded image
  if (this->next_image_data_ != nullptr) {
    this->free_image_buffer_(this->next_image_data_);
  }

  // Load next image into PSRAM
  const auto &next_image = this->images_[next_index];
  std::vector<uint8_t> jpeg_data;
  if (!this->read_file_(next_image.path, jpeg_data)) {
    return;
  }

  // Decode to temporary buffer
  std::vector<uint8_t> rgb565_temp;
  int width, height;
  if (!this->decode_jpeg_xxx_(jpeg_data, rgb565_temp, width, height,
                               this->canvas_width_, this->canvas_height_)) {
    return;
  }

  // Allocate in PSRAM and copy
  size_t size = rgb565_temp.size();
  this->next_image_data_ = this->allocate_image_buffer_(size);
  if (this->next_image_data_ != nullptr) {
    memcpy(this->next_image_data_, rgb565_temp.data(), size);
    this->next_image_width_ = width;
    this->next_image_height_ = height;
    this->next_image_size_ = size;
    this->next_image_index_ = next_index;
  }
}
```

### FileManager Integration

The picture viewer registers a callback with FileManager:

```cpp
void PictureViewer::setup() {
  // Register callback with FileManager
  if (this->file_manager_ != nullptr) {
    this->file_manager_->add_on_directory_changed_callback(
      [this](const storage_host::DirectoryChangeInfo &info) {
        this->on_directory_changed_(info);
      }
    );
  }
}

void PictureViewer::on_directory_changed_(const storage_host::DirectoryChangeInfo &info) {
  ESP_LOGI(TAG, "Directory changed: %zu files", info.file_count);

  // Refresh image list
  this->refresh_images();

  // If current image was deleted, show next
  if (this->current_index_ >= static_cast<int>(this->images_.size())) {
    if (!this->images_.empty()) {
      this->show_image(0);
    } else {
      this->current_index_ = -1;
    }
  }
}
```

### JPEG Decoder Selection

The component automatically selects the best decoder:

```cpp
bool PictureViewer::load_jpeg_(...) {
#ifdef USE_HARDWARE_JPEG_DECODER
  // ESP32-P4: Hardware decoder
  return this->decode_jpeg_hardware_(...);
#elif defined(USE_ESP_JPEG_DECODER)
  // ESP32-S2/S3: esp_jpeg decoder
  return this->decode_jpeg_esp_(...);
#elif defined(USE_JPEGDEC)
  // Other platforms: JPEGDec library
  return this->decode_jpeg_jpegdec_(...);
#else
  #error "No JPEG decoder available"
#endif
}
```

## Platform-Specific Optimizations

### ESP32-S2/S3 (esp_jpeg)

```cpp
bool PictureViewer::decode_jpeg_esp_(...) {
  esp_jpeg_image_cfg_t jpeg_cfg = {
    .indata = jpeg_data.data(),
    .indata_size = jpeg_data.size(),
    .outbuf = (uint8_t *)output_buffer,
    .outbuf_size = output_size,
    .out_format = JPEG_IMAGE_FORMAT_RGB565,
    .out_scale = JPEG_IMAGE_SCALE_0,  // Or calculate scale factor
    .flags = {
      .swap_color_bytes = 0,
    }
  };

  esp_jpeg_image_output_t outimg;
  esp_err_t ret = esp_jpeg_decode(&jpeg_cfg, &outimg);

  if (ret == ESP_OK) {
    width = outimg.width;
    height = outimg.height;
    return true;
  }
  return false;
}
```

### ESP32-P4 (Hardware Decoder)

```cpp
bool PictureViewer::decode_jpeg_hardware_(...) {
  jpeg_decode_cfg_t decode_cfg = {
    .output_format = JPEG_DECODE_OUT_FORMAT_RGB565,
  };

  jpeg_decode_picture_info_t picture_info;
  ESP_ERROR_CHECK(jpeg_decoder_get_info(jpeg_data.data(), jpeg_data.size(), &picture_info));

  // Allocate output in PSRAM
  size_t output_size = picture_info.width * picture_info.height * 2;  // RGB565
  uint8_t *output = this->allocate_image_buffer_(output_size);

  ESP_ERROR_CHECK(jpeg_decoder_process(this->hw_decoder_, &decode_cfg,
                                        jpeg_data.data(), jpeg_data.size(),
                                        output, output_size, &out_size));

  width = picture_info.width;
  height = picture_info.height;
  return true;
}
```

## Home Assistant Integration

The example YAML includes Home Assistant services:

```yaml
# In Home Assistant
service: esphome.picture_viewer_show_image
data:
  index: 5

service: esphome.picture_viewer_start_slideshow

service: esphome.picture_viewer_set_interval
data:
  seconds: 10
```

## Performance Tips

1. **Use PSRAM** - Essential for smooth operation with large images
2. **Enable esp_jpeg** - Much faster than JPEGDec on S2/S3
3. **Pre-size images** - Resize photos to display resolution for faster loading
4. **Limit resolution** - 800x600 is usually sufficient for embedded displays
5. **Use SPI mode 6** - Fastest SPI clock for SD cards

## Troubleshooting

### Images not loading

- Check file patterns in FileManager configuration
- Verify SD card is mounted (check logs)
- Ensure JPEG files are valid (not corrupted)
- Check PSRAM is enabled in platformio_options

### Slideshow stuttering

- Enable PSRAM for pre-loading
- Reduce image file sizes
- Increase slideshow interval
- Check SD card speed (use fast cards)

### Out of memory errors

- Enable PSRAM
- Reduce canvas size
- Disable thumbnails
- Use lower resolution images

## License

Part of ESPHome - https://esphome.io
