# Picture Viewer - Quick Start Guide

## What You Get

A complete picture viewer system that replaces your old `sd_image` approach with:

✅ **Dynamic loading** - No more boot-time-only loading
✅ **Mount/unmount support** - SD cards and USB drives can be hot-swapped
✅ **FileManager integration** - Automatic detection of new/deleted images
✅ **PSRAM optimization** - Images stored in PSRAM for smooth performance
✅ **Pre-loading** - Next image loaded in background during slideshow
✅ **Platform-optimized JPEG** - esp_jpeg (S2/S3), hardware decoder (P4), JPEGDec (fallback)
✅ **Full LVGL UI** - Thumbnails, controls, fullscreen mode

## File Structure

```
esphome/
├── components/
│   ├── picture_viewer/
│   │   ├── __init__.py        # Python configuration
│   │   ├── picture_viewer.h   # Component header
│   │   └── README.md          # Full documentation
│   └── storage_host/
│       ├── file_manager.h     # File monitoring
│       └── file_manager.cpp
└── examples/
    ├── picture_viewer_example.yaml  # Complete YAML config
    └── PICTURE_VIEWER_QUICKSTART.md # This file
```

## Implementation Status

### ✅ Completed

1. **FileManager component** - Monitors directories for changes
2. **Picture viewer architecture** - Complete header with all features
3. **Python codegen** - YAML configuration support
4. **YAML example** - Full LVGL UI with all controls
5. **Documentation** - Comprehensive README with implementation details

### ⚙️ Needs Implementation (C++)

The C++ implementation file (`picture_viewer.cpp`) needs to be created with:

1. **PSRAM allocation** - Example code provided in README
2. **JPEG decoding** - Three backends (esp_jpeg, hardware, JPEGDec)
3. **Image resizing** - Bilinear interpolation for smooth scaling
4. **Slideshow logic** - Pre-loading and auto-advance
5. **LVGL canvas updates** - Draw RGB565 data to canvas
6. **Thumbnail generation** - ESP32-P4 hardware-accelerated

## Quick Start

### 1. Copy Files

```bash
# If using as external component
cp -r components/picture_viewer /path/to/your/project/components/

# Or add to ESPHome
cp -r components/picture_viewer /path/to/esphome/esphome/components/
```

### 2. Update Your Configuration

Replace your old `storage_images` config with:

```yaml
# Old way (removed)
storage_host:
  storage_images:
    - id: my_image
      file: /sd/photo.jpg
      auto_load: true

# New way (dynamic)
storage_host:
  id: storage
  file_managers:
    - id: photo_monitor
      watch_directory: /sd/photos
      patterns: ["*.jpg", "*.jpeg"]
      on_file_added:
        - lambda: id(viewer)->refresh_images();

picture_viewer:
  id: viewer
  file_manager_id: photo_monitor
  canvas_id: photo_canvas
  slideshow_interval: 5s
```

### 3. Add LVGL UI

See `examples/picture_viewer_example.yaml` for the complete LVGL page setup with:

- Main canvas (650x400) for photos
- Thumbnail panel (150px wide, right side)
- Control buttons (play, pause, next, previous, fullscreen)
- Info label showing "current / total"

### 4. Platform-Specific Setup

**ESP32-S2/S3:**
```yaml
esp32:
  framework:
    type: esp-idf
    sdkconfig_options:
      CONFIG_ESP_JPEG_DECODE_ENABLED: y  # Enable esp_jpeg
```

**ESP32-P4:**
```yaml
esp32:
  variant: ESP32P4
  framework:
    type: esp-idf
    # Hardware JPEG decoder auto-enabled
```

**Other platforms:**
```yaml
# JPEGDec library will be auto-added
```

### 5. Enable PSRAM (Highly Recommended)

```yaml
esp32:
  board: esp32-s3-devkitc-1
  framework:
    type: esp-idf
    sdkconfig_options:
      CONFIG_SPIRAM: y
      CONFIG_SPIRAM_MODE_OCT: y  # For S3 with octal PSRAM
```

## Key Features Explained

### FileManager Integration

```yaml
file_managers:
  - id: photo_monitor
    watch_directory: /sd/photos
    scan_interval: 10s
    on_file_added:
      - lambda: |-
          ESP_LOGI("photos", "New: %s", x.filename.c_str());
          id(viewer)->refresh_images();
    on_file_deleted:
      - lambda: |-
          ESP_LOGI("photos", "Deleted: %s", x.filename.c_str());
          id(viewer)->refresh_images();
```

