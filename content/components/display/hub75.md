---
description: "Instructions for setting up HUB75 RGB LED matrix displays."
title: "HUB75 RGB LED Matrix Display"
params:
  seo:
    description: Instructions for setting up HUB75 RGB LED matrix displays.
    image: hub75.svg
---

The `hub75` display platform allows you to use HUB75 RGB LED matrix panels with ESPHome. This component uses a high-performance DMA-based driver that provides efficient, low-CPU-overhead driving of LED matrix panels.

HUB75 displays are RGB LED matrix panels that use parallel row updating to create dynamic, colorful displays. They are commonly available in sizes like 64×32, 64×64, and can be chained together to create larger displays.

## Hardware Requirements

**Supported ESP32 Variants:**

- ESP32 (original)
- ESP32-S2
- ESP32-S3
- ESP32-C6
- ESP32-P4

> [!WARNING]
> This component does **NOT** work with ESP32-C3, ESP32-C2, or ESP32-H2.

**Memory Considerations:**

- Uses significant internal SRAM for DMA buffering
- Larger displays or longer chains require more memory
- ESP32-S3 with PSRAM can help with memory constraints
- Memory usage increases with panel resolution and chain length

**Power Supply:**

- Requires dedicated power supply for the LED panels
- Proper grounding between ESP32 and panel is essential
- Add capacitors to power lines to prevent flickering

**Supported Panel Types:**

- "Two scan" panels (1/16 and 1/32 scan rates)
- Panels with various driver chips (FM6124, FM6126A, ICN2038S, MBI5124, DP3246, etc.)

## Pin Connections

HUB75 panels use a standard connector with the following pins:

| HUB75 Pin | Description          | ESPHome Config |
|-----------|----------------------|----------------|
| R1        | Red data (top half)  | `r1_pin`       |
| G1        | Green data (top)     | `g1_pin`       |
| B1        | Blue data (top)      | `b1_pin`       |
| R2        | Red data (bottom)    | `r2_pin`       |
| G2        | Green data (bottom)  | `g2_pin`       |
| B2        | Blue data (bottom)   | `b2_pin`       |
| A         | Row address bit 0    | `a_pin`        |
| B         | Row address bit 1    | `b_pin`        |
| C         | Row address bit 2    | `c_pin`        |
| D         | Row address bit 3    | `d_pin`        |
| E         | Row address bit 4    | `e_pin`        |
| LAT       | Latch/Strobe         | `lat_pin`      |
| OE        | Output Enable        | `oe_pin`       |
| CLK       | Clock                | `clk_pin`      |
| GND       | Ground               | N/A            |

> [!NOTE]
> The E pin is only required for 1/32 scan panels (panels with 32 or 64 rows). It can be omitted for 1/16 scan panels.

