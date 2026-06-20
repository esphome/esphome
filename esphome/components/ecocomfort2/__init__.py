import esphome.codegen as cg
from esphome.components import ble_client, fan
import esphome.config_validation as cv
from esphome.const import CONF_ID

from .const import CONF_ECOCOMFORT2_ID

CODEOWNERS = ["@gledian"]
DEPENDENCIES = ["ble_client"]
MULTI_CONF = True

ecocomfort2_ns = cg.esphome_ns.namespace("ecocomfort2")
Ecocomfort2Hub = ecocomfort2_ns.class_(
    "Ecocomfort2Hub", ble_client.BLEClientNode, cg.PollingComponent
)
Ecocomfort2Fan = ecocomfort2_ns.class_("Ecocomfort2Fan", fan.Fan, cg.Component)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Ecocomfort2Hub),
        }
    )
    .extend(ble_client.BLE_CLIENT_SCHEMA)
    .extend(cv.polling_component_schema("30s"))
)

ECOCOMFORT2_CLIENT_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ECOCOMFORT2_ID): cv.use_id(Ecocomfort2Hub),
    }
)


async def register_ecocomfort2_child(var, config):
    parent = await cg.get_variable(config[CONF_ECOCOMFORT2_ID])
    cg.add(parent.register_child(var))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await ble_client.register_ble_node(var, config)
