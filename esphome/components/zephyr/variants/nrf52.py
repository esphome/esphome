import logging
from pathlib import Path
import subprocess

from esphome import pins
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ADVANCED,
    CONF_BOARD,
    CONF_FRAMEWORK,
    CONF_ID,
    CONF_OTA,
    CONF_RESET_PIN,
    CONF_SOURCE,
    KEY_CORE,
    KEY_FRAMEWORK_VERSION,
    ThreadModel,
    Toolchain,
)
from esphome.core import CORE, CoroPriority, EsphomeError, coroutine_with_priority
from esphome.framework_helpers import run_command_ok
from esphome.types import ConfigType

from ..const import (
    ADVANCED_SCHEMA,
    BOOTLOADER_MCUBOOT,
    CONF_BOOTLOADER,
    CONF_RUNNER,
    KEY_BOOTLOADER,
    KEY_MODULE_REQUESTS,
    ZEPHYR_VARIANT_NRF52,
    zephyr_ns,
)
from ..partitions import BootLayout
from ..usb_baud_rate import CONF_USB_BAUD_RATE, UsbBaudRate, declare_component_id
from . import (
    MAINLINE,
    NCS,
    ZephyrVariant,
    qualify_board,
    resolve_framework_version,
    set_core_data,
)

_LOGGER = logging.getLogger(__name__)

_DEFAULT_BOARD = "adafruit_feather_nrf52840"

# advanced: bootloader: -- deliberately local to this variant, not platform: nrf52's own
# BOOTLOADER_* constants (see the sdk= comment below re: issue #11 entanglement).
# BOOTLOADER_MCUBOOT (default): MCUboot is built as a sysbuild child image (see
# _bootloader_to_code).
# BOOTLOADER_ADAFRUIT_NRF52_SD140_V6 skips MCUboot entirely -- the stock board's own
# devicetree (nordic/nrf52840_partition_uf2_sdv6.dtsi, included by
# adafruit_itsybitsy_nrf52840.dts) already reserves the flash regions occupied by a
# factory-installed Adafruit bootloader + SoftDevice v6 as read-only partitions, and
# points zephyr,code-partition at the free gap between them, so the app build lands
# there without overwriting either. See:
# https://learn.adafruit.com/introducing-the-adafruit-nrf52840-feather/hathach-memory-map
BOOTLOADER_ADAFRUIT_NRF52_SD140_V6 = "adafruit_nrf52_sd140_v6"
BOOTLOADER_ADAFRUIT_NRF52_SD140_V7 = "adafruit_nrf52_sd140_v7"

# Where each SoftDevice ends (upstream nordic/nrf52840_partition_uf2_sdv{6,7}.dtsi) and
# where the Adafruit UF2 bootloader starts; the app must fit between them.
_SOFTDEVICE_END = {
    BOOTLOADER_ADAFRUIT_NRF52_SD140_V6: 0x26000,
    BOOTLOADER_ADAFRUIT_NRF52_SD140_V7: 0x27000,
}
_ADAFRUIT_BOOTLOADER_START = 0xF4000
_FLASH_SIZE = 0x100000  # nRF52840

CONF_DFU = "dfu"
DeviceFirmwareUpdate = zephyr_ns.class_("DeviceFirmwareUpdate", cg.Component)

_DFU_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): declare_component_id(DeviceFirmwareUpdate),
        cv.GenerateID(CONF_USB_BAUD_RATE): declare_component_id(UsbBaudRate),
        cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
    }
)


def _dfu_schema(value: bool | ConfigType) -> ConfigType | None:
    if isinstance(value, bool):
        return _DFU_SCHEMA({}) if value else None
    return _DFU_SCHEMA(value)


_ADVANCED_SCHEMA = ADVANCED_SCHEMA.extend(
    {
        # 1200 baud touch -> Adafruit UF2 bootloader, 2001 baud -> plain reboot.
        # Validated in config_schema() once the target platform is known (pin lookup).
        cv.Optional(CONF_DFU): cv.Any(cv.boolean, dict),
        cv.Optional(CONF_BOOTLOADER, default=BOOTLOADER_MCUBOOT): cv.one_of(
            BOOTLOADER_MCUBOOT,
            BOOTLOADER_ADAFRUIT_NRF52_SD140_V6,
            BOOTLOADER_ADAFRUIT_NRF52_SD140_V7,
            lower=True,
        ),
    }
)

