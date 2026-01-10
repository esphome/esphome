import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart, sensor
from esphome.const import CONF_ID, UNIT_EMPTY, ICON_EMPTY

CODEOWNERS = ["@JakeLC15"]
# With a big thanks to [akis]
# Version 2026.1.0
DEPENDENCIES = ["uart"]
DS1603L_ns = cg.esphome_ns.namespace("DS1603L")
DS1603L = DS1603L_ns.class_("DS1603L", cg.PollingComponent, uart.UARTDevice)

CONF_LEVEL = "liquid_level"
CONF_VOLUME = "liquid_volume"
CONF_MINVOLUME = "min_volume"
CONF_MAXVOLUME = "max_volume"
CONF_MINLEVEL = "min_level"
CONF_MAXLEVEL = "max_level"

# Configuration schema for DS1603L
CONFIG_SCHEMA = uart.UART_DEVICE_SCHEMA.extend(
    
    {
        cv.GenerateID(): cv.declare_id(DS1603L),
        cv.Optional(CONF_LEVEL): sensor.sensor_schema(
            unit_of_measurement=UNIT_EMPTY,  # Unit is customizable in YAML
            icon=ICON_EMPTY,                # Icon is customizable in YAML
            accuracy_decimals=0,            # Default to 0 decimal place
        ),
        cv.Optional(CONF_VOLUME): sensor.sensor_schema(
            unit_of_measurement=UNIT_EMPTY,  # Unit is customizable in YAML
            icon=ICON_EMPTY,                # Icon is customizable in YAML
            accuracy_decimals=0,            # Default to 0 decimal place
        ),
        cv.Optional(CONF_MINVOLUME, default=0.0): cv.float_,
        cv.Optional(CONF_MAXVOLUME, default=1000.0): cv.float_,
        cv.Optional(CONF_MINLEVEL, default=0.0): cv.float_,
        cv.Optional(CONF_MAXLEVEL, default=1000.0): cv.float_,
    }
).extend(cv.polling_component_schema("60s"))  # Default update interval is 60s

# Generate the C++ code from YAML
async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])  # Create a new DS1603L instance
    await cg.register_component(var, config)  # Register the component
    await uart.register_uart_device(var, config)  # Register UART communication
    
    if CONF_LEVEL in config: 
        sens = await sensor.new_sensor(config[CONF_LEVEL]) 
        cg.add(var.set_liquid_level_sensor(sens))
    if CONF_VOLUME in config: 
        sens = await sensor.new_sensor(config[CONF_VOLUME]) 
        cg.add(var.set_liquid_volume_sensor(sens))
    if CONF_MINVOLUME in config: 
        cg.add(var.set_min_volume(config[CONF_MINVOLUME]))
    if CONF_MAXVOLUME in config: 
        cg.add(var.set_max_volume(config[CONF_MAXVOLUME]))
    if CONF_MINLEVEL in config: 
        cg.add(var.set_min_level(config[CONF_MINLEVEL]))
    if CONF_MAXLEVEL in config: 
        cg.add(var.set_max_level(config[CONF_MAXLEVEL]))
    
    
