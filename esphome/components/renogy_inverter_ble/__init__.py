"""ESPHome component: read a Renogy inverter with built-in Bluetooth over BLE → sensors.

Tested against the Renogy PUH 12V 3000W Pure Sine Wave Inverter (UPS transfer switch, SKU
RIV1230PU-126); applies to the RNGRIU family.

The inverter speaks Modbus-over-BLE but requires a proprietary init read on characteristic
0xFFD4 before it answers (the reason a generic modbus_controller times out). This hub connects
via ble_client, performs that init, then reads holding registers 4000 (main) and 4408 (load),
publishing the values to the configured sensors.
"""

import esphome.codegen as cg
from esphome.components import ble_client
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@emilioaray-dev"]
DEPENDENCIES = ["ble_client"]
AUTO_LOAD = ["sensor"]
MULTI_CONF = True

CONF_RENOGY_INVERTER_BLE_ID = "renogy_inverter_ble_id"

renogy_inverter_ble_ns = cg.esphome_ns.namespace("renogy_inverter_ble")
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


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await ble_client.register_ble_node(var, config)
