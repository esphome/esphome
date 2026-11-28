import logging
from typing import Any

from esphome import automation, pins
import esphome.codegen as cg
from esphome.components import display
from esphome.components.esp32 import add_idf_component
import esphome.config_validation as cv
from esphome.const import (
    CONF_AUTO_CLEAR_ENABLED,
    CONF_BIT_DEPTH,
    CONF_BOARD,
    CONF_BRIGHTNESS,
    CONF_CLK_PIN,
    CONF_GAMMA_CORRECT,
    CONF_ID,
    CONF_OE_PIN,
    CONF_ROTATION,
    CONF_UPDATE_INTERVAL,
)
from esphome.core import EnumValue
import esphome.final_validate as fv
from esphome.helpers import add_class_to_obj
from esphome.schema_extractors import SCHEMA_EXTRACT, schema_extractor
from esphome.types import ConfigType

from . import boards, hub75_ns

_LOGGER = logging.getLogger(__name__)

DEPENDENCIES = ["esp32"]
CODEOWNERS = ["@stuartparmenter"]

# Load all board presets
BOARDS = boards.BoardRegistry.get_boards()

# Constants
CONF_HUB75_ID = "hub75_id"

# Panel dimensions
CONF_PANEL_WIDTH = "panel_width"
CONF_PANEL_HEIGHT = "panel_height"

# Multi-panel layout
CONF_LAYOUT_ROWS = "layout_rows"
CONF_LAYOUT_COLS = "layout_cols"
CONF_LAYOUT = "layout"

# Panel hardware
CONF_SCAN_WIRING = "scan_wiring"
CONF_SCAN_RATE = "scan_rate"
CONF_SCAN_SEGMENT_WIDTH = "scan_segment_width"
CONF_SCAN_FLAGS = "scan_flags"
CONF_SHIFT_DRIVER = "shift_driver"
CONF_ROW_DECODER = "row_decoder"

# RGB pins
CONF_R1_PIN = "r1_pin"
CONF_G1_PIN = "g1_pin"
CONF_B1_PIN = "b1_pin"
CONF_R2_PIN = "r2_pin"
CONF_G2_PIN = "g2_pin"
CONF_B2_PIN = "b2_pin"

# Address pins
CONF_A_PIN = "a_pin"
CONF_B_PIN = "b_pin"
CONF_C_PIN = "c_pin"
CONF_D_PIN = "d_pin"
CONF_E_PIN = "e_pin"

# Control pins
CONF_LAT_PIN = "lat_pin"

NEVER = 4294967295  # uint32_t max - value used when update_interval is "never"

# Pin mapping from config keys to board keys
PIN_MAPPING = {
    CONF_R1_PIN: "r1",
    CONF_G1_PIN: "g1",
    CONF_B1_PIN: "b1",
    CONF_R2_PIN: "r2",
    CONF_G2_PIN: "g2",
    CONF_B2_PIN: "b2",
    CONF_A_PIN: "a",
    CONF_B_PIN: "b",
    CONF_C_PIN: "c",
    CONF_D_PIN: "d",
    CONF_E_PIN: "e",
    CONF_LAT_PIN: "lat",
    CONF_OE_PIN: "oe",
    CONF_CLK_PIN: "clk",
}

# Required pins (E pin is optional)
REQUIRED_PINS = [key for key in PIN_MAPPING if key != CONF_E_PIN]

# Configuration
CONF_CLOCK_SPEED = "clock_speed"
CONF_LATCH_BLANKING = "latch_blanking"
CONF_CLOCK_PHASE = "clock_phase"
CONF_DOUBLE_BUFFER = "double_buffer"
CONF_MIN_REFRESH_RATE = "min_refresh_rate"

# Map to hub75 library enums (in global namespace)
Hub75ShiftDriver = cg.global_ns.enum("Hub75ShiftDriver", is_class=True)
SHIFT_DRIVERS = Hub75ShiftDriver.options(
    "GENERIC", "FM6126A", "ICN2038S", "FM6124", "MBI5124", "DP3246"
)

Hub75PanelLayout = cg.global_ns.enum("Hub75PanelLayout", is_class=True)
PANEL_LAYOUTS = Hub75PanelLayout.options(
    "HORIZONTAL",
    "TOP_LEFT_DOWN",
    "TOP_RIGHT_DOWN",
    "BOTTOM_LEFT_UP",
    "BOTTOM_RIGHT_UP",
    "TOP_LEFT_DOWN_ZIGZAG",
    "TOP_RIGHT_DOWN_ZIGZAG",
    "BOTTOM_LEFT_UP_ZIGZAG",
    "BOTTOM_RIGHT_UP_ZIGZAG",
)

