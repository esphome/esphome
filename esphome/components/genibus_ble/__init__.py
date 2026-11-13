import esphome.codegen as cg
from esphome.components import ble_client, genibus
import esphome.config_validation as cv
import esphome.final_validate as fv
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


def _final_validate(config: ConfigType) -> ConfigType:
    # Two buses on one connection would both take every reply of the pump
    client_id = config[ble_client.CONF_BLE_CLIENT_ID]
    users = [
        conf
        for conf in fv.full_config.get().get(DOMAIN, [])
        if conf[ble_client.CONF_BLE_CLIENT_ID] == client_id
    ]
    if len(users) > 1:
        raise cv.Invalid(
            f"BLE client '{client_id}' is used by more than one {DOMAIN}; "
            "give each pump its own ble_client",
            path=[ble_client.CONF_BLE_CLIENT_ID],
        )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    var = await ble_client.new_ble_node_component(config)
    genibus.apply_genibus_bus_config(var, config)
