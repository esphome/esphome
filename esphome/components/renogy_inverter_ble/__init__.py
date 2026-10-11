import esphome.codegen as cg
from esphome.components import ble_client
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.types import ConfigType

AUTO_LOAD = ["sensor"]
CODEOWNERS = ["@emilioaray-dev"]
DEPENDENCIES = ["ble_client"]
DOMAIN = "renogy_inverter_ble"
MULTI_CONF = True

CONF_RENOGY_INVERTER_BLE_ID = "renogy_inverter_ble_id"

renogy_inverter_ble_ns = cg.esphome_ns.namespace(DOMAIN)
RenogyInverterBle = renogy_inverter_ble_ns.class_(
    "RenogyInverterBle", ble_client.BLEClientNode, cg.PollingComponent
)

RENOGY_INVERTER_BLE_COMPONENT_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_RENOGY_INVERTER_BLE_ID): cv.use_id(RenogyInverterBle),
    }
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(RenogyInverterBle),
        }
    )
    .extend(ble_client.BLE_CLIENT_SCHEMA)
    .extend(cv.polling_component_schema("30s"))
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await ble_client.register_ble_node(var, config)
