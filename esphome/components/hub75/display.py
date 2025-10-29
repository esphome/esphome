from esphome import pins
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
    CONF_ID,
    CONF_LAMBDA,
    CONF_OE_PIN,
    CONF_UPDATE_INTERVAL,
)
import esphome.final_validate as fv

from . import boards, hub75_ns

DEPENDENCIES = ["esp32"]
CODEOWNERS = ["@stuartparmenter"]

# Load all board presets
BOARDS = boards.BoardConfig.get_boards()

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
CONF_SHIFT_DRIVER = "shift_driver"

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

# Configuration
CONF_CLOCK_SPEED = "clock_speed"
CONF_LATCH_BLANKING = "latch_blanking"
CONF_CLOCK_PHASE = "clock_phase"
CONF_DOUBLE_BUFFER = "double_buffer"
CONF_MIN_REFRESH_RATE = "min_refresh_rate"

# Map to hub75 library enums (in global namespace)
ShiftDriver = cg.global_ns.enum("ShiftDriver", is_class=True)
SHIFT_DRIVERS = {
    "GENERIC": ShiftDriver.GENERIC,
    "FM6126A": ShiftDriver.FM6126A,
    "ICN2038S": ShiftDriver.ICN2038S,
    "FM6124": ShiftDriver.FM6124,
    "MBI5124": ShiftDriver.MBI5124,
    "DP3246": ShiftDriver.DP3246,
}

PanelLayout = cg.global_ns.enum("PanelLayout", is_class=True)
PANEL_LAYOUTS = {
    "HORIZONTAL": PanelLayout.HORIZONTAL,
    "TOP_LEFT_DOWN": PanelLayout.TOP_LEFT_DOWN,
    "TOP_RIGHT_DOWN": PanelLayout.TOP_RIGHT_DOWN,
    "BOTTOM_LEFT_UP": PanelLayout.BOTTOM_LEFT_UP,
    "BOTTOM_RIGHT_UP": PanelLayout.BOTTOM_RIGHT_UP,
    "TOP_LEFT_DOWN_ZIGZAG": PanelLayout.TOP_LEFT_DOWN_ZIGZAG,
    "TOP_RIGHT_DOWN_ZIGZAG": PanelLayout.TOP_RIGHT_DOWN_ZIGZAG,
    "BOTTOM_LEFT_UP_ZIGZAG": PanelLayout.BOTTOM_LEFT_UP_ZIGZAG,
    "BOTTOM_RIGHT_UP_ZIGZAG": PanelLayout.BOTTOM_RIGHT_UP_ZIGZAG,
}

ScanPattern = cg.global_ns.enum("ScanPattern", is_class=True)
SCAN_PATTERNS = {
    "STANDARD_TWO_SCAN": ScanPattern.STANDARD_TWO_SCAN,
    "FOUR_SCAN_16PX_HIGH": ScanPattern.FOUR_SCAN_16PX_HIGH,
    "FOUR_SCAN_32PX_HIGH": ScanPattern.FOUR_SCAN_32PX_HIGH,
    "FOUR_SCAN_64PX_HIGH": ScanPattern.FOUR_SCAN_64PX_HIGH,
}

Hub75ClockSpeed = cg.global_ns.enum("Hub75ClockSpeed", is_class=True)
CLOCK_SPEEDS = {
    "8MHZ": Hub75ClockSpeed.HZ_8M,
    "10MHZ": Hub75ClockSpeed.HZ_10M,
    "16MHZ": Hub75ClockSpeed.HZ_16M,
    "20MHZ": Hub75ClockSpeed.HZ_20M,
}

HUB75Display = hub75_ns.class_("HUB75Display", cg.PollingComponent, display.Display)
Hub75Config = cg.global_ns.struct("Hub75Config")
Hub75Pins = cg.global_ns.struct("Hub75Pins")


