"""Reads analog gauges out of frames from an esp32_camera."""

from itertools import pairwise
import math

import esphome.codegen as cg
from esphome.components import (
    camera,
    camera_decoder,
    esp32_camera,
    gauge_needle_detector,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_CALIBRATION,
    CONF_CONTRAST,
    CONF_DEBUG,
    CONF_ID,
    CONF_MAX_VALUE,
    CONF_MIN_VALUE,
    CONF_RAW,
    CONF_RESOLUTION,
    CONF_ROTATION,
)
import esphome.final_validate as fv
from esphome.types import ConfigType

AUTO_LOAD = ["gauge_needle_detector", "sensor"]
CODEOWNERS = ["@nliaudat"]
DEPENDENCIES = ["esp32_camera", "camera_decoder"]
DOMAIN = "analog_gauge_reader"

CONF_CAMERA_ID = "camera_id"
CONF_DECODER_ID = "decoder_id"
CONF_DIALS = "dials"
CONF_MIN_ANGLE = "min_angle"
CONF_MAX_ANGLE = "max_angle"
CONF_ANGLE_OFFSET = "angle_offset"
CONF_CLOCKWISE = "clockwise"
CONF_SCALE = "scale"
CONF_CROP_X = "crop_x"
CONF_CROP_Y = "crop_y"
CONF_CROP_W = "crop_w"
CONF_CROP_H = "crop_h"
CONF_TARGET_COLOR = "target_color"
CONF_COLOR_TOLERANCE = "color_tolerance"
CONF_PAUSED = "paused"
CONF_AUTO_CONTRAST = "auto_contrast"
CONF_STACKED_DIGITS = "stacked_digits"
CONF_NEEDLE_TYPE = "needle_type"
CONF_ALGORITHM = "algorithm"
CONF_CONFIDENCE = "confidence"
CONF_MAPPED = "mapped"
CONF_MIN_SCAN_RADIUS = "min_scan_radius"
CONF_MAX_SCAN_RADIUS = "max_scan_radius"
CONF_DEADZONE_DIAMETER = "deadzone_diameter"
CONF_PROCESS_CHANNEL = "process_channel"


analog_gauge_reader_ns = cg.esphome_ns.namespace("analog_gauge_reader")
AnalogGaugeReader = analog_gauge_reader_ns.class_(
    "AnalogGaugeReader", cg.PollingComponent
)

# Config strings map to enumerators in to_code; a codegen enum value is a C++ expression.
NeedleType = gauge_needle_detector.gauge_needle_detector_ns.enum("NeedleType")
Algorithm = gauge_needle_detector.gauge_needle_detector_ns.enum("Algorithm")
ProcessChannel = analog_gauge_reader_ns.enum("ProcessChannel")

NEEDLE_TYPES: dict[str, cg.MockObj] = NeedleType.options(
    "DARK", "LIGHT", prefix="NEEDLE_TYPE_"
)
ALGORITHMS: dict[str, cg.MockObj] = Algorithm.options(
    "legacy",
    "radial_profile",
    "hough_transform",
    "template_match",
    "auto",
    prefix="ALGORITHM_",
)
PROCESS_CHANNELS: dict[str, cg.MockObj] = ProcessChannel.options(
    "GRAYSCALE",
    "RED",
    "GREEN",
    "BLUE",
    prefix="PROCESS_CHANNEL_",
)