# GPIO -> nRF52840 SAADC analog-input name. Fixed silicon fact (AIN0-AIN7 datasheet
# pin assignment), independently defined here rather than imported from
# esphome/components/nrf52/const.py's identical AIN_TO_GPIO -- that module belongs to
# the separate NCS-based `platform: nrf52`, and this variant is deliberately
# independent of it (see issue #11: avoiding entanglement between the two was the
# point of building this as a mainline-Zephyr variant in the first place).
# Cross-checked against this exact board's own devicetree: the vbatt divider node
# in adafruit_feather_nrf52840_common.dtsi uses `io-channels = <&adc 5>` (AIN5),
# which is GPIO 29 here -- matching Adafruit's documented P0.29 battery-sense pin.
_ADC_AIN_MAP = {
    2: "AIN0",
    3: "AIN1",
    4: "AIN2",
    5: "AIN3",
    28: "AIN4",
    29: "AIN5",
    30: "AIN6",
    31: "AIN7",
}

# nrf52840.dtsi: gpio0 (32 pins, flat 0-31) + gpio1 (ngpios=<16>, flat 32-47).
# Free-mux via NRF_PSEL -- no fixed-function subset to exclude, shared by every
# free-mux GPIO signal (UART, SPI).
_GPIO_MATRIX_PINS = frozenset(range(48))

# Registry entries — collected by variants/__init__.py
VARIANT_NAME = ZEPHYR_VARIANT_NRF52
VARIANT = ZephyrVariant(
    # Resets from the start of flash (nordic/nrf52840_partition.dtsi). An Adafruit
    # bootloader moves this; see to_code().
    boot=BootLayout(0x0),
    # NCS (nRF Connect SDK) is the default -- Nordic's own vendor SDK, which is where
    # real hardware support/testing effort for this chip is expected to concentrate.
    # Mainline Zephyr stays available as an alternate (framework: type: zephyr) for
    # anyone who wants to avoid NCS's licensing/tooling footprint, or who hit a
    # regression only present on one side.
    sdk=NCS,
    sdk_name="ncs",
    alt_sdks={"zephyr": MAINLINE},
    family="nordic",
    valid_toolchains=(Toolchain.SDK_ZEPHYR,),
    toolchain="arm-zephyr-eabi",
    # OpenThread included: issue #11's tracked entanglement was specifically about
    # platform: nrf52's NCS-based OpenThread/Zigbee stack coupling -- picking
    # framework: type: zephyr (mainline) sidesteps that entirely, the same OpenThread
    # source build the esp32-family variants already use. The default
    # framework: type: ncs here uses NCS's own OpenThread, so that original coupling
    # concern applies again there. Zigbee here is a radio-capability declaration only --
    # zigbee_zephyr.py's own gate additionally requires the "zigbee" module (NCS_ZIGBEE_TEMPLATE
    # via NCS.modules) to actually resolve, which only ncs (not mainline) offers.
    transports=frozenset({"openthread", "ble", "zigbee"}),
    soc="nrf52840",
    # No "scratch": neither board defines a scratch_partition (upstream's stock
    # nrf52840 layout never had one either), and move/offset don't need one.
    swap_methods=frozenset({"move", "offset"}),
    adc_ain_map=_ADC_AIN_MAP,
    uart_node_labels={},
    pwm_node_labels=["pwm0", "pwm1", "pwm2", "pwm3"],
    uart_valid_pins={"tx": _GPIO_MATRIX_PINS, "rx": _GPIO_MATRIX_PINS},
    spi_valid_pins={
        "clk": _GPIO_MATRIX_PINS,
        "mosi": _GPIO_MATRIX_PINS,
        "miso": _GPIO_MATRIX_PINS,
    },
)


def config_schema(config: ConfigType) -> ConfigType:
    config = dict(config)
    if CONF_BOARD not in config:
        config[CONF_BOARD] = _DEFAULT_BOARD
    config[CONF_ADVANCED] = _ADVANCED_SCHEMA(config.get(CONF_ADVANCED, {}))
    config[CONF_BOARD] = qualify_board(VARIANT, config[CONF_BOARD])
    _, framework_ver, sdk_name, _ = resolve_framework_version(
        VARIANT, "nrf52", config, "nRF52840 support"
    )
    set_core_data(
        VARIANT_NAME,
        config[CONF_BOARD],
        config[CONF_ADVANCED][CONF_BOOTLOADER],
        framework_ver,
        config,
        framework_type=sdk_name,
        sdk_source=config[CONF_FRAMEWORK].get(CONF_SOURCE),
        runner=config[CONF_ADVANCED].get(CONF_RUNNER),
    )
    # On by default with an Adafruit bootloader; `dfu: false` turns it off.
    dfu = config[CONF_ADVANCED].get(CONF_DFU)
    if dfu is None and config[CONF_ADVANCED][CONF_BOOTLOADER] != BOOTLOADER_MCUBOOT:
        dfu = True
    if dfu is not None:
        with cv.prepend_path([CONF_ADVANCED, CONF_DFU]):
            if config[CONF_ADVANCED][CONF_BOOTLOADER] == BOOTLOADER_MCUBOOT:
                raise cv.Invalid(
                    f"'{CONF_DFU}' needs an Adafruit bootloader, not '{BOOTLOADER_MCUBOOT}'"
                )
            config[CONF_ADVANCED][CONF_DFU] = _dfu_schema(dfu)
    return config


