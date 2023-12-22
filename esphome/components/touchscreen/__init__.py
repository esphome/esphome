import esphome.config_validation as cv
import esphome.codegen as cg

from esphome.components import display
from esphome import automation
from esphome.const import (
    CONF_ON_TOUCH,
    CONF_ON_RELEASE,
    CONF_MIRROR_X,
    CONF_MIRROR_Y,
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

CONF_DISPLAY = "display"
CONF_TOUCHSCREEN_ID = "touchscreen_id"
CONF_REPORT_INTERVAL = "report_interval"  # not used yet:
CONF_ON_UPDATE = "on_update"
CONF_TOUCH_TIMEOUT = "touch_timeout"

CONF_CALIBRATION = "calibration"
CONF_X_MIN = "x_min"
CONF_X_MAX = "x_max"
CONF_Y_MIN = "y_min"
CONF_Y_MAX = "y_max"


def validate_calibration(config):
    if CONF_CALIBRATION in config:
        if (
            cv.int_(config[CONF_CALIBRATION][CONF_X_MIN]) != 0
            and cv.int_(config[CONF_CALIBRATION][CONF_X_MAX]) != 0
            and abs(
                cv.int_(config[CONF_CALIBRATION][CONF_X_MIN])
                - cv.int_(config[CONF_CALIBRATION][CONF_X_MAX])
            )
            < 10
        ):
            raise cv.Invalid("Calibration X values difference must be more then 10")

        if (
            cv.int_(config[CONF_CALIBRATION][CONF_Y_MIN]) != 0
            and cv.int_(config[CONF_CALIBRATION][CONF_Y_MAX]) != 0
            and abs(
                cv.int_(config[CONF_CALIBRATION][CONF_Y_MIN])
                - cv.int_(config[CONF_CALIBRATION][CONF_Y_MAX])
            )
            < 10
        ):
            raise cv.Invalid("Calibration Y values difference must be more then 10")

    return config


def calibration_schema(max):
    return cv.Schema(
        {
            cv.Optional(CONF_X_MIN, default=0): cv.int_range(min=0, max=4095),
            cv.Optional(CONF_X_MAX, default=max): cv.int_range(min=0, max=4095),
            cv.Optional(CONF_Y_MIN, default=0): cv.int_range(min=0, max=4095),
            cv.Optional(CONF_Y_MAX, default=max): cv.int_range(min=0, max=4095),
        },
        validate_calibration,
    )


TOUCHSCREEN_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_DISPLAY): cv.use_id(display.Display),
        cv.Optional(CONF_TRANSFORM): cv.Schema(
            {
                cv.Optional(CONF_SWAP_XY, default=False): cv.boolean,
                cv.Optional(CONF_MIRROR_X, default=False): cv.boolean,
                cv.Optional(CONF_MIRROR_Y, default=False): cv.boolean,
            }
        ),
        cv.Optional(CONF_CALIBRATION): calibration_schema(0),
        cv.Optional(CONF_ON_TOUCH): automation.validate_automation(single=True),
        cv.Optional(CONF_ON_UPDATE): automation.validate_automation(single=True),
        cv.Optional(CONF_ON_RELEASE): automation.validate_automation(single=True),
    }
).extend(cv.polling_component_schema("50ms"))


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
        cg.add(
            var.set_calibration(
                config[CONF_CALIBRATION][CONF_X_MIN],
                config[CONF_CALIBRATION][CONF_X_MAX],
                config[CONF_CALIBRATION][CONF_Y_MIN],
                config[CONF_CALIBRATION][CONF_Y_MAX],
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