Hub75ScanWiring = cg.global_ns.enum("Hub75ScanWiring", is_class=True)
SCAN_WIRINGS = Hub75ScanWiring.options(
    "STANDARD_TWO_SCAN",
    "SCAN_1_4_16PX_FULL",
    "SCAN_1_8_32PX_INTERLEAVED",
    "SCAN_1_8_32PX_FULL",
    "SCAN_1_10_40PX_FULL",
    "SCAN_1_16_64PX_BITSWAP",
)

# Remove before 2027.6.0
DEPRECATED_SCAN_WIRINGS = {
    "SCAN_1_4_16PX_HIGH": "SCAN_1_4_16PX_FULL",
    "SCAN_1_8_32PX_HIGH": "SCAN_1_8_32PX_INTERLEAVED",
    "SCAN_1_8_40PX_HIGH": "SCAN_1_10_40PX_FULL",
    "SCAN_1_8_64PX_HIGH": "SCAN_1_16_64PX_BITSWAP",
}

Hub75RowDecoder = cg.global_ns.enum("Hub75RowDecoder", is_class=True)
ROW_DECODERS = Hub75RowDecoder.options("BINARY", "SM5368", "DIRECT")

# Same bit values as Hub75ScanFlags in the library
SCAN_FLAGS = {
    "REVERSE_SLOTS": 1 << 0,
    "ALTERNATE_SEGMENTS": 1 << 1,
    "REVERSE_EVEN_SLOTS": 1 << 2,
    "REVERSE_ODD_SLOTS": 1 << 3,
    "REVERSE_BLOCK_ROWS": 1 << 4,
}

# Rows per row block and whether bits 3 and 4 of y are swapped, for each scan_wiring
# other than STANDARD_TWO_SCAN (from resolve_scan_geometry() in the library)
SCAN_WIRING_BLOCKS = {
    "SCAN_1_4_16PX_FULL": (4, False),
    "SCAN_1_8_32PX_INTERLEAVED": (8, False),
    "SCAN_1_8_32PX_FULL": (8, False),
    "SCAN_1_10_40PX_FULL": (10, False),
    "SCAN_1_16_64PX_BITSWAP": (8, True),
}

# Limits from the library's scan_patterns.h
MAX_BINARY_ROW_ADDRESSES = 32
MAX_FOLDED_ROWS = 256
MAX_DMA_WIDTH = 65535


@schema_extractor("enum")
def _validate_scan_wiring(value: Any) -> str:
    """Validate scan_wiring against the allowed names."""
    if value is SCHEMA_EXTRACT:
        return SCAN_WIRINGS

    value = cv.string(value).upper().replace(" ", "_")

    # Remove before 2027.6.0
    if (new_value := DEPRECATED_SCAN_WIRINGS.get(value)) is not None:
        _LOGGER.warning(
            "[hub75] '%s: %s' is deprecated, use '%s: %s'. Will be removed in 2027.6.0",
            CONF_SCAN_WIRING,
            value,
            CONF_SCAN_WIRING,
            new_value,
        )
        value = new_value

    # Validate against allowed values
    if value not in SCAN_WIRINGS:
        raise cv.Invalid(
            f"Unknown scan wiring '{value}'. "
            f"Valid options are: {', '.join(sorted(SCAN_WIRINGS.keys()))}"
        )

    # Return as EnumValue like cv.enum does
    result = add_class_to_obj(value, EnumValue)
    result.enum_value = SCAN_WIRINGS[value]
    return result


Hub75ClockSpeed = cg.global_ns.enum("Hub75ClockSpeed", is_class=True)
CLOCK_SPEEDS = {
    "8MHZ": Hub75ClockSpeed.HZ_8M,
    "10MHZ": Hub75ClockSpeed.HZ_10M,
    "16MHZ": Hub75ClockSpeed.HZ_16M,
    "20MHZ": Hub75ClockSpeed.HZ_20M,
}

Hub75Rotation = cg.global_ns.enum("Hub75Rotation", is_class=True)
ROTATIONS = {
    0: Hub75Rotation.ROTATE_0,
    90: Hub75Rotation.ROTATE_90,
    180: Hub75Rotation.ROTATE_180,
    270: Hub75Rotation.ROTATE_270,
}

