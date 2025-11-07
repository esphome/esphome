# Picture Viewer LVGL Page Guide

This guide explains how to use the LVGL page configuration for the picture viewer component.

## Quick Start

### Option 1: Use the Complete Example

The `picture_viewer_example.yaml` file contains everything you need:

```bash
# Copy to your project
cp picture_viewer_example.yaml my_photo_frame.yaml

# Edit hardware pins and settings
nano my_photo_frame.yaml

# Compile and flash
esphome run my_photo_frame.yaml
```

### Option 2: Integrate into Existing YAML

Copy the LVGL section from `picture_viewer_lvgl_page.yaml` into your existing configuration:

```yaml
# Your existing config
esphome:
  name: my-device

esp32:
  board: esp32-s3-devkitc-1

# Add storage_host and picture_viewer
storage_host:
  id: storage
  mounts:
    - id: sd_mount
      path: /sd
      platform: sd_mmc
  file_managers:
    - id: photo_monitor
      watch_directory: /sd/photos
      patterns: ["*.jpg", "*.jpeg"]

picture_viewer:
  id: viewer
  file_manager_id: photo_monitor
  canvas_id: photo_canvas
  display_id: main_display
  watch_directory: /sd/photos
  slideshow_interval: 5s

# Copy the entire lvgl: section from picture_viewer_lvgl_page.yaml
lvgl:
  # ... (full LVGL configuration)
```

## Layout Breakdown

### Display Dimensions

Two pre-configured layouts are available:

**Standard Version** - `picture_viewer_lvgl_page.yaml` for **800x480** displays
**Large Version** - `picture_viewer_lvgl_page_1280x800.yaml` for **1280x800** displays

Both can be easily adapted for other sizes:

```
┌─────────────────────────────────┬───────────┐
│                                 │  Photos   │ ← Thumbnail Panel (150px)
│         Main Canvas             ├───────────┤
│         (650x400)               │ [thumb1]  │
│      Photo Display Area         │ [thumb2]  │
│                                 │ [thumb3]  │
│                                 │   ...     │
├─────────────────────────────────┼───────────┤
│  [◄] [▶] [►] [⛶] [⚙]  5 / 23   │           │ ← Control Panel (80px)
└─────────────────────────────────┴───────────┘
        ↑ Main Area (650px)
```

### Layout Comparison

**800x480 Layout:**
- Main canvas: 650x400
- Thumbnail panel: 150px wide
- Control panel: 80px high
- Buttons: 70x60
- 8 thumbnails (120x90)

**1280x800 Layout:**
- Main canvas: 1040x700
- Thumbnail panel: 240px wide
- Control panel: 100px high
- Buttons: 90x80
- 10 thumbnails (200x150)

### Customizing for Different Screen Sizes

**For 480x320 displays:**

```yaml
lvgl:
  pages:
    - id: photo_viewer_page
      widgets:
        - obj:
            id: main_area
            width: 380  # 480 - 100 (thumbnail panel)
            height: 320
            widgets:
              - canvas:
                  id: photo_canvas
                  width: 380
                  height: 240  # 320 - 80 (control panel)

        - obj:
            id: thumbnail_panel
            width: 100  # Narrower thumbnails
            height: 320
```

**For custom sizes:**

Use the following formula:
- Thumbnail panel: ~18-20% of total width (150-240px)
- Main area: 80-82% of total width
- Control panel: ~10-12% of total height
- Canvas: Main area width × (total height - control panel height)

## Features Explained

### 1. Main Canvas

The photo display area where images are shown:

```yaml
- canvas:
    id: photo_canvas
    width: 650
    height: 400
    bg_color: 0x000000
```

The `picture_viewer` component renders images directly to this canvas.

### 2. Control Buttons

**Previous (◄)** - Shows previous image:
```yaml
on_click:
  - lambda: id(viewer)->previous_image();
```

**Next (►)** - Shows next image:
```yaml
on_click:
  - lambda: id(viewer)->next_image();
```

**Play/Pause (▶/⏸)** - Toggles slideshow:
```yaml
on_click:
  - lambda: id(viewer)->toggle_slideshow();
```

**Fullscreen (⛶)** - Hides thumbnails for larger view:
```yaml
on_click:
  - lambda: |-
      bool is_fs = id(viewer)->is_fullscreen();
      id(viewer)->set_fullscreen(!is_fs);

      if (!is_fs) {
        // Hide thumbnail panel
        lv_obj_add_flag(id(thumbnail_panel), LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_width(id(photo_canvas), 800);
      } else {
        // Show thumbnail panel
        lv_obj_clear_flag(id(thumbnail_panel), LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_width(id(photo_canvas), 650);
      }
```

**Settings (⚙)** - Opens settings page:
```yaml
on_click:
  - lvgl.page.show: settings_page
```

### 3. Thumbnail Panel

Static thumbnail buttons (8 slots) that can be clicked to jump to specific photos:

```yaml
- button:
    id: thumb_btn_1
    width: 120
    height: 90
    on_click:
      - lambda: id(viewer)->show_image(0);  # Show first image
```

**Note:** For ESP32-P4, the `picture_viewer` component can generate actual thumbnail images using hardware JPEG decoder and display them on these buttons.

### 4. Settings Page

Adjustable slideshow interval (1-60 seconds):

```yaml
- slider:
    id: interval_slider
    min_value: 1
    max_value: 60
    value: 5
    on_value:
      - lambda: |-
          int seconds = (int)x;
          id(viewer)->set_slideshow_interval_runtime(seconds * 1000);
```

