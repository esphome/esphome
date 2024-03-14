import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins, automation, core
from esphome.components import i2c
from esphome.automation import maybe_simple_id
from esphome.const import (
    CONF_ID,
    CONF_FREQUENCY,
)


CODEOWNERS = ["@X-Ryl669"]
DEPENDENCIES = ["i2c"]
MULTI_CONF = True


at581x_ns = cg.esphome_ns.namespace("at581x")
AT581XComponent = at581x_ns.class_("AT581XComponent", cg.Component, i2c.I2CDevice)


CONF_AT581X_ID = "at581x_id"


CONF_SENSING_DISTANCE = "sensing_distance"
CONF_SENSITIVITY = "sensitivity"
CONF_DETECTION_PIN = "detection_pin"
CONF_POWERON_SELFCHECK_TIME = "poweron_selfcheck_time"
CONF_PROTECT_TIME = "protect_time"
CONF_TRIGGER_BASE = "trigger_base"
CONF_TRIGGER_KEEP = "trigger_keep"
CONF_STAGE_GAIN = "stage_gain"
CONF_POWER_CONSUMPTION = "power_consumption"
CONF_HW_FRONTEND_RESET = "hw_frontend_reset"

RADAR_ALLOWED_FREQ = [
    5696e6,
    5715e6,
    5730e6,
    5748e6,
    5765e6,
    5784e6,
    5800e6,
    5819e6,
    5836e6,
    5851e6,
    5869e6,
    5888e6,
]
RADAR_ALLOWED_CUR_CONSUMPTION = [
    48e-6,
    56e-6,
    63e-6,
    70e-6,
    77e-6,
    91e-6,
    105e-6,
    115e-6,
    40e-6,
    44e-6,
    47e-6,
    51e-6,
    54e-6,
    61e-6,
    68e-6,
    78e-6,
]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(AT581XComponent),
        cv.Optional(
            CONF_DETECTION_PIN, default=21
        ): pins.internal_gpio_input_pin_schema,
    }
)

CONFIG_SCHEMA = cv.All(
    CONFIG_SCHEMA.extend(i2c.i2c_device_schema(0x28)).extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    pin = await cg.gpio_pin_expression(config[CONF_DETECTION_PIN])
    cg.add(var.set_detection_pin(pin))


# Actions
AT581XResetAction = at581x_ns.class_("AT581XResetAction", automation.Action)
AT581XSettingsAction = at581x_ns.class_("AT581XSettingsAction", automation.Action)


@automation.register_action(
    "at581x.reset",
    AT581XResetAction,
    maybe_simple_id(
        {
            cv.Required(CONF_ID): cv.use_id(AT581XComponent),
        }
    ),
)
async def at581x_reset_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])

    return var


RADAR_SETTINGS_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.use_id(AT581XComponent),
        cv.Optional(CONF_HW_FRONTEND_RESET): cv.templatable(cv.boolean),
        cv.Optional(CONF_FREQUENCY, default="5800MHz"): cv.templatable(
            cv.All(cv.frequency, cv.one_of(*RADAR_ALLOWED_FREQ))
        ),
        cv.Optional(CONF_SENSING_DISTANCE, default=823): cv.templatable(
            cv.int_range(min=0, max=1023)
        ),
        cv.Optional(CONF_POWERON_SELFCHECK_TIME, default="2000ms"): cv.templatable(
            cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(max=core.TimePeriod(milliseconds=65535)),
            )
        ),
        cv.Optional(CONF_POWER_CONSUMPTION, default="70uA"): cv.templatable(
            cv.All(cv.current, cv.one_of(*RADAR_ALLOWED_CUR_CONSUMPTION))
        ),
        cv.Optional(CONF_PROTECT_TIME, default="1000ms"): cv.templatable(
            cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(
                    min=core.TimePeriod(milliseconds=1),
                    max=core.TimePeriod(milliseconds=65535),
                ),
            )
        ),
        cv.Optional(CONF_TRIGGER_BASE, default="500ms"): cv.templatable(
            cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(
                    min=core.TimePeriod(milliseconds=1),
                    max=core.TimePeriod(milliseconds=65535),
                ),
            )
        ),
        cv.Optional(CONF_TRIGGER_KEEP, default="1500ms"): cv.templatable(
            cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(
                    min=core.TimePeriod(milliseconds=1),
                    max=core.TimePeriod(milliseconds=65535),
                ),
            )
        ),
        cv.Optional(CONF_STAGE_GAIN, default=3): cv.templatable(
            cv.int_range(min=0, max=12)
        ),
    }
).add_extra(
    cv.has_at_least_one_key(
        CONF_HW_FRONTEND_RESET,
        CONF_FREQUENCY,
        CONF_SENSING_DISTANCE,
    )
)


@automation.register_action(
    "at581x.settings",
    AT581XSettingsAction,
    RADAR_SETTINGS_SCHEMA,
)
async def at581x_settings_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])

    # Radar configuration
    if CONF_HW_FRONTEND_RESET in config:
        cg.add(var.set_hw_frontend_reset(1))

    if CONF_FREQUENCY in config:
        cg.add(var.set_frequency(config[CONF_FREQUENCY]))

    if CONF_SENSING_DISTANCE in config:
        cg.add(var.set_sensing_distance(config[CONF_SENSING_DISTANCE]))

    if CONF_POWERON_SELFCHECK_TIME in config:
        cg.add(var.set_poweron_selfcheck_time(config[CONF_POWERON_SELFCHECK_TIME]))

    if CONF_PROTECT_TIME in config:
        cg.add(var.set_protect_time(config[CONF_PROTECT_TIME]))

    if CONF_TRIGGER_BASE in config:
        cg.add(var.set_trigger_base(config[CONF_TRIGGER_BASE]))

    if CONF_TRIGGER_KEEP in config:
        cg.add(var.set_trigger_keep(config[CONF_TRIGGER_KEEP]))

    if CONF_STAGE_GAIN in config:
        cg.add(var.set_stage_gain(config[CONF_STAGE_GAIN]))

    if CONF_POWER_CONSUMPTION in config:
        cg.add(var.set_power_consumption(config[CONF_POWER_CONSUMPTION]))

    return var