HUB75Display = hub75_ns.class_("HUB75Display", cg.PollingComponent, display.Display)
Hub75Config = cg.global_ns.struct("Hub75Config")
Hub75Pins = cg.global_ns.struct("Hub75Pins")


def _merge_board_pins(config: ConfigType) -> ConfigType:
    """Merge board preset pins with explicit pin overrides."""
    board_name = config.get(CONF_BOARD)

    if board_name is None:
        # No board specified - validate that all required pins are present
        errs = [
            cv.Invalid(
                f"Required pin '{pin_name}' is missing. "
                f"Either specify a board preset or provide all pin mappings manually.",
                path=[pin_name],
            )
            for pin_name in REQUIRED_PINS
            if pin_name not in config
        ]

        if errs:
            raise cv.MultipleInvalid(errs)

        # E_PIN is optional
        return config

    # Get board configuration
    if board_name not in BOARDS:
        raise cv.Invalid(
            f"Unknown board '{board_name}'. Available boards: {', '.join(sorted(BOARDS.keys()))}"
        )

    board = BOARDS[board_name]

    # Merge board pins with explicit overrides
    # Explicit pins in config take precedence over board defaults
    for conf_key, board_key in PIN_MAPPING.items():
        if conf_key in config or (board_pin := board.get_pin(board_key)) is None:
            continue
        # Create pin config
        pin_config = {"number": board_pin}
        if conf_key in board.ignore_strapping_pins:
            pin_config["ignore_strapping_warning"] = True

        # Validate through pin schema to add required fields (id, etc.)
        config[conf_key] = pins.gpio_output_pin_schema(pin_config)

    return config


def _validate_config(config: ConfigType) -> ConfigType:
    """Validate driver and layout requirements."""
    errs: list[cv.Invalid] = []

    # MBI5124 requires inverted clock phase
    driver = config.get(CONF_SHIFT_DRIVER, "GENERIC")
    if driver == "MBI5124" and not config.get(CONF_CLOCK_PHASE, False):
        errs.append(
            cv.Invalid(
                "MBI5124 shift driver requires 'clock_phase: true' to be set",
                path=[CONF_CLOCK_PHASE],
            )
        )

    # Prevent conflicting min_refresh_rate + update_interval configuration
    # min_refresh_rate is auto-calculated from update_interval unless using LVGL mode
    update_interval = config.get(CONF_UPDATE_INTERVAL)
    if CONF_MIN_REFRESH_RATE in config and update_interval is not None:
        # Handle both integer (NEVER) and time object cases
        interval_ms = (
            update_interval
            if isinstance(update_interval, int)
            else update_interval.total_milliseconds
        )
        if interval_ms != NEVER:
            errs.append(
                cv.Invalid(
                    "Cannot set both 'min_refresh_rate' and 'update_interval' (except 'never'). "
                    "Refresh rate is auto-calculated from update_interval. "
                    "Remove 'min_refresh_rate' or use 'update_interval: never' for LVGL mode.",
                    path=[CONF_MIN_REFRESH_RATE],
                )
            )

    if (err := _validate_scan_mapping(config)) is not None:
        errs.append(err)

    # Validate layout configuration (validate effective config including C++ defaults)
    layout = config.get(CONF_LAYOUT, "HORIZONTAL")
    layout_rows = config.get(CONF_LAYOUT_ROWS, 1)
    layout_cols = config.get(CONF_LAYOUT_COLS, 1)
    is_zigzag = "ZIGZAG" in layout

    # Single panel (1x1) should use HORIZONTAL
    if layout_rows == 1 and layout_cols == 1 and layout != "HORIZONTAL":
        errs.append(
            cv.Invalid(
                f"Single panel (layout_rows=1, layout_cols=1) should use 'layout: HORIZONTAL' (got {layout})",
                path=[CONF_LAYOUT],
            )
        )

    # HORIZONTAL layout requires single row
    if layout == "HORIZONTAL" and layout_rows != 1:
        errs.append(
            cv.Invalid(
                f"HORIZONTAL layout requires 'layout_rows: 1' (got {layout_rows}). "
                "For multi-row grids, use TOP_LEFT_DOWN or other grid layouts.",
                path=[CONF_LAYOUT_ROWS],
            )
        )

    # Grid layouts (non-HORIZONTAL) require more than one panel
    if layout != "HORIZONTAL" and layout_rows == 1 and layout_cols == 1:
        errs.append(
            cv.Invalid(
                f"Grid layout '{layout}' requires multiple panels (layout_rows > 1 or layout_cols > 1)",
                path=[CONF_LAYOUT],
            )
        )

    # Serpentine layouts (non-ZIGZAG) require multiple rows
    # Serpentine physically rotates alternate rows upside down (Y-coordinate inversion)
    # Single-row chains should use HORIZONTAL or ZIGZAG variants
    if not is_zigzag and layout != "HORIZONTAL" and layout_rows == 1:
        errs.append(
            cv.Invalid(
                f"Serpentine layout '{layout}' requires layout_rows > 1 "
                f"(got layout_rows={layout_rows}). "
                "Serpentine wiring physically rotates alternate rows upside down. "
                "For single-row chains, use 'layout: HORIZONTAL' or add '_ZIGZAG' suffix.",
                path=[CONF_LAYOUT_ROWS],
            )
        )

    # ZIGZAG layouts require actual grid (both rows AND cols > 1)
    if is_zigzag and (layout_rows == 1 or layout_cols == 1):
        errs.append(
            cv.Invalid(
                f"ZIGZAG layout '{layout}' requires both layout_rows > 1 AND layout_cols > 1 "
                f"(got rows={layout_rows}, cols={layout_cols}). "
                "For single row/column chains, use non-zigzag layouts or HORIZONTAL.",
                path=[CONF_LAYOUT],
            )
        )

    if errs:
        raise cv.MultipleInvalid(errs)

    return config