When you:
- Copy new JPEGs to SD card → Automatically detected and added
- Delete photos → List automatically updated
- Swap SD cards → New photos loaded when mounted

### PSRAM Optimization

Images are allocated in PSRAM for better performance:

```cpp
// Automatically uses PSRAM if available
uint8_t *buffer = viewer->allocate_image_buffer_(640 * 480 * 2);  // RGB565

// Current image in PSRAM
uint8_t *current_image_data_;

// Next image pre-loaded in PSRAM for smooth slideshow
uint8_t *next_image_data_;
```

### Pre-loading Next Image

During slideshow, the next image loads in the background:

```
Current image: photo1.jpg (displayed)
Next image:    photo2.jpg (loading in PSRAM...)
               ↓
User sees:     photo1.jpg
Background:    photo2.jpg [████████████] 100%
               ↓
Instant swap:  photo2.jpg (no loading delay!)
Next image:    photo3.jpg (loading in PSRAM...)
```

### Platform-Optimized JPEG

The component automatically selects the fastest decoder:

| Platform | Decoder | Speed | Notes |
|----------|---------|-------|-------|
| ESP32-S2/S3 | esp_jpeg | ⚡⚡⚡ Very Fast | Built into ESP-IDF |
| ESP32-P4 | Hardware | ⚡⚡⚡⚡ Fastest | Dedicated JPEG engine |
| ESP32/ESP8266 | JPEGDec | ⚡⚡ Fast | Software decoder |

## API Usage Examples

### Manual Control

```yaml
# Show specific image
on_click:
  - lambda: id(viewer)->show_image(5);

# Navigate
on_press:
  - lambda: id(viewer)->next_image();

# Slideshow
on_click:
  - lambda: id(viewer)->start_slideshow();
```

### Home Assistant Integration

```yaml
api:
  services:
    - service: show_image
      variables:
        index: int
      then:
        - lambda: id(viewer)->show_image(index);

    - service: set_slideshow_interval
      variables:
        seconds: int
      then:
        - lambda: id(viewer)->set_slideshow_interval_runtime(seconds * 1000);
```

Then in Home Assistant:

```yaml
service: esphome.picture_viewer_show_image
data:
  index: 10

service: esphome.picture_viewer_set_slideshow_interval
data:
  seconds: 15
```

### Dynamic Updates

```yaml
# When file added/deleted
on_file_added:
  - lambda: |-
      id(viewer)->refresh_images();
      // Show new image if list was empty
      if (id(viewer)->get_current_index() < 0) {
        id(viewer)->show_image(0);
      }
```

## UI Layout

```
┌─────────────────────────────────┬───────────┐
│                                 │  Photos   │
│                                 ├───────────┤
│                                 │ [thumb1]  │
│         Main Canvas             │ [thumb2]  │
│         (650x400)               │ [thumb3]  │
│                                 │ [thumb4]  │
│                                 │ [thumb5]  │
│                                 │ [thumb6]  │
├─────────────────────────────────┤ [thumb7]  │
│  [◄] [▶] [►] [⛶]    [5 / 23]   │ [thumb8]  │
└─────────────────────────────────┴───────────┘
```

## Next Steps

1. **Review** `examples/picture_viewer_example.yaml` - Complete working example
2. **Implement** `picture_viewer.cpp` - Use code snippets from README.md
3. **Test** with your hardware - Adjust pins and display settings
4. **Customize** UI - Modify LVGL layout to your preferences

## Implementation Checklist

- [ ] Copy component files to your project
- [ ] Implement `picture_viewer.cpp` (use README examples)
- [ ] Configure SD card pins
- [ ] Configure display settings
- [ ] Enable PSRAM in platformio_options
- [ ] Add LVGL UI from example YAML
- [ ] Test with sample JPEGs
- [ ] Customize UI layout
- [ ] Add Home Assistant integration (optional)

## Need Help?

Check the full documentation in `components/picture_viewer/README.md` for:
- Complete implementation examples
- PSRAM allocation code
- JPEG decoder integration
- Performance optimization tips
- Troubleshooting guide

Happy viewing! 📸✨
