import esphome.codegen as cg
from esphome.components import ble_client, esp32_ble, time
from esphome.components.esp32_ble import (
    CONF_AUTH_REQ_MODE,
    CONF_IO_CAPABILITY,
    CONF_MAX_KEY_SIZE,
)
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_TIME_ID
from esphome.cpp_generator import MockObj
import esphome.final_validate as fv
from esphome.types import ConfigType

DOMAIN = "ecocomfort2"
CONF_ECOCOMFORT2_ID = "ecocomfort2_id"

CODEOWNERS = ["@gledian"]
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
            cv.Optional(CONF_TIME_ID): cv.use_id(time.RealTimeClock),
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
    if (time_id := config.get(CONF_TIME_ID)) is not None:
        cg.add(var.set_time(await cg.get_variable(time_id)))


def _final_validate(config: ConfigType) -> None:
    # The unit only talks over an encrypted, bonded link
    ble_conf = fv.full_config.get()[esp32_ble.DOMAIN]
    if (
        ble_conf[CONF_IO_CAPABILITY] != "none"
        or ble_conf.get(CONF_AUTH_REQ_MODE) != "sc_bond"
        or ble_conf.get(CONF_MAX_KEY_SIZE) != 16
    ):
        raise cv.Invalid(
            "ecocomfort2 needs esp32_ble configured with io_capability: none, "
            "auth_req_mode: sc_bond and max_key_size: 16"
        )


FINAL_VALIDATE_SCHEMA = _final_validate
