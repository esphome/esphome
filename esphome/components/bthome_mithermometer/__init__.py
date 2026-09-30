import esphome.codegen as cg
from esphome.components import ble_device_base
import esphome.config_validation as cv
from esphome.const import CONF_BINDKEY, CONF_ID, CONF_MAC_ADDRESS
from esphome.core import HexInt
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

CODEOWNERS = ["@nagyrobi"]
AUTO_LOAD = ["ble_device_base"]

CONF_REPLAY_PROTECTION = "replay_protection"


bthome_mithermometer_ns = cg.esphome_ns.namespace("bthome_mithermometer")
BTHomeMiThermometer = bthome_mithermometer_ns.class_(
    "BTHomeMiThermometer", ble_device_base.ESPBTDeviceListener, cg.Component
)


def bthome_mithermometer_base_schema(
    extra_schema: cv.Schema | dict | None = None,
) -> cv.All:
    if extra_schema is None:
        extra_schema = {}
    return cv.All(
        ble_device_base.rename_legacy_hub_id("bthome_mithermometer"),
        cv.Schema(
            {
                cv.GenerateID(CONF_ID): cv.declare_id(BTHomeMiThermometer),
                cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
                cv.Optional(CONF_BINDKEY): cv.bind_key,
                cv.Optional(CONF_REPLAY_PROTECTION, default=False): cv.boolean,
            }
        )
        .extend(cv.COMPONENT_SCHEMA)
        .extend(extra_schema)
        .extend(ble_device_base.BLE_DEVICE_SCHEMA),
        _validate_replay_protection,
    )


def _validate_replay_protection(config: ConfigType) -> ConfigType:
    if config[CONF_REPLAY_PROTECTION] and CONF_BINDKEY not in config:
        raise cv.Invalid(
            f"'{CONF_REPLAY_PROTECTION}' requires '{CONF_BINDKEY}'",
            path=[CONF_REPLAY_PROTECTION],
        )
    return config


async def setup_bthome_mithermometer(var: MockObj, config: ConfigType) -> None:
    await cg.register_component(var, config)
    await ble_device_base.register_ble_device(var, config)
    cg.add(var.set_address(config[CONF_MAC_ADDRESS].as_hex))
    if bindkey := config.get(CONF_BINDKEY):
        bindkey_bytes = [
            HexInt(int(bindkey[index : index + 2], 16))
            for index in range(0, len(bindkey), 2)
        ]
        cg.add(var.set_bindkey(cg.ArrayInitializer(*bindkey_bytes)))
    if config[CONF_REPLAY_PROTECTION]:
        cg.add(var.set_replay_protection(True))