def _merge_board_pins(config):
    """Merge board preset pins with explicit pin overrides."""
    board_name = config.get(CONF_BOARD)

    if board_name is None:
        # No board specified - validate that all required pins are present
        required_pins = [
            CONF_R1_PIN,
            CONF_G1_PIN,
            CONF_B1_PIN,
            CONF_R2_PIN,
            CONF_G2_PIN,
            CONF_B2_PIN,
            CONF_A_PIN,
            CONF_B_PIN,
            CONF_C_PIN,
            CONF_D_PIN,
            CONF_LAT_PIN,
            CONF_OE_PIN,
            CONF_CLK_PIN,
        ]
        errs = [
            cv.Invalid(
                f"Required pin '{pin_name}' is missing. "
                f"Either specify a board preset or provide all pin mappings manually.",
                path=[pin_name],
            )
            for pin_name in required_pins
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
    pin_mapping = {
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

    for conf_key, board_key in pin_mapping.items():
        if conf_key not in config:  # Only use board default if not explicitly set
            board_pin = board.get_pin(board_key)
            if board_pin is not None:
                # Create pin config
                pin_config = {"number": board_pin}
                if conf_key == CONF_A_PIN and board.a_pin_ignore_strapping:
                    pin_config["ignore_strapping_warning"] = True

                # Validate through pin schema to add required fields (id, etc.)
                config[conf_key] = pins.gpio_output_pin_schema(pin_config)

    return config


def _validate_config(config):
    """Validate driver and layout requirements"""
    errs = []

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


def _final_validate(config):
    """Validate LVGL-specific requirements when using HUB75 display"""
    # Local import to avoid circular dependencies
    try:
        from esphome.components.lvgl import DOMAIN as LVGL_DOMAIN
    except ImportError:
        # LVGL component not available in this ESPHome installation
        return config

    full_config = fv.full_config.get()

    # Check if LVGL component is loaded
    if LVGL_DOMAIN not in full_config:
        return config

    errs = []

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

    return config


FINAL_VALIDATE_SCHEMA = cv.Schema(_final_validate)


CONFIG_SCHEMA = cv.All(
    display.FULL_DISPLAY_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(HUB75Display),
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
            cv.Optional(CONF_SCAN_WIRING): cv.enum(
                SCAN_PATTERNS, upper=True, space="_"
            ),
            cv.Optional(CONF_SHIFT_DRIVER): cv.enum(SHIFT_DRIVERS, upper=True),
            # Display configuration
            cv.Optional(CONF_DOUBLE_BUFFER): cv.boolean,
            cv.Optional(CONF_BRIGHTNESS): cv.int_range(min=0, max=255),
            cv.Optional(CONF_BIT_DEPTH): cv.int_range(min=6, max=12),
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


async def to_code(config):
    add_idf_component(
        name="hub75",
        repo="https://github.com/stuartparmenter/hub75-esp32.git",
        ref="main",
        path="components/hub75",
    )

    # ========================================
    # Step 1: Await all pin expressions
    # ========================================

    # RGB pins
    r1_pin = await cg.gpio_pin_expression(config[CONF_R1_PIN])
    g1_pin = await cg.gpio_pin_expression(config[CONF_G1_PIN])
    b1_pin = await cg.gpio_pin_expression(config[CONF_B1_PIN])
    r2_pin = await cg.gpio_pin_expression(config[CONF_R2_PIN])
    g2_pin = await cg.gpio_pin_expression(config[CONF_G2_PIN])
    b2_pin = await cg.gpio_pin_expression(config[CONF_B2_PIN])

    # Address pins
    a_pin = await cg.gpio_pin_expression(config[CONF_A_PIN])
    b_pin = await cg.gpio_pin_expression(config[CONF_B_PIN])
    c_pin = await cg.gpio_pin_expression(config[CONF_C_PIN])
    d_pin = await cg.gpio_pin_expression(config[CONF_D_PIN])

    if CONF_E_PIN in config:
        e_pin = await cg.gpio_pin_expression(config[CONF_E_PIN])
        e_pin_num = cg.RawExpression(f"static_cast<int8_t>({e_pin.get_pin()})")
    else:
        e_pin_num = -1

    # Control pins
    lat_pin = await cg.gpio_pin_expression(config[CONF_LAT_PIN])
    oe_pin = await cg.gpio_pin_expression(config[CONF_OE_PIN])
    clk_pin = await cg.gpio_pin_expression(config[CONF_CLK_PIN])

    # ========================================
    # Step 2: Calculate min_refresh_rate
    # ========================================

    if CONF_MIN_REFRESH_RATE in config:
        # Explicitly set (only valid with LVGL mode due to schema validation)
        min_refresh = config[CONF_MIN_REFRESH_RATE]
    else:
        # Auto-calculate based on update_interval
        update_interval = config.get(CONF_UPDATE_INTERVAL)
        if update_interval is None:
            # Not set - default to 60 Hz
            min_refresh = 60
        else:
            # Handle both integer (NEVER) and time object cases
            update_interval_ms = (
                update_interval
                if isinstance(update_interval, int)
                else update_interval.total_milliseconds
            )
            if update_interval_ms in (NEVER, 0):
                # LVGL mode or never - default to 60 Hz
                min_refresh = 60
            else:
                # ESPHome-driven display - match refresh to update rate (ms → Hz)
                min_refresh = int(round(1000 / update_interval_ms))
                # Clamp to schema range (should match cv.int_range in CONFIG_SCHEMA)
                min_refresh = max(40, min(200, min_refresh))

    # ========================================
    # Step 3: Build Hub75Pins struct
    # ========================================

    # Cast pin numbers from uint8_t to int8_t (hub75 library uses signed to support -1 for unused pins)
    pins_struct = cg.StructInitializer(
        Hub75Pins,
        ("r1", cg.RawExpression(f"static_cast<int8_t>({r1_pin.get_pin()})")),
        ("g1", cg.RawExpression(f"static_cast<int8_t>({g1_pin.get_pin()})")),
        ("b1", cg.RawExpression(f"static_cast<int8_t>({b1_pin.get_pin()})")),
        ("r2", cg.RawExpression(f"static_cast<int8_t>({r2_pin.get_pin()})")),
        ("g2", cg.RawExpression(f"static_cast<int8_t>({g2_pin.get_pin()})")),
        ("b2", cg.RawExpression(f"static_cast<int8_t>({b2_pin.get_pin()})")),
        ("a", cg.RawExpression(f"static_cast<int8_t>({a_pin.get_pin()})")),
        ("b", cg.RawExpression(f"static_cast<int8_t>({b_pin.get_pin()})")),
        ("c", cg.RawExpression(f"static_cast<int8_t>({c_pin.get_pin()})")),
        ("d", cg.RawExpression(f"static_cast<int8_t>({d_pin.get_pin()})")),
        ("e", e_pin_num),  # Already -1 or a pin number
        ("lat", cg.RawExpression(f"static_cast<int8_t>({lat_pin.get_pin()})")),
        ("oe", cg.RawExpression(f"static_cast<int8_t>({oe_pin.get_pin()})")),
        ("clk", cg.RawExpression(f"static_cast<int8_t>({clk_pin.get_pin()})")),
    )

    # ========================================
    # Step 4: Build Hub75Config struct
    # ========================================

    # Build Hub75Config struct - field order MUST match C++ struct declaration
    # Optional fields not specified by user will use C++ defaults
    config_fields = [
        ("panel_width", config[CONF_PANEL_WIDTH]),
        ("panel_height", config[CONF_PANEL_HEIGHT]),
        # scan_pattern omitted - uses C++ default
    ]

    if CONF_SCAN_WIRING in config:
        config_fields.append(("scan_wiring", config[CONF_SCAN_WIRING]))
    if CONF_SHIFT_DRIVER in config:
        config_fields.append(("shift_driver", config[CONF_SHIFT_DRIVER]))

    if CONF_LAYOUT_ROWS in config:
        config_fields.append(("layout_rows", config[CONF_LAYOUT_ROWS]))
    if CONF_LAYOUT_COLS in config:
        config_fields.append(("layout_cols", config[CONF_LAYOUT_COLS]))
    if CONF_LAYOUT in config:
        config_fields.append(("layout", config[CONF_LAYOUT]))

    config_fields.append(("pins", pins_struct))

    if CONF_CLOCK_SPEED in config:
        config_fields.append(("output_clock_speed", config[CONF_CLOCK_SPEED]))
    if CONF_BIT_DEPTH in config:
        config_fields.append(("bit_depth", config[CONF_BIT_DEPTH]))

    config_fields.append(("min_refresh_rate", min_refresh))

    if CONF_LATCH_BLANKING in config:
        config_fields.append(("latch_blanking", config[CONF_LATCH_BLANKING]))

    if CONF_DOUBLE_BUFFER in config:
        config_fields.append(("double_buffer", config[CONF_DOUBLE_BUFFER]))

    # temporal_dither omitted - uses C++ default (false)

    if CONF_CLOCK_PHASE in config:
        config_fields.append(("clk_phase_inverted", config[CONF_CLOCK_PHASE]))

    # gamma_mode omitted - uses C++ default (CIE1931)

    if CONF_BRIGHTNESS in config:
        config_fields.append(("brightness", config[CONF_BRIGHTNESS]))

    hub75_config = cg.StructInitializer(Hub75Config, *config_fields)

    # ========================================
    # Step 5: Create HUB75Display with config
    # ========================================

    var = cg.new_Pvariable(config[CONF_ID], hub75_config)

    # ========================================
    # Step 6: Register display and set lambda
    # ========================================

    await display.register_display(var, config)

    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA], [(display.DisplayRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))