def _scan_flags_mask(config: ConfigType) -> int:
    """Combine the scan_flags list into the library's Hub75ScanFlags bits."""
    mask = 0
    for flag in config.get(CONF_SCAN_FLAGS, []):
        mask |= flag.enum_value
    return mask


def _scan_rows_fit(
    height: int, rows_per_address: int, block_rows: int, bitswap: bool, flags: int
) -> bool:
    """Check that every panel row gets its own place in the shift chain.

    Follows ScanPatternRemap::remap() in the library for column 0, which is all
    the library itself checks.
    """
    slots = rows_per_address // 2
    slot_shift = 1 if slots == 2 else 2
    chain_rows = 2 * height // rows_per_address
    seen: set[tuple[int, int]] = set()
    for y in range(height):
        if bitswap and (y & 8) != ((y & 16) >> 1):
            y = ((y & 0b11000) ^ 0b11000) + (y & 0b11100111)
        block_index, row = divmod(y, block_rows)
        block = block_index & (slots - 1)
        slot = slots - 1 - block if slots == 2 else block
        if flags & SCAN_FLAGS["REVERSE_SLOTS"]:
            slot = slots - 1 - slot
        if flags & SCAN_FLAGS["REVERSE_BLOCK_ROWS"]:
            row = block_rows - 1 - row
        chain_row = (block_index >> slot_shift) * block_rows + row
        if chain_row >= chain_rows or (slot, chain_row) in seen:
            return False
        seen.add((slot, chain_row))
    return True