Auto-start slideshow toggle:

```yaml
- switch:
    id: autostart_switch
    on_value:
      - lambda: |-
          if (x) {
            id(viewer)->start_slideshow();
          } else {
            id(viewer)->stop_slideshow();
          }
```

### 5. Auto-Update Interval

Updates photo counter and play/pause icon every second:

```yaml
interval:
  - interval: 1s
    then:
      - lambda: |-
          size_t count = id(viewer)->get_image_count();
          int index = id(viewer)->get_current_index();

          char buf[32];
          snprintf(buf, sizeof(buf), "%d / %zu", index + 1, count);
          lv_label_set_text(id(photo_counter), buf);

          bool is_playing = (id(viewer)->get_slideshow_mode() ==
                            picture_viewer::SlideshowMode::PLAYING);
          lv_label_set_text(id(play_pause_icon), is_playing ? "⏸" : "▶");
```

## Customization Tips

### Change Colors

**Dark theme (default):**
```yaml
theme:
  obj:
    bg_color: 0x000000
    text_color: 0xFFFFFF
  btn:
    bg_color: 0x2196F3  # Blue buttons
```

**Light theme:**
```yaml
theme:
  obj:
    bg_color: 0xFFFFFF
    text_color: 0x000000
  btn:
    bg_color: 0x4CAF50  # Green buttons
```

**Custom accent color:**
```yaml
theme:
  btn:
    bg_color: 0xFF5722  # Orange
  slider:
    indicator_color: 0xFF5722  # Match slider to buttons
```

### Add More Thumbnails

To show more than 8 thumbnails, add more buttons in the `thumbnail_list`:

```yaml
- obj:
    id: thumbnail_list
    widgets:
      # ... existing 8 buttons ...

      # Add more:
      - button:
          id: thumb_btn_9
          width: 120
          height: 90
          on_click:
            - lambda: id(viewer)->show_image(8);

      - button:
          id: thumb_btn_10
          width: 120
          height: 90
          on_click:
            - lambda: id(viewer)->show_image(9);
```

### Change Button Icons

Replace Unicode symbols with custom fonts or images:

```yaml
# Current (Unicode):
- label:
    text: "◄"
    text_font: montserrat_32

# Custom font with icons:
- label:
    text: "\uF053"  # FontAwesome left arrow
    text_font: fontawesome_32
```

### Add Photo Info Overlay

Show filename or EXIF data on the canvas:

```yaml
- obj:
    id: photo_info_overlay
    width: 650
    height: 40
    bg_color: 0x000000
    bg_opa: 180  # Semi-transparent
    y: 360  # Bottom of canvas
    widgets:
      - label:
          id: photo_filename
          text: ""
          text_color: 0xFFFFFF

# Update in interval:
interval:
  - interval: 1s
    then:
      - lambda: |-
          auto *img = id(viewer)->get_current_image();
          if (img != nullptr) {
            lv_label_set_text(id(photo_filename), img->filename.c_str());
          }
```

### Disable Thumbnails

Hide the thumbnail panel completely:

```yaml
- obj:
    id: main_area
    width: 800  # Full width

- obj:
    id: thumbnail_panel
    width: 0  # Or remove entirely
    flags: HIDDEN
```

## Integration with FileManager

The picture viewer automatically updates when files are added/removed via FileManager:

```yaml
file_managers:
  - id: photo_monitor
    watch_directory: /sd/photos
    patterns: ["*.jpg", "*.jpeg"]
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
- Copy photos to SD card → Automatically detected and added to slideshow
- Delete photos → List updated, viewer adjusts current index
- Swap SD cards → New photos loaded when card is mounted

## Troubleshooting

### Photos not showing

Check that:
1. FileManager is scanning the correct directory
2. JPEG files match the patterns (`*.jpg`, `*.jpeg`)
3. Files are large enough (check `min_size` setting)
4. SD card is mounted (check logs)

### UI elements misaligned

Adjust dimensions for your display:
```yaml
# Check your display size
display:
  - platform: st7789v
    width: 320   # Your actual width
    height: 480  # Your actual height
```

Then update canvas and panel sizes accordingly.

### Slideshow not starting

Verify picture viewer configuration:
```yaml
picture_viewer:
  id: viewer
  canvas_id: photo_canvas      # Must match LVGL canvas ID
  display_id: main_display     # Must match display ID
  slideshow_interval: 5s
```

### Thumbnails showing numbers instead of images

This is normal for ESP32-S2/S3. Hardware-accelerated thumbnails are only available on ESP32-P4. The numbers are placeholder buttons that still work for navigation.

## Performance Tips

1. **Enable PSRAM** for smooth operation:
   ```yaml
   esp32:
     framework:
       type: esp-idf
       sdkconfig_options:
         CONFIG_SPIRAM: y
   ```

2. **Pre-resize images** to display resolution (650x400 or smaller)

3. **Adjust scan interval** to reduce SD card access:
   ```yaml
   file_managers:
     - scan_interval: 30s  # Instead of 10s
   ```

4. **Reduce buffer size** if experiencing memory issues:
   ```yaml
   lvgl:
     buffer_size: 15%  # Instead of 25%
   ```

## Next Steps

1. **Test with sample photos** - Copy some JPEGs to your SD card
2. **Adjust layout** for your display size
3. **Customize theme** to match your project
4. **Add Home Assistant integration** (see `picture_viewer_example.yaml`)
5. **Implement thumbnail loading** for ESP32-P4

Happy viewing! 📸✨
