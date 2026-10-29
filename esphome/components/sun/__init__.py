import contextlib
import re
from typing import Any

from esphome import automation
import esphome.codegen as cg
from esphome.components import time
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_LATITUDE, CONF_LONGITUDE, CONF_TIME_ID
from esphome.types import ConfigType

CODEOWNERS = ["@OttoWinter"]
DOMAIN = "sun"
sun_ns = cg.esphome_ns.namespace("sun")

Sun = sun_ns.class_("Sun")

CONF_SUN_ID = "sun_id"
CONF_ELEVATION = "elevation"
CONF_ON_SUNRISE = "on_sunrise"
CONF_ON_SUNSET = "on_sunset"

# Default sun elevation is a bit below horizon because sunset
# means time when the entire sun disk is below the horizon
DEFAULT_ELEVATION = -0.83333

ELEVATION_MAP = {
    "sunrise": 0.0,
    "sunset": 0.0,
    "civil": -6.0,
    "nautical": -12.0,
    "astronomical": -18.0,
}


def elevation(value: Any) -> float:
    if isinstance(value, str):
        with contextlib.suppress(cv.Invalid):
            value = ELEVATION_MAP[
                cv.one_of(*ELEVATION_MAP, lower=True, space="_")(value)
            ]
    value = cv.angle(value)
    return cv.float_range(min=-180, max=180)(value)


# Parses sexagesimal values like 22°57′7″S
LAT_LON_REGEX = re.compile(
    r"([+\-])?\s*"
    r"(?:([0-9]+)\s*°)?\s*"
    r"(?:([0-9]+)\s*[′\'])?\s*"
    r'(?:([0-9]+)\s*[″"])?\s*'
    r"([NESW])?"
)


def parse_latlon(value: Any) -> float:
    if isinstance(value, str) and value.endswith("°"):
        # strip trailing degree character
        value = value[:-1]
    try:
        return cv.float_(value)
    except cv.Invalid:
        pass

    value = cv.string_strict(value)
    m = LAT_LON_REGEX.match(value)

    if m is None:
        raise cv.Invalid("Invalid format for latitude/longitude")
    sign = m.group(1)
    deg = m.group(2)
    minute = m.group(3)
    second = m.group(4)
    d = m.group(5)

    val = float(deg or 0) + float(minute or 0) / 60 + float(second or 0) / 3600
    if sign == "-":
        val *= -1
    if d and d in "SW":
        val *= -1
    return val


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(Sun),
        cv.GenerateID(CONF_TIME_ID): cv.use_id(time.RealTimeClock),
        cv.Required(CONF_LATITUDE): cv.All(
            parse_latlon, cv.float_range(min=-90, max=90)
        ),
        cv.Required(CONF_LONGITUDE): cv.All(
            parse_latlon, cv.float_range(min=-180, max=180)
        ),
        cv.Optional(CONF_ON_SUNRISE): automation.validate_automation(
            {cv.Optional(CONF_ELEVATION, default=DEFAULT_ELEVATION): elevation}
        ),
        cv.Optional(CONF_ON_SUNSET): automation.validate_automation(
            {cv.Optional(CONF_ELEVATION, default=DEFAULT_ELEVATION): elevation}
        ),
    }
)


_DIRECTIONS = (
    (CONF_ON_SUNRISE, "SUN_SUNRISE_AUTOMATION_COUNT", "add_sunrise_automation"),
    (CONF_ON_SUNSET, "SUN_SUNSET_AUTOMATION_COUNT", "add_sunset_automation"),
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.add_variable(var.set_time, config[CONF_TIME_ID])
    cg.add(var.set_latitude(config[CONF_LATITUDE]))
    cg.add(var.set_longitude(config[CONF_LONGITUDE]))

    if not any(key in config for key, _, _ in _DIRECTIONS):
        return
    cg.add_define("USE_SUN_ELEVATION_POLL")
    for key, count_define, method in _DIRECTIONS:
        if confs := config.get(key):
            cg.add_define(count_define, len(confs))
            for conf in confs:
                obj = await automation.new_automation([], conf)
                cg.add(getattr(var, method)(obj, conf[CONF_ELEVATION]))
    cg.add(var.start_elevation_poll())


SUN_HORIZON_CONDITION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(Sun),
        cv.Optional(CONF_ELEVATION, default=DEFAULT_ELEVATION): cv.templatable(
            elevation
        ),
    }
)

automation.register_apply_condition(
    "sun.is_above_horizon",
    SUN_HORIZON_CONDITION_SCHEMA,
    automation.ApplyCall("elevation() > {}", ((CONF_ELEVATION, cg.double),)),
)

automation.register_apply_condition(
    "sun.is_below_horizon",
    SUN_HORIZON_CONDITION_SCHEMA,
    automation.ApplyCall("elevation() < {}", ((CONF_ELEVATION, cg.double),)),
)
