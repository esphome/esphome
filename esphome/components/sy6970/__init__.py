import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.const import CONF_ID
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
# it on its own interval (independent of update_interval) while it is
# enabled, so "40S" (the chip's power-on default) is safe to leave running
# rather than disabling it, and any update_interval - including `never` -
# stays valid.
I2C_WATCHDOG_TIMEOUTS = {
    "DISABLED": I2CWatchdogTimeout.I2C_WATCHDOG_DISABLED,
    "40S": I2CWatchdogTimeout.I2C_WATCHDOG_40S,
    "80S": I2CWatchdogTimeout.I2C_WATCHDOG_80S,
    "160S": I2CWatchdogTimeout.I2C_WATCHDOG_160S,
}

CONFIG_SCHEMA = (
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
    .extend(i2c.i2c_device_schema(0x6A))
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
