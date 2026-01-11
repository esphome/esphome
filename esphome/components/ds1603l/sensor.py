import esphome.codegen as cg
from esphome.components import sensor, uart
import esphome.config_validation as cv
from esphome.const import CONF_ID, ICON_EMPTY, UNIT_EMPTY

DEPENDENCIES = ["uart"]
ds1603l_ns = cg.esphome_ns.namespace("ds1603l")
Ds1603l = ds1603l_ns.class_("Ds1603l", cg.PollingComponent, uart.UARTDevice)

CONF_LIQUID_LEVEL = "liquid_level"
CONF_LIQUID_VOLUME = "liquid_volume"
CONF_MIN_VOLUME = "min_volume"
CONF_MAX_VOLUME = "max_volume"
CONF_MIN_LEVEL = "min_level"
CONF_MAX_LEVEL = "max_level"

# Configuration schema for ds1603l
CONFIG_SCHEMA = uart.UART_DEVICE_SCHEMA.extend(

    {
        cv.GenerateID(): cv.declare_id(Ds1603l),
        cv.Optional(CONF_LIQUID_LEVEL): sensor.sensor_schema(
            unit_of_measurement=UNIT_EMPTY,  # Unit is customizable in YAML
            icon=ICON_EMPTY,                # Icon is customizable in YAML
            accuracy_decimals=0,            # Default to 0 decimal place
        ),
        cv.Optional(CONF_LIQUID_VOLUME): sensor.sensor_schema(
            unit_of_measurement=UNIT_EMPTY,  # Unit is customizable in YAML
            icon=ICON_EMPTY,                # Icon is customizable in YAML
            accuracy_decimals=0,            # Default to 0 decimal place
        ),
        cv.Optional(CONF_MIN_VOLUME, default=0.0): cv.float_,
        cv.Optional(CONF_MAX_VOLUME, default=1000.0): cv.float_,
        cv.Optional(CONF_MIN_LEVEL, default=0.0): cv.float_,
        cv.Optional(CONF_MAX_LEVEL, default=1000.0): cv.float_,
    }
).extend(cv.polling_component_schema("60s"))  # Default update interval is 60s

# Generate the C++ code from YAML
async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])  # Create a new DS1603L instance
    await cg.register_component(var, config)  # Register the component
    await uart.register_uart_device(var, config)  # Register UART communication

    if CONF_LIQUID_LEVEL in config:
        sens = await sensor.new_sensor(config[CONF_LIQUID_LEVEL])
        cg.add(var.set_liquid_level_sensor(sens))
    if CONF_LIQUID_VOLUME in config:
        sens = await sensor.new_sensor(config[CONF_LIQUID_VOLUME])
        cg.add(var.set_liquid_volume_sensor(sens))
    if CONF_MIN_VOLUME in config:
        cg.add(var.set_min_volume(config[CONF_MIN_VOLUME]))
    if CONF_MAX_VOLUME in config:
        cg.add(var.set_max_volume(config[CONF_MAX_VOLUME]))
    if CONF_MIN_LEVEL in config:
        cg.add(var.set_min_level(config[CONF_MIN_LEVEL]))
    if CONF_MAX_LEVEL in config:
        cg.add(var.set_max_level(config[CONF_MAX_LEVEL]))
