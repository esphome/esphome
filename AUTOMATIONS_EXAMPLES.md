# Storage Component Automations - Usage Examples

This document shows examples of how to use the automations for the SD MMC Card and USB MSC Host components.

## SD MMC Card Component

### Trigger: `on_mounted`

Triggered when the SD card is successfully mounted.

```yaml
sd_mmc_card:
  id: my_sd_card
  clk_pin: GPIO14
  cmd_pin: GPIO15
  data0_pin: GPIO2
  on_mounted:
    - lambda: |-
        ESP_LOGI("main", "SD Card mounted at: %s", mount_path.c_str());
    - logger.log: "SD Card is ready!"
```

### Action: `sd_mmc_card.mount`

Manually mount the SD card.

```yaml
# Simple usage
button:
  - platform: template
    name: "Mount SD Card"
    on_press:
      - sd_mmc_card.mount: my_sd_card

# With condition
button:
  - platform: template
    name: "Mount if not mounted"
    on_press:
      - if:
          condition:
            not:
              sd_mmc_card.is_mounted: my_sd_card
          then:
            - sd_mmc_card.mount: my_sd_card
```

### Action: `sd_mmc_card.unmount`

Manually unmount the SD card.

```yaml
button:
  - platform: template
    name: "Unmount SD Card"
    on_press:
      - sd_mmc_card.unmount: my_sd_card
```

### Action: `sd_mmc_card.list_files`

List files in a directory on the SD card.

```yaml
# List files in mount root
button:
  - platform: template
    name: "List Files"
    on_press:
      - sd_mmc_card.list_files: my_sd_card

# List files in specific directory
button:
  - platform: template
    name: "List Images"
    on_press:
      - sd_mmc_card.list_files:
          id: my_sd_card
          path: "/sdcard/images"
```

### Condition: `sd_mmc_card.is_mounted`

Check if the SD card is currently mounted.

```yaml
button:
  - platform: template
    name: "Do something with SD Card"
    on_press:
      - if:
          condition:
            sd_mmc_card.is_mounted: my_sd_card
          then:
            - logger.log: "Card is mounted, proceeding..."
            - sd_mmc_card.list_files: my_sd_card
          else:
            - logger.log: "Card not mounted, mounting first..."
            - sd_mmc_card.mount: my_sd_card
```

## USB MSC Host Component

### Trigger: `on_mounted`

Triggered when a USB mass storage device is successfully mounted.

```yaml
usb_msc_host:
  id: usb_host
  usb_host_id: usb_bus
  devices:
    - id: usb_stick
      mount_path: "/usb"
      on_mounted:
        - lambda: |-
            ESP_LOGI("main", "USB device mounted at: %s", mount_path.c_str());
        - logger.log: "USB stick is ready!"
```

### Action: `usb_msc_host.remount`

Remount the USB device (unmount + mount).

```yaml
button:
  - platform: template
    name: "Remount USB"
    on_press:
      - usb_msc_host.remount: usb_stick
```

### Action: `usb_msc_host.unmount`

Unmount the USB device.

```yaml
button:
  - platform: template
    name: "Unmount USB"
    on_press:
      - usb_msc_host.unmount: usb_stick
```

### Action: `usb_msc_host.list_files`

List files on the USB device.

```yaml
button:
  - platform: template
    name: "List USB Files"
    on_press:
      - usb_msc_host.list_files: usb_stick
```

### Condition: `usb_msc_host.is_mounted`

Check if the USB device is currently mounted.

```yaml
button:
  - platform: template
    name: "Access USB Data"
    on_press:
      - if:
          condition:
            usb_msc_host.is_mounted: usb_stick
          then:
            - logger.log: "USB is mounted!"
            - usb_msc_host.list_files: usb_stick
          else:
            - logger.log: "USB not mounted"
```

## Complete Example

Here's a complete example showing all features working together:

```yaml
esphome:
  name: storage-demo

esp32:
  board: esp32-s3-devkitc-1

logger:

# USB Host setup
usb_host:
  id: usb_bus

# USB MSC Host with automation
usb_msc_host:
  id: usb_host
  usb_host_id: usb_bus
  devices:
    - id: usb_stick
      mount_path: "/usb"
      on_mounted:
        - logger.log: "USB mounted, listing files..."
        - usb_msc_host.list_files: usb_stick

# SD MMC Card with automation
sd_mmc_card:
  id: sd_card
  clk_pin: GPIO14
  cmd_pin: GPIO15
  data0_pin: GPIO2
  data1_pin: GPIO4
  data2_pin: GPIO12
  data3_pin: GPIO13
  on_mounted:
    - logger.log: "SD Card mounted!"
    - sd_mmc_card.list_files: sd_card

# Storage Host for images
storage_host:
  id: storage
  storage_images:
    - id: my_image
      file: "/usb/photo.jpg"
      format: RGB565
      auto_load: true

# Buttons to control storage
button:
  - platform: template
    name: "Mount SD Card"
    on_press:
      - sd_mmc_card.mount: sd_card

  - platform: template
    name: "Unmount SD Card"
    on_press:
      - sd_mmc_card.unmount: sd_card

  - platform: template
    name: "List SD Files"
    on_press:
      - if:
          condition:
            sd_mmc_card.is_mounted: sd_card
          then:
            - sd_mmc_card.list_files: sd_card
          else:
            - logger.log: "SD Card not mounted!"

  - platform: template
    name: "Remount USB"
    on_press:
      - usb_msc_host.remount: usb_stick

  - platform: template
    name: "List USB Files"
    on_press:
      - if:
          condition:
            usb_msc_host.is_mounted: usb_stick
          then:
            - usb_msc_host.list_files: usb_stick
          else:
            - logger.log: "USB not mounted!"
```

## Advanced: Auto-reload images on mount

```yaml
sd_mmc_card:
  id: sd_card
  clk_pin: GPIO14
  cmd_pin: GPIO15
  data0_pin: GPIO2
  on_mounted:
    - logger.log: "SD Card mounted, reloading images..."
    # Reload all storage images that reference this mount
    - lambda: |-
        // Images will auto-load via the callback system
        ESP_LOGI("main", "Mount ready event sent to all storage images");

storage_host:
  storage_images:
    - id: wallpaper
      file: "/sdcard/wallpaper.jpg"
      # This image will automatically load when SD card is mounted
      # thanks to the callback system
```
