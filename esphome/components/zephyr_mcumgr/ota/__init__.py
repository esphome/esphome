import esphome.codegen as cg
from esphome.components.nrf52.boards import BOOTLOADER_CONFIG
from esphome.components.ota import BASE_OTA_SCHEMA, OTAComponent, ota_to_code
from esphome.components.zephyr import (
    HexValue,
    zephyr_add_cdc_acm,
    zephyr_add_overlay,
    zephyr_add_prj_conf,
    zephyr_data,
)
from esphome.components.zephyr.const import (
    BOOTLOADER_MCUBOOT,
    KEY_BOOTLOADER,
    KEY_SYSBUILD,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_HARDWARE_UART,
    CONF_ID,
    KEY_CORE,
    KEY_FRAMEWORK_VERSION,
    Framework,
)
from esphome.core import CORE, coroutine_with_priority
from esphome.coroutine import CoroPriority
from esphome.types import ConfigType

CODEOWNERS = ["@tomaszduda23"]
DEPENDENCIES = ["zephyr"]

ZephyrMcumgrOTAComponent = cg.esphome_ns.namespace("zephyr_mcumgr").class_(
    "OTAComponent", OTAComponent
)

CONF_BLE = "ble"
CONF_TRANSPORT = "transport"


def _validate_transport(conf: ConfigType) -> ConfigType:
    transport = conf[CONF_TRANSPORT]
    if transport[CONF_BLE] or CONF_HARDWARE_UART in transport:
        return conf
    raise cv.Invalid(
        f"At least one transport protocol has to be enabled. Set '{CONF_BLE}: true' or '{CONF_HARDWARE_UART}'"
    )


UARTS = {
    "CDC": ("cdc_acm_uart0", 0),
    "CDC1": ("cdc_acm_uart1", 1),
    "UART0": ("uart0", -1),
    "UART1": ("uart1", -1),
}


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ZephyrMcumgrOTAComponent),
            cv.Optional(CONF_TRANSPORT, default={CONF_BLE: True}): cv.Schema(
                {
                    cv.Optional(CONF_BLE, default=False): cv.boolean,
                    cv.Optional(
                        CONF_HARDWARE_UART,
                    ): cv.one_of(*UARTS, upper=True),
                }
            ),
        }
    )
    .extend(BASE_OTA_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA),
    _validate_transport,
    cv.only_with_framework(Framework.ZEPHYR),
)


KEY_ZEPHYR_BLE_SERVER = "zephyr_ble_server"


def _validate_ble_server(config: ConfigType) -> None:
    if (
        config[CONF_TRANSPORT][CONF_BLE]
        and KEY_ZEPHYR_BLE_SERVER not in CORE.loaded_integrations
    ):
        raise cv.Invalid(f"'{KEY_ZEPHYR_BLE_SERVER}' component is required for BLE OTA")


def _validate_bootloader(config: ConfigType) -> None:
    bootloader = zephyr_data()[KEY_BOOTLOADER]
    if bootloader == BOOTLOADER_MCUBOOT:
        return
    if bootloader not in BOOTLOADER_CONFIG:
        raise cv.Invalid(f"{bootloader} does not support OTA")
    framework_ver: cv.Version = CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION]
    if framework_ver < cv.Version(2, 9, 2):
        raise cv.Invalid(
            "OTA with Adafruit_nRF52_Bootloader requires at least SDK 2.9.2"
        )


def _final_validate(config: ConfigType) -> None:
    _validate_ble_server(config)
    _validate_bootloader(config)


FINAL_VALIDATE_SCHEMA = _final_validate


