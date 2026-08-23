"""
LVGL 9.5 SVG Widget for ESPHome

Renders static SVG images using the ThorVG vector engine built into LVGL 9.5.
The SVG is rasterised once (at the requested size) in a FreeRTOS task with a
64 KB PSRAM stack, then displayed via an lv_canvas widget.

Requirements:
- LV_USE_THORVG_INTERNAL must be enabled
- LV_USE_VECTOR_GRAPHIC must be enabled
- LV_USE_SVG must be enabled

Usage in ESPHome YAML:

    Method 1 - Embedded in firmware (auto-detects size from SVG viewBox):
    - svg:
        id: my_icon
        file: "icons/home.svg"     # Local file, embedded in firmware
        # width/height auto-detected from viewBox

    Method 2 - File on filesystem (SD card, LittleFS):
    - svg:
        id: my_icon
        src: "/sdcard/icons/home.svg"   # File path on ESP32 filesystem
        width: 64                       # Required (can't read at compile time)
        height: 64

    Method 3 - Embedded with resize (render at custom size for screen layout):
    - svg:
        id: my_icon
        file: "icons/home.svg"     # Source viewBox is 512x512
        width: 64                  # Render at 64x64 instead
        height: 64

Note: ThorVG rendering requires a large stack (32 KB+). The rendering is
deferred to a FreeRTOS task with stack allocated in PSRAM to avoid overflow.
"""

from pathlib import Path
import re

from esphome import codegen as cg, config_validation as cv
from esphome.const import CONF_FILE, CONF_HEIGHT, CONF_RAW_DATA_ID, CONF_WIDTH
from esphome.core import CORE

from ..defines import CONF_MAIN, CONF_SRC, add_lv_use
from ..lv_validation import size
from ..lvcode import lv_obj
from ..types import LvType
from . import Widget, WidgetType

_SVG_INCLUDES_ADDED: set[str] = set()

CONF_SVG = "svg"

lv_canvas_t = LvType("lv_canvas_t")


def svg_path_validator(value):
    """Validate SVG source file path (on ESP32 filesystem)."""
    value = cv.string(value)
    if not value.startswith("/"):
        raise cv.Invalid(
            f"SVG src must be an absolute path starting with '/', got: '{value}'. "
            f"Example: '/sdcard/icons/home.svg'"
        )
    if not value.lower().endswith(".svg"):
        raise cv.Invalid(f"SVG src must be an .svg file, got: '{value}'")
    return value


def svg_file_validator(value):
    """Validate and resolve local SVG file path (to embed in firmware)."""
    value = cv.string(value)
    path = CORE.relative_config_path(value)
    if not Path(path).is_file():
        raise cv.Invalid(f"SVG file not found: {path}")
    return str(path)


def _parse_svg_dimensions(svg_text):
    """Extract width/height from SVG viewBox or width/height attributes.

    Returns (width, height) as integers, or (None, None) if not found.
    """
    # Try viewBox first: viewBox="minX minY width height"
    m = re.search(r'viewBox\s*=\s*"([^"]*)"', svg_text)
    if not m:
        m = re.search(r"viewBox\s*=\s*'([^']*)'", svg_text)
    if m:
        parts = m.group(1).split()
        if len(parts) == 4:
            try:
                return int(float(parts[2])), int(float(parts[3]))
            except ValueError:
                pass

    # Fallback: explicit width/height attributes (unitless or px)
    w_match = re.search(r'\bwidth\s*=\s*["\']?([\d.]+)', svg_text)
    h_match = re.search(r'\bheight\s*=\s*["\']?([\d.]+)', svg_text)
    if w_match and h_match:
        try:
            return int(float(w_match.group(1))), int(float(h_match.group(1)))
        except ValueError:
            pass

    return None, None


CONF_SVG_WIDTH = "svg_width"
CONF_SVG_HEIGHT = "svg_height"