async def to_code(config: ConfigType) -> None:
    from .. import zephyr_add_prj_conf, zephyr_set_boot_layout, zephyr_setup_preferences

    cg.add_build_flag("-DUSE_ZEPHYR_VARIANT_NRF52")
    cg.add_define("ESPHOME_BOARD", config[CONF_BOARD])
    cg.add_define("ESPHOME_VARIANT", "NRF52")
    cg.add_define(ThreadModel.SINGLE)
    zephyr_setup_preferences()
    zephyr_add_prj_conf("REBOOT", True)
    zephyr_add_prj_conf("HWINFO", True)

    if (
        sd_end := _SOFTDEVICE_END.get(config[CONF_ADVANCED][CONF_BOOTLOADER])
    ) is not None:
        # The SoftDevice starts the app right after itself; writing over it or the
        # bootloader bricks the board.
        zephyr_set_boot_layout(
            BootLayout(
                sd_end, ((0x0, sd_end), (_ADAFRUIT_BOOTLOADER_START, _FLASH_SIZE))
            )
        )

    CORE.add_job(_bootloader_to_code, config)
    if dfu_config := config[CONF_ADVANCED].get(CONF_DFU):
        CORE.add_job(_dfu_to_code, dfu_config)


@coroutine_with_priority(CoroPriority.DIAGNOSTICS)
async def _dfu_to_code(dfu_config: ConfigType) -> None:
    from .. import zephyr_add_usb_baud_rate

    cg.add_define("USE_ZEPHYR_NRF52_DFU")
    usb_baud_rate = await zephyr_add_usb_baud_rate(dfu_config)
    var = cg.new_Pvariable(dfu_config[CONF_ID], usb_baud_rate)
    if (reset_pin := dfu_config.get(CONF_RESET_PIN)) is not None:
        cg.add(var.set_reset_pin(await cg.gpio_pin_expression(reset_pin)))
    await cg.register_component(var, dfu_config)


@coroutine_with_priority(CoroPriority.FINAL)
async def _bootloader_to_code(config: ConfigType) -> None:
    from .. import zephyr_add_sysbuild_conf, zephyr_data

    # ncs-zigbee's default devicetree has no boot/slot1 partitions, so zigbee only
    # gets MCUboot when OTA is actually configured. Deferred to FINAL priority: this
    # variant's own to_code() (above) always runs before zigbee:'s (zigbee depends on
    # zephyr), so checking the module request there would be premature -- by FINAL,
    # zigbee_zephyr.py's request_zephyr_module("zigbee") call has already run if
    # zigbee: is configured.
    # TBD - single slot MCUBOOT
    if (
        "zigbee" not in zephyr_data()[KEY_MODULE_REQUESTS] or CORE.config.get(CONF_OTA)
    ) and zephyr_data()[KEY_BOOTLOADER] == BOOTLOADER_MCUBOOT:
        # west build always runs with --sysbuild (build_zephyr.py), but sysbuild
        # still needs to be told which bootloader to build as its "mcuboot" child
        # image -- without this, only the (unsigned) app image gets built and
        # flashed.
        zephyr_add_sysbuild_conf("BOOTLOADER_MCUBOOT", True)
        # sysbuild's own BOOT_SIGNATURE_TYPE choice overrides a per-image setting.
        zephyr_add_sysbuild_conf("BOOT_SIGNATURE_TYPE_ECDSA_P256", True)


# Host side of `advanced: dfu:`: Adafruit bootloaders take a DFU package over USB serial,
# so no drive has to be mounted. Installed on first use, only for this variant.
_NRFUTIL_REQUIREMENT = (
    "adafruit-nrfutil @ git+https://github.com/adafruit/Adafruit_nRF52_nrfutil.git"
    "@7fdfe15feee5f304fb7d9b031721dcefa1f72b58"
)
# (dev-type, sd-req) per bootloader, from Nordic SoftDevice release notes.
_GENPKG_PARAMS = {
    BOOTLOADER_ADAFRUIT_NRF52_SD140_V6: ("0x0052", "0x00B6"),
    BOOTLOADER_ADAFRUIT_NRF52_SD140_V7: ("0x0052", "0x00CA"),
}
_DFU_PACKAGE = "firmware.zip"


