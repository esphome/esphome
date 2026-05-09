import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL, CONF_TYPE, UNIT_PERCENT, STATE_CLASS_MEASUREMENT

from .. import ROJAFLEX_DEVICE_SCHEMA, RojaflexDevice, register_rojaflex_device, rojaflex_ns

DEPENDENCIES = ["rojaflex"]

SENSOR_TYPES = {
    "motor_pct": "MOTOR_PCT",
}

RojaflexSensor = rojaflex_ns.class_("RojaflexSensor", sensor.Sensor, cg.PollingComponent, RojaflexDevice)
RojaflexSensorType = rojaflex_ns.enum("RojaflexSensorType", is_class=True)

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        RojaflexSensor,
        unit_of_measurement=UNIT_PERCENT,
        accuracy_decimals=0,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(ROJAFLEX_DEVICE_SCHEMA)
    .extend(
        {
            cv.Required(CONF_TYPE): cv.enum(SENSOR_TYPES, lower=True),
            cv.Required(CONF_CHANNEL): cv.int_range(min=0, max=15),
        }
    )
    .extend(cv.polling_component_schema("2s"))
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    await register_rojaflex_device(var, config)
    cg.add(var.set_sensor_type(getattr(RojaflexSensorType, SENSOR_TYPES[config[CONF_TYPE]])))
    cg.add(var.set_channel(config[CONF_CHANNEL]))