def _validate_scan_mapping(config: ConfigType) -> cv.Invalid | None:
    """Check the panel size and scan options the way the library does in begin().

    Follows resolve_scan_geometry() and the row decoder checks in
    Hub75Driver::begin(), which reject a bad configuration only at boot, with a
    log line and a dark panel. Returns the first problem, as the library does.
    """
    width = config[CONF_PANEL_WIDTH]
    height = config[CONF_PANEL_HEIGHT]
    wiring = config.get(CONF_SCAN_WIRING, "STANDARD_TWO_SCAN")
    is_standard = wiring == "STANDARD_TWO_SCAN"
    flags = _scan_flags_mask(config)

    # Every scan_wiring other than STANDARD_TWO_SCAN is a four-scan panel
    rows_per_address = 2 if is_standard else 4
    scan_rate = height // rows_per_address
    if (explicit_rate := config.get(CONF_SCAN_RATE)) is not None:
        if not is_standard and explicit_rate != scan_rate:
            return cv.Invalid(
                f"scan_rate {explicit_rate} does not match scan_wiring '{wiring}' "
                f"(expected {scan_rate}). Remove scan_rate, or use "
                "'scan_wiring: STANDARD_TWO_SCAN' with a custom scan_rate.",
                path=[CONF_SCAN_RATE],
            )
        scan_rate = explicit_rate
        rows_per_address, remainder = divmod(height, scan_rate)
        if remainder != 0 or rows_per_address not in (2, 4, 8):
            return cv.Invalid(
                f"scan_rate must be panel_height / 2, / 4 or / 8 "
                f"({height // 2}, {height // 4} or {height // 8} for panel_height {height})",
                path=[CONF_SCAN_RATE],
            )

    if scan_rate == 0:
        return cv.Invalid(
            f"panel_height {height} is too small for scan_wiring '{wiring}'",
            path=[CONF_PANEL_HEIGHT],
        )
    row_decoder = config.get(CONF_ROW_DECODER, "BINARY")
    if row_decoder == "BINARY" and scan_rate > MAX_BINARY_ROW_ADDRESSES:
        return cv.Invalid(
            f"The panel has {scan_rate} row addresses, but 'row_decoder: BINARY' "
            f"supports at most {MAX_BINARY_ROW_ADDRESSES} (address pins A to E)",
            path=[CONF_SCAN_RATE if CONF_SCAN_RATE in config else CONF_PANEL_HEIGHT],
        )
    chain_width = (
        width
        * (rows_per_address // 2)
        * config.get(CONF_LAYOUT_ROWS, 1)
        * config.get(CONF_LAYOUT_COLS, 1)
    )
    if chain_width > MAX_DMA_WIDTH:
        return cv.Invalid(
            f"The panels are too wide to drive: panel_width x layout_rows x layout_cols "
            f"(x2 for four-scan, x4 for eight-scan panels) is {chain_width}, "
            f"at most {MAX_DMA_WIDTH} is supported",
            path=[CONF_PANEL_WIDTH],
        )

    if rows_per_address == 2:
        if height % 2 != 0:
            return cv.Invalid(
                f"panel_height must be even (got {height})", path=[CONF_PANEL_HEIGHT]
            )
        for key in (CONF_SCAN_SEGMENT_WIDTH, CONF_SCAN_FLAGS):
            if key in config:
                return cv.Invalid(
                    f"{key} needs a four- or eight-scan panel: set scan_rate to "
                    "panel_height / 4 or / 8, or set a scan_wiring other than "
                    "STANDARD_TWO_SCAN",
                    path=[key],
                )
    else:
        block_rows, bitswap = SCAN_WIRING_BLOCKS.get(wiring, (scan_rate, False))
        if (segment_width := config.get(CONF_SCAN_SEGMENT_WIDTH)) is not None:
            if width % segment_width != 0:
                return cv.Invalid(
                    f"scan_segment_width {segment_width} must divide panel_width {width}",
                    path=[CONF_SCAN_SEGMENT_WIDTH],
                )
        else:
            segment_width = width
            if wiring == "SCAN_1_8_32PX_INTERLEAVED" and height >= 8:
                segment_width = width // (height // 8) or width
            if width % segment_width != 0:
                return cv.Invalid(
                    f"scan_wiring '{wiring}' uses {segment_width} pixel segments, which "
                    f"do not divide panel_width {width}. Set scan_segment_width.",
                    path=[CONF_SCAN_WIRING],
                )
        if flags & SCAN_FLAGS["ALTERNATE_SEGMENTS"] and segment_width == width:
            return cv.Invalid(
                "alternate_segments needs a scan_segment_width smaller than panel_width",
                path=[CONF_SCAN_FLAGS],
            )
        if height > MAX_FOLDED_ROWS:
            return cv.Invalid(
                f"Four- and eight-scan panels can have at most {MAX_FOLDED_ROWS} rows "
                f"(panel_height is {height})",
                path=[CONF_PANEL_HEIGHT],
            )
        if not _scan_rows_fit(height, rows_per_address, block_rows, bitswap, flags):
            return cv.Invalid(
                f"scan_wiring '{wiring}' does not fit panel_height {height}",
                path=[CONF_SCAN_WIRING],
            )

    # Row decoder checks from Hub75Driver::begin(). Pins A to D are always required.
    if row_decoder == "BINARY" and scan_rate > 16 and CONF_E_PIN not in config:
        return cv.Invalid(
            f"The panel has {scan_rate} row addresses, which need e_pin",
            path=[CONF_E_PIN],
        )
    if row_decoder == "DIRECT" and scan_rate not in (2, 4):
        return cv.Invalid(
            f"'row_decoder: DIRECT' needs a panel with 2 or 4 row addresses "
            f"(this one has {scan_rate})",
            path=[CONF_ROW_DECODER],
        )
    return None


def _final_validate(config: ConfigType) -> None:
    """Validate requirements when using HUB75 display."""
    # Local imports to avoid circular dependencies
    from esphome.components.esp32 import get_esp32_variant
    from esphome.components.esp32.const import VARIANT_ESP32P4
    from esphome.components.lvgl import DOMAIN as LVGL_DOMAIN
    from esphome.components.psram import DOMAIN as PSRAM_DOMAIN

    full_config = fv.full_config.get()
    errs: list[cv.Invalid] = []

    # ESP32-P4 requires PSRAM
    variant = get_esp32_variant()
    if variant == VARIANT_ESP32P4 and PSRAM_DOMAIN not in full_config:
        errs.append(
            cv.Invalid(
                "HUB75 display on ESP32-P4 requires PSRAM. Add 'psram:' to your configuration.",
                path=[CONF_ID],
            )
        )

    # LVGL-specific validation
    if LVGL_DOMAIN in full_config:
        # Check update_interval (converted from "never" to NEVER constant)
        update_interval = config.get(CONF_UPDATE_INTERVAL)
        if update_interval is not None:
            # Handle both integer (NEVER) and time object cases
            interval_ms = (
                update_interval
                if isinstance(update_interval, int)
                else update_interval.total_milliseconds
            )
            if interval_ms != NEVER:
                errs.append(
                    cv.Invalid(
                        "HUB75 display with LVGL must have 'update_interval: never'. "
                        "LVGL manages its own refresh timing.",
                        path=[CONF_UPDATE_INTERVAL],
                    )
                )

        # Check auto_clear_enabled
        auto_clear = config[CONF_AUTO_CLEAR_ENABLED]
        if auto_clear is not False:
            errs.append(
                cv.Invalid(
                    f"HUB75 display with LVGL must have 'auto_clear_enabled: false' (got '{auto_clear}'). "
                    "LVGL manages screen clearing.",
                    path=[CONF_AUTO_CLEAR_ENABLED],
                )
            )

        # Check double_buffer (C++ default: false)
        double_buffer = config.get(CONF_DOUBLE_BUFFER, False)
        if double_buffer is not False:
            errs.append(
                cv.Invalid(
                    f"HUB75 display with LVGL must have 'double_buffer: false' (got '{double_buffer}'). "
                    "LVGL uses its own buffering strategy.",
                    path=[CONF_DOUBLE_BUFFER],
                )
            )

    if errs:
        raise cv.MultipleInvalid(errs)


FINAL_VALIDATE_SCHEMA = cv.Schema(_final_validate)


CONFIG_SCHEMA = cv.All(
    display.FULL_DISPLAY_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(HUB75Display),
            # Override rotation - store Hub75Rotation directly (driver handles rotation)
            cv.Optional(CONF_ROTATION): cv.enum(ROTATIONS, int=True),
            # Board preset (optional - provides default pin mappings)
            cv.Optional(CONF_BOARD): cv.one_of(*BOARDS.keys(), lower=True),
            # Panel dimensions
            cv.Required(CONF_PANEL_WIDTH): cv.positive_int,
            cv.Required(CONF_PANEL_HEIGHT): cv.positive_int,
            # Multi-panel layout
            cv.Optional(CONF_LAYOUT_ROWS): cv.positive_int,
            cv.Optional(CONF_LAYOUT_COLS): cv.positive_int,
            cv.Optional(CONF_LAYOUT): cv.enum(PANEL_LAYOUTS, upper=True, space="_"),
            # Panel hardware configuration
            cv.Optional(CONF_SCAN_WIRING): _validate_scan_wiring,
            cv.Optional(CONF_SCAN_RATE): cv.int_range(min=1, max=255),
            cv.Optional(CONF_SCAN_SEGMENT_WIDTH): cv.int_range(min=1, max=65535),
            cv.Optional(CONF_SCAN_FLAGS): cv.ensure_list(
                cv.enum(SCAN_FLAGS, upper=True, space="_")
            ),
            cv.Optional(CONF_SHIFT_DRIVER): cv.enum(SHIFT_DRIVERS, upper=True),
            cv.Optional(CONF_ROW_DECODER): cv.enum(ROW_DECODERS, upper=True),
            # Display configuration
            cv.Optional(CONF_DOUBLE_BUFFER): cv.boolean,
            cv.Optional(CONF_BRIGHTNESS): cv.uint8_t,
            cv.Optional(CONF_BIT_DEPTH): cv.int_range(min=4, max=12),
            cv.Optional(CONF_GAMMA_CORRECT): cv.enum(
                {"LINEAR": 0, "CIE1931": 1, "GAMMA_2_2": 2}, upper=True
            ),
            cv.Optional(CONF_MIN_REFRESH_RATE): cv.int_range(min=40, max=200),
            # RGB data pins
            cv.Optional(CONF_R1_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_G1_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_B1_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_R2_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_G2_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_B2_PIN): pins.gpio_output_pin_schema,
            # Address pins
            cv.Optional(CONF_A_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_B_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_C_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_D_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_E_PIN): pins.gpio_output_pin_schema,
            # Control pins
            cv.Optional(CONF_LAT_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_OE_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_CLK_PIN): pins.gpio_output_pin_schema,
            # Timing configuration
            cv.Optional(CONF_CLOCK_SPEED): cv.enum(CLOCK_SPEEDS, upper=True),
            cv.Optional(CONF_LATCH_BLANKING): cv.positive_int,
            cv.Optional(CONF_CLOCK_PHASE): cv.boolean,
        }
    ),
    _merge_board_pins,
    _validate_config,
)


