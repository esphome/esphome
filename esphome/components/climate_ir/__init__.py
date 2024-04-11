import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate, sensor, remote_base
from esphome.const import CONF_SUPPORTS_COOL, CONF_SUPPORTS_HEAT, CONF_SENSOR

DEPENDENCIES = ["remote_transmitter"]
AUTO_LOAD = ["sensor", "remote_base"]
CODEOWNERS = ["@glmnet"]

climate_ir_ns = cg.esphome_ns.namespace("climate_ir")
ClimateIR = climate_ir_ns.class_(
    "ClimateIR",
    climate.Climate,
    cg.Component,
    remote_base.RemoteReceiverListener,
    remote_base.RemoteTransmittable,
)

CLIMATE_IR_SCHEMA = (
    climate.CLIMATE_SCHEMA.extend(
        {
            cv.Optional(CONF_SUPPORTS_COOL, default=True): cv.boolean,
            cv.Optional(CONF_SUPPORTS_HEAT, default=True): cv.boolean,
            cv.Optional(CONF_SENSOR): cv.use_id(sensor.Sensor),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(remote_base.REMOTE_TRANSMITTABLE_SCHEMA)
)

CLIMATE_IR_WITH_RECEIVER_SCHEMA = CLIMATE_IR_SCHEMA.extend(
    {
        cv.Optional(remote_base.CONF_RECEIVER_ID): cv.use_id(
            remote_base.RemoteReceiverBase
        ),
    }
)


async def register_climate_ir(var, config):
    await cg.register_component(var, config)
    await climate.register_climate(var, config)
    await remote_base.register_transmittable(var, config)
    cg.add(var.set_supports_cool(config[CONF_SUPPORTS_COOL]))
    cg.add(var.set_supports_heat(config[CONF_SUPPORTS_HEAT]))
    if remote_base.CONF_RECEIVER_ID in config:
        await remote_base.register_listener(var, config)
    if sensor_id := config.get(CONF_SENSOR):
        sens = await cg.get_variable(sensor_id)
        cg.add(var.set_sensor(sens))
