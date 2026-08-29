import esphome.codegen as cg
from esphome.components.nrf52.boards import BOOTLOADER_CONFIG
from esphome.components.ota import (
    BASE_OTA_SCHEMA,
    SWAP_METHOD_SCHEMA,
    OTAComponent,
    ota_to_code,
)
from esphome.components.zephyr import (
    KEY_BOARD,
    VARIANTS,
    HexValue,
    mcuboot,
    zephyr_add_cdc_acm,
    zephyr_add_overlay,
    zephyr_add_prj_conf,
    zephyr_add_sysbuild_conf,
    zephyr_data,
    zephyr_variant,
    zephyr_variant_family,
)
from esphome.components.zephyr.const import (
    BOOTLOADER_MCUBOOT,
    KEY_BOOTLOADER,
    KEY_FRAMEWORK_TYPE,
    ZEPHYR_VARIANT_EFR32MG24,
    ZEPHYR_VARIANT_NRF52,
    ZEPHYR_VARIANT_NRF54L15,
    ZEPHYR_VARIANT_NRF54LM20A,
    ZEPHYR_VARIANT_RP2040,
    ZEPHYR_VARIANT_RP2350,
)
import esphome.config_validation as cv
from esphome.const import CONF_HARDWARE_UART, CONF_ID, KEY_CORE, KEY_FRAMEWORK_VERSION
from esphome.core import CORE, coroutine_with_priority
from esphome.coroutine import CoroPriority
from esphome.types import ConfigType

CODEOWNERS = ["@tomaszduda23"]
DEPENDENCIES = ["zephyr"]


def AUTO_LOAD() -> list[str]:
    # Legacy platform: nrf52 doesn't touch the shared ota component's own backend
    # file at all -- this OTAComponent is entirely separate. platform: zephyr does:
    # it always compiles ota_backend_zephyr.cpp regardless of which ota: platform
    # is selected (see ota's own AUTO_LOAD()), and that backend needs sha256 (NCS's
    # PSA crypto drivers, and mainline's default crypto backend, can't do MD5).
    return ["sha256"] if CORE.is_zephyr else []


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


# Platform: zephyr variants this MCUboot image-manager path has been validated
# against. platform: zephyr on other variants has its own hardened OTA path
# instead, ota_backend_zephyr.cpp, reached via platform: esphome.
ZEPHYR_VARIANTS = (
    ZEPHYR_VARIANT_NRF52,
    ZEPHYR_VARIANT_NRF54L15,
    ZEPHYR_VARIANT_NRF54LM20A,
    ZEPHYR_VARIANT_EFR32MG24,
    ZEPHYR_VARIANT_RP2040,
    ZEPHYR_VARIANT_RP2350,
)

# Families whose boards actually have a USB peripheral this CDC-ACM transport can
# use -- nordic (native USB) and legacy platform: nrf52. EFR32MG24's xg24_ek2703a
# has no USB device controller node at all (board.yaml doesn't list "usb"), so
# CDC/CDC1 would fail with an opaque "undefined node label 'zephyr_udc0'"
# devicetree error instead of a clear config-time one -- same rule logger's own
# hardware_uart: already applies (UART_SELECTION_ZEPHYR_USB_CDC is the only zephyr
# family list that includes USB_CDC).
_CDC_CAPABLE_FAMILIES = {"nordic", "rpi_pico"}

CDC_IDS = {"CDC": 0, "CDC1": 1}
UARTS = ("CDC", "CDC1", "UART0", "UART1")