DEFAULT_REFRESH_RATE = 60  # Hz


def _calculate_min_refresh_rate(config: ConfigType) -> int:
    """Calculate minimum refresh rate for the display.

    Priority:
    1. Explicit min_refresh_rate setting (user override)
    2. Derived from update_interval (ms to Hz conversion)
    3. Default 60 Hz (for LVGL or unspecified interval)
    """
    if CONF_MIN_REFRESH_RATE in config:
        return config[CONF_MIN_REFRESH_RATE]

    update_interval = config.get(CONF_UPDATE_INTERVAL)
    if update_interval is None:
        return DEFAULT_REFRESH_RATE

    # update_interval can be TimePeriod object or NEVER constant (int)
    interval_ms = (
        update_interval
        if isinstance(update_interval, int)
        else update_interval.total_milliseconds
    )

    # "never" or zero means external refresh (e.g., LVGL)
    if interval_ms in (NEVER, 0):
        return DEFAULT_REFRESH_RATE

    # Convert ms interval to Hz, clamped to valid range [40, 200]
    return max(40, min(200, int(round(1000 / interval_ms))))


def _build_pins_struct(
    pin_expressions: dict[str, Any], e_pin_num: int | cg.RawExpression
) -> cg.StructInitializer:
    """Build Hub75Pins struct from pin expressions."""

    def pin_cast(pin: Any) -> cg.RawExpression:
        return cg.RawExpression(f"static_cast<int8_t>({pin.get_pin()})")

    return cg.StructInitializer(
        Hub75Pins,
        ("r1", pin_cast(pin_expressions["r1"])),
        ("g1", pin_cast(pin_expressions["g1"])),
        ("b1", pin_cast(pin_expressions["b1"])),
        ("r2", pin_cast(pin_expressions["r2"])),
        ("g2", pin_cast(pin_expressions["g2"])),
        ("b2", pin_cast(pin_expressions["b2"])),
        ("a", pin_cast(pin_expressions["a"])),
        ("b", pin_cast(pin_expressions["b"])),
        ("c", pin_cast(pin_expressions["c"])),
        ("d", pin_cast(pin_expressions["d"])),
        ("e", e_pin_num),
        ("lat", pin_cast(pin_expressions["lat"])),
        ("oe", pin_cast(pin_expressions["oe"])),
        ("clk", pin_cast(pin_expressions["clk"])),
    )