> [!TIP]
> Use a [board preset](#board-presets) to automatically configure pins for common hardware, or specify all pins manually if using custom wiring.

## Usage

```yaml
# Example using a board preset (recommended)
display:
  - platform: hub75
    id: matrix_display
    board: adafruit-matrix-portal-s3
    panel_width: 64
    panel_height: 32
    lambda: |-
      it.print(0, 0, id(font), "Hello World!");
```

> [!NOTE]
> Unlike some other display components, HUB75 does **NOT** require an SPI bus configuration. It uses DMA internally for efficient display updates.

## Board Presets

Board presets automatically configure all pin mappings for popular HUB75 controller boards. This is the recommended approach for supported hardware.

**Available Board Presets:**

- **`adafruit-matrix-portal-s3`** - Adafruit Matrix Portal S3
- **`apollo-automation-m1-rev4`** - Apollo Automation M1 Rev 4
- **`apollo-automation-m1-rev6`** - Apollo Automation M1 Rev 6
- **`huidu-hd-wf2`** - Huidu HD-WF2

### Using a Board Preset

```yaml
display:
  - platform: hub75
    id: matrix_display
    board: adafruit-matrix-portal-s3
    panel_width: 64
    panel_height: 32
```

### Overriding Specific Pins

You can override individual pins while still using a board preset:

```yaml
display:
  - platform: hub75
    id: matrix_display
    board: apollo-automation-m1-rev4
    panel_width: 64
    panel_height: 32
    r2_pin: GPIO21  # Override just one pin
    g2_pin: GPIO22
```

### Manual Pin Configuration

If you're not using a supported board, specify all pins manually:

```yaml
display:
  - platform: hub75
    id: matrix_display
    panel_width: 64
    panel_height: 32
    r1_pin: GPIO25
    g1_pin: GPIO26
    b1_pin: GPIO27
    r2_pin: GPIO21
    g2_pin: GPIO22
    b2_pin: GPIO32
    a_pin: GPIO23
    b_pin: GPIO19
    c_pin: GPIO5
    d_pin: GPIO17
    e_pin: GPIO18  # Optional, omit for 1/16 scan panels
    lat_pin: GPIO4
    oe_pin: GPIO15
    clk_pin: GPIO16
```

## Configuration Variables

### Board Configuration (Recommended)

- **board** (*Optional*, string): Board preset name. One of: `adafruit-matrix-portal-s3`, `apollo-automation-m1-rev4`, `apollo-automation-m1-rev6`, `huidu-hd-wf2`. When specified, automatically configures all pin mappings.

### Panel Dimensions (Required)

- **panel_width** (**Required**, int): Width of a single panel in pixels (e.g., `64`).
- **panel_height** (**Required**, int): Height of a single panel in pixels (e.g., `32`).

### Multi-Panel Layout (Optional)

For creating larger displays by chaining multiple panels:

- **layout_rows** (*Optional*, int): Number of panels arranged vertically. Defaults to `1`.
- **layout_cols** (*Optional*, int): Number of panels arranged horizontally. Defaults to `1`.
- **layout** (*Optional*, enum): Physical panel chaining pattern. Defaults to `HORIZONTAL`. One of:
  - `HORIZONTAL` - Simple left-to-right horizontal chain (single row)
  - `TOP_LEFT_DOWN` - Serpentine: start top-left, rows top→bottom
  - `TOP_RIGHT_DOWN` - Serpentine: start top-right, rows top→bottom
  - `BOTTOM_LEFT_UP` - Serpentine: start bottom-left, rows bottom→top
  - `BOTTOM_RIGHT_UP` - Serpentine: start bottom-right, rows bottom→top
  - `TOP_LEFT_DOWN_ZIGZAG` - Zigzag: start top-left (all panels upright)
  - `TOP_RIGHT_DOWN_ZIGZAG` - Zigzag: start top-right (all panels upright)
  - `BOTTOM_LEFT_UP_ZIGZAG` - Zigzag: start bottom-left (all panels upright)
  - `BOTTOM_RIGHT_UP_ZIGZAG` - Zigzag: start bottom-right (all panels upright)

> [!NOTE]
> The total display size will be `panel_width × layout_cols` by `panel_height × layout_rows` pixels.

### Panel Hardware (Optional)

- **scan_wiring** (*Optional*, enum): Panel scan wiring pattern. Defaults to `STANDARD_TWO_SCAN`. One of:
  - `STANDARD_TWO_SCAN` - Standard 1/16 or 1/32 scan (most common)
  - `FOUR_SCAN_16PX_HIGH` - Four-scan for 16px high panels
  - `FOUR_SCAN_32PX_HIGH` - Four-scan for 32px high panels
  - `FOUR_SCAN_64PX_HIGH` - Four-scan for 64px high panels

- **shift_driver** (*Optional*, enum): LED shift register driver chip type. Defaults to `GENERIC`. One of:
  - `GENERIC` - Standard shift register (default)
  - `FM6124` - FM6124 driver
  - `FM6126A` - FM6126A / ICN2038S driver (very common)
  - `ICN2038S` - Alias for FM6126A
  - `MBI5124` - MBI5124 driver (requires `clock_phase: true`)
  - `DP3246` - DP3246 driver

### Display Configuration (Optional)

- **brightness** (*Optional*, int): Initial brightness level (0-255). Defaults to `128`.
- **bit_depth** (*Optional*, int): Color bit depth (6-12). Higher values = better color accuracy but slower refresh. Defaults to `8`.
- **gamma_correct** (*Optional*, enum): Gamma correction mode. One of:
  - `LINEAR` - No gamma correction (raw values)
  - `CIE1931` - CIE 1931 perceptual curve (recommended for most displays)
  - `GAMMA_2_2` - Standard 2.2 gamma curve
- **double_buffer** (*Optional*, boolean): Enable double buffering to prevent tearing. Defaults to `false`. Set to `false` when using LVGL.
- **update_interval** (*Optional*, [Time](/guides/configuration-types#time)): Display update frequency. Defaults to `16ms` (~60 FPS). Set to `never` when using LVGL.
- **min_refresh_rate** (*Optional*, int): Minimum panel refresh rate in Hz (40-200). The panel may refresh faster than this, but won't go slower. Auto-calculated from `update_interval` (defaults to 60 Hz when `update_interval: never`). Rarely needs to be set manually.

### Pin Configuration

**Required when `board` is not specified:**

- **r1_pin** ([Pin Schema](/guides/configuration-types#pin-schema)): Red data pin for top half.
- **g1_pin** ([Pin Schema](/guides/configuration-types#pin-schema)): Green data pin for top half.
- **b1_pin** ([Pin Schema](/guides/configuration-types#pin-schema)): Blue data pin for top half.
- **r2_pin** ([Pin Schema](/guides/configuration-types#pin-schema)): Red data pin for bottom half.
- **g2_pin** ([Pin Schema](/guides/configuration-types#pin-schema)): Green data pin for bottom half.
- **b2_pin** ([Pin Schema](/guides/configuration-types#pin-schema)): Blue data pin for bottom half.
- **a_pin** ([Pin Schema](/guides/configuration-types#pin-schema)): Row address bit 0.
- **b_pin** ([Pin Schema](/guides/configuration-types#pin-schema)): Row address bit 1.
- **c_pin** ([Pin Schema](/guides/configuration-types#pin-schema)): Row address bit 2.
- **d_pin** ([Pin Schema](/guides/configuration-types#pin-schema)): Row address bit 3.
- **e_pin** (*Optional*, [Pin Schema](/guides/configuration-types#pin-schema)): Row address bit 4. Required for 1/32 scan panels (32+ rows), omit for 1/16 scan panels.
- **lat_pin** ([Pin Schema](/guides/configuration-types#pin-schema)): Latch/strobe pin.
- **oe_pin** ([Pin Schema](/guides/configuration-types#pin-schema)): Output enable pin.
- **clk_pin** ([Pin Schema](/guides/configuration-types#pin-schema)): Clock pin.

### Advanced Timing (Optional)

- **clock_speed** (*Optional*, enum): Output clock speed. Defaults to `20MHZ`. One of:
  - `8MHZ` - 8 MHz
  - `10MHZ` - 10 MHz
  - `16MHZ` - 16 MHz
  - `20MHZ` - 20 MHz (default)

- **latch_blanking** (*Optional*, int): Number of clock cycles OE is blanked during LAT pulse. Defaults to `1`.
- **clock_phase** (*Optional*, boolean): Invert clock phase. Defaults to `false`. Required to be `true` for MBI5124 driver.

### Standard Display Options

All standard [graphical display configuration](/components/display#display-configuration) options are also available, including **lambda**, **pages**, **rotation**, and **auto_clear_enabled**.

## Multi-Panel Layouts

You can chain multiple panels together to create larger displays. The component supports both simple horizontal chains and complex 2D grid arrangements.

### Horizontal Chain

For a simple left-to-right horizontal chain, use `layout_cols`:

```yaml
# Three 64x32 panels chained horizontally = 192x32 total
display:
  - platform: hub75
    id: matrix_display
    board: adafruit-matrix-portal-s3
    panel_width: 64
    panel_height: 32
    layout_cols: 3
    layout: HORIZONTAL
```

### 2D Grid Layouts

For panels arranged in rows and columns, specify both `layout_rows` and `layout_cols`, along with the appropriate `layout` pattern:

```yaml
# Four 64x32 panels in 2x2 grid = 128x64 total
display:
  - platform: hub75
    id: matrix_display
    board: adafruit-matrix-portal-s3
    panel_width: 64
    panel_height: 32
    layout_rows: 2
    layout_cols: 2
    layout: TOP_LEFT_DOWN_ZIGZAG
```

> [!TIP]
> **Serpentine layouts** (e.g., `TOP_LEFT_DOWN`) physically rotate alternate rows upside down to minimize cable length. **Zigzag layouts** (e.g., `TOP_LEFT_DOWN_ZIGZAG`) keep all panels upright but require longer cables between rows.

## Using with LVGL

When using this display with {{< docref "/components/lvgl/index" "LVGL" >}}, you must configure the display as follows:

```yaml
display:
  - platform: hub75
    id: matrix_display
    board: adafruit-matrix-portal-s3
    panel_width: 64
    panel_height: 32
    update_interval: never
    auto_clear_enabled: false
    double_buffer: false
    # LVGL configuration goes separately
```

The three key settings for LVGL are:

- `update_interval: never` - LVGL controls when to update (minimum refresh rate auto-defaults to 60 Hz)
- `auto_clear_enabled: false` - LVGL handles clearing
- `double_buffer: false` - LVGL manages its own buffering

## Configuration Examples

### Basic Single Panel (with Board Preset)

```yaml
display:
  - platform: hub75
    id: matrix_display
    board: adafruit-matrix-portal-s3
    panel_width: 64
    panel_height: 32
    lambda: |-
      it.print(0, 0, id(font), "Hello!");
```

### Horizontally Chained Panels

```yaml
# Three 64x32 panels chained horizontally = 192x32 total
display:
  - platform: hub75
    id: matrix_display
    board: apollo-automation-m1-rev4
    panel_width: 64
    panel_height: 32
    layout_cols: 3
    layout: HORIZONTAL
    lambda: |-
      it.printf(0, 0, id(font), "Width: %d", it.get_width());
```

### Manual Pin Configuration

```yaml
# Using custom pins without a board preset
display:
  - platform: hub75
    id: matrix_display
    panel_width: 64
    panel_height: 32
    r1_pin: GPIO25
    g1_pin: GPIO26
    b1_pin: GPIO27
    r2_pin: GPIO21
    g2_pin: GPIO22
    b2_pin: GPIO32
    a_pin: GPIO23
    b_pin: GPIO19
    c_pin: GPIO5
    d_pin: GPIO17
    e_pin: GPIO18  # Required for 1/32 scan panels
    lat_pin: GPIO4
    oe_pin: GPIO15
    clk_pin: GPIO16
```

### Advanced Configuration with Shift Driver

```yaml
display:
  - platform: hub75
    id: matrix_display
    board: adafruit-matrix-portal-s3
    panel_width: 64
    panel_height: 32
    shift_driver: FM6126A
    clock_speed: 20MHZ
    brightness: 200
    bit_depth: 10
    latch_blanking: 2
    lambda: |-
      it.fill(Color(255, 0, 0));  # Red background
```

### 2×2 Grid Layout

```yaml
# Four 64x32 panels in a 2x2 grid = 128x64 total
display:
  - platform: hub75
    id: matrix_display
    board: adafruit-matrix-portal-s3
    panel_width: 64
    panel_height: 32
    layout_rows: 2
    layout_cols: 2
    layout: TOP_LEFT_DOWN_ZIGZAG
    lambda: |-
      it.printf(32, 28, id(font), "128x64");
```

## Important Notes

- **ESP32 support**: This component works with ESP32, ESP32-S2, ESP32-S3, ESP32-C6, and ESP32-P4. It does NOT work with ESP32-C3, ESP32-C2, or ESP32-H2.
- **Memory limitations**: The DMA buffer can consume significant RAM. Larger displays or longer panel chains may not fit in available memory. ESP32-S3 with PSRAM is recommended for large installations.
- **Board presets**: Using a board preset is the easiest way to get started. It automatically configures all pins correctly for popular hardware.
- **Pin configuration**: If not using a board preset, all pins (except `e_pin`) must be specified manually. There are no default pin values.
- **Driver compatibility**: Different panels use different shift register chips. If colors appear wrong or the display doesn't work, try different `shift_driver` settings. FM6126A/ICN2038S is very common in modern panels.
- **Power supply**: Always use a dedicated power supply for the panels. LED matrices can draw significant current (multiple amps for larger displays).
- **E pin requirement**: The E pin is required for 1/32 scan panels (32 or 64 rows). It can be omitted for 1/16 scan panels (16 rows).
- **Clock phase**: MBI5124 driver requires `clock_phase: true` to function correctly.

## See Also

- {{< docref "index/" >}}
- {{< docref "/components/lvgl/index" "LVGL Component" >}}
- {{< apiref "hub75/hub75_component.h" "hub75/hub75_component.h" >}}
- [HUB75 ESP32 Driver Library](https://github.com/stuartparmenter/hub75-esp32)
