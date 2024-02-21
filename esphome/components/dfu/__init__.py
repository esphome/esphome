import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import output
from esphome.const import (
    CONF_ID,
)

from esphome.core import CORE
from esphome.components.zephyr import zephyr_add_prj_conf
import esphome.final_validate as fv

dfu_ns = cg.esphome_ns.namespace("dfu")
DeviceFirmwareUpdate = dfu_ns.class_("DeviceFirmwareUpdate", cg.Component)

CONF_RESET_OUTPUT = "reset_output"

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(DeviceFirmwareUpdate),
            cv.Required(CONF_RESET_OUTPUT): cv.use_id(output.BinaryOutput),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_on_nrf52,
)


# TODO move this to common place with OTA
def _validate_mcumgr(config):
    if CORE.using_zephyr:
        fconf = fv.full_config.get()
        try:
            bootloader = fconf.get_config_for_path(["nrf52", "bootloader"])
            if bootloader != "adafruit":
                raise cv.Invalid(f"'{bootloader}' bootloader does not support DFU")
        except KeyError:
            pass


FINAL_VALIDATE_SCHEMA = _validate_mcumgr


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    reset_output = await cg.get_variable(config[CONF_RESET_OUTPUT])
    cg.add(var.set_reset_output(reset_output))
    zephyr_add_prj_conf("CDC_ACM_DTE_RATE_CALLBACK_SUPPORT", True)
    await cg.register_component(var, config)