def validate_svg_source(config):
    """Validate source and extract dimensions from SVG if using file method."""
    has_src = CONF_SRC in config
    has_file = CONF_FILE in config

    if has_src and has_file:
        raise cv.Invalid(
            "Cannot specify both 'src' and 'file'. "
            "Use 'src' for filesystem path or 'file' for embedded."
        )
    if not has_src and not has_file:
        raise cv.Invalid(
            "Must specify either 'src' (filesystem path) or 'file' (embedded in firmware)."
        )

    # For src method, width and height are required
    if has_src and (CONF_WIDTH not in config or CONF_HEIGHT not in config):
        raise cv.Invalid(
            "'width' and 'height' are required when using 'src' "
            "(filesystem path). Cannot auto-detect dimensions at compile time."
        )

    # For file method, auto-detect dimensions from SVG
    if has_file:
        file_path = config[CONF_FILE]
        path = Path(file_path)
        try:
            with path.open(encoding="utf-8") as f:
                svg_text = f.read()
            svg_w, svg_h = _parse_svg_dimensions(svg_text)
        except (OSError, UnicodeDecodeError) as err:
            raise cv.Invalid(f"Error reading SVG file {file_path}: {err}") from err

        # Use auto-detected dimensions unless the user explicitly provided them
        if CONF_WIDTH not in config and CONF_HEIGHT not in config:
            if svg_w is None or svg_h is None:
                raise cv.Invalid(
                    f"Cannot auto-detect dimensions from SVG file {file_path}. "
                    f"Please specify 'width' and 'height' manually, or add a "
                    f"viewBox attribute to the SVG."
                )
            config[CONF_SVG_WIDTH] = svg_w
            config[CONF_SVG_HEIGHT] = svg_h
        elif CONF_WIDTH in config and CONF_HEIGHT in config:
            # User specified both - use those
            pass
        else:
            raise cv.Invalid(
                "Specify both 'width' and 'height', or neither (for auto-detect)."
            )

    return config


SVG_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_WIDTH): size,
        cv.Optional(CONF_HEIGHT): size,
        cv.Optional(CONF_SRC): svg_path_validator,
        cv.Optional(CONF_FILE): svg_file_validator,
        cv.GenerateID(CONF_RAW_DATA_ID): cv.declare_id(cg.uint8),
    }
).add_extra(validate_svg_source)


class SvgType(WidgetType):
    def __init__(self):
        super().__init__(
            CONF_SVG,
            lv_canvas_t,
            (CONF_MAIN,),
            SVG_SCHEMA,
            modify_schema={},
            lv_name="canvas",  # Creates lv_canvas_create() at runtime
        )

    def get_uses(self):
        return ("CANVAS", "SVG", "THORVG_INTERNAL", "VECTOR_GRAPHIC")

    async def to_code(self, w: Widget, config):
        add_lv_use("CANVAS")
        add_lv_use("SVG")
        add_lv_use("THORVG_INTERNAL")
        add_lv_use("VECTOR_GRAPHIC")

        from ..lvcode import lv_add

        # Determine dimensions
        if CONF_SVG_WIDTH in config:
            width = config[CONF_SVG_WIDTH]
            height = config[CONF_SVG_HEIGHT]
        else:
            width = config[CONF_WIDTH]
            height = config[CONF_HEIGHT]

        # Set widget size
        lv_obj.set_size(w.obj, width, height)

        # Note: Widget visibility during async render is managed by svg_loader.h
        # (user's 'hidden' config is saved and restored after rendering)

        # Add include once
        if CONF_SVG not in _SVG_INCLUDES_ADDED:
            _SVG_INCLUDES_ADDED.add(CONF_SVG)
            cg.add_global(
                cg.RawStatement('#include "esphome/components/lvgl/svg_loader.h"')
            )

        # Get hidden config
        user_wants_hidden = "true" if config.get("hidden", False) else "false"

        if src := config.get(CONF_SRC):
            # ------- Filesystem SVG -------
            # The file is read inside the async render task (on the large
            # PSRAM stack).  We only pass the path string here.
            lv_add(
                cg.RawStatement(f"""
    esphome::lvgl::svg_setup_and_render_file({w.obj}, "{src}", {width}, {height}, {user_wants_hidden});""")
            )

        elif file_path := config.get(CONF_FILE):
            # ------- Embedded SVG -------
            with Path(file_path).open("rb") as f:
                svg_data = f.read()

            # Ensure null-terminated (ThorVG expects C string)
            svg_data_with_null = svg_data + b"\x00"

            raw_data_id = config[CONF_RAW_DATA_ID]
            prog_arr = cg.progmem_array(raw_data_id, list(svg_data_with_null))

            lv_add(
                cg.RawStatement(f"""
    esphome::lvgl::svg_setup_and_render({w.obj}, (const char *){prog_arr}, {len(svg_data)}, {width}, {height}, {user_wants_hidden});""")
            )


svg_spec = SvgType()
