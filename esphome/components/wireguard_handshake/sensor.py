import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, wireguard
from esphome.const import (
    DEVICE_CLASS_TIMESTAMP,
    ENTITY_CATEGORY_DIAGNOSTIC,
)

CONF_WIREGUARD_ID = "wireguard_id"

DEPENDENCIES = ["wireguard"]
CODEOWNERS = ["@droscy"]

wireguard_handshake_ns = cg.esphome_ns.namespace("wireguard_handshake")

WireguardHandshake = wireguard_handshake_ns.class_(
    "WireguardHandshake",
    sensor.Sensor,
    cg.PollingComponent,
)

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        WireguardHandshake,
        device_class=DEVICE_CLASS_TIMESTAMP,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    )
    .extend(
        {
            cv.GenerateID(CONF_WIREGUARD_ID): cv.use_id(wireguard.Wireguard),
        }
    )
    .extend(cv.polling_component_schema("60s"))
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    cg.add(var.set_wireguard(await cg.get_variable(config[CONF_WIREGUARD_ID])))
