import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ENABLE_ON_BOOT, CONF_ID
from esphome.types import ConfigType

DEPENDENCIES = ["rp2"]
CODEOWNERS = ["@bdraco"]

rp2040_ble_ns = cg.esphome_ns.namespace("rp2040_ble")
RP2040BLE = rp2040_ble_ns.class_("RP2040BLE", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(RP2040BLE),
        cv.Optional(CONF_ENABLE_ON_BOOT, default=True): cv.boolean,
    }
).extend(cv.COMPONENT_SCHEMA)


def _validate_board(config: ConfigType) -> ConfigType:
    from esphome.components.rp2 import board_has_wifi, get_board

    if not board_has_wifi():
        raise cv.Invalid(
            f"Board '{get_board()}' does not have Bluetooth support (no CYW43 wireless "
            f"chip). Use a board like 'rpipicow' or 'rpipico2w'."
        )
    return config


FINAL_VALIDATE_SCHEMA = _validate_board


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_enable_on_boot(config[CONF_ENABLE_ON_BOOT]))

    # Enable Bluetooth in the arduino-pico build
    # This switches linking from liblwip.a to liblwip-bt.a and defines
    # ENABLE_CLASSIC, ENABLE_BLE, CYW43_ENABLE_BLUETOOTH
    cg.add_build_flag("-DPIO_FRAMEWORK_ARDUINO_ENABLE_BLUETOOTH")

    cg.add_define("USE_RP2040_BLE")
