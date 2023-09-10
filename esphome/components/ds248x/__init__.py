import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.const import (
    CONF_ID,
    CONF_SLEEP_PIN,
    CONF_DS248X_ID,
    CONF_ADDRESS,
    CONF_INDEX,
)
from esphome.components import i2c, sensor

MULTI_CONF = True
AUTO_LOAD = ["sensor"]
DEPENDENCIES = ["i2c"]

CONF_BUS_SLEEP = "bus_sleep"
CONF_HUB_SLEEP = "hub_sleep"
CONF_ACTIVE_PULLUP = "active_pullup"
CONF_STRONG_PULLUP = "strong_pullup"
CONF_CHANNEL_COUNT = "channel_count"
CONF_CHANNEL = "channel"

ds248x_ns = cg.esphome_ns.namespace("ds248x")
DS248x1Wire = ds248x_ns.class_("DS248x1Wire")
DS248xComponent = ds248x_ns.class_(
    "DS248xComponent", cg.PollingComponent, i2c.I2CDevice, DS248x1Wire
)
DS248xSensor = ds248x_ns.class_("DS248xSensor", sensor.Sensor)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(DS248xComponent),
            cv.Optional(CONF_SLEEP_PIN): pins.internal_gpio_output_pin_schema,
            cv.Optional(CONF_BUS_SLEEP, default=False): cv.boolean,
            cv.Optional(CONF_HUB_SLEEP, default=False): cv.boolean,
            cv.Optional(CONF_ACTIVE_PULLUP, default=False): cv.boolean,
            cv.Optional(CONF_STRONG_PULLUP, default=False): cv.boolean,
            cv.Optional(CONF_CHANNEL_COUNT, default=1): cv.uint8_t,
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x18))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_bus_sleep(config[CONF_BUS_SLEEP]))
    cg.add(var.set_hub_sleep(config[CONF_HUB_SLEEP]))
    cg.add(var.set_active_pullup(config[CONF_ACTIVE_PULLUP]))
    cg.add(var.set_strong_pullup(config[CONF_STRONG_PULLUP]))
    cg.add(var.set_channel_count(config[CONF_CHANNEL_COUNT]))

    if CONF_SLEEP_PIN in config:
        pin = await cg.gpio_pin_expression(config[CONF_SLEEP_PIN])
        cg.add(var.set_sleep_pin(pin))


async def register_ds248x_sensor(var, config):
    """Register an 1wire device connected to the ds248x with the given config.

    Sets the ds248x to use and the address and index.

    This is a coroutine, you need to await it with a 'yield' expression!
    """
    parent = await cg.get_variable(config[CONF_DS248X_ID])
    cg.add(var.set_parent(parent))
    if CONF_ADDRESS in config:
        cg.add(var.set_address(config[CONF_ADDRESS]))
    else:
        cg.add(var.set_index(config[CONF_INDEX]))

    cg.add(parent.register_sensor(var))


def ds248x_sensor_schema():
    """Create a schema for a ds248x sensor.

    :return: The ds248x device schema, `extend` this in your config schema.
    """
    schema = {
        cv.GenerateID(CONF_DS248X_ID): cv.use_id(DS248x1Wire),
        cv.Optional(CONF_ADDRESS): cv.hex_int,
        cv.Optional(CONF_INDEX): cv.positive_int,
        cv.Optional(CONF_CHANNEL): cv.positive_int,
    }

    # TODO check if channel lower then CONF[CHANNEL_COUNT]
    # TODO cv.has_exactly_one_key(CONF_ADDRESS, CONF_INDEX)
    return cv.Schema(schema)
