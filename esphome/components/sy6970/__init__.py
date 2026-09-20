import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_UPDATE_INTERVAL
from esphome.types import ConfigType

CODEOWNERS = ["@linkedupbits"]
DEPENDENCIES = ["i2c"]
MULTI_CONF = True

CONF_SY6970_ID = "sy6970_id"
CONF_ENABLE_STATUS_LED = "enable_status_led"
CONF_INPUT_CURRENT_LIMIT = "input_current_limit"
CONF_CHARGE_VOLTAGE = "charge_voltage"
CONF_CHARGE_CURRENT = "charge_current"
CONF_PRECHARGE_CURRENT = "precharge_current"
CONF_CHARGE_ENABLED = "charge_enabled"
CONF_ENABLE_ADC = "enable_adc"
CONF_I2C_WATCHDOG_TIMEOUT = "i2c_watchdog_timeout"

sy6970_ns = cg.esphome_ns.namespace("sy6970")
SY6970Component = sy6970_ns.class_(
    "SY6970Component", cg.PollingComponent, i2c.I2CDevice
)
SY6970Listener = sy6970_ns.class_("SY6970Listener")
I2CWatchdogTimeout = sy6970_ns.enum("I2CWatchdogTimeout")

# The chip's own I2C watchdog (REG07[5:4]) reverts charge_enabled and the
# STAT LED setting back to power-on defaults once it elapses; ESPHome kicks
# it automatically on every update() while it is enabled, so "40S" (the
# chip's power-on default) is safe to leave running rather than disabling it.
I2C_WATCHDOG_TIMEOUTS = {
    "DISABLED": I2CWatchdogTimeout.I2C_WATCHDOG_DISABLED,
    "40S": I2CWatchdogTimeout.I2C_WATCHDOG_40S,
    "80S": I2CWatchdogTimeout.I2C_WATCHDOG_80S,
    "160S": I2CWatchdogTimeout.I2C_WATCHDOG_160S,
}
I2C_WATCHDOG_TIMEOUT_SECONDS = {
    "DISABLED": None,
    "40S": 40,
    "80S": 80,
    "160S": 160,
}


def _validate_watchdog_vs_update_interval(config: ConfigType) -> ConfigType:
    timeout_seconds = I2C_WATCHDOG_TIMEOUT_SECONDS[config[CONF_I2C_WATCHDOG_TIMEOUT]]
    if timeout_seconds is None:
        return config

    update_interval = config[CONF_UPDATE_INTERVAL]
    if not hasattr(update_interval, "total_seconds"):
        # e.g. `update_interval: never` - nothing will ever kick the watchdog.
        raise cv.Invalid(
            f"`{CONF_I2C_WATCHDOG_TIMEOUT}` requires a numeric `{CONF_UPDATE_INTERVAL}` "
            "so the watchdog can be kicked on every update; disable the watchdog "
            f"({CONF_I2C_WATCHDOG_TIMEOUT}: DISABLED) if that is not possible."
        )
    if update_interval.total_seconds >= timeout_seconds:
        raise cv.Invalid(
            f"`{CONF_UPDATE_INTERVAL}` ({update_interval.total_seconds}s) must be shorter "
            f"than `{CONF_I2C_WATCHDOG_TIMEOUT}` ({timeout_seconds}s), or the watchdog will "
            "time out between updates and revert charge_enabled/the STAT LED setting."
        )
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SY6970Component),
            cv.Optional(CONF_ENABLE_STATUS_LED, default=True): cv.boolean,
            cv.Optional(CONF_INPUT_CURRENT_LIMIT, default=500): cv.int_range(
                min=100, max=3200
            ),
            cv.Optional(CONF_CHARGE_VOLTAGE, default=4208): cv.int_range(
                min=3840, max=4608
            ),
            cv.Optional(CONF_CHARGE_CURRENT, default=2048): cv.int_range(
                min=0, max=5056
            ),
            cv.Optional(CONF_PRECHARGE_CURRENT, default=128): cv.int_range(
                min=64, max=1024
            ),
            cv.Optional(CONF_CHARGE_ENABLED, default=True): cv.boolean,
            cv.Optional(CONF_ENABLE_ADC, default=True): cv.boolean,
            cv.Optional(CONF_I2C_WATCHDOG_TIMEOUT, default="40S"): cv.enum(
                I2C_WATCHDOG_TIMEOUTS, upper=True
            ),
        }
    )
    .extend(cv.polling_component_schema("5s"))
    .extend(i2c.i2c_device_schema(0x6A)),
    _validate_watchdog_vs_update_interval,
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(
        config[CONF_ID],
        config[CONF_ENABLE_STATUS_LED],
        config[CONF_INPUT_CURRENT_LIMIT],
        config[CONF_CHARGE_VOLTAGE],
        config[CONF_CHARGE_CURRENT],
        config[CONF_PRECHARGE_CURRENT],
        config[CONF_CHARGE_ENABLED],
        config[CONF_ENABLE_ADC],
        config[CONF_I2C_WATCHDOG_TIMEOUT],
    )
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
