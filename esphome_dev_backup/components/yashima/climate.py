import esphome.codegen as cg
from esphome.components import climate, remote_transmitter, sensor
from esphome.components.remote_base import CONF_TRANSMITTER_ID
import esphome.config_validation as cv
from esphome.const import CONF_SENSOR, CONF_SUPPORTS_COOL, CONF_SUPPORTS_HEAT

AUTO_LOAD = ["sensor"]

yashima_ns = cg.esphome_ns.namespace("yashima")
YashimaClimate = yashima_ns.class_("YashimaClimate", climate.Climate, cg.Component)

CONFIG_SCHEMA = cv.All(
    climate.climate_schema(YashimaClimate)
    .extend(
        {
            cv.GenerateID(CONF_TRANSMITTER_ID): cv.use_id(
                remote_transmitter.RemoteTransmitterComponent
            ),
            cv.Optional(CONF_SUPPORTS_COOL, default=True): cv.boolean,
            cv.Optional(CONF_SUPPORTS_HEAT, default=True): cv.boolean,
            cv.Optional(CONF_SENSOR): cv.use_id(sensor.Sensor),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await climate.new_climate(config)
    await cg.register_component(var, config)

    cg.add(var.set_supports_cool(config[CONF_SUPPORTS_COOL]))
    cg.add(var.set_supports_heat(config[CONF_SUPPORTS_HEAT]))
    if CONF_SENSOR in config:
        sens = await cg.get_variable(config[CONF_SENSOR])
        cg.add(var.set_sensor(sens))

    transmitter = await cg.get_variable(config[CONF_TRANSMITTER_ID])
    cg.add(var.set_transmitter(transmitter))
