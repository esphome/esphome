from esphome import pins
import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_SLEEP_PIN, CONF_TYPE

CODEOWNERS = ["@tomwellnitz"]
MULTI_CONF = True
DEPENDENCIES = ["i2c"]

CONF_DS248X_ID = "ds248x_id"
CONF_BUS_SLEEP = "bus_sleep"
CONF_HUB_SLEEP = "hub_sleep"
CONF_ACTIVE_PULLUP = "active_pullup"

CONF_RESET_LOW_TIME = "reset_low_time"
CONF_MASTER_SAMPLE_TIME = "master_sample_time"
CONF_WRITE_0_LOW_TIME = "write_0_low_time"
CONF_RECOVERY_TIME = "recovery_time"
CONF_ACTIVE_PULLUP_RESISTANCE = "active_pullup_resistance"

TYPE_DS2482_100 = "ds2482-100"
TYPE_DS2482_101 = "ds2482-101"
TYPE_DS2482_800 = "ds2482-800"
TYPE_DS2484 = "ds2484"

CHANNEL_COUNTS = {
    TYPE_DS2482_100: 1,
    TYPE_DS2482_101: 1,
    TYPE_DS2482_800: 8,
    TYPE_DS2484: 1,
}

ds248x_ns = cg.esphome_ns.namespace("ds248x")
DS248xComponent = ds248x_ns.class_("DS248xComponent", cg.Component, i2c.I2CDevice)


def _component_schema(*extras):
    schema = cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(DS248xComponent),
            cv.Optional(CONF_ACTIVE_PULLUP, default=False): cv.boolean,
        }
    )
    for extra in extras:
        schema = schema.extend(extra)
    return schema.extend(cv.COMPONENT_SCHEMA).extend(i2c.i2c_device_schema(0x18))


SLEEP_SCHEMA = {
    cv.Optional(CONF_SLEEP_PIN): pins.internal_gpio_output_pin_schema,
    cv.Optional(CONF_BUS_SLEEP, default=False): cv.boolean,
    cv.Optional(CONF_HUB_SLEEP, default=False): cv.boolean,
}

DS2484_SCHEMA = {
    cv.Optional(CONF_RESET_LOW_TIME): cv.int_range(min=0, max=15),
    cv.Optional(CONF_MASTER_SAMPLE_TIME): cv.int_range(min=0, max=15),
    cv.Optional(CONF_WRITE_0_LOW_TIME): cv.int_range(min=0, max=15),
    cv.Optional(CONF_RECOVERY_TIME): cv.int_range(min=0, max=15),
    cv.Optional(CONF_ACTIVE_PULLUP_RESISTANCE): cv.enum(
        {
            # DS2484 Table 7: value codes 0-5 map to 500 ohm, 6-15 map to 1000 ohm.
            "500ohm": 0,
            "1000ohm": 6,
        }
    ),
}

CONFIG_SCHEMA = cv.typed_schema(
    {
        TYPE_DS2482_100: _component_schema(),
        TYPE_DS2482_101: _component_schema(SLEEP_SCHEMA),
        TYPE_DS2482_800: _component_schema(),
        TYPE_DS2484: _component_schema(SLEEP_SCHEMA, DS2484_SCHEMA),
    },
    key=CONF_TYPE,
    lower=True,
)


def get_channel_count(config):
    return CHANNEL_COUNTS[config[CONF_TYPE]]


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_active_pullup(config[CONF_ACTIVE_PULLUP]))
    cg.add(var.set_channel_count(get_channel_count(config)))

    if CONF_BUS_SLEEP in config:
        cg.add(var.set_bus_sleep(config[CONF_BUS_SLEEP]))
    if CONF_HUB_SLEEP in config:
        cg.add(var.set_hub_sleep(config[CONF_HUB_SLEEP]))

    if CONF_RESET_LOW_TIME in config:
        cg.add(var.set_val_trstl(config[CONF_RESET_LOW_TIME]))
    if CONF_MASTER_SAMPLE_TIME in config:
        cg.add(var.set_val_tmsp(config[CONF_MASTER_SAMPLE_TIME]))
    if CONF_WRITE_0_LOW_TIME in config:
        cg.add(var.set_val_tw0l(config[CONF_WRITE_0_LOW_TIME]))
    if CONF_RECOVERY_TIME in config:
        cg.add(var.set_val_trec0(config[CONF_RECOVERY_TIME]))
    if CONF_ACTIVE_PULLUP_RESISTANCE in config:
        cg.add(var.set_val_rwpu(config[CONF_ACTIVE_PULLUP_RESISTANCE]))

    if CONF_SLEEP_PIN in config:
        pin = await cg.gpio_pin_expression(config[CONF_SLEEP_PIN])
        cg.add(var.set_sleep_pin(pin))
