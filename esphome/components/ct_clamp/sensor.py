import esphome.codegen as cg
from esphome.components import sensor, voltage_sampler
import esphome.config_validation as cv
from esphome.const import CONF_SENSOR
from esphome.types import ConfigType

AUTO_LOAD = ["voltage_sampler"]
CODEOWNERS = ["@jesserockz"]

CONF_SAMPLE_DURATION = "sample_duration"

ct_clamp_ns = cg.esphome_ns.namespace("ct_clamp")
CTClampSensor = ct_clamp_ns.class_("CTClampSensor", sensor.Sensor, cg.PollingComponent)

CONFIG_SCHEMA = (
    sensor.sensor_schema(CTClampSensor, kind=sensor.CURRENT, accuracy_decimals=2)
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


async def to_code(config: ConfigType) -> None:
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)

    await cg.add_variable(var.set_source, config[CONF_SENSOR])
    cg.add(var.set_sample_duration(config[CONF_SAMPLE_DURATION]))
