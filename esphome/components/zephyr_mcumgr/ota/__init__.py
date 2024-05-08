import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components.ota import BASE_OTA_SCHEMA, ota_to_code
from esphome.const import (
    CONF_ID,
    CONF_NUM_ATTEMPTS,
    CONF_OTA,
    CONF_REBOOT_TIMEOUT,
)
from esphome.core import CORE, coroutine_with_priority
import esphome.final_validate as fv
from esphome.components.zephyr.const import BOOTLOADER_MCUBOOT
from esphome.components.zephyr import zephyr_add_prj_conf

DEPENDENCIES = ["zephyr_ble_server"]
AUTO_LOAD = ["zephyr_mcumgr"]

esphome = cg.esphome_ns.namespace("esphome")
ZephyrMcumgrOTAComponent = cg.esphome_ns.namespace("zephyr_mcumgr").class_(
    "OTAComponent", cg.Component
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ZephyrMcumgrOTAComponent),
            cv.Optional(
                CONF_REBOOT_TIMEOUT, default="5min"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_NUM_ATTEMPTS, default="10"): cv.positive_not_null_int,
        }
    )
    .extend(BASE_OTA_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


def _validate_mcumgr(config):
    if CORE.using_zephyr:
        fconf = fv.full_config.get()
        try:
            bootloader = fconf.get_config_for_path(["nrf52", "bootloader"])
            if bootloader != BOOTLOADER_MCUBOOT:
                raise cv.Invalid(f"'{bootloader}' bootloader does not support OTA")
        except KeyError:
            pass


FINAL_VALIDATE_SCHEMA = _validate_mcumgr

# TODO cdc ota, check if ble server is enabled


@coroutine_with_priority(50.0)
async def to_code(config):
    CORE.data[CONF_OTA] = {}

    var = cg.new_Pvariable(config[CONF_ID])
    await ota_to_code(var, config)
    cg.add_define("USE_OTA")

    await cg.register_component(var, config)
    # mcumgr begin
    zephyr_add_prj_conf("NET_BUF", True)
    zephyr_add_prj_conf("ZCBOR", True)
    zephyr_add_prj_conf("MCUMGR", True)

    zephyr_add_prj_conf("MCUMGR_GRP_IMG", True)

    zephyr_add_prj_conf("IMG_MANAGER", True)
    zephyr_add_prj_conf("STREAM_FLASH", True)
    zephyr_add_prj_conf("FLASH_MAP", True)
    zephyr_add_prj_conf("FLASH", True)

    zephyr_add_prj_conf("BOOTLOADER_MCUBOOT", True)

    zephyr_add_prj_conf("MCUMGR_MGMT_NOTIFICATION_HOOKS", True)
    zephyr_add_prj_conf("MCUMGR_GRP_IMG_STATUS_HOOKS", True)
    zephyr_add_prj_conf("MCUMGR_GRP_IMG_UPLOAD_CHECK_HOOK", True)
    # mcumgr ble
    zephyr_add_prj_conf("MCUMGR_TRANSPORT_BT", True)
    zephyr_add_prj_conf("MCUMGR_TRANSPORT_BT_REASSEMBLY", True)

    zephyr_add_prj_conf("MCUMGR_GRP_OS", True)
    zephyr_add_prj_conf("MCUMGR_GRP_OS_MCUMGR_PARAMS", True)

    zephyr_add_prj_conf("NCS_SAMPLE_MCUMGR_BT_OTA_DFU_SPEEDUP", True)
