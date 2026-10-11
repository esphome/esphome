import esphome.codegen as cg
from esphome.components import ble_client
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

DOMAIN = "ecocomfort2"
CONF_ECOCOMFORT2_ID = "ecocomfort2_id"

CODEOWNERS = ["@gledian"]
# Pairing needs esp32_ble: io_capability: none, auth_req_mode: sc_bond, max_key_size: 16
DEPENDENCIES = ["ble_client"]
MULTI_CONF = True

ecocomfort2_ns = cg.esphome_ns.namespace(DOMAIN)
Ecocomfort2Hub = ecocomfort2_ns.class_(
    "Ecocomfort2Hub", ble_client.BLEClientNode, cg.PollingComponent
)

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

_request_child_slot = cg.slot_counter("ECOCOMFORT2_CHILD_COUNT")


async def register_ecocomfort2_child(var: MockObj, config: ConfigType) -> None:
    parent = await cg.get_variable(config[CONF_ECOCOMFORT2_ID])
    _request_child_slot(str(parent))
    cg.add(parent.register_child(var))


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await ble_client.register_ble_node(var, config)