@coroutine_with_priority(CoroPriority.OTA_UPDATES)
async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await ota_to_code(var, config)

    await cg.register_component(var, config)

    zephyr_add_prj_conf("NET_BUF", True)
    zephyr_add_prj_conf("ZCBOR", True)
    zephyr_add_prj_conf("MCUMGR", True)

    zephyr_add_prj_conf("MCUMGR_GRP_IMG", True)

    zephyr_add_prj_conf("IMG_MANAGER", True)
    zephyr_add_prj_conf("STREAM_FLASH", True)
    zephyr_add_prj_conf("FLASH_MAP", True)
    zephyr_add_prj_conf("FLASH", True)

    zephyr_add_prj_conf("IMG_ERASE_PROGRESSIVELY", True)

    zephyr_add_prj_conf("BOOTLOADER_MCUBOOT", True)

    zephyr_add_prj_conf("MCUMGR_MGMT_NOTIFICATION_HOOKS", True)
    zephyr_add_prj_conf("MCUMGR_GRP_IMG_STATUS_HOOKS", True)
    zephyr_add_prj_conf("MCUMGR_GRP_IMG_UPLOAD_CHECK_HOOK", True)
    transport = config[CONF_TRANSPORT]
    if transport[CONF_BLE]:
        zephyr_add_prj_conf("MCUMGR_TRANSPORT_BT", True)
        zephyr_add_prj_conf("MCUMGR_TRANSPORT_BT_REASSEMBLY", True)

        zephyr_add_prj_conf("MCUMGR_GRP_OS", True)
        zephyr_add_prj_conf("MCUMGR_GRP_OS_MCUMGR_PARAMS", True)

        zephyr_add_prj_conf("NCS_SAMPLE_MCUMGR_BT_OTA_DFU_SPEEDUP", True)
    if CONF_HARDWARE_UART in transport:
        uart = UARTS[transport[CONF_HARDWARE_UART]]
        uart_name = uart[0]
        cdc_id = uart[1]
        if cdc_id >= 0:
            zephyr_add_cdc_acm(config, cdc_id)
        zephyr_add_prj_conf("MCUMGR_TRANSPORT_UART", True)
        zephyr_add_prj_conf("BASE64", True)
        zephyr_add_prj_conf("CONSOLE", True)
        zephyr_add_overlay(
            f"""
                / {{
                    chosen {{
                        zephyr,uart-mcumgr = &{uart_name};
                    }};
                }};
                """
        )
    framework_ver = CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION]
    if framework_ver >= cv.Version(2, 9, 2):
        zephyr_data()[KEY_SYSBUILD] = True

    bootloader = zephyr_data()[KEY_BOOTLOADER]
    if bootloader != BOOTLOADER_MCUBOOT:
        sections = BOOTLOADER_CONFIG[bootloader]
        # Derive partition addresses from the SoftDevice and bootloader sections so
        # that the DTS flash map matches what the Partition Manager produces:
        #   MCUboot sits immediately after the SoftDevice, then slot0, then slot1.
        mcuboot_size = 0x9000
        sd_end = next(s.address + s.size for s in sections if "SoftDevice" in s.name)
        bl_start = next(s.address for s in sections if "Adafruit" in s.name)
        slot0_start = sd_end + mcuboot_size
        # Align slot size down to a 4 KB sector boundary
        slot_size = ((bl_start - slot0_start) // 2 // 0x1000) * 0x1000
        slot1_start = slot0_start + slot_size

        def _mcuboot_partition_overlay() -> str:
            def part(name, start, size):
                return f"""
                {name}: partition@{start:x} {{
                    reg = <0x{start:x} 0x{size:x}>;
                }};"""

            return f"""
                /delete-node/ &boot_partition;
                /delete-node/ &storage_partition;
                /delete-node/ &code_partition;
                /delete-node/ &reserved_partition_0;

                &flash0 {{
                    partitions {{
                        compatible = "fixed-partitions";
                        #address-cells = <1>;
                        #size-cells = <1>;
                        {part("slot0_partition", slot0_start, slot_size)}
                        {part("slot1_partition", slot1_start, slot_size)}
                    }};
                }};
            """

        def _code_partition_overlay() -> str:
            return """
                / {
                    chosen {
                        zephyr,code-partition = &slot0_partition;
                    };
                };
                """

        zephyr_add_overlay(_mcuboot_partition_overlay())
        zephyr_add_overlay(_mcuboot_partition_overlay(), "mcuboot")
        zephyr_add_overlay(_code_partition_overlay())
        zephyr_add_overlay(_code_partition_overlay(), "mcuboot")
        # mcuboot is second bootloader. It's only task is to swap partitions.
        # recovery can be done by first bootloader. Keep it small.
        zephyr_add_overlay(
            """
                &zephyr_udc0 {
                    status = "disabled";
                };
            """,
            "mcuboot",
        )
        zephyr_add_prj_conf("USB_DEVICE_STACK", False, image="mcuboot")
        zephyr_add_prj_conf("CONSOLE", False, image="mcuboot")
        zephyr_add_prj_conf(
            "PM_PARTITION_SIZE_MCUBOOT", HexValue(mcuboot_size), image="mcuboot"
        )
