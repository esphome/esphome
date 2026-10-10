import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_CHANNEL,
    CONF_ID,
    DEVICE_CLASS_TEMPERATURE,
    ICON_THERMOMETER,
    UNIT_CELSIUS,
)

from .. import ADC_CHANNEL, BASE_SCHEMA, CONF_M5IOE1_ID, m5ioe1_ns

DEPENDENCIES = ["m5ioe1"]


M5IOE1ADCSensor = m5ioe1_ns.class_(
    "M5IOE1ADCSensor", sensor.Sensor, cg.PollingComponent
)

M5IOE1TemperatureSensor = m5ioe1_ns.class_(
    "M5IOE1TemperatureSensor", sensor.Sensor, cg.PollingComponent
)

TEMPERATURE_SCHEMA = (
    sensor.sensor_schema(
        M5IOE1TemperatureSensor,
        unit_of_measurement=UNIT_CELSIUS,
        device_class=DEVICE_CLASS_TEMPERATURE,
        icon=ICON_THERMOMETER,
    )
    .extend(BASE_SCHEMA)
    .extend(cv.polling_component_schema("60s"))
)

ADC_SCHEMA = (
    sensor.sensor_schema(M5IOE1ADCSensor)
    .extend({cv.Required(CONF_CHANNEL): cv.enum(ADC_CHANNEL)})
    .extend(BASE_SCHEMA)
    .extend(cv.polling_component_schema("60s"))
)

# Either ADC or temperature config
CONFIG_SCHEMA = cv.typed_schema(
    {
        "temperature": TEMPERATURE_SCHEMA,
        "adc": ADC_SCHEMA,
    },
    key="type",
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_parented(var, config[CONF_M5IOE1_ID])
    await sensor.register_sensor(var, config)
    await cg.register_component(var, config)

    if CONF_CHANNEL in config:
        cg.add(var.set_channel(config[CONF_CHANNEL]))
