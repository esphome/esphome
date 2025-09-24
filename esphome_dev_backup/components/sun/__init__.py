import re

from esphome import automation
import esphome.codegen as cg
from esphome.components import time
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_LATITUDE,
    CONF_LONGITUDE,
    CONF_TIME_ID,
    CONF_TRIGGER_ID,
)

CODEOWNERS = ["@OttoWinter"]
sun_ns = cg.esphome_ns.namespace("sun")

Sun = sun_ns.class_("Sun")
SunTrigger = sun_ns.class_(
    "SunTrigger", cg.PollingComponent, automation.Trigger.template()
)
SunCondition = sun_ns.class_("SunCondition", automation.Condition)

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


def elevation(value):
    if isinstance(value, str):
        try:
            value = ELEVATION_MAP[
                cv.one_of(*ELEVATION_MAP, lower=True, space="_")(value)
            ]
        except cv.Invalid:
            pass
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


def parse_latlon(value):
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
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(SunTrigger),
                cv.Optional(CONF_ELEVATION, default=DEFAULT_ELEVATION): elevation,
            }
        ),
        cv.Optional(CONF_ON_SUNSET): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(SunTrigger),
                cv.Optional(CONF_ELEVATION, default=DEFAULT_ELEVATION): elevation,
            }
        ),
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    time_ = await cg.get_variable(config[CONF_TIME_ID])
    cg.add(var.set_time(time_))
    cg.add(var.set_latitude(config[CONF_LATITUDE]))
    cg.add(var.set_longitude(config[CONF_LONGITUDE]))

    for conf in config.get(CONF_ON_SUNRISE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID])
        await cg.register_component(trigger, conf)
        await cg.register_parented(trigger, var)
        cg.add(trigger.set_sunrise(True))
        cg.add(trigger.set_elevation(conf[CONF_ELEVATION]))
        await automation.build_automation(trigger, [], conf)

    for conf in config.get(CONF_ON_SUNSET, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID])
        await cg.register_component(trigger, conf)
        await cg.register_parented(trigger, var)
        cg.add(trigger.set_sunrise(False))
        cg.add(trigger.set_elevation(conf[CONF_ELEVATION]))
        await automation.build_automation(trigger, [], conf)


@automation.register_condition(
    "sun.is_above_horizon",
    SunCondition,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(Sun),
            cv.Optional(CONF_ELEVATION, default=DEFAULT_ELEVATION): cv.templatable(
                elevation
            ),
        }
    ),
)
async def sun_above_horizon_to_code(config, condition_id, template_arg, args):
    var = cg.new_Pvariable(condition_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    templ = await cg.templatable(config[CONF_ELEVATION], args, cg.double)
    cg.add(var.set_elevation(templ))
    cg.add(var.set_above(True))
    return var


@automation.register_condition(
    "sun.is_below_horizon",
    SunCondition,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(Sun),
            cv.Optional(CONF_ELEVATION, default=DEFAULT_ELEVATION): cv.templatable(
                elevation
            ),
        }
    ),
)
async def sun_below_horizon_to_code(config, condition_id, template_arg, args):
    var = cg.new_Pvariable(condition_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    templ = await cg.templatable(config[CONF_ELEVATION], args, cg.double)
    cg.add(var.set_elevation(templ))
    cg.add(var.set_above(False))
    return var
