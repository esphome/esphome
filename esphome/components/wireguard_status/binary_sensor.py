import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor, wireguard
from esphome.const import DEVICE_CLASS_CONNECTIVITY

CONF_WIREGUARD_ID = "wireguard_id"

DEPENDENCIES = ["wireguard"]
CODEOWNERS = ["@droscy"]

wireguard_status_ns = cg.esphome_ns.namespace("wireguard_status")

WireguardStatus = wireguard_status_ns.class_(
    "WireguardStatus",
    binary_sensor.BinarySensor,
    cg.PollingComponent,
)

CONFIG_SCHEMA = (
    binary_sensor.binary_sensor_schema(
        WireguardStatus,
        device_class=DEVICE_CLASS_CONNECTIVITY,
    )
    .extend(
        {
            cv.GenerateID(CONF_WIREGUARD_ID): cv.use_id(wireguard.Wireguard),
        }
    )
    .extend(cv.polling_component_schema("10s"))
)


async def to_code(config):
    var = await binary_sensor.new_binary_sensor(config)
    await cg.register_component(var, config)
    cg.add(var.set_wireguard(await cg.get_variable(config[CONF_WIREGUARD_ID])))
