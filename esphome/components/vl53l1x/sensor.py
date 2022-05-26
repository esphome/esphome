import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import (
    STATE_CLASS_MEASUREMENT,
    ICON_ARROW_EXPAND_VERTICAL,
    CONF_ADDRESS,
    CONF_ENABLE_PIN,
    CONF_OFFSET,
    UNIT_METER,
)
from esphome import pins

DEPENDENCIES = ["i2c"]


VL53L1X_I2C_ADDR = 0x29
DEFAULT_CONF_IO_2V8 = False
DEFAULT_POLLING_TIME = "60s"
DEFAULT_CONF_LONG_RANGE = True
DEFAULT_CONF_TIMING_BUDGET = 100
DEFAULT_CONF_OFFSET = 0


vl53l1x_ns = cg.esphome_ns.namespace("vl53l1x")
VL53L1XSensor = vl53l1x_ns.class_(
    "VL53L1XSensor", sensor.Sensor, cg.PollingComponent, i2c.I2CDevice
)

CONF_IO_2V8 = "io_2v8"
CONF_GPIO1_PIN = "irq_pin"
CONF_LONG_RANGE = "long_range"
CONF_TIMING_BUDGET = "timing_budget"


def check_keys(obj):
    if obj[CONF_ADDRESS] != VL53L1X_I2C_ADDR and CONF_ENABLE_PIN not in obj:
        msg = f"Address other then {hex(VL53L1X_I2C_ADDR)} requires enable_pin definition to allow sensor\r"
        msg += "re-addressing. Also if you have more then one VL53 device on the same\r"
        msg += "i2c bus, then all VL53 devices must have enable_pin defined."
        raise cv.Invalid(msg)
    return obj


def check_timing_budget(obj):
    if obj[CONF_TIMING_BUDGET] not in (15, 20, 33, 50, 100, 200, 500):
        msg = "Timing budget must be one of:\r"
        msg += "15, 20, 33, 50, 100, 200 or 500 (ms)!\r"
        raise cv.Invalid(msg)
    return obj


# Valid timing budgets: 15, 20, 33, 50, 100, 200 and 500ms!
CONFIG_SCHEMA = cv.All(
    sensor.sensor_schema(
        VL53L1XSensor,
        unit_of_measurement=UNIT_METER,
        icon=ICON_ARROW_EXPAND_VERTICAL,
        accuracy_decimals=3,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(
        {
            cv.Optional(CONF_ENABLE_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_GPIO1_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_IO_2V8, default=DEFAULT_CONF_IO_2V8): cv.boolean,
            cv.Optional(CONF_LONG_RANGE, default=DEFAULT_CONF_LONG_RANGE): cv.boolean,
            cv.Optional(
                CONF_TIMING_BUDGET, default=DEFAULT_CONF_TIMING_BUDGET
            ): cv.uint16_t,
            cv.Optional(CONF_OFFSET, default=DEFAULT_CONF_OFFSET): cv.int_,
        }
    )
    .extend(cv.polling_component_schema(DEFAULT_POLLING_TIME))
    .extend(i2c.i2c_device_schema(VL53L1X_I2C_ADDR)),
    check_keys,
    check_timing_budget,
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)

    if CONF_ENABLE_PIN in config:
        enable = await cg.gpio_pin_expression(config[CONF_ENABLE_PIN])
        cg.add(var.set_enable_pin(enable))

    if CONF_GPIO1_PIN in config:
        irq = await cg.gpio_pin_expression(config[CONF_GPIO1_PIN])
        cg.add(var.set_irg_pin(irq))

    cg.add(var.set_io_2v8(config[CONF_IO_2V8]))
    cg.add(var.set_long_range(config[CONF_LONG_RANGE]))
    cg.add(var.set_timing_budget(config[CONF_TIMING_BUDGET]))
    cg.add(var.set_offset(config[CONF_OFFSET]))

    await i2c.register_i2c_device(var, config)
