import esphome.codegen as cg
from esphome.components import ble_client, genibus
import esphome.config_validation as cv
from esphome.types import ConfigType

AUTO_LOAD = ["genibus"]
CODEOWNERS = ["@tomwellnitz"]
DEPENDENCIES = ["ble_client"]
DOMAIN = "genibus_ble"
MULTI_CONF = True

genibus_ble_ns = cg.esphome_ns.namespace("genibus_ble")
GenibusBLE = genibus_ble_ns.class_(
    "GenibusBLE", genibus.GenibusBus, ble_client.BLEClientNode
)

# 0xF8 is the master address the Grundfos GO app uses
CONFIG_SCHEMA = (
    genibus.genibus_bus_schema(0xF8)
    .extend({cv.GenerateID(): cv.declare_id(GenibusBLE)})
    .extend(ble_client.BLE_CLIENT_SCHEMA)
)


async def to_code(config: ConfigType) -> None:
    var = await ble_client.new_ble_node_component(config)
    genibus.apply_genibus_bus_config(var, config)
