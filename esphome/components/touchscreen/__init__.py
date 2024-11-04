from esphome import automation
import esphome.codegen as cg
from esphome.components import display
import esphome.config_validation as cv
from esphome.const import (
    CONF_CALIBRATION,
    CONF_DISPLAY,
    CONF_MIRROR_X,
    CONF_MIRROR_Y,
    CONF_ON_RELEASE,
    CONF_ON_TOUCH,
    CONF_ON_UPDATE,
    CONF_SWAP_XY,
    CONF_TRANSFORM,
)
from esphome.core import coroutine_with_priority

CODEOWNERS = ["@jesserockz", "@nielsnl68"]
DEPENDENCIES = ["display"]

IS_PLATFORM_COMPONENT = True

touchscreen_ns = cg.esphome_ns.namespace("touchscreen")

Touchscreen = touchscreen_ns.class_("Touchscreen", cg.PollingComponent)
TouchRotation = touchscreen_ns.enum("TouchRotation")
TouchPoint = touchscreen_ns.struct("TouchPoint")
TouchPoints_t = cg.std_vector.template(TouchPoint)
TouchPoints_t_const_ref = TouchPoints_t.operator("ref").operator("const")
TouchListener = touchscreen_ns.class_("TouchListener")

CONF_TOUCHSCREEN_ID = "touchscreen_id"
CONF_REPORT_INTERVAL = "report_interval"  # not used yet:
CONF_TOUCH_TIMEOUT = "touch_timeout"


CONF_X_MIN = "x_min"
CONF_X_MAX = "x_max"
CONF_Y_MIN = "y_min"
CONF_Y_MAX = "y_max"


def validate_calibration(calibration_config):
    x_min = calibration_config[CONF_X_MIN]
    x_max = calibration_config[CONF_X_MAX]
    y_min = calibration_config[CONF_Y_MIN]
    y_max = calibration_config[CONF_Y_MAX]
    if x_max < x_min:
        raise cv.Invalid(
            "x_min must be smaller than x_max. To mirror the direction use the 'transform' options"
        )
    if y_max < y_min:
        raise cv.Invalid(
            "y_min must be smaller than y_max. To mirror the direction use the 'transform' options"
        )
    x_delta = x_max - x_min
    y_delta = y_max - y_min
    if x_delta < 10 or y_delta < 10:
        raise cv.Invalid("Calibration value range must be greater than 10")
    return calibration_config


CALIBRATION_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(CONF_X_MIN): cv.int_range(min=0, max=4095),
            cv.Required(CONF_X_MAX): cv.int_range(min=0, max=4095),
            cv.Required(CONF_Y_MIN): cv.int_range(min=0, max=4095),
            cv.Required(CONF_Y_MAX): cv.int_range(min=0, max=4095),
        }
    ),
    validate_calibration,
)


def touchscreen_schema(default_touch_timeout=cv.UNDEFINED, calibration_required=False):
    calibration = (
        cv.Required(CONF_CALIBRATION)
        if calibration_required
        else cv.Optional(CONF_CALIBRATION)
    )
    return cv.Schema(
        {
            cv.GenerateID(CONF_DISPLAY): cv.use_id(display.Display),
            cv.Optional(CONF_TRANSFORM): cv.Schema(
                {
                    cv.Optional(CONF_SWAP_XY, default=False): cv.boolean,
                    cv.Optional(CONF_MIRROR_X, default=False): cv.boolean,
                    cv.Optional(CONF_MIRROR_Y, default=False): cv.boolean,
                }
            ),
            cv.Optional(CONF_TOUCH_TIMEOUT, default=default_touch_timeout): cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(max=cv.TimePeriod(milliseconds=65535)),
            ),
            calibration: CALIBRATION_SCHEMA,
            cv.Optional(CONF_ON_TOUCH): automation.validate_automation(single=True),
            cv.Optional(CONF_ON_UPDATE): automation.validate_automation(single=True),
            cv.Optional(CONF_ON_RELEASE): automation.validate_automation(single=True),
        }
    ).extend(cv.polling_component_schema("50ms"))


TOUCHSCREEN_SCHEMA = touchscreen_schema(cv.UNDEFINED)


async def register_touchscreen(var, config):
    await cg.register_component(var, config)

    disp = await cg.get_variable(config[CONF_DISPLAY])
    cg.add(var.set_display(disp))

    if CONF_TOUCH_TIMEOUT in config:
        cg.add(var.set_touch_timeout(config[CONF_TOUCH_TIMEOUT]))

    if CONF_TRANSFORM in config:
        transform = config[CONF_TRANSFORM]
        cg.add(var.set_swap_xy(transform[CONF_SWAP_XY]))
        cg.add(var.set_mirror_x(transform[CONF_MIRROR_X]))
        cg.add(var.set_mirror_y(transform[CONF_MIRROR_Y]))

    if CONF_CALIBRATION in config:
        calibration_config = config[CONF_CALIBRATION]
        cg.add(
            var.set_calibration(
                calibration_config[CONF_X_MIN],
                calibration_config[CONF_X_MAX],
                calibration_config[CONF_Y_MIN],
                calibration_config[CONF_Y_MAX],
            )
        )

    if CONF_ON_TOUCH in config:
        await automation.build_automation(
            var.get_touch_trigger(),
            [(TouchPoint, "touch"), (TouchPoints_t_const_ref, "touches")],
            config[CONF_ON_TOUCH],
        )

    if CONF_ON_UPDATE in config:
        await automation.build_automation(
            var.get_update_trigger(),
            [(TouchPoints_t_const_ref, "touches")],
            config[CONF_ON_UPDATE],
        )

    if CONF_ON_RELEASE in config:
        await automation.build_automation(
            var.get_release_trigger(),
            [],
            config[CONF_ON_RELEASE],
        )


@coroutine_with_priority(100.0)
async def to_code(config):
    cg.add_global(touchscreen_ns.using)
    cg.add_define("USE_TOUCHSCREEN")
