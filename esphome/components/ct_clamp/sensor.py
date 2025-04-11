import esphome.codegen as cg
from esphome.components import sensor, voltage_sampler
import esphome.config_validation as cv
from esphome.const import (
    CONF_SENSOR,
    DEVICE_CLASS_CURRENT,
    STATE_CLASS_MEASUREMENT,
    UNIT_AMPERE,
)

AUTO_LOAD = ["voltage_sampler"]
CODEOWNERS = ["@jesserockz"]

CONF_SAMPLE_DURATION = "sample_duration"

ct_clamp_ns = cg.esphome_ns.namespace("ct_clamp")
CTClampSensor = ct_clamp_ns.class_("CTClampSensor", sensor.Sensor, cg.PollingComponent)

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        CTClampSensor,
        unit_of_measurement=UNIT_AMPERE,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_CURRENT,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(
        {
            cv.Required(CONF_SENSOR): cv.use_id(voltage_sampler.VoltageSampler),
            cv.Optional(
                CONF_SAMPLE_DURATION, default="200ms"
            ): cv.positive_time_period_milliseconds,
        }
    )
    .extend(cv.polling_component_schema("60s"))
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)

    sens = await cg.get_variable(config[CONF_SENSOR])
    cg.add(var.set_source(sens))
    cg.add(var.set_sample_duration(config[CONF_SAMPLE_DURATION]))
