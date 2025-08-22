from esphome import automation
from esphome.automation import Trigger
import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.const import (
    CONF_FILTER,
    CONF_ID,
    CONF_INTERRUPT_PIN,
    CONF_MODE,
    CONF_TRIGGER_ID,
)
from esphome.core import Lambda
from esphome.cpp_generator import ExpressionStatement, MockObj

CODEOWNERS = ["@DT-art1"]
DEPENDENCIES = ["i2c"]
MULTI_CONF = True

amg8833_ns = cg.esphome_ns.namespace("amg8833")
AMG8833 = amg8833_ns.class_("AMG8833", cg.PollingComponent, i2c.I2CDevice)

std_array = cg.std_ns.class_("array")
measurement_arguments = std_array.template(
    std_array.template(cg.float_, 8), 8
).operator("ref")

FPS = amg8833_ns.enum("FPS")
Mode = amg8833_ns.enum("Mode")

CONF_AMG8833_ID = "amg8833_id"

CONF_FPS = "fps"
CONF_MOTION_HYSTERESIS = "motion_hysteresis"
CONF_MOTION_MAXIMUM = "motion_maximum"
CONF_MOTION_MINIMUM = "motion_minimum"
CONF_ON_MEASUREMENT = "on_measurement"
CONF_PRESENCE_HYSTERESIS = "presence_hysteresis"
CONF_PRESENCE_LOWER = "presence_lower"
CONF_PRESENCE_UPPER = "presence_upper"
CONF_SOFTWARE_OUTPUT = "software_output"

CONF_FPS_SELECTS = {"FPS_10": FPS.FPS_10, "FPS_1": FPS.FPS_1}

CONF_MODE_SELECTS = {"MOTION": Mode.MOTION, "PRESENCE": Mode.PRESENCE}

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(AMG8833),
            cv.Optional(CONF_FILTER, default=True): cv.boolean,
            cv.Optional(CONF_FPS, default="FPS_1"): cv.enum(
                CONF_FPS_SELECTS, upper=True
            ),
            cv.Optional(CONF_INTERRUPT_PIN, default=False): cv.boolean,
            cv.Optional(CONF_MODE, default="MOTION"): cv.enum(
                CONF_MODE_SELECTS, upper=True
            ),
            cv.Optional(CONF_MOTION_HYSTERESIS, default=0.25): cv.float_range(0.0),
            cv.Optional(CONF_MOTION_MINIMUM, default=-0.5): cv.float_range(max=0.0),
            cv.Optional(CONF_MOTION_MAXIMUM, default=0.5): cv.float_range(0.0),
            cv.Optional(CONF_PRESENCE_HYSTERESIS, default=0.5): cv.float_range(0.0),
            cv.Optional(CONF_PRESENCE_LOWER, default=18.0): cv.float_range(0.0),
            cv.Optional(CONF_PRESENCE_UPPER, default=25.0): cv.float_range(0.0),
            cv.Optional(CONF_SOFTWARE_OUTPUT, default=False): cv.boolean,
            cv.Optional(CONF_ON_MEASUREMENT): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        Trigger.template(measurement_arguments)
                    ),
                }
            ),
        }
    )
    .extend(cv.polling_component_schema("0.1s"))
    .extend(i2c.i2c_device_schema(0x69))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_filter(config[CONF_FILTER]))
    cg.add(var.set_fps(config[CONF_FPS]))
    cg.add(var.set_interrupt_pin(config[CONF_INTERRUPT_PIN]))
    cg.add(var.set_mode(config[CONF_MODE]))
    cg.add(
        var.set_motion_thresholds(
            config[CONF_MOTION_MINIMUM],
            config[CONF_MOTION_MAXIMUM],
            config[CONF_MOTION_HYSTERESIS],
        )
    )
    cg.add(
        var.set_presence_thresholds(
            config[CONF_PRESENCE_LOWER],
            config[CONF_PRESENCE_UPPER],
            config[CONF_PRESENCE_HYSTERESIS],
        )
    )
    cg.add(var.set_software_output(config[CONF_SOFTWARE_OUTPUT]))

    for conf in config.get(CONF_ON_MEASUREMENT, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID])
        trigger = await automation.build_automation(
            trigger, [(measurement_arguments, "measurement")], conf
        )
        trigger = Lambda(
            str(ExpressionStatement(trigger.trigger(MockObj("measurement"))))
        )
        trigger = await cg.process_lambda(
            trigger, [(measurement_arguments, "measurement")]
        )
        cg.add(var.add_measurement_callback(trigger))