DIAL_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.string,  # String ID for logs and sensor entries
        cv.Optional(CONF_NEEDLE_TYPE, default="DARK"): cv.one_of(
            "DARK", "LIGHT", upper=True
        ),
        cv.Optional(CONF_PROCESS_CHANNEL, default="GRAYSCALE"): cv.one_of(
            *PROCESS_CHANNELS, upper=True
        ),
        cv.Optional(CONF_ALGORITHM, default="radial_profile"): cv.one_of(
            *ALGORITHMS, lower=True
        ),
        cv.Optional(CONF_SCALE, default=1.0): cv.positive_not_null_float,
        cv.Optional(CONF_CROP_X, default=0): cv.int_range(min=0),
        cv.Optional(CONF_CROP_Y, default=0): cv.int_range(min=0),
        cv.Optional(CONF_CROP_W, default=64): cv.positive_int,
        cv.Optional(CONF_CROP_H, default=64): cv.positive_int,
        cv.Optional(CONF_MIN_ANGLE, default=0): cv.float_range(min=0.0, max=360.0),
        cv.Optional(CONF_MAX_ANGLE, default=360): cv.float_range(min=0.0, max=360.0),
        cv.Optional(CONF_ANGLE_OFFSET, default=0): cv.float_range(min=0.0, max=360.0),
        cv.Optional(CONF_CLOCKWISE, default=True): cv.boolean,
        cv.Optional(CONF_MIN_VALUE, default=0): cv.float_,
        cv.Optional(CONF_MAX_VALUE, default=10): cv.float_,
        cv.Optional(CONF_MIN_SCAN_RADIUS, default=0.3): cv.float_range(
            min=0.0, max=1.0
        ),
        cv.Optional(CONF_MAX_SCAN_RADIUS, default=0.9): cv.float_range(
            min=0.0, max=1.0
        ),
        cv.Optional(CONF_DEADZONE_DIAMETER, default=0.0): cv.float_range(min=0.0),
        cv.Optional(CONF_AUTO_CONTRAST, default=True): cv.boolean,
        cv.Optional(CONF_CONTRAST, default=1.0): cv.float_range(min=0.0),
        cv.Optional(CONF_TARGET_COLOR): cv.All(
            cv.hex_int, cv.int_range(min=0, max=0xFFFFFF)
        ),
        cv.Optional(CONF_COLOR_TOLERANCE, default=0.0): cv.float_range(
            min=0.0, max=442.0
        ),
        cv.Optional(CONF_CALIBRATION): cv.All(
            cv.ensure_list(
                cv.Schema(
                    {
                        cv.Required(CONF_RAW): cv.float_,
                        cv.Required(CONF_MAPPED): cv.float_,
                    }
                )
            ),
            cv.Length(min=1, max=255),
        ),
    }
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(AnalogGaugeReader),
        cv.Required(CONF_CAMERA_ID): cv.use_id(esp32_camera.ESP32Camera),
        cv.Required(CONF_DECODER_ID): cv.use_id(camera.Decoder),
        cv.Required(CONF_DIALS): cv.All(cv.ensure_list(DIAL_SCHEMA), cv.Length(min=1)),
        cv.Optional(CONF_PAUSED, default=False): cv.boolean,
        cv.Optional(CONF_DEBUG, default=False): cv.boolean,
        cv.Optional(CONF_STACKED_DIGITS, default=False): cv.boolean,
    }
).extend(cv.polling_component_schema("60s"))

_KEY_CAMERA_FRAME = "_analog_gauge_camera_frame"


def _enum_name(value: object) -> str:
    """Return the C++ enumerator name behind a validated ``cv.enum`` value."""
    return str(getattr(value, "enum_value", value)).rsplit("::", 1)[-1]


def _resolution_dimensions(resolution: object) -> tuple[int, int]:
    """Map an esp32_camera resolution to its pixel dimensions."""
    name = _enum_name(resolution).upper().removeprefix("ESP32_CAMERA_SIZE_")
    parts = name.split("X")
    if len(parts) == 2 and parts[0].isdigit() and parts[1].isdigit():
        return int(parts[0]), int(parts[1])
    raise cv.Invalid(f"Unknown camera resolution {resolution}")


def _needs_color(dial: ConfigType) -> bool:
    """Whether the dial is read from the colour channels of the frame."""
    return CONF_TARGET_COLOR in dial or dial[CONF_PROCESS_CHANNEL] != "GRAYSCALE"


