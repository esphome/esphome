from esphome import automation, core
from esphome.automation import maybe_simple_id
import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.const import CONF_FREQUENCY, CONF_ID
from esphome.types import ConfigType

CODEOWNERS = ["@X-Ryl669"]
DEPENDENCIES = ["i2c"]
MULTI_CONF = True


at581x_ns = cg.esphome_ns.namespace("at581x")
AT581XComponent = at581x_ns.class_("AT581XComponent", cg.Component, i2c.I2CDevice)


CONF_AT581X_ID = "at581x_id"


CONF_SENSING_DISTANCE = "sensing_distance"
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
    }
)

CONFIG_SCHEMA = cv.All(
    CONFIG_SCHEMA.extend(i2c.i2c_device_schema(0x28)).extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)


# Actions


automation.register_apply_action(
    "at581x.reset",
    maybe_simple_id({cv.Required(CONF_ID): cv.use_id(AT581XComponent)}),
    automation.ApplyCall("reset_hardware_frontend()"),
)


def _megahertz(value: float) -> int:
    return int(value / 1000000)


def _microamps(value: float) -> int:
    return int(value * 1000000)


RADAR_SETTINGS_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.use_id(AT581XComponent),
        cv.Optional(CONF_HW_FRONTEND_RESET): cv.templatable(cv.boolean),
        cv.Optional(CONF_FREQUENCY, default="5800MHz"): cv.templatable(
            cv.All(cv.frequency, cv.one_of(*RADAR_ALLOWED_FREQ), _megahertz)
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
            cv.All(cv.current, cv.one_of(*RADAR_ALLOWED_CUR_CONSUMPTION), _microamps)
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


# i2c_write_config() must follow the setters: it flushes the staged values.
automation.register_apply_action(
    "at581x.settings",
    RADAR_SETTINGS_SCHEMA,
    automation.ApplyField(CONF_FREQUENCY, "set_frequency", cg.int_),
    automation.ApplyField(CONF_SENSING_DISTANCE, "set_sensing_distance", cg.int_),
    automation.ApplyField(
        CONF_POWERON_SELFCHECK_TIME, "set_poweron_selfcheck_time", cg.int_
    ),
    automation.ApplyField(CONF_POWER_CONSUMPTION, "set_power_consumption", cg.int_),
    automation.ApplyField(CONF_PROTECT_TIME, "set_protect_time", cg.int_),
    automation.ApplyField(CONF_TRIGGER_BASE, "set_trigger_base", cg.int_),
    automation.ApplyField(CONF_TRIGGER_KEEP, "set_trigger_keep", cg.int_),
    automation.ApplyField(CONF_STAGE_GAIN, "set_stage_gain", cg.int_),
    automation.ApplyCall("i2c_write_config()"),
    automation.ApplyField(
        CONF_HW_FRONTEND_RESET, "reset_hardware_frontend_if", cg.bool_
    ),
)