def _validate_platform(conf: ConfigType) -> ConfigType:
    # Two ways to end up on a supported chip: the legacy platform: nrf52
    # component, or platform: zephyr with one of ZEPHYR_VARIANTS (a separate,
    # mainline/NCS-based port).
    if not (CORE.is_nrf52 or (CORE.is_zephyr and zephyr_variant() in ZEPHYR_VARIANTS)):
        raise cv.Invalid(
            "This feature is only available on nrf52 (platform: nrf52, or "
            "platform: zephyr with variant: nrf52, nrf54l15, nrf54lm20a, efr32mg24, "
            "rp2040, or rp2350)."
        )
    hw_uart = conf[CONF_TRANSPORT].get(CONF_HARDWARE_UART)
    if hw_uart in CDC_IDS and not (
        CORE.is_nrf52 or zephyr_variant_family() in _CDC_CAPABLE_FAMILIES
    ):
        raise cv.Invalid(
            f"'{hw_uart}' is not available on this variant -- it has no USB "
            f"peripheral. Use 'UART0'/'UART1', or 'ble: true', instead.",
            [CONF_TRANSPORT, CONF_HARDWARE_UART],
        )
    return conf


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
            **SWAP_METHOD_SCHEMA,
        }
    )
    .extend(BASE_OTA_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA),
    _validate_transport,
    _validate_platform,
    mcuboot.validate_swap_method,
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

    mcuboot.apply_swap_method(config)

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
    zephyr_add_sysbuild_conf("BOOTLOADER_MCUBOOT", True)

    zephyr_add_prj_conf("MCUMGR_MGMT_NOTIFICATION_HOOKS", True)
    zephyr_add_prj_conf("MCUMGR_GRP_IMG_STATUS_HOOKS", True)
    zephyr_add_prj_conf("MCUMGR_GRP_IMG_UPLOAD_CHECK_HOOK", True)
    transport = config[CONF_TRANSPORT]
    if transport[CONF_BLE]:
        zephyr_add_prj_conf("MCUMGR_TRANSPORT_BT", True)
        zephyr_add_prj_conf("MCUMGR_TRANSPORT_BT_REASSEMBLY", True)

        zephyr_add_prj_conf("MCUMGR_GRP_OS", True)
        zephyr_add_prj_conf("MCUMGR_GRP_OS_MCUMGR_PARAMS", True)

        # NCS-only sample Kconfig (undefined in mainline Zephyr -- EFR32MG24 is the
        # first supported variant that isn't NCS-based, and mainline treats an
        # undefined-symbol assignment as a fatal Kconfig warning).
        if zephyr_data().get(KEY_FRAMEWORK_TYPE) == "ncs":
            zephyr_add_prj_conf("NCS_SAMPLE_MCUMGR_BT_OTA_DFU_SPEEDUP", True)
    if CONF_HARDWARE_UART in transport:
        hw_uart = transport[CONF_HARDWARE_UART]
        if hw_uart in CDC_IDS:
            # zephyr_add_cdc_acm() reuses a board-provided cdc-acm-uart node (e.g.
            # Nordic's common cdc_acm_serial.dtsi, which several boards -- including
            # xiao_ble -- already include) instead of declaring a new cdc_acm_uart{id}
            # node, whenever one is already present. Its return value is the label
            # that actually ended up in the devicetree; a hardcoded cdc_acm_uart{id}
            # name may not have been declared at all when that happens, so it can't
            # be used directly below or the devicetree reference is left dangling.
            uart_name = zephyr_add_cdc_acm(config, CDC_IDS[hw_uart])
        elif CORE.is_zephyr:
            # Physical UART peripherals are numbered/named differently per variant
            # (e.g. nRF54 uses uart20/uart30, EFR32MG24 has only usart0), and some
            # variants declare no portable mapping at all (resolved per board from
            # DTS instead) -- see resolve_uart_node_label()'s own docstring.
            from esphome.components.zephyr.dts_lookup import resolve_uart_node_label

            uart_name = resolve_uart_node_label(
                zephyr_data()[KEY_BOARD],
                hw_uart,
                VARIANTS[zephyr_variant()].uart_node_labels,
            )
        else:
            # Legacy platform: nrf52 always uses uart0/uart1.
            uart_name = hw_uart.lower()
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
