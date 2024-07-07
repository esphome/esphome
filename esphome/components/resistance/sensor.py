import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, resistance_sampler
from esphome.const import (
    CONF_REFERENCE_VOLTAGE,
    CONF_SENSOR,
    STATE_CLASS_MEASUREMENT,
    UNIT_OHM,
    ICON_FLASH,
)

AUTO_LOAD = ["resistance_sampler"]

resistance_ns = cg.esphome_ns.namespace("resistance")
ResistanceSensor = resistance_ns.class_(
    "ResistanceSensor",
    cg.Component,
    sensor.Sensor,
    resistance_sampler.ResistanceSampler,
)

CONF_CONFIGURATION = "configuration"
CONF_RESISTOR = "resistor"

ResistanceConfiguration = resistance_ns.enum("ResistanceConfiguration")
CONFIGURATIONS = {
    "DOWNSTREAM": ResistanceConfiguration.DOWNSTREAM,
    "UPSTREAM": ResistanceConfiguration.UPSTREAM,
}

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        ResistanceSensor,
        unit_of_measurement=UNIT_OHM,
        icon=ICON_FLASH,
        accuracy_decimals=1,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(
        {
            cv.Required(CONF_SENSOR): cv.use_id(sensor.Sensor),
            cv.Required(CONF_CONFIGURATION): cv.enum(CONFIGURATIONS, upper=True),
            cv.Required(CONF_RESISTOR): cv.resistance,
            cv.Optional(CONF_REFERENCE_VOLTAGE, default="3.3V"): cv.voltage,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)

    sens = await cg.get_variable(config[CONF_SENSOR])
    cg.add(var.set_sensor(sens))
    cg.add(var.set_configuration(config[CONF_CONFIGURATION]))
    cg.add(var.set_resistor(config[CONF_RESISTOR]))
    cg.add(var.set_reference_voltage(config[CONF_REFERENCE_VOLTAGE]))
