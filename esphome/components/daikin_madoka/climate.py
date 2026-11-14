import esphome.codegen as cg
from esphome.components import ble_client, climate, esp32_ble
import esphome.config_validation as cv
import esphome.final_validate as fv
from esphome.types import ConfigType

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

# IO capabilities that can confirm a numeric comparison pairing
_NUMERIC_COMPARISON_CAPABILITIES = ("display_yes_no", "keyboard_display")


def _final_validate(config: ConfigType) -> ConfigType:
    # Pairing uses numeric comparison (NC_REQ). The esp32_ble default ("none")
    # cannot satisfy the MITM encryption request and fails silently at runtime.
    io_capability = fv.full_config.get()[esp32_ble.DOMAIN][esp32_ble.CONF_IO_CAPABILITY]
    if io_capability not in _NUMERIC_COMPARISON_CAPABILITIES:
        raise cv.Invalid(
            "daikin_madoka requires MITM-capable BLE pairing (numeric comparison), "
            "set 'esp32_ble' 'io_capability' to 'display_yes_no' or 'keyboard_display' "
            f"(found '{io_capability}')"
        )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    var = await climate.new_climate(config)
    await cg.register_component(var, config)
    await ble_client.register_ble_node(var, config)
