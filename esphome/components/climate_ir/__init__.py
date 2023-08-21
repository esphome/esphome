import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import (
    climate,
    remote_transmitter,
    remote_receiver,
    sensor,
    remote_base,
)
from esphome.components.remote_base import CONF_RECEIVER_ID, CONF_TRANSMITTER_ID
from esphome.const import CONF_SUPPORTS_COOL, CONF_SUPPORTS_HEAT, CONF_SENSOR

AUTO_LOAD = ["sensor", "remote_base"]
CODEOWNERS = ["@glmnet"]

climate_ir_ns = cg.esphome_ns.namespace("climate_ir")
ClimateIR = climate_ir_ns.class_(
    "ClimateIR", climate.Climate, cg.Component, remote_base.RemoteReceiverListener
)

CLIMATE_IR_SCHEMA = climate.CLIMATE_SCHEMA.extend(
    {
        cv.GenerateID(CONF_TRANSMITTER_ID): cv.use_id(
            remote_transmitter.RemoteTransmitterComponent
        ),
        cv.Optional(CONF_SUPPORTS_COOL, default=True): cv.boolean,
        cv.Optional(CONF_SUPPORTS_HEAT, default=True): cv.boolean,
        cv.Optional(CONF_SENSOR): cv.use_id(sensor.Sensor),
    }
).extend(cv.COMPONENT_SCHEMA)

CLIMATE_IR_WITH_RECEIVER_SCHEMA = CLIMATE_IR_SCHEMA.extend(
    {
        cv.Optional(CONF_RECEIVER_ID): cv.use_id(
            remote_receiver.RemoteReceiverComponent
        ),
    }
)


async def register_climate_ir(var, config):
    await cg.register_component(var, config)
    await climate.register_climate(var, config)

    cg.add(var.set_supports_cool(config[CONF_SUPPORTS_COOL]))
    cg.add(var.set_supports_heat(config[CONF_SUPPORTS_HEAT]))
    if sensor_id := config.get(CONF_SENSOR):
        sens = await cg.get_variable(sensor_id)
        cg.add(var.set_sensor(sens))
    if receiver_id := config.get(CONF_RECEIVER_ID):
        receiver = await cg.get_variable(receiver_id)
        cg.add(receiver.register_listener(var))

    transmitter = await cg.get_variable(config[CONF_TRANSMITTER_ID])
    cg.add(var.set_transmitter(transmitter))
