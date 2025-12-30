from esphome import pins
import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_SLEEP_PIN

CODEOWNERS = ["@tomwellnitz"]
MULTI_CONF = True
DEPENDENCIES = ["i2c"]

CONF_DS248X_ID = "ds248x_id"
CONF_BUS_SLEEP = "bus_sleep"
CONF_HUB_SLEEP = "hub_sleep"
CONF_ACTIVE_PULLUP = "active_pullup"
CONF_STRONG_PULLUP = "strong_pullup"
CONF_OVERDRIVE_SPEED = "overdrive_speed"
CONF_CHANNEL_COUNT = "channel_count"

CONF_DS2484_RESET_LOW_TIME = "ds2484_reset_low_time"
CONF_DS2484_MASTER_SAMPLE_TIME = "ds2484_master_sample_time"
CONF_DS2484_WRITE_0_LOW_TIME = "ds2484_write_0_low_time"
CONF_DS2484_RECOVERY_TIME = "ds2484_recovery_time"
CONF_DS2484_ACTIVE_PULLUP_RESISTANCE = "ds2484_active_pullup_resistance"

ds248x_ns = cg.esphome_ns.namespace("ds248x")
DS248xComponent = ds248x_ns.class_("DS248xComponent", cg.Component, i2c.I2CDevice)


def validate_ds2484_config(config):
    if config[CONF_CHANNEL_COUNT] != 1:
        ds2484_keys = [
            CONF_DS2484_RESET_LOW_TIME,
            CONF_DS2484_MASTER_SAMPLE_TIME,
            CONF_DS2484_WRITE_0_LOW_TIME,
            CONF_DS2484_RECOVERY_TIME,
            CONF_DS2484_ACTIVE_PULLUP_RESISTANCE,
        ]
        for key in ds2484_keys:
            if key in config:
                raise cv.Invalid(
                    f"{key} is only available for single-channel devices (channel_count: 1)"
                )
    return config


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(DS248xComponent),
            cv.Optional(CONF_SLEEP_PIN): pins.internal_gpio_output_pin_schema,
            cv.Optional(CONF_BUS_SLEEP, default=False): cv.boolean,
            cv.Optional(CONF_HUB_SLEEP, default=False): cv.boolean,
            cv.Optional(CONF_ACTIVE_PULLUP, default=False): cv.boolean,
            cv.Optional(CONF_STRONG_PULLUP, default=False): cv.boolean,
            cv.Optional(CONF_OVERDRIVE_SPEED, default=False): cv.boolean,
            cv.Optional(CONF_CHANNEL_COUNT, default=1): cv.one_of(1, 8, int=True),
            cv.Optional(CONF_DS2484_RESET_LOW_TIME): cv.int_range(min=0, max=15),
            cv.Optional(CONF_DS2484_MASTER_SAMPLE_TIME): cv.int_range(min=0, max=15),
            cv.Optional(CONF_DS2484_WRITE_0_LOW_TIME): cv.int_range(min=0, max=15),
            cv.Optional(CONF_DS2484_RECOVERY_TIME): cv.int_range(min=0, max=15),
            cv.Optional(CONF_DS2484_ACTIVE_PULLUP_RESISTANCE): cv.enum(
                {
                    "1000ohm": 0,
                    "500ohm": 1,
                }
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x18))
    .add_extra(validate_ds2484_config)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_active_pullup(config[CONF_ACTIVE_PULLUP]))
    cg.add(var.set_strong_pullup(config[CONF_STRONG_PULLUP]))
    cg.add(var.set_overdrive_speed(config[CONF_OVERDRIVE_SPEED]))
    cg.add(var.set_bus_sleep(config[CONF_BUS_SLEEP]))
    cg.add(var.set_hub_sleep(config[CONF_HUB_SLEEP]))
    cg.add(var.set_channel_count(config[CONF_CHANNEL_COUNT]))

    if CONF_DS2484_RESET_LOW_TIME in config:
        cg.add(var.set_val_trstl(config[CONF_DS2484_RESET_LOW_TIME]))
    if CONF_DS2484_MASTER_SAMPLE_TIME in config:
        cg.add(var.set_val_tmsp(config[CONF_DS2484_MASTER_SAMPLE_TIME]))
    if CONF_DS2484_WRITE_0_LOW_TIME in config:
        cg.add(var.set_val_tw0l(config[CONF_DS2484_WRITE_0_LOW_TIME]))
    if CONF_DS2484_RECOVERY_TIME in config:
        cg.add(var.set_val_trec0(config[CONF_DS2484_RECOVERY_TIME]))
    if CONF_DS2484_ACTIVE_PULLUP_RESISTANCE in config:
        cg.add(var.set_val_rwpu(config[CONF_DS2484_ACTIVE_PULLUP_RESISTANCE]))

    if CONF_SLEEP_PIN in config:
        pin = await cg.gpio_pin_expression(config[CONF_SLEEP_PIN])
        cg.add(var.set_sleep_pin(pin))
