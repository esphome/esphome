import esphome.codegen as cg
from esphome.components import sensor, voltage_sampler
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL, CONF_ID

from .. import ADC128S102, adc128s102_ns

AUTO_LOAD = ["voltage_sampler"]
DEPENDENCIES = ["adc128s102"]

ADC128S102Sensor = adc128s102_ns.class_(
    "ADC128S102Sensor",
    sensor.Sensor,
    cg.PollingComponent,
    voltage_sampler.VoltageSampler,
)
CONF_ADC128S102_ID = "adc128s102_id"

CONFIG_SCHEMA = (
    sensor.sensor_schema(ADC128S102Sensor)
    .extend(
        {
            cv.GenerateID(CONF_ADC128S102_ID): cv.use_id(ADC128S102),
            cv.Required(CONF_CHANNEL): cv.int_range(min=0, max=7),
        }
    )
    .extend(cv.polling_component_schema("60s"))
)


async def to_code(config):
    var = cg.new_Pvariable(
        config[CONF_ID],
        config[CONF_CHANNEL],
    )
    await cg.register_parented(var, config[CONF_ADC128S102_ID])
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)