def _append_config_fields(
    config: ConfigType,
    field_mapping: list[tuple[str, str]],
    config_fields: list[tuple[str, Any]],
) -> None:
    """Append config fields from mapping if present in config."""
    for conf_key, struct_field in field_mapping:
        if conf_key in config:
            config_fields.append((struct_field, config[conf_key]))


def _build_config_struct(
    config: ConfigType, pins_struct: cg.StructInitializer, min_refresh: int
) -> cg.StructInitializer:
    """Build Hub75Config struct from config.

    Fields must be added in declaration order (see hub75_types.h) to satisfy
    C++ designated initializer requirements. The order is:
      1. fields_before_pins (panel_width through layout)
      2. rotation
      3. pins
      4. output_clock_speed
      5. min_refresh_rate
      6. fields_after_min_refresh (latch_blanking through scan_flags)
    """
    fields_before_pins = [
        (CONF_PANEL_WIDTH, "panel_width"),
        (CONF_PANEL_HEIGHT, "panel_height"),
        # scan_pattern - auto-calculated, not set
        (CONF_SCAN_WIRING, "scan_wiring"),
        (CONF_SHIFT_DRIVER, "shift_driver"),
        (CONF_LAYOUT_ROWS, "layout_rows"),
        (CONF_LAYOUT_COLS, "layout_cols"),
        (CONF_LAYOUT, "layout"),
    ]
    fields_after_min_refresh = [
        (CONF_LATCH_BLANKING, "latch_blanking"),
        (CONF_DOUBLE_BUFFER, "double_buffer"),
        (CONF_CLOCK_PHASE, "clk_phase_inverted"),
        (CONF_BRIGHTNESS, "brightness"),
        (CONF_ROW_DECODER, "row_decoder"),
        (CONF_SCAN_RATE, "scan_rate"),
        (CONF_SCAN_SEGMENT_WIDTH, "scan_segment_width"),
    ]

    config_fields: list[tuple[str, Any]] = []

    _append_config_fields(config, fields_before_pins, config_fields)

    # Rotation - config already contains Hub75Rotation enum from cv.enum
    if CONF_ROTATION in config:
        config_fields.append(("rotation", config[CONF_ROTATION]))

    config_fields.append(("pins", pins_struct))

    if CONF_CLOCK_SPEED in config:
        config_fields.append(("output_clock_speed", config[CONF_CLOCK_SPEED]))

    config_fields.append(("min_refresh_rate", min_refresh))

    _append_config_fields(config, fields_after_min_refresh, config_fields)

    if mask := _scan_flags_mask(config):
        config_fields.append(
            ("scan_flags", cg.RawExpression(f"static_cast<Hub75ScanFlags>({mask})"))
        )

    return cg.StructInitializer(Hub75Config, *config_fields)


