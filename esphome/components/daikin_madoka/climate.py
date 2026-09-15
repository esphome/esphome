import esphome.codegen as cg
from esphome.components import ble_client, climate
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@Petapton"]
DEPENDENCIES = ["ble_client"]

daikin_madoka_ns = cg.esphome_ns.namespace("daikin_madoka")
DaikinMadoka = daikin_madoka_ns.class_(
    "DaikinMadoka", climate.Climate, ble_client.BLEClientNode, cg.PollingComponent
)

CONFIG_SCHEMA = (
    climate.climate_schema(DaikinMadoka)
    .extend(ble_client.BLE_CLIENT_SCHEMA)
    .extend(cv.polling_component_schema("10s"))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await climate.register_climate(var, config)
    await ble_client.register_ble_node(var, config)