def uses_serial_dfu() -> bool:
    from .. import zephyr_data

    return zephyr_data()[KEY_BOOTLOADER] in _GENPKG_PARAMS


def _ensure_nrfutil(python_bin: Path, env: dict) -> None:
    probe = subprocess.run(
        [str(python_bin), "-c", "import nordicsemi"], capture_output=True, check=False
    )
    if probe.returncode == 0:
        return
    _LOGGER.info("Installing adafruit-nrfutil")
    if not run_command_ok(
        [str(python_bin), "-m", "pip", "install", _NRFUTIL_REQUIREMENT], env=env
    ):
        raise EsphomeError("Can't install adafruit-nrfutil")


def build_dfu_package(python_bin: Path, env: dict) -> None:
    """Make firmware.zip from the application hex, for the Adafruit serial bootloader."""
    from .. import zephyr_data

    build_dir = CORE.relative_build_path(".west_build")
    hex_file = build_dir / "zephyr" / "zephyr" / "zephyr.hex"
    if not hex_file.is_file():
        raise EsphomeError(
            f"{hex_file} not found; cannot build the DFU package for this framework"
        )
    _ensure_nrfutil(python_bin, env)
    dev_type, sd_req = _GENPKG_PARAMS[zephyr_data()[KEY_BOOTLOADER]]
    if not run_command_ok(
        [
            str(python_bin),
            "-m",
            "nordicsemi.__main__",
            "dfu",
            "genpkg",
            "--dev-type",
            dev_type,
            "--sd-req",
            sd_req,
            "--application",
            str(hex_file),
            str(CORE.relative_build_path(_DFU_PACKAGE)),
        ],
        env=env,
        stream_output=True,
    ):
        raise EsphomeError("Failed to create the Adafruit DFU package")


def _wait_for_port(host: str, present: bool, timeout: float) -> bool:
    import time  # noqa: PLC0415

    from serial.tools.list_ports import comports  # noqa: PLC0415

    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        time.sleep(0.1)
        if (host in {p.device for p in comports()}) == present:
            return True
    return False


def upload_serial_dfu(host: str) -> bool:
    """Touch the port at 1200 baud so the firmware reboots into the bootloader, then send
    firmware.zip over the bootloader's serial port."""
    import time  # noqa: PLC0415

    import serial  # noqa: PLC0415

    from esphome.__main__ import check_permissions  # noqa: PLC0415

    from .. import resolve_zephyr_modules, zephyr_data, zephyr_variant  # noqa: PLC0415
    from ..const import KEY_FRAMEWORK_TYPE  # noqa: PLC0415
    from ..framework_west import check_and_install as west_install  # noqa: PLC0415
    from . import VARIANTS, resolve_sdk  # noqa: PLC0415

    package = CORE.relative_build_path(_DFU_PACKAGE)
    if not package.is_file():
        raise EsphomeError("Firmware not found. Please compile first.")
    _, sdk = resolve_sdk(
        VARIANTS[zephyr_variant()], zephyr_data().get(KEY_FRAMEWORK_TYPE)
    )
    python_bin, _, env = west_install(
        sdk,
        str(CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION]),
        zephyr_data()["west_version"],
        zephyr_data()["ninja_version"],
        zephyr_data()["sdk_source"],
        modules=resolve_zephyr_modules(),
    )
    _ensure_nrfutil(python_bin, env)

    check_permissions(host)
    try:
        serial.Serial(host, baudrate=1200, timeout=1).close()
    except serial.SerialException as err:
        raise EsphomeError(f"Failed to open {host}: {err}") from err
    if not _wait_for_port(host, present=False, timeout=5):
        _LOGGER.warning(
            "Device did not leave %s within 5 s; it may not have entered bootloader mode",
            host,
        )
    if not _wait_for_port(host, present=True, timeout=10):
        raise EsphomeError(
            f"DFU port {host!r} did not reappear within 10 s. "
            "Check that the device entered DFU mode."
        )
    # Wait for udev to finish setting permissions on the new port.
    for _ in range(100):
        try:
            check_permissions(host)
            break
        except EsphomeError:
            time.sleep(0.05)
    else:
        check_permissions(host)
    # Let the bootloader finish starting up before sending the first packet.
    time.sleep(2)
    if not run_command_ok(
        [
            str(python_bin),
            "-m",
            "nordicsemi.__main__",
            "dfu",
            "serial",
            "-pkg",
            str(package),
            "-p",
            host,
            "-b",
            "115200",
            "--singlebank",
        ],
        env=env,
        stream_output=True,
    ):
        raise EsphomeError("nRF52 serial DFU upload failed")
    return True