async def to_code(config: ConfigType) -> None:
    add_idf_component(
        name="esphome/esp-hub75",
        ref="0.4.0",
    )

    # Set compile-time configuration via build flags (so external library sees them)
    if CONF_BIT_DEPTH in config:
        cg.add_build_flag(f"-DHUB75_BIT_DEPTH={config[CONF_BIT_DEPTH]}")

    if CONF_GAMMA_CORRECT in config:
        cg.add_build_flag(f"-DHUB75_GAMMA_MODE={config[CONF_GAMMA_CORRECT].enum_value}")

    # Await all pin expressions
    pin_expressions = {
        "r1": await cg.gpio_pin_expression(config[CONF_R1_PIN]),
        "g1": await cg.gpio_pin_expression(config[CONF_G1_PIN]),
        "b1": await cg.gpio_pin_expression(config[CONF_B1_PIN]),
        "r2": await cg.gpio_pin_expression(config[CONF_R2_PIN]),
        "g2": await cg.gpio_pin_expression(config[CONF_G2_PIN]),
        "b2": await cg.gpio_pin_expression(config[CONF_B2_PIN]),
        "a": await cg.gpio_pin_expression(config[CONF_A_PIN]),
        "b": await cg.gpio_pin_expression(config[CONF_B_PIN]),
        "c": await cg.gpio_pin_expression(config[CONF_C_PIN]),
        "d": await cg.gpio_pin_expression(config[CONF_D_PIN]),
        "lat": await cg.gpio_pin_expression(config[CONF_LAT_PIN]),
        "oe": await cg.gpio_pin_expression(config[CONF_OE_PIN]),
        "clk": await cg.gpio_pin_expression(config[CONF_CLK_PIN]),
    }

    # E pin is optional
    if CONF_E_PIN in config:
        e_pin = await cg.gpio_pin_expression(config[CONF_E_PIN])
        e_pin_num = cg.RawExpression(f"static_cast<int8_t>({e_pin.get_pin()})")
    else:
        e_pin_num = -1

    # Build structs
    min_refresh = _calculate_min_refresh_rate(config)
    pins_struct = _build_pins_struct(pin_expressions, e_pin_num)
    hub75_config = _build_config_struct(config, pins_struct, min_refresh)

    # The driver rotates the panel itself (config_.rotation above), so the base class must not rotate too
    config.pop(CONF_ROTATION, None)

    # Create display and register
    var = cg.new_Pvariable(config[CONF_ID], hub75_config)
    await display.register_display(var, config)

    await display.add_writer(var, config)


automation.register_apply_action(
    "hub75.set_brightness",
    cv.maybe_simple_value(
        {
            cv.GenerateID(): cv.use_id(HUB75Display),
            cv.Required(CONF_BRIGHTNESS): cv.templatable(cv.uint8_t),
        },
        key=CONF_BRIGHTNESS,
    ),
    automation.ApplyField(CONF_BRIGHTNESS, "set_brightness", cg.uint8),
)
