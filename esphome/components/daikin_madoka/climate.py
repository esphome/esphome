import esphome.codegen as cg
from esphome.components import ble_client, climate, esp32_ble
import esphome.config_validation as cv
from esphome.const import CONF_ID
import esphome.final_validate as fv

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


def _final_validate(config) -> None:
    # Pairing uses numeric comparison (NC_REQ), which requires the controller to
    # expose a display. The esp32_ble default ("none") cannot satisfy the MITM
    # encryption request and fails silently at runtime, so enforce the capability
    # at config time.
    full_config = fv.full_config.get()
    ble_config = full_config.get("esp32_ble", {})
    io_capability = ble_config.get(esp32_ble.CONF_IO_CAPABILITY, "none")
    if io_capability != "display_yes_no":
        raise cv.Invalid(
            "daikin_madoka requires MITM-capable BLE pairing (numeric comparison), "
            f"set 'esp32_ble' 'io_capability: display_yes_no' (found '{io_capability}')"
        )


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await climate.register_climate(var, config)
    await ble_client.register_ble_node(var, config)