def _final_validate(config: ConfigType) -> ConfigType:
    """Resolve the frame size and check it against the dials and the decoder."""
    camera_config = fv.get_declaring_config(config[CONF_CAMERA_ID])
    width, height = _resolution_dimensions(camera_config[CONF_RESOLUTION])
    pixel_format = camera_config[esp32_camera.CONF_PIXEL_FORMAT]
    # A non-zero jpeg_quality makes esp32_camera convert the frame to JPEG, so
    # the listener receives JPEG even when pixel_format is not JPEG.
    jpeg_quality = camera_config[esp32_camera.CONF_JPEG_QUALITY]
    delivered = "JPEG" if pixel_format == "JPEG" or jpeg_quality != 0 else pixel_format
    # The decoder only decodes JPEG, so reject raw frames at config time.
    if delivered != "JPEG":
        raise cv.Invalid(
            f"analog_gauge_reader needs JPEG frames, but the referenced camera "
            f"delivers {delivered}. Set pixel_format: JPEG, or set a jpeg_quality "
            f"so the camera converts the frame to JPEG.",
            path=[CONF_CAMERA_ID],
        )
    decoder_config = fv.get_declaring_config(config[CONF_DECODER_ID])
    # The decoder rotates first, so a quarter turn swaps the dimensions.
    if decoder_config[CONF_ROTATION] in (90, 270):
        width, height = height, width
    output_format = _enum_name(decoder_config[camera_decoder.CONF_OUTPUT_TYPE])
    if output_format not in ("PIXEL_FORMAT_RGB888", "PIXEL_FORMAT_GRAYSCALE"):
        raise cv.Invalid(
            f"analog_gauge_reader reads RGB888 or GRAYSCALE frames, but the "
            f"referenced camera_decoder outputs {output_format}. Set "
            f"output_type: RGB888 or GRAYSCALE.",
            path=[CONF_DECODER_ID],
        )
    if (
        any(_needs_color(dial) for dial in config[CONF_DIALS])
        and output_format != "PIXEL_FORMAT_RGB888"
    ):
        raise cv.Invalid(
            "Dials using target_color or a colour process_channel need the "
            "decoder's RGB888 output. Set output_type: RGB888 on the "
            "referenced camera_decoder.",
            path=[CONF_DECODER_ID],
        )
    if config[CONF_STACKED_DIGITS]:
        # Each dial carries one digit, so adjacent dials must be one decade apart.
        scales = sorted(dial[CONF_SCALE] for dial in config[CONF_DIALS])
        for lower, upper in pairwise(scales):
            if not math.isclose(upper, lower * 10.0, rel_tol=1e-6):
                raise cv.Invalid(
                    f"stacked_digits needs dial scales that are consecutive powers "
                    f"of ten (1, 10, 100, ...), got "
                    f"{', '.join(f'{scale:g}' for scale in scales)}",
                    path=[CONF_DIALS],
                )
    # Sensor entries and logs name a dial by this id, so it must be unique.
    dial_ids = [dial[CONF_ID] for dial in config[CONF_DIALS]]
    if len(set(dial_ids)) != len(dial_ids):
        raise cv.Invalid("Duplicate dial id; ids must be unique", path=[CONF_DIALS])
    for dial in config[CONF_DIALS]:
        if CONF_TARGET_COLOR in dial and dial[CONF_PROCESS_CHANNEL] != "GRAYSCALE":
            raise cv.Invalid(
                f"Dial '{dial[CONF_ID]}' sets both target_color and process_channel. "
                f"target_color compares all three channels, so it cannot be combined "
                f"with a single-channel process_channel; remove one of the two.",
                path=[CONF_DIALS],
            )
        if dial[CONF_MIN_SCAN_RADIUS] >= dial[CONF_MAX_SCAN_RADIUS]:
            raise cv.Invalid(
                f"Dial '{dial[CONF_ID]}' scan window is empty", path=[CONF_DIALS]
            )
        sweep = (dial[CONF_MAX_ANGLE] - dial[CONF_MIN_ANGLE]) % 360.0
        full_circle = (dial[CONF_MIN_ANGLE], dial[CONF_MAX_ANGLE]) == (0.0, 360.0)
        if CONF_CALIBRATION not in dial and abs(sweep) < 1e-6 and not full_circle:
            raise cv.Invalid(
                f"Dial '{dial[CONF_ID]}' has a zero-width angle sweep "
                f"({dial[CONF_MIN_ANGLE]} to {dial[CONF_MAX_ANGLE]}). Set the "
                f"range the needle travels, or a full 0 to 360.",
                path=[CONF_DIALS],
            )
        if (
            dial[CONF_CROP_X] + dial[CONF_CROP_W] > width
            or dial[CONF_CROP_Y] + dial[CONF_CROP_H] > height
        ):
            raise cv.Invalid(
                f"Dial '{dial[CONF_ID]}' crop "
                f"({dial[CONF_CROP_X]},{dial[CONF_CROP_Y]} "
                f"{dial[CONF_CROP_W]}x{dial[CONF_CROP_H]}) does not fit in the "
                f"camera's {width}x{height} frame",
                path=[CONF_DIALS],
            )
        if dial[CONF_DEADZONE_DIAMETER] >= min(dial[CONF_CROP_W], dial[CONF_CROP_H]):
            raise cv.Invalid(
                f"Dial '{dial[CONF_ID]}' deadzone_diameter "
                f"({dial[CONF_DEADZONE_DIAMETER]}) must be smaller than the crop "
                f"({dial[CONF_CROP_W]}x{dial[CONF_CROP_H]})",
                path=[CONF_DIALS],
            )
    config[_KEY_CAMERA_FRAME] = (width, height)
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    cam = await cg.get_variable(config[CONF_CAMERA_ID])
    decoder = await cg.get_variable(config[CONF_DECODER_ID])
    frame_width, frame_height = config[_KEY_CAMERA_FRAME]
    var = cg.new_Pvariable(config[CONF_ID], cam, decoder, frame_width, frame_height)
    await cg.register_component(var, config)
    cg.add_define("USE_ANALOG_GAUGE_READER")
    # Compile in only the detectors this config uses; "auto" and debug compare all.
    algorithms = {dial[CONF_ALGORITHM] for dial in config[CONF_DIALS]}
    if "auto" in algorithms or config[CONF_DEBUG]:
        algorithms = set(gauge_needle_detector.ALGORITHM_DEFINES)
    gauge_needle_detector.enable_detectors(algorithms)
    cg.add(var.set_pause_processing(config[CONF_PAUSED]))
    cg.add(var.set_debug(config[CONF_DEBUG]))

    dials = config[CONF_DIALS]
    cg.add_define("ANALOG_GAUGE_READER_DIAL_COUNT", len(dials))
    if config[CONF_STACKED_DIGITS]:
        cg.add_define("USE_ANALOG_GAUGE_READER_STACKED_DIGITS")
    if calibration_sizes := [
        len(d[CONF_CALIBRATION]) for d in dials if CONF_CALIBRATION in d
    ]:
        cg.add_define("ANALOG_GAUGE_READER_CALIBRATION_POINTS", max(calibration_sizes))
    cg.add(
        var.set_max_crop(
            max(d[CONF_CROP_W] for d in dials), max(d[CONF_CROP_H] for d in dials)
        )
    )
    if any(_needs_color(dial) for dial in dials):
        cg.add_define("USE_ANALOG_GAUGE_READER_COLOR")
        scratch = cg.static_buffer(
            config[CONF_ID],
            cg.uint8,
            max(d[CONF_CROP_W] * d[CONF_CROP_H] for d in dials),
        )
        cg.add(var.set_scratch_buffer(scratch))

    for dial in dials:
        channels = 3 if _needs_color(dial) else 1
        crop = cg.static_buffer(
            config[CONF_ID],
            cg.uint8,
            dial[CONF_CROP_W] * dial[CONF_CROP_H] * channels,
        )
        calibration = None
        if points := dial.get(CONF_CALIBRATION):
            # Sorted here so the firmware does not sort at boot.
            pairs = ", ".join(
                f"{{ {item[CONF_RAW]}f, {item[CONF_MAPPED]}f }}"
                for item in sorted(points, key=lambda item: item[CONF_RAW])
            )
            calibration = cg.RawExpression(f"{{{{ {pairs} }}}}")
        # The fields follow the order of the C++ struct, which designated initializers require.
        s = cg.StructInitializer(
            analog_gauge_reader_ns.struct("DialConfig"),
            ("id", dial[CONF_ID]),
            ("crop", crop),
            ("calibration", calibration),
            ("scale", dial[CONF_SCALE]),
            ("crop_x", dial[CONF_CROP_X]),
            ("crop_y", dial[CONF_CROP_Y]),
            ("crop_w", dial[CONF_CROP_W]),
            ("crop_h", dial[CONF_CROP_H]),
            ("min_angle", dial[CONF_MIN_ANGLE]),
            ("max_angle", dial[CONF_MAX_ANGLE]),
            ("angle_offset", dial[CONF_ANGLE_OFFSET]),
            ("min_value", dial[CONF_MIN_VALUE]),
            ("max_value", dial[CONF_MAX_VALUE]),
            ("contrast", dial[CONF_CONTRAST]),
            ("color_tolerance", dial[CONF_COLOR_TOLERANCE]),
            ("min_scan_radius", dial[CONF_MIN_SCAN_RADIUS]),
            ("max_scan_radius", dial[CONF_MAX_SCAN_RADIUS]),
            ("deadzone_diameter", dial[CONF_DEADZONE_DIAMETER]),
            ("target_color", dial.get(CONF_TARGET_COLOR)),
            ("needle_type", NEEDLE_TYPES[dial[CONF_NEEDLE_TYPE]]),
            ("algorithm", ALGORITHMS[dial[CONF_ALGORITHM]]),
            ("process_channel", PROCESS_CHANNELS[dial[CONF_PROCESS_CHANNEL]]),
            ("channels", channels),
            ("calibration_count", len(points) if points else None),
            ("clockwise", dial[CONF_CLOCKWISE]),
            ("auto_contrast", dial[CONF_AUTO_CONTRAST]),
        )
        cg.add(var.add_dial(s))
